// AsyncAllowed (m5a-plan.md §5.9) and its use as the PacketSequence predicate.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "AsyncAllowed.h"
#include "PacketSequence.h"
#include "decoders/PacketDecoders.h"

#include "NetworkTestSupport.h"

namespace aion::gameserver::scenario {
namespace {

using network::test::PacketWriter;

/** what PacketSequence::match and AsyncAllowed::predicate need of a recorded packet (the shape of GameSession::Packet) */
struct Recorded {
	std::string name;
	std::vector<uint8_t> data;
};

std::vector<uint8_t> playerState(int32_t objectId) {
	return PacketWriter().D(objectId).C(0).C(0).C(0).data;
}

std::vector<uint8_t> npcInfo(int32_t objectId) {
	return PacketWriter().F(1).F(2).F(3).D(objectId).D(0).data;
}

std::vector<uint8_t> deletePacket(int32_t objectId) {
	return PacketWriter().D(objectId).C(0).data;
}

std::vector<uint8_t> systemMessage(int32_t messageId) {
	return PacketWriter().C(0).C(0).D(0).D(messageId).C(0).C(0).data;
}

/** SM_MOVE.java:37-42: object id, x, y, z, heading, movement mask (the mask 0 writes nothing after itself) */
std::vector<uint8_t> move(int32_t objectId) {
	return PacketWriter().D(objectId).F(1).F(2).F(3).C(0).C(0).data;
}

/**
 * SM_EMOTION.java:94-97: object id, emotion type, state, speed - the whole body for an emotion type with an empty switch arm. CHANGE_SPEED
 * adds the two attack speeds and a zero byte (SM_EMOTION.java:170-175), which is 16 bytes instead of 11.
 */
std::vector<uint8_t> emotion(int32_t objectId, uint8_t emotionType) {
	PacketWriter writer;
	writer.D(objectId).C(emotionType).H(0).F(1.5f);
	if (emotionType == decoders::EMOTION_CHANGE_SPEED)
		writer.H(1500).H(1500).C(0);
	return writer.data;
}

/** SM_LOOKATOBJECT.java:24-26: who looks, what it looks at, its heading */
std::vector<uint8_t> lookAt(int32_t objectId, int32_t targetObjectId) {
	return PacketWriter().D(objectId).D(targetObjectId).C(30).data;
}

/** SM_ATTACK.java:70-76: attacker, attackno, time, the two animations, target (the rest of the body is not read here) */
std::vector<uint8_t> attack(int32_t attackerObjectId, int32_t targetObjectId) {
	return PacketWriter().D(attackerObjectId).C(1).H(500).C(0).C(0).D(targetObjectId).C(100).C(100).H(0).H(0).C(0).data;
}

/** SM_ATTACK_STATUS.java:60: the creature whose HP or MP changed, then the value, the type, the percentage, a skill id and two flags */
std::vector<uint8_t> attackStatus(int32_t objectId) {
	return PacketWriter().D(objectId).D(-17).C(0).C(62).H(0).C(0).C(0).data;
}

TEST(AsyncAllowedTest, TheDefaultSetIsTheUnconditionalPartOfThePlan) {
	const AsyncAllowed allowed = AsyncAllowed::m5aDefault();
	EXPECT_EQ(allowed.unconditionalNames(), (std::vector<std::string>{"SM_GAME_TIME", "SM_PONG", "SM_WEATHER"}));
	EXPECT_TRUE(allowed.allows("SM_GAME_TIME", {}));
	EXPECT_TRUE(allowed.allows("SM_PONG", {}));
	EXPECT_TRUE(allowed.allows("SM_WEATHER", {}));
	EXPECT_FALSE(allowed.allows("SM_PLAYER_SPAWN", {}));
	// the conditional members are off until a case turns them on
	EXPECT_FALSE(allowed.allows("SM_PLAYER_STATE", playerState(7)));
	EXPECT_FALSE(allowed.allows("SM_NPC_INFO", npcInfo(7)));
	EXPECT_FALSE(allowed.allows("SM_SYSTEM_MESSAGE", systemMessage(AsyncAllowed::STR_SERVER_SHUTDOWN)));
}

TEST(AsyncAllowedTest, PlayerStateIsAsyncOnlyForTheOwnPlayer) {
	AsyncAllowed allowed = AsyncAllowed::m5aDefault();
	allowed.selfPlayerState(0x0BADF00D);
	EXPECT_TRUE(allowed.allows("SM_PLAYER_STATE", playerState(0x0BADF00D)));
	EXPECT_FALSE(allowed.allows("SM_PLAYER_STATE", playerState(0x0BADF00E)));
}

TEST(AsyncAllowedTest, NpcInfoAndDeleteAreAsyncOnlyForTemporarySpawns) {
	AsyncAllowed allowed = AsyncAllowed::m5aDefault();
	allowed.temporarySpawnUpdates([](int32_t objectId) { return objectId >= 0x9000; });
	EXPECT_TRUE(allowed.allows("SM_NPC_INFO", npcInfo(0x9001)));
	EXPECT_FALSE(allowed.allows("SM_NPC_INFO", npcInfo(0x8FFF)));
	EXPECT_TRUE(allowed.allows("SM_DELETE", deletePacket(0x9002)));
	EXPECT_FALSE(allowed.allows("SM_DELETE", deletePacket(0x100)));

	// the region move window (case 5) does not enable them: there every SM_NPC_INFO and SM_DELETE belongs to the sequence
	const AsyncAllowed duringMove = AsyncAllowed::m5aDefault();
	EXPECT_FALSE(duringMove.allows("SM_NPC_INFO", npcInfo(0x9001)));
	EXPECT_FALSE(duringMove.allows("SM_DELETE", deletePacket(0x9002)));
}

TEST(AsyncAllowedTest, OnlyTheShutdownSystemMessageIsAsync) {
	AsyncAllowed allowed = AsyncAllowed::m5aDefault();
	allowed.serverShutdownMessage();
	EXPECT_TRUE(allowed.allows("SM_SYSTEM_MESSAGE", systemMessage(AsyncAllowed::STR_SERVER_SHUTDOWN)));
	EXPECT_FALSE(allowed.allows("SM_SYSTEM_MESSAGE", systemMessage(1300643)));
}

TEST(AsyncAllowedTest, NpcActivityIsAsyncOnlyBetweenAnnouncedNpcs) {
	// m5b-plan.md D2/G-05: a walking npc's SM_MOVE is async, the character's own is not (§5.6 M3 is the assertion that would otherwise go)
	AsyncAllowed allowed = AsyncAllowed::m5aDefault();
	const int32_t player = 0x0BADF00D;
	const int32_t npc = 0x100, otherNpc = 0x101;
	// the shape of AnnouncedNpcs: exactly the ids an SM_NPC_INFO announced, which never includes the character
	allowed.selfPlayerState(player).npcActivity([](int32_t objectId) { return objectId == 0x100 || objectId == 0x101; });

	EXPECT_TRUE(allowed.allows("SM_MOVE", move(npc)));
	EXPECT_FALSE(allowed.allows("SM_MOVE", move(player))) << "the moving character's own SM_MOVE must stay a sequence failure";
	EXPECT_FALSE(allowed.allows("SM_MOVE", move(0x9000))) << "an object the gate never saw an SM_NPC_INFO for is not an npc";

	// the four emotions EmoteManager broadcasts, and nothing else of the 55 emotion types
	EXPECT_TRUE(allowed.allows("SM_EMOTION", emotion(npc, decoders::EMOTION_WALK)));
	EXPECT_TRUE(allowed.allows("SM_EMOTION", emotion(npc, decoders::EMOTION_CHANGE_SPEED)));
	EXPECT_TRUE(allowed.allows("SM_EMOTION", emotion(npc, decoders::EMOTION_NEUTRALMODE_IN_MOVE)));
	EXPECT_TRUE(allowed.allows("SM_EMOTION", emotion(npc, decoders::EMOTION_ATTACKMODE_IN_MOVE)));
	EXPECT_FALSE(allowed.allows("SM_EMOTION", emotion(npc, 18))) << "DIE: an npc dying in the M5a gate is news";
	EXPECT_FALSE(allowed.allows("SM_EMOTION", emotion(npc, 3))) << "STAND is no EmoteManager emote, so no npc AI can send it";
	EXPECT_FALSE(allowed.allows("SM_EMOTION", emotion(player, decoders::EMOTION_WALK)));

	// an allowed emotion is consumed exactly, so a body with one byte too many fails instead of passing as "some emote"
	std::vector<uint8_t> trailing = emotion(npc, decoders::EMOTION_WALK);
	trailing.push_back(0);
	EXPECT_FALSE(allowed.allows("SM_EMOTION", trailing));

	// an npc fight: every object id the packet names has to be an announced npc, so nothing about the character is ever absorbed
	EXPECT_TRUE(allowed.allows("SM_LOOKATOBJECT", lookAt(npc, otherNpc)));
	EXPECT_TRUE(allowed.allows("SM_LOOKATOBJECT", lookAt(npc, 0))) << "target 0 is 'looks at nothing' (SM_LOOKATOBJECT.java:19)";
	EXPECT_FALSE(allowed.allows("SM_LOOKATOBJECT", lookAt(npc, player))) << "an npc that targets the character is news";
	EXPECT_FALSE(allowed.allows("SM_LOOKATOBJECT", lookAt(player, npc)));

	EXPECT_TRUE(allowed.allows("SM_ATTACK", attack(npc, otherNpc)));
	EXPECT_FALSE(allowed.allows("SM_ATTACK", attack(npc, player))) << "the M5a character is attacked by nobody; that must fail the sequence";
	EXPECT_FALSE(allowed.allows("SM_ATTACK", attack(player, npc))) << "the M5a character attacks nobody";

	EXPECT_TRUE(allowed.allows("SM_ATTACK_STATUS", attackStatus(npc)));
	EXPECT_FALSE(allowed.allows("SM_ATTACK_STATUS", attackStatus(player))) << "the character's HP changing is news in an M5a run";

	// off until a case turns it on
	const AsyncAllowed silent = AsyncAllowed::m5aDefault();
	EXPECT_FALSE(silent.allows("SM_MOVE", move(npc)));
	EXPECT_FALSE(silent.allows("SM_EMOTION", emotion(npc, decoders::EMOTION_WALK)));
	EXPECT_FALSE(silent.allows("SM_LOOKATOBJECT", lookAt(npc, otherNpc)));
	EXPECT_FALSE(silent.allows("SM_ATTACK", attack(npc, otherNpc)));
	EXPECT_FALSE(silent.allows("SM_ATTACK_STATUS", attackStatus(npc)));
}

TEST(AsyncAllowedTest, AnUndecodableBodyIsNeverAsync) {
	AsyncAllowed allowed = AsyncAllowed::m5aDefault();
	allowed.selfPlayerState(7).serverShutdownMessage().npcActivity([](int32_t) { return true; });
	EXPECT_FALSE(allowed.allows("SM_PLAYER_STATE", {}));
	EXPECT_FALSE(allowed.allows("SM_SYSTEM_MESSAGE", std::vector<uint8_t>{1, 2, 3}));
	EXPECT_FALSE(allowed.allows("SM_MOVE", std::vector<uint8_t>{1, 2, 3}));
	EXPECT_FALSE(allowed.allows("SM_EMOTION", std::vector<uint8_t>{1, 2, 3}));
	EXPECT_FALSE(allowed.allows("SM_LOOKATOBJECT", std::vector<uint8_t>{1, 2, 3}));
	EXPECT_FALSE(allowed.allows("SM_ATTACK", std::vector<uint8_t>{1, 2, 3}));
	EXPECT_FALSE(allowed.allows("SM_ATTACK_STATUS", std::vector<uint8_t>{1, 2, 3}));
}

TEST(AsyncAllowedTest, DrivesThePacketSequenceMatcher) {
	// the level ready part of §5.8, shortened: the own SM_PLAYER_STATE and a periodic SM_GAME_TIME may arrive at any position
	const PacketSequence sequence =
		PacketSequence::parse("SM_PLAYER_INFO, SM_ACCOUNT_PROPERTIES, (SM_NPC_INFO | SM_GATHERABLE_INFO)+, SM_RIFT_ANNOUNCE");
	AsyncAllowed allowed = AsyncAllowed::m5aDefault();
	allowed.selfPlayerState(0x0BADF00D).temporarySpawnUpdates([](int32_t objectId) { return objectId == 0x9001; });

	std::vector<Recorded> packets = {
		{"SM_PLAYER_INFO", {}},
		{"SM_PLAYER_STATE", playerState(0x0BADF00D)},
		{"SM_ACCOUNT_PROPERTIES", {}},
		{"SM_GAME_TIME", {}},
		{"SM_NPC_INFO", npcInfo(0x100)},
		{"SM_NPC_INFO", npcInfo(0x101)},
		{"SM_RIFT_ANNOUNCE", {}},
		{"SM_NPC_INFO", npcInfo(0x9001)}, // a temporary spawn at an hour change, after the sequence ended
		{"SM_PONG", {}},
	};
	std::vector<std::string> names;
	for (const Recorded& packet : packets)
		names.push_back(packet.name);

	EXPECT_TRUE(sequence.match(names, allowed.predicate(packets)).matched);
	// without the async set the same recording does not match
	EXPECT_FALSE(sequence.match(names).matched);

	// a foreign player's state is not async, so it breaks the sequence
	packets[1].data = playerState(0x0BADF00E);
	const PacketSequence::Result result = sequence.match(names, allowed.predicate(packets));
	EXPECT_FALSE(result.matched);
	EXPECT_EQ(result.failedAt, 1u);
}

} // namespace
} // namespace aion::gameserver::scenario
