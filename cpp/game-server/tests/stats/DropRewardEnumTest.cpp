// P5-01, written by the M5b-3 loot lane under the I-03 lease (m5b3-plan.md L-06): the DropRewardEnum companion
// (utils/stats/DropRewardEnumInfo.h). Its one caller, DropRegistrationService.getReductionDropRate (DropRegistrationService.java:198-201),
// passes `npc.getLevel() - highestLevel`: the reduction of level-based drop rules for a killer above the npc's level (tests/economy covers that
// caller).
//
// Every expectation is the Java value of the cited source line (DropRewardEnum.java), not a value read back from the port.

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <utility>

#include "aion/gameserver/utils/stats/DropRewardEnum.h"
#include "aion/gameserver/utils/stats/DropRewardEnumInfo.h"

namespace aion::gameserver::utils::stats::test {
namespace {

// DropRewardEnum.java:7-12 in ordinal order: MINUS_10(-10, 0) ... MINUS_5(-5, 100) - the constructor takes (levelDifference, dropRewardPercent),
// the order the fields are declared in is the other way round (:14-15), so a port that swapped them would read 100 for MINUS_10
TEST(DropRewardEnumTest, TheConstantsCarryJavasConstructorArguments) {
	const std::pair<DropRewardEnum, std::pair<int32_t, int32_t>> rows[] = {{DropRewardEnum::MINUS_10, {-10, 0}},
		{DropRewardEnum::MINUS_9, {-9, 40}}, {DropRewardEnum::MINUS_8, {-8, 60}}, {DropRewardEnum::MINUS_7, {-7, 70}},
		{DropRewardEnum::MINUS_6, {-6, 80}}, {DropRewardEnum::MINUS_5, {-5, 100}}};
	for (const auto& [constant, values] : rows) {
		EXPECT_EQ(levelDifference(constant), values.first) << static_cast<int32_t>(constant);
		EXPECT_EQ(rewardPercent(constant), values.second) << static_cast<int32_t>(constant);
	}
}

// dropRewardFrom (DropRewardEnum.java:30-43): the table between the two bounds, found by the linear search over values()
TEST(DropRewardEnumTest, DropRewardFromLooksUpEveryLevelDifferenceBetweenTheBounds) {
	EXPECT_EQ(dropRewardFrom(-9), 40);
	EXPECT_EQ(dropRewardFrom(-8), 60);
	EXPECT_EQ(dropRewardFrom(-7), 70);
	EXPECT_EQ(dropRewardFrom(-6), 80);
}

// The two bounds are inclusive (`<=` MINUS_10 at :31, `>=` MINUS_5 at :33) - unlike XPRewardEnum.xpRewardFrom's strict comparisons - and
// everything beyond them clamps. -5 and above is 100, the value getReductionDropRate turns into "no reduction" (null, :200).
TEST(DropRewardEnumTest, DropRewardFromClampsAtAndBeyondBothBounds) {
	EXPECT_EQ(dropRewardFrom(-10), 0) << "MINUS_10 itself";
	EXPECT_EQ(dropRewardFrom(-11), 0) << "below MINUS_10";
	EXPECT_EQ(dropRewardFrom(std::numeric_limits<int32_t>::min()), 0);
	EXPECT_EQ(dropRewardFrom(-5), 100) << "MINUS_5 itself";
	EXPECT_EQ(dropRewardFrom(-4), 100) << "above MINUS_5";
	EXPECT_EQ(dropRewardFrom(0), 100) << "same level: the npc level minus the killer's";
	EXPECT_EQ(dropRewardFrom(12), 100) << "an npc above the killer";
	EXPECT_EQ(dropRewardFrom(std::numeric_limits<int32_t>::max()), 100);
}

} // namespace
} // namespace aion::gameserver::utils::stats::test
