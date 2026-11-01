#pragma once

// FakeGameClient (handlers-and-porting-plan.md §3.1 item 4): the client side of the Aion 4.8 game connection for tests that drive the game server
// like the real game client - key exchange, client packet encryption (the inverse of the server's decrypt), server packet decryption and frame
// validation. The protocol itself (cipher, opcode obfuscation, frames) is the client protocol library's GameClientCrypto (cpp/client-protocol),
// which the UE client uses too; this class adds the blocking test socket and gtest expectations.
// C++ only, test support of P4-15 (it belongs in tests/support once the manifest has a TEST_SUPPORT entry for it).

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "NetworkTestSupport.h"
#include "aion/clientprotocol/GameClientCrypto.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::test {

/** The client's cipher state, opcode obfuscation and frames live in the client protocol library (cpp/client-protocol) */
using FakeGameClientCrypto = clientprotocol::GameClientCrypto;

/** A blocking fake Aion game client connected to a game server port. Not thread safe. */
class FakeGameClient {
public:
	/** A decrypted and validated server packet */
	using ServerPacket = clientprotocol::ServerPacket;

	explicit FakeGameClient(uint16_t port) : socket(port) {}

	/**
	 * Reads SM_KEY (the unencrypted first frame) and derives the key. @return the key as sent (enciphered)
	 * @throws std::runtime_error if the first frame is no valid SM_KEY
	 */
	int32_t readKey(std::chrono::milliseconds timeout = 5s) {
		auto frame = socket.readFrame(timeout);
		if (!frame)
			throw std::runtime_error("no SM_KEY received");
		ServerPacket key = parseServerFrame(*frame);
		if (key.opcode != FakeGameClientCrypto::SM_KEY_OPCODE || key.data.size() != 4)
			throw std::runtime_error("the first server packet is no SM_KEY: opcode " + std::to_string(key.opcode));
		const int32_t sentKey = PacketReader(key.data).D();
		crypto.setBaseKey(FakeGameClientCrypto::decipherKey(sentKey));
		keyReceived = true;
		return sentKey;
	}

	/** Builds a client frame [u16 length][wire opcode][0x65][~wire opcode][data], encrypting everything after the length */
	std::vector<uint8_t> buildFrame(int32_t opcode, std::span<const uint8_t> data, bool advanceKey = true) {
		return crypto.buildClientFrame(opcode, data, advanceKey);
	}

	/** Sends an encrypted client packet with the given (decoded) opcode */
	void sendPacket(int32_t opcode, std::span<const uint8_t> data = {}) { socket.send(buildFrame(opcode, data)); }

	/**
	 * Sends a frame the server cannot validate after decryption (byte 2 is not 0x65) without advancing the client key, like the server that
	 * does not advance its client key for rejected packets.
	 */
	void sendCorruptPacket(size_t size = 16) {
		std::vector<uint8_t> frame(2 + size, uint8_t{0x11});
		frame[0] = static_cast<uint8_t>(frame.size());
		frame[1] = static_cast<uint8_t>(frame.size() >> 8);
		crypto.encryptClientBody(std::span<uint8_t>(frame).subspan(2), false);
		socket.send(frame);
	}

	/** Sends bytes as they are */
	void sendRaw(std::span<const uint8_t> bytes) { socket.send(bytes); }

	/** @return the next server packet (decrypted after SM_KEY), std::nullopt on timeout or close */
	std::optional<ServerPacket> readPacket(std::chrono::milliseconds timeout = 5s) {
		auto frame = socket.readFrame(timeout);
		if (!frame)
			return std::nullopt;
		if (keyReceived)
			crypto.decryptServerBody(std::span<uint8_t>(*frame).subspan(2));
		return parseServerFrame(*frame);
	}

	/** @return the next server packet, failing the test if there is none or it has another opcode */
	ServerPacket expectPacket(int32_t opcode, std::chrono::milliseconds timeout = 5s) {
		std::optional<ServerPacket> packet = readPacket(timeout);
		if (!packet) {
			ADD_FAILURE() << "expected server packet " << opcode << " but got none (closed: " << socket.isClosed() << ")";
			return ServerPacket{};
		}
		EXPECT_EQ(packet->opcode, opcode) << "unexpected server packet";
		return *packet;
	}

	/** Validates the 5 header bytes of a (decrypted) server frame. @throws std::runtime_error if they are inconsistent */
	static ServerPacket parseServerFrame(std::span<const uint8_t> frame) { return clientprotocol::GameClientCrypto::parseServerFrame(frame); }

	TestSocket socket;
	FakeGameClientCrypto crypto;
	bool keyReceived = false;
};

} // namespace aion::gameserver::network::test
