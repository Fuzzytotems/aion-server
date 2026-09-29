// The kill helpers lifted into FightSupport.h (m5d-plan.md G-02) against a fake server socket (FakeServerSocket.h): recordFight decodes a
// slice of the recording with the combat decoders and keeps what does not decode, and waitForRespawnAt answers a new object at the spot, never
// one announced there before the death. The bodies are written from the Java writeImpl (SM_ATTACK.java:44-147, SM_ATTACK_STATUS.java:122-158,
// SM_STATUPDATE_HP.java:26-29, SM_ATTACK_RESPONSE.java:45-48, SM_EMOTION.java:93-133, SM_NPC_INFO.writeImpl as VisibilityDecodersTest.cpp
// spells it); the npc is the M5d gate's striped kerub 210133 at two of its Poeta spots (spawns/Npcs/210010000_Poeta.xml:1224-1226). No game
// server is involved and nothing includes a C++ serverpackets header.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <thread>
#include <vector>

#include "FakeServerSocket.h"
#include "FightSupport.h"
#include "GameSession.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {
namespace {

using namespace std::chrono_literals;
using network::test::PacketWriter;
using Fixture = fake::SessionFixture;

/** the Java opcodes of the packets (ServerPacketsOpcodes.java:21, 23, 32, 40, 55, 72, 133); GameSessionTest.ServerPacketNames pins the names */
constexpr int32_t SM_STATUPDATE_HP = 3;
constexpr int32_t SM_ATTACK_STATUS = 5;
constexpr int32_t SM_NPC_INFO = 14;
constexpr int32_t SM_DELETE = 22;
constexpr int32_t SM_EMOTION = 37;
constexpr int32_t SM_ATTACK = 54;
constexpr int32_t SM_ATTACK_RESPONSE = 115;

constexpr int32_t KERUB = 210133;
constexpr int32_t OTHER_KERUB = 210134;
constexpr int32_t PLAYER = 0x400B0001;

/** two spots of 210133 (210010000_Poeta.xml:1225-1226) */
OracleMonsterSpot spot(float x, float y, float z) {
	OracleMonsterSpot result;
	result.x = x;
	result.y = y;
	result.z = z;
	return result;
}
const OracleMonsterSpot SPOT_A = spot(1038.58f, 989.604f, 129.484f);
const OracleMonsterSpot SPOT_B = spot(1050.87f, 996.053f, 131.596f);

/** AttackStatus.NORMALHIT (AttackStatus.java:17): a hit that lands, which takes SM_ATTACK's default counter arm (SM_ATTACK.java:75-83) */
constexpr int32_t NORMALHIT = 10;

/**
 * SM_ATTACK.java:44-147 without a proc effect: one NORMALHIT result per damage, each with shield type 0. The first result is a NORMALHIT, so
 * the counter short is the default arm's writeH(0) (:82); a DODGE (0) there would write 128 (:67-69)
 */
std::vector<uint8_t> attackBytes(int32_t attacker, int32_t target, const std::vector<int32_t>& damages) {
	PacketWriter w;
	w.D(attacker).C(0).H(0).C(0).C(0); // :45-49
	w.D(target).C(90).C(100);          // :51-54
	w.H(0).H(0);                       // :57-85 ATTACK_COUNTER_NONE, :92
	w.C(static_cast<int32_t>(damages.size()));
	for (const int32_t damage : damages)
		w.D(damage).C(NORMALHIT).C(0); // :107-111, shield type 0 writes nothing more
	w.C(0);                            // :146
	return w.data;
}

/**
 * SM_ATTACK_STATUS(creature, value) (SM_ATTACK_STATUS.java:117-119): TYPE.REGULAR (5), skill 0 and LOG.REGULAR (191). REGULAR takes the
 * `default:` arm, which writes the value unchanged (:149-151), then the type, the HP percentage, the skill, the log and no critical (:153-157)
 */
std::vector<uint8_t> attackStatusBytes(int32_t creature, int32_t damage) {
	return PacketWriter().D(creature).D(damage).C(5).C(80).H(0).C(191).C(0).data;
}

/** SM_NPC_INFO.writeImpl, VisibilityDecodersTest.cpp's npcInfoBytes without gear */
std::vector<uint8_t> npcInfoBytes(int32_t objectId, int32_t templateId, const OracleMonsterSpot& at) {
	PacketWriter w;
	w.F(at.x).F(at.y).F(at.z);
	w.D(objectId);
	w.D(templateId).D(templateId);
	w.C(38).H(65).C(93);
	w.D(templateId + 1).D(0);
	w.H(0).C(0).D(0);
	w.D(0).S("");
	w.C(100).D(1234).C(2);
	w.D(0); // no gear
	w.F(1.5f).F(2.5f).F(6.0f).H(1500).H(1500);
	w.C(0);
	w.F(at.x).F(at.y).F(at.z);
	w.C(0).H(0);
	for (int i = 0; i < 8; i++)
		w.C(0);
	w.C(0).H(1).C(0).D(0).D(0).D(0);
	return w.data;
}

/** reads `count` packets the fake server sent into the session's recording */
void readInto(GameSession& session, size_t count) {
	for (size_t i = 0; i < count; i++)
		ASSERT_TRUE(session.next(5s).has_value()) << "packet " << i;
}

// ---- recordFight ------------------------------------------------------------------------------------------------------------------------

TEST(FightSupportTest, RecordFightDecodesTheSliceFromItsStart) {
	Fixture fixture;
	const int32_t kerub = 0x40010001;
	fixture.server.sendServerPacket(SM_ATTACK, attackBytes(PLAYER, kerub, {99})); // 0: before the slice
	fixture.server.sendServerPacket(SM_ATTACK, attackBytes(PLAYER, kerub, {12, 5}));
	fixture.server.sendServerPacket(SM_ATTACK_STATUS, attackStatusBytes(kerub, 17));
	fixture.server.sendServerPacket(SM_ATTACK, attackBytes(kerub, PLAYER, {7}));
	fixture.server.sendServerPacket(SM_STATUPDATE_HP, PacketWriter().D(53).D(60).data);
	fixture.server.sendServerPacket(SM_DELETE, PacketWriter().D(kerub).C(0).data); // not a fight packet: skipped
	fixture.server.sendServerPacket(SM_ATTACK_RESPONSE, std::vector<uint8_t>{4, 9});
	fixture.server.sendServerPacket(SM_EMOTION, PacketWriter().D(kerub).C(18).H(0).F(0.0f).D(PLAYER).data);
	fixture.server.sendServerPacket(SM_ATTACK_STATUS, PacketWriter().D(kerub).data); // 8: does not decode
	// one at a time, 2 ms apart, so that every packet has its own receivedAt and an `at` taken from the wrong packet shows
	for (size_t i = 0; i < 9; i++) {
		readInto(*fixture.session, 1);
		std::this_thread::sleep_for(2ms);
	}
	const std::vector<GameSession::Packet>& recorded = fixture.session->recorded();
	ASSERT_EQ(recorded.size(), 9u);
	for (size_t i = 1; i < recorded.size(); i++)
		ASSERT_LT(recorded[i - 1].receivedAt, recorded[i].receivedAt) << "packet " << i;

	const FightRecording recording = recordFight(*fixture.session, 1);
	ASSERT_EQ(recording.attacks.size(), 2u) << "the attack before `from` is not in the slice";
	EXPECT_EQ(recording.attacks[0].index, 1u);
	EXPECT_EQ(recording.attacks[0].totalDamage, 17) << "the sum of the results' damage";
	EXPECT_EQ(recording.attacks[1].index, 3u);
	EXPECT_EQ(recording.attacks[1].attack.attackerObjectId, kerub);
	EXPECT_EQ(recording.attacks[0].at, recorded[1].receivedAt);
	EXPECT_EQ(recording.attacks[1].at, recorded[3].receivedAt);
	ASSERT_EQ(recording.attacksBy(PLAYER).size(), 1u);
	EXPECT_EQ(recording.attacksBy(PLAYER)[0].index, 1u);
	EXPECT_EQ(recording.attacksBetween(kerub, PLAYER).size(), 1u);
	EXPECT_TRUE(recording.attacksBetween(PLAYER, PLAYER).empty());

	// the index and the time of every entry are its own packet's: M5b's A6 merges the statuses and the HP updates by index
	ASSERT_EQ(recording.statuses.size(), 1u);
	EXPECT_EQ(recording.statuses[0].index, 2u);
	EXPECT_EQ(recording.statuses[0].at, recorded[2].receivedAt);
	EXPECT_EQ(recording.statuses[0].status.value, 17);
	EXPECT_EQ(recording.statusesOf(kerub).size(), 1u);
	EXPECT_TRUE(recording.statusesOf(PLAYER).empty());
	ASSERT_EQ(recording.hpUpdates.size(), 1u);
	EXPECT_EQ(recording.hpUpdates[0].index, 4u);
	EXPECT_EQ(recording.hpUpdates[0].at, recorded[4].receivedAt);
	EXPECT_EQ(recording.hpUpdates[0].hp.currentHp, 53);
	EXPECT_EQ(recording.hpUpdates[0].hp.maxHp, 60);
	ASSERT_EQ(recording.responses.size(), 1u);
	EXPECT_EQ(recording.responses[0].first, 6u);
	EXPECT_EQ(recording.responses[0].second.attackCount, 9);
	ASSERT_EQ(recording.emotions.size(), 1u);
	EXPECT_EQ(recording.emotions[0].first, 7u);
	EXPECT_EQ(recording.emotions[0].second.targetObjectId, PLAYER);

	ASSERT_EQ(recording.decodeFailures.size(), 1u) << "a body that does not decode is kept, not dropped";
	EXPECT_EQ(recording.decodeFailures[0].rfind("SM_ATTACK_STATUS at 8: ", 0), 0u) << recording.decodeFailures[0];

	EXPECT_EQ(recordFight(*fixture.session, 0).attacks.size(), 3u);
	EXPECT_TRUE(recordFight(*fixture.session, 9).attacks.empty()) << "an empty slice";
}

// ---- waitForRespawnAt -------------------------------------------------------------------------------------------------------------------

TEST(FightSupportTest, TheRespawnIsANewObjectAtTheSpot) {
	Fixture fixture;
	constexpr int32_t earlierCorpse = 0x40020001, dead = 0x40020002, respawn = 0x40020003;
	fixture.server.sendServerPacket(SM_NPC_INFO, npcInfoBytes(earlierCorpse, KERUB, SPOT_A)); // 0: killed by an earlier case
	fixture.server.sendServerPacket(SM_NPC_INFO, npcInfoBytes(dead, KERUB, SPOT_A));          // 1: the one that dies
	readInto(*fixture.session, 2);
	const size_t diedAt = fixture.session->recorded().size();
	// after the death: the earlier corpse comes back into view, npcs that are not the kerub at the spot - another template, another spot, and
	// the same template at the spot's x and y but 2 m higher (every axis counts) - then the respawn
	OracleMonsterSpot aboveA = SPOT_A;
	aboveA.z += 2.0f;
	fixture.server.sendServerPacket(SM_NPC_INFO, npcInfoBytes(earlierCorpse, KERUB, SPOT_A));
	fixture.server.sendServerPacket(SM_NPC_INFO, npcInfoBytes(0x40020010, OTHER_KERUB, SPOT_A));
	fixture.server.sendServerPacket(SM_NPC_INFO, npcInfoBytes(0x40020011, KERUB, SPOT_B));
	fixture.server.sendServerPacket(SM_NPC_INFO, npcInfoBytes(0x40020012, KERUB, aboveA));
	fixture.server.sendServerPacket(SM_DELETE, PacketWriter().D(dead).C(0).data);
	fixture.server.sendServerPacket(SM_NPC_INFO, npcInfoBytes(respawn, KERUB, SPOT_A));

	const std::optional<int32_t> found = waitForRespawnAt(*fixture.session, SPOT_A, KERUB, diedAt, 5s);
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(*found, respawn) << "neither the corpse announced again nor another template, spot or height";
	EXPECT_EQ(fixture.session->recorded().size(), diedAt + 6) << "the call read up to the respawn and recorded everything on the way";

	// asked again, the respawn is already recorded and is answered without reading
	EXPECT_EQ(waitForRespawnAt(*fixture.session, SPOT_A, KERUB, diedAt, 1ms), respawn);
	// a death at the respawn's own index sees it as announced before
	EXPECT_FALSE(waitForRespawnAt(*fixture.session, SPOT_A, KERUB, diedAt + 6, 300ms).has_value());
	// the other template at the same spot has its own respawn answer
	EXPECT_EQ(waitForRespawnAt(*fixture.session, SPOT_A, OTHER_KERUB, diedAt - 1, 1ms), 0x40020010);
}

TEST(FightSupportTest, WaitForRespawnAtWaitsForTheSocketAndGivesUp) {
	Fixture fixture;
	fixture.server.sendServerPacket(SM_NPC_INFO, npcInfoBytes(0x40030001, KERUB, SPOT_B));
	readInto(*fixture.session, 1);

	// the respawn is in the socket, not yet in the recording: the call has to read it
	fixture.server.sendServerPacket(SM_NPC_INFO, npcInfoBytes(0x40030002, KERUB, SPOT_B));
	EXPECT_EQ(waitForRespawnAt(*fixture.session, SPOT_B, KERUB, 1, 5s), 0x40030002) << "the respawn read from the socket";
	EXPECT_EQ(fixture.session->recorded().size(), 2u);

	const auto timeoutStart = std::chrono::steady_clock::now();
	EXPECT_FALSE(waitForRespawnAt(*fixture.session, SPOT_A, KERUB, 2, 300ms).has_value()) << "nothing at spot A";
	EXPECT_GE(std::chrono::steady_clock::now() - timeoutStart, 250ms);

	fixture.server.close();
	const auto closedStart = std::chrono::steady_clock::now();
	EXPECT_FALSE(waitForRespawnAt(*fixture.session, SPOT_A, KERUB, 2, 10s).has_value());
	EXPECT_LT(std::chrono::steady_clock::now() - closedStart, 5s) << "a closed connection is not waited out";
}

} // namespace
} // namespace aion::gameserver::scenario
