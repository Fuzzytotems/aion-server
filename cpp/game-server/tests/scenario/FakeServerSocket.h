#pragma once

// A fake game server socket for the GameSession self-tests (GameSessionFightTest.cpp, GameSessionCastTest.cpp): it accepts one GameSession and
// exchanges frames with it, so a test can assert what went over the wire without a game server or a login server. It speaks only the frame
// layer of FakeGameClient, and it reads the client's frames back with its own copy of the cipher.
//
// Two properties of FakeGameClient make that possible without a key exchange: a session that never read SM_KEY does not decrypt server frames
// (FakeGameClient::readPacket), so the fake server can send them plain; and both cipher keys start at 0 and grow by the body size on each
// packet, so FakeGameClientCrypto::decryptServerBody is the exact inverse of the client's encryptClientBody here.
//
// Moved here unchanged from GameSessionFightTest.cpp when the cast test (m5b2-plan.md G-02) needed the same socket.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include "GameSession.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario::fake {

/** The static server packet code of a server frame (AionServerPacket.write: [u16 size][wire opcode][0x44][~wire opcode][body]) */
inline constexpr uint8_t SERVER_PACKET_CODE = 0x44;
/** ... and of a client frame (FakeGameClient::buildFrame) */
inline constexpr uint8_t CLIENT_PACKET_CODE = 0x65;

struct ClientPacket {
	int32_t opcode = -1;
	std::vector<uint8_t> data;
	std::chrono::steady_clock::time_point at;
};

/** A socket that accepts one GameSession and exchanges frames with it. */
class FakeServer {
public:
	FakeServer() : acceptor(io, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0)) {}

	uint16_t port() const { return acceptor.local_endpoint().port(); }

	void accept() { socket = std::make_unique<network::test::TestSocket>(acceptor); }

	void sendServerPacket(int32_t opcode, std::span<const uint8_t> body = {}) {
		const uint16_t wire = network::test::FakeGameClientCrypto::serverWireOpcode(opcode);
		network::test::PacketWriter frame;
		frame.H(static_cast<int32_t>(2 + 5 + body.size())).H(wire).C(SERVER_PACKET_CODE).H(static_cast<uint16_t>(~wire)).B(body);
		socket->send(frame.data);
	}

	/** @return the next client packet, decrypted and with its opcode decoded, std::nullopt on timeout */
	std::optional<ClientPacket> receive(std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
		std::optional<std::vector<uint8_t>> frame = socket->readFrame(timeout);
		if (!frame)
			return std::nullopt;
		ClientPacket packet;
		packet.at = std::chrono::steady_clock::now();
		std::span<uint8_t> body(frame->data() + 2, frame->size() - 2);
		crypto.decryptServerBody(body); // the inverse of the client's encryptClientBody, both keys starting at 0
		if (body.size() < 5)
			throw std::runtime_error("client frame too short");
		const uint16_t wire = static_cast<uint16_t>(body[0] | body[1] << 8);
		const uint16_t check = static_cast<uint16_t>(body[3] | body[4] << 8);
		if (body[2] != CLIENT_PACKET_CODE || static_cast<uint16_t>(~wire) != check)
			throw std::runtime_error("invalid client frame header");
		// the inverse of FakeGameClientCrypto::clientWireOpcode, which is ((((opcode + 207) ^ 0xEF) + 0x0C) ^ 0xEF)
		const int32_t deobfuscated = ((((wire ^ 0xEF) - 0x0C) & 0xFFFF) ^ 0xEF) & 0xFFFF;
		packet.opcode = deobfuscated - network::test::FakeGameClientCrypto::INTERNAL_VERSION;
		packet.data.assign(body.begin() + 5, body.end());
		return packet;
	}

	/**
	 * Starts a thread that timestamps every client frame as it arrives. It is the only way to measure when a packet was *sent*: a passive
	 * server reads three frames out of its socket buffer in one go, all three timestamped after the call is over. From here on the socket
	 * belongs to that thread until stopReceiving().
	 */
	void startReceiving() {
		stop = false;
		reader = std::thread([this] {
			while (!stop)
				if (std::optional<ClientPacket> packet = receive(std::chrono::milliseconds(50)))
					received.push_back(*packet);
		});
	}

	/** Joins the reader thread and returns what it read, in arrival order */
	std::vector<ClientPacket> stopReceiving() {
		stop = true;
		reader.join();
		return std::move(received);
	}

	void close() { socket->close(); }

private:
	asio::io_context io;
	asio::ip::tcp::acceptor acceptor;
	std::unique_ptr<network::test::TestSocket> socket;
	network::test::FakeGameClientCrypto crypto;
	std::thread reader;
	std::atomic<bool> stop{false};
	std::vector<ClientPacket> received;
};

/** A connected pair: the fake server and the GameSession under test. */
struct SessionFixture {
	FakeServer server;
	std::unique_ptr<GameSession> session;

	SessionFixture() {
		session = std::make_unique<GameSession>(server.port());
		server.accept();
	}
};

} // namespace aion::gameserver::scenario::fake
