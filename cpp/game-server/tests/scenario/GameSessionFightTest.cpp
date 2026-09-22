// GameSession::fightUntil (m5b-plan.md G-02) against a fake server socket: the pace of the CM_ATTACKs, the predicate that ends the fight, the
// timeout, the attack limit and a closed connection. No game server and no login server are involved - the fake server here speaks only the
// frame layer of FakeGameClient, and it reads the client's frames back with its own copy of the cipher, so what the test asserts is what went
// over the wire.
//
// Two properties of FakeGameClient make that possible without a key exchange: a session that never read SM_KEY does not decrypt server frames
// (FakeGameClient::readPacket), so the fake server can send them plain; and both cipher keys start at 0 and grow by the body size on each
// packet, so FakeGameClientCrypto::decryptServerBody is the exact inverse of the client's encryptClientBody here.

#include <gtest/gtest.h>

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

namespace aion::gameserver::scenario {
namespace {

using namespace std::chrono_literals;
using network::test::FakeGameClientCrypto;
using network::test::PacketReader;
using network::test::PacketWriter;
using network::test::TestSocket;

/** The static server packet code of a server frame (AionServerPacket.write: [u16 size][wire opcode][0x44][~wire opcode][body]) */
constexpr uint8_t SERVER_PACKET_CODE = 0x44;
/** ... and of a client frame (FakeGameClient::buildFrame) */
constexpr uint8_t CLIENT_PACKET_CODE = 0x65;
/** SM_NPC_INFO, a server packet the fight really sees; any opcode would do here (GameSessionTest.ServerPacketNames pins the name) */
constexpr int32_t SM_NPC_INFO = 14;

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

	void accept() { socket = std::make_unique<TestSocket>(acceptor); }

	void sendServerPacket(int32_t opcode, std::span<const uint8_t> body = {}) {
		const uint16_t wire = FakeGameClientCrypto::serverWireOpcode(opcode);
		PacketWriter frame;
		frame.H(static_cast<int32_t>(2 + 5 + body.size())).H(wire).C(SERVER_PACKET_CODE).H(static_cast<uint16_t>(~wire)).B(body);
		socket->send(frame.data);
	}

	/** @return the next client packet, decrypted and with its opcode decoded, std::nullopt on timeout */
	std::optional<ClientPacket> receive(std::chrono::milliseconds timeout = 5s) {
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
		packet.opcode = deobfuscated - FakeGameClientCrypto::INTERNAL_VERSION;
		packet.data.assign(body.begin() + 5, body.end());
		return packet;
	}

