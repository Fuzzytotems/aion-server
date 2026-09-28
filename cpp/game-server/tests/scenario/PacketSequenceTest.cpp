// PacketSequence (m5a-plan.md F-04, §5.8, §5.9): the notation parser and the matcher with the async-allowed set.

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "PacketSequence.h"

namespace aion::gameserver::scenario {
namespace {

using Names = std::vector<std::string>;

TEST(PacketSequenceTest, ParsesThePlanNotation) {
	PacketSequence sequence = PacketSequence::parse(
		"SM_HOUSE_SCRIPTS, SM_SKILL_LIST+, [SM_SKILL_COOLDOWN], SM_WAREHOUSE_INFO{43}, [SM_UI_SETTINGS]{0..3}, SM_WINDSTREAM_ANNOUNCE*, "
		"(SM_NPC_INFO | SM_GATHERABLE_INFO)+");
	const auto& elements = sequence.elements();
	ASSERT_EQ(elements.size(), 7u);
	EXPECT_EQ(elements[0].names, Names{"SM_HOUSE_SCRIPTS"});
	EXPECT_EQ(elements[0].min, 1);
	EXPECT_EQ(elements[0].max, 1);
	EXPECT_EQ(elements[1].min, 1);
	EXPECT_EQ(elements[1].max, -1);
	EXPECT_EQ(elements[2].min, 0);
	EXPECT_EQ(elements[2].max, 1);
	EXPECT_EQ(elements[3].min, 43);
	EXPECT_EQ(elements[3].max, 43);
	EXPECT_EQ(elements[4].min, 0);
	EXPECT_EQ(elements[4].max, 3);
	EXPECT_EQ(elements[5].min, 0);
	EXPECT_EQ(elements[5].max, -1);
	EXPECT_EQ(elements[6].names, (Names{"SM_NPC_INFO", "SM_GATHERABLE_INFO"}));
	EXPECT_EQ(elements[6].min, 1);
	EXPECT_EQ(elements[6].max, -1);
	EXPECT_EQ(sequence.toString(), "SM_HOUSE_SCRIPTS, SM_SKILL_LIST+, [SM_SKILL_COOLDOWN], SM_WAREHOUSE_INFO{43}, SM_UI_SETTINGS{0..3}, "
									 "SM_WINDSTREAM_ANNOUNCE*, (SM_NPC_INFO | SM_GATHERABLE_INFO)+");
}

TEST(PacketSequenceTest, RejectsMalformedPatterns) {
	EXPECT_THROW(PacketSequence::parse("SM_A, [SM_B"), std::invalid_argument);
	EXPECT_THROW(PacketSequence::parse("SM_A{2..1}"), std::invalid_argument);
	EXPECT_THROW(PacketSequence::parse("SM_A?"), std::invalid_argument);
	EXPECT_THROW(PacketSequence::parse("SM_A, , SM_B"), std::invalid_argument);
	EXPECT_TRUE(PacketSequence::parse("").elements().empty());
}

TEST(PacketSequenceTest, MatchesCountsOptionalsAndAlternatives) {
	PacketSequence sequence = PacketSequence::parse("SM_A, SM_B+, [SM_C], SM_D{2}, (SM_E | SM_F)*");
	EXPECT_TRUE(sequence.match({"SM_A", "SM_B", "SM_D", "SM_D"}).matched);
	EXPECT_TRUE(sequence.match({"SM_A", "SM_B", "SM_B", "SM_C", "SM_D", "SM_D", "SM_F", "SM_E", "SM_F"}).matched);
	EXPECT_FALSE(sequence.match({"SM_A", "SM_D", "SM_D"}).matched);                 // SM_B+ needs one
	EXPECT_FALSE(sequence.match({"SM_A", "SM_B", "SM_C", "SM_C", "SM_D", "SM_D"}).matched); // [SM_C] at most once
	EXPECT_FALSE(sequence.match({"SM_A", "SM_B", "SM_D"}).matched);                 // SM_D{2}
	EXPECT_FALSE(sequence.match({"SM_A", "SM_B", "SM_D", "SM_D", "SM_G"}).matched);  // trailing unknown packet
	EXPECT_TRUE(PacketSequence::parse("SM_A{0..2}, SM_A").match({"SM_A", "SM_A", "SM_A"}).matched); // backtracking into the next element
	EXPECT_TRUE(PacketSequence().match({}).matched);
}

TEST(PacketSequenceTest, NamesWhereTheMatchStopped) {
	PacketSequence sequence = PacketSequence::parse("SM_A, SM_B, SM_C");
	PacketSequence::Result result = sequence.match({"SM_A", "SM_B", "SM_X"});
	EXPECT_FALSE(result.matched);
	EXPECT_EQ(result.failedAt, 2u);
	EXPECT_NE(result.message.find("packet #2 SM_X"), std::string::npos) << result.message;
	EXPECT_NE(result.message.find("expected SM_C"), std::string::npos) << result.message;

	PacketSequence::Result tooShort = sequence.match({"SM_A"});
	EXPECT_FALSE(tooShort.matched);
	EXPECT_EQ(tooShort.failedAt, 1u);
	EXPECT_NE(tooShort.message.find("the end of the packets"), std::string::npos) << tooShort.message;
}

TEST(PacketSequenceTest, AsyncAllowedPacketsMayAppearAnywhereButAreMatchedFirst) {
	// §5.9: SM_GAME_TIME is part of the sequence once and may also arrive periodically at any position
	PacketSequence sequence = PacketSequence::parse("SM_PLAYER_SPAWN, SM_GAME_TIME, SM_WAREHOUSE_INFO{2}");
	Names packets = {"SM_PONG", "SM_PLAYER_SPAWN", "SM_GAME_TIME", "SM_WAREHOUSE_INFO", "SM_GAME_TIME", "SM_WAREHOUSE_INFO", "SM_PONG"};
	auto async = [&packets](size_t index) { return packets[index] == "SM_PONG" || packets[index] == "SM_GAME_TIME"; };
	EXPECT_TRUE(sequence.match(packets, async).matched);
	EXPECT_FALSE(sequence.match(packets).matched);

	// an async packet does not replace a required one
	Names missing = {"SM_PLAYER_SPAWN", "SM_PONG", "SM_WAREHOUSE_INFO", "SM_WAREHOUSE_INFO"};
	auto pongOnly = [&missing](size_t index) { return missing[index] == "SM_PONG"; };
	EXPECT_FALSE(sequence.match(missing, pongOnly).matched);
}

TEST(PacketSequenceTest, LongSequencesMatchQuickly) {
	PacketSequence sequence = PacketSequence::parse("SM_INVENTORY_INFO{3}, SM_WAREHOUSE_INFO{43}, (SM_NPC_INFO | SM_GATHERABLE_INFO)+, SM_RIFT_ANNOUNCE");
	Names packets(3, "SM_INVENTORY_INFO");
	packets.insert(packets.end(), 43, "SM_WAREHOUSE_INFO");
	for (int i = 0; i < 400; i++)
		packets.push_back(i % 7 == 0 ? "SM_GATHERABLE_INFO" : "SM_NPC_INFO");
	packets.push_back("SM_RIFT_ANNOUNCE");
	EXPECT_TRUE(sequence.match(packets).matched);
	packets.back() = "SM_NPC_INFO";
	EXPECT_FALSE(sequence.match(packets).matched);
}

} // namespace
} // namespace aion::gameserver::scenario
