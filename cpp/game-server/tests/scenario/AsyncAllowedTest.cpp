// AsyncAllowed (m5a-plan.md §5.9) and its use as the PacketSequence predicate.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "AsyncAllowed.h"
#include "PacketSequence.h"

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

TEST(AsyncAllowedTest, AnUndecodableBodyIsNeverAsync) {
	AsyncAllowed allowed = AsyncAllowed::m5aDefault();
	allowed.selfPlayerState(7).serverShutdownMessage();
	EXPECT_FALSE(allowed.allows("SM_PLAYER_STATE", {}));
	EXPECT_FALSE(allowed.allows("SM_SYSTEM_MESSAGE", std::vector<uint8_t>{1, 2, 3}));
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
