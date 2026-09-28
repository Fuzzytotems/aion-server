// GameSession::castAndWait (m5b2-plan.md G-02) against the fake server socket of FakeServerSocket.h: what goes over the wire, which packets end
// the wait - the CASTER's SM_CASTSPELL_RESULT or SM_SKILL_CANCEL of the SKILL, never another creature's or another skill's - the interruption
// that X5 sends into a running cast, the timeout of a refused cast and a closed connection. No game server is involved; the server packets are
// written here field by field from the Java writeImpl, the same way SkillDecodersTest.cpp builds its bodies.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

#include "FakeServerSocket.h"
#include "GameSession.h"
#include "decoders/PacketDecoders.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {
namespace {

using namespace std::chrono_literals;
using fake::ClientPacket;
using fake::SessionFixture;
using network::test::PacketReader;
using network::test::PacketWriter;

/** the opcodes of the generated server table (GameSessionTest.ServerPacketNames pins the names) */
constexpr int32_t SM_NPC_INFO = 14;
constexpr int32_t SM_CASTSPELL = 33;
constexpr int32_t SM_SKILL_CANCEL = 42;
constexpr int32_t SM_CASTSPELL_RESULT = 43;

constexpr int32_t MAGE = 0x00A13001;
constexpr int32_t MONSTER = 0x00A13002;

/** SM_CASTSPELL.java:47-78 with an object target */
std::vector<uint8_t> castSpell(int32_t effector, uint16_t spellId, uint16_t castDuration) {
	return PacketWriter().D(effector).H(spellId).C(1).C(0).D(MONSTER).H(castDuration).C(0).F(1.0f).C(1).data;
}

/** SM_CASTSPELL_RESULT.java:52-121 with an object target and an empty effect list (status byte 16) */
std::vector<uint8_t> castSpellResult(int32_t effector, uint16_t skillId) {
	return PacketWriter().D(effector).C(0).D(MONSTER).H(skillId).C(1).D(0).H(0).C(0).C(16).C(0).C(0).H(0).data;
}

/** SM_SKILL_CANCEL.java:22-23 */
std::vector<uint8_t> skillCancel(int32_t creature, uint16_t skillId) {
	return PacketWriter().D(creature).H(skillId).data;
}

GameSession::CastRequest flameBolt() {
	GameSession::CastRequest request;
	request.spellId = 1282;
	request.level = 1;
	request.targetType = 0;
	request.targetObjectId = MONSTER;
	request.hitTime = 1100;
	return request;
}

TEST(GameSessionCastTest, CastAndWaitSendsTheCastAndStopsAtTheCastersResult) {
	SessionFixture fixture;
	// everything a fight around the cast can put in between, in front of the two packets that matter
	fixture.server.sendServerPacket(SM_NPC_INFO, PacketWriter().D(1).data);
	fixture.server.sendServerPacket(SM_CASTSPELL, castSpell(MONSTER, 16419, 2500));        // the monster casts too (X9)
	fixture.server.sendServerPacket(SM_CASTSPELL, castSpell(MONSTER, 1282, 2500));         // ... even the same skill id
	fixture.server.sendServerPacket(SM_CASTSPELL, castSpell(MAGE, 1328, 0));               // another skill of the caster
	fixture.server.sendServerPacket(SM_CASTSPELL, castSpell(MAGE, 1282, 2000));            // the cast bar
	fixture.server.sendServerPacket(SM_CASTSPELL_RESULT, castSpellResult(MONSTER, 1282));  // the same skill id from somebody else
	fixture.server.sendServerPacket(SM_CASTSPELL_RESULT, castSpellResult(MAGE, 1328));     // the caster's other skill
	fixture.server.sendServerPacket(SM_SKILL_CANCEL, skillCancel(MONSTER, 1282));          // a cancel of another creature
	fixture.server.sendServerPacket(SM_CASTSPELL_RESULT, castSpellResult(MAGE, 1282));     // the end of the cast
	fixture.server.sendServerPacket(SM_NPC_INFO, PacketWriter().D(2).data);                 // after the end: left for the caller

	const GameSession::CastOutcome outcome = fixture.session->castAndWait(MAGE, flameBolt(), 5s);
	EXPECT_TRUE(outcome.ended());
	EXPECT_FALSE(outcome.closed);
	EXPECT_FALSE(outcome.skillCancel.has_value());
	EXPECT_FALSE(outcome.interruptionSentAt.has_value());
	ASSERT_TRUE(outcome.castSpell.has_value());
	ASSERT_TRUE(outcome.castSpellResult.has_value());
	EXPECT_EQ(*outcome.castSpell, outcome.firstPacket + 4) << "the caster's SM_CASTSPELL of THIS skill, not the monster's or another skill's";
	EXPECT_EQ(*outcome.castSpellResult, outcome.firstPacket + 8) << "the caster's result of this skill ends the wait";
	const auto& recorded = fixture.session->recorded();
	ASSERT_EQ(recorded.size(), outcome.firstPacket + 9) << "every packet up to the end is recorded, and nothing after it is read";
	EXPECT_EQ(recorded[*outcome.castSpellResult].name, "SM_CASTSPELL_RESULT");
	EXPECT_LT(outcome.elapsed, 5s);

	// what went over the wire: one CM_CASTSPELL with the request's fields in CM_CASTSPELL.readImpl order
	const std::optional<ClientPacket> sent = fixture.server.receive(2s);
	ASSERT_TRUE(sent.has_value());
	EXPECT_EQ(sent->opcode, GameSession::CM_CASTSPELL);
	EXPECT_EQ(sent->data, GameSession::buildCM_CASTSPELL(1282, 1, 0, MONSTER, 1100));
	EXPECT_FALSE(fixture.server.receive(200ms).has_value()) << "nothing else is sent without an interruption";

	// the packet after the end is still there for the caller
	const std::optional<GameSession::Packet> after = fixture.session->next(2s);
	ASSERT_TRUE(after.has_value());
	EXPECT_EQ(after->name, "SM_NPC_INFO");
}

TEST(GameSessionCastTest, CastAndWaitStopsAtTheCastersCancel) {
	SessionFixture fixture;
	fixture.server.sendServerPacket(SM_CASTSPELL, castSpell(MAGE, 1282, 2000));
	fixture.server.sendServerPacket(SM_SKILL_CANCEL, skillCancel(MAGE, 1328)); // another skill's cancel does not end this cast
	fixture.server.sendServerPacket(SM_SKILL_CANCEL, skillCancel(MAGE, 1282));

	const GameSession::CastOutcome outcome = fixture.session->castAndWait(MAGE, flameBolt(), 5s);
	EXPECT_TRUE(outcome.ended());
	EXPECT_FALSE(outcome.castSpellResult.has_value()) << "X5: a cancelled cast has no result";
	ASSERT_TRUE(outcome.skillCancel.has_value());
	EXPECT_EQ(*outcome.skillCancel, outcome.firstPacket + 2);
	ASSERT_TRUE(outcome.castSpell.has_value());
	EXPECT_EQ(*outcome.castSpell, outcome.firstPacket);
}

TEST(GameSessionCastTest, CastAndWaitSendsTheInterruptionIntoTheRunningCast) {
	// X5: a CM_MOVE 300 ms into the cast. Nothing answers here, so the call runs into its timeout, and the reader thread timestamps both packets.
	SessionFixture fixture;
	fixture.server.startReceiving();
	GameSession::CastInterruption move;
	move.after = 300ms;
	move.opcode = GameSession::CM_MOVE;
	move.body = GameSession::buildCM_MOVE(1.0f, 2.0f, 3.0f, 0, 0);
	const GameSession::CastOutcome outcome = fixture.session->castAndWait(MAGE, flameBolt(), 900ms, move);
	const std::vector<ClientPacket> sent = fixture.server.stopReceiving();

	EXPECT_FALSE(outcome.ended()) << "a cast nobody answers never ends";
	EXPECT_FALSE(outcome.castSpell.has_value());
	EXPECT_FALSE(outcome.closed);
	EXPECT_GE(outcome.elapsed, 900ms - 30ms) << "the timeout is waited out";
	EXPECT_LT(outcome.elapsed, 5s);
	ASSERT_TRUE(outcome.interruptionSentAt.has_value());
	const auto sentAfter = std::chrono::duration_cast<std::chrono::milliseconds>(*outcome.interruptionSentAt - outcome.sentAt);
	EXPECT_GE(sentAfter, 300ms);
	EXPECT_LT(sentAfter, 300ms + 200ms) << "the interruption was late";

	ASSERT_EQ(sent.size(), 2u) << "the cast and the interruption, once";
	EXPECT_EQ(sent[0].opcode, GameSession::CM_CASTSPELL);
	EXPECT_EQ(sent[1].opcode, GameSession::CM_MOVE);
	EXPECT_EQ(sent[1].data, move.body);
	const auto gap = std::chrono::duration_cast<std::chrono::milliseconds>(sent[1].at - sent[0].at);
	EXPECT_GE(gap, 300ms - 30ms) << "the server saw the move 300 ms into the cast";
	EXPECT_LT(gap, 300ms + 200ms);
}

TEST(GameSessionCastTest, NoInterruptionAfterTheCastEnded) {
	SessionFixture fixture;
	fixture.server.sendServerPacket(SM_CASTSPELL, castSpell(MAGE, 1282, 0));
	fixture.server.sendServerPacket(SM_CASTSPELL_RESULT, castSpellResult(MAGE, 1282));
	GameSession::CastInterruption move;
	move.after = 200ms;
	move.opcode = GameSession::CM_MOVE;
	move.body = GameSession::buildCM_MOVE(1.0f, 2.0f, 3.0f, 0, 0);
	const GameSession::CastOutcome outcome = fixture.session->castAndWait(MAGE, flameBolt(), 5s, move);
	EXPECT_TRUE(outcome.castSpellResult.has_value());
	EXPECT_FALSE(outcome.interruptionSentAt.has_value()) << "an instant cast ended before the interruption was due";
	ASSERT_TRUE(fixture.server.receive(2s).has_value());
	EXPECT_FALSE(fixture.server.receive(400ms).has_value()) << "only the CM_CASTSPELL went out";
}

TEST(GameSessionCastTest, CastAndWaitNoticesAClosedConnectionAndRejectsABadBody) {
	{
		SessionFixture fixture;
		fixture.server.close();
		const GameSession::CastOutcome outcome = fixture.session->castAndWait(MAGE, flameBolt(), 10s);
		EXPECT_TRUE(outcome.closed);
		EXPECT_FALSE(outcome.ended());
		EXPECT_LT(outcome.elapsed, 5s) << "a closed connection must not be waited out";
	}
	{
		// the three packets are decoded with the independent decoders, so a body the Java writeImpl cannot produce fails the wait
		SessionFixture fixture;
		std::vector<uint8_t> bad = castSpell(MAGE, 1282, 2000);
		bad.push_back(0);
		fixture.server.sendServerPacket(SM_CASTSPELL, bad);
		EXPECT_THROW(fixture.session->castAndWait(MAGE, flameBolt(), 5s), decoders::DecodeError);
	}
}

} // namespace
} // namespace aion::gameserver::scenario
