// GameSession::fightUntil (m5b-plan.md G-02) against a fake server socket: the pace of the CM_ATTACKs, the predicate that ends the fight, the
// timeout, the attack limit and a closed connection. No game server and no login server are involved - the fake server (FakeServerSocket.h)
// speaks only the frame layer of FakeGameClient, and it reads the client's frames back with its own copy of the cipher, so what the test
// asserts is what went over the wire.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

#include "FakeServerSocket.h"
#include "GameSession.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {
namespace {

using namespace std::chrono_literals;
using fake::ClientPacket;
using network::test::PacketReader;
using network::test::PacketWriter;
using FightFixture = fake::SessionFixture;

/** SM_NPC_INFO, a server packet the fight really sees; any opcode would do here (GameSessionTest.ServerPacketNames pins the name) */
constexpr int32_t SM_NPC_INFO = 14;

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