	/**
	 * Starts a thread that timestamps every client frame as it arrives. It is the only way to measure when an attack was *sent*: a passive
	 * server reads three frames out of its socket buffer in one go, all three timestamped after the fight is over. From here on the socket
	 * belongs to that thread until stopReceiving().
	 */
	void startReceiving() {
		stop = false;
		reader = std::thread([this] {
			while (!stop)
				if (std::optional<ClientPacket> packet = receive(50ms))
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
	std::unique_ptr<TestSocket> socket;
	FakeGameClientCrypto crypto;
	std::thread reader;
	std::atomic<bool> stop{false};
	std::vector<ClientPacket> received;
};

/** A connected pair: the fake server and the GameSession under test. */
struct FightFixture {
	FakeServer server;
	std::unique_ptr<GameSession> session;

	FightFixture() {
		session = std::make_unique<GameSession>(server.port());
		server.accept();
	}
};

TEST(GameSessionFightTest, FightUntilPacesTheAttacksAtTheAttackSpeed) {
	constexpr auto attackSpeed = 200ms;
	FightFixture fixture;
	fixture.server.startReceiving();
	GameSession::FightOutcome outcome = fixture.session->fightUntil(0x1234, attackSpeed, [](const GameSession::Packet&) { return false; }, 5s, 3);
	std::vector<ClientPacket> attacks = fixture.server.stopReceiving();

	EXPECT_FALSE(outcome.done);
	EXPECT_FALSE(outcome.closed);
	EXPECT_EQ(outcome.attacksSent, 3);
	// three attacks (at 0, 1 and 2 intervals) plus the interval the third is given to be answered in
	EXPECT_GE(outcome.elapsed, 3 * attackSpeed - 60ms);
	ASSERT_EQ(attacks.size(), 3u) << "maxAttacks is 3";

	for (size_t i = 0; i < attacks.size(); i++) {
		EXPECT_EQ(attacks[i].opcode, GameSession::CM_ATTACK);
		PacketReader body(attacks[i].data);
		EXPECT_EQ(body.D(), 0x1234);                            // target object id
		EXPECT_EQ(body.C(), static_cast<int>(i));               // attackno: the running count of the call
		EXPECT_EQ(static_cast<uint16_t>(body.H()), 0);          // time
		EXPECT_EQ(body.C(), 0);                                 // type
		EXPECT_EQ(body.remaining(), 0u);
		if (i > 0) {
			// what the pace is for: PlayerController.attackTarget rejects an attack that is more than 300 ms early
			const auto gap = std::chrono::duration_cast<std::chrono::milliseconds>(attacks[i].at - attacks[i - 1].at);
			EXPECT_GE(gap, attackSpeed - GameSession::ATTACK_INTERVAL_TOLERANCE) << "attack " << i << " would be rejected as a hack";
			EXPECT_GE(gap, attackSpeed - 30ms) << "attack " << i << " was not paced at the attack speed";
			EXPECT_LT(gap, attackSpeed * 4) << "attack " << i << " was far too late";
		}
	}
}

TEST(GameSessionFightTest, FightUntilStopsAtTheFirstPacketThePredicateAccepts) {
	FightFixture fixture;
	const std::vector<uint8_t> first = PacketWriter().D(1).data;
	const std::vector<uint8_t> second = PacketWriter().D(2).data;
	fixture.server.sendServerPacket(SM_NPC_INFO, first);
	fixture.server.sendServerPacket(SM_NPC_INFO, second);

	int seen = 0;
	GameSession::FightOutcome outcome = fixture.session->fightUntil(7, 2s, [&](const GameSession::Packet& packet) {
		seen++;
		return packet.data == second;
	}, 5s, 10);

	EXPECT_TRUE(outcome.done);
	EXPECT_EQ(seen, 2) << "the predicate sees every packet, in arrival order, exactly once";
	EXPECT_EQ(outcome.attacksSent, 1) << "the fight ended inside the first interval";
	EXPECT_LT(outcome.elapsed, 2s) << "it did not wait for the next attack";
	// the packets are recorded like every other server packet, from firstPacket on
	ASSERT_EQ(fixture.session->recorded().size(), outcome.firstPacket + 2);
	EXPECT_EQ(fixture.session->recorded()[outcome.firstPacket].name, "SM_NPC_INFO");
	EXPECT_EQ(fixture.session->recorded()[outcome.firstPacket + 1].data, second);
}

TEST(GameSessionFightTest, FightUntilRespectsTheTimeoutAndNoticesAClosedConnection) {
	{
		FightFixture fixture;
		GameSession::FightOutcome outcome =
			fixture.session->fightUntil(7, 10s, [](const GameSession::Packet&) { return false; }, 300ms, 60);
		EXPECT_FALSE(outcome.done);
		EXPECT_EQ(outcome.attacksSent, 1) << "the second attack was not due before the timeout";
		EXPECT_GE(outcome.elapsed, 300ms - 30ms);
		EXPECT_LT(outcome.elapsed, 3s);
	}
	{
		FightFixture fixture;
		fixture.server.close();
		GameSession::FightOutcome outcome =
			fixture.session->fightUntil(7, 10s, [](const GameSession::Packet&) { return false; }, 10s, 60);
		EXPECT_TRUE(outcome.closed);
		EXPECT_FALSE(outcome.done);
		EXPECT_LT(outcome.elapsed, 5s) << "a closed connection must not be waited out";
	}
}

} // namespace
} // namespace aion::gameserver::scenario
