// P5-08 toy pet feeding (m5j-plan.md §18.3 stage 2 CP1, item E-01): PetFeedProgress's counters and its packet word, PetFeedCalculator's
// point table, its feed progress and its reward choice. The flavours are a two-flavour pet_feed document (full counts 10 and 100) published
// before the calculator's static initializer first runs in this process (PetTestSupport.h).
//
// Golden vectors: computed from the Java formulas (PetFeedCalculator.java:63-200, PetFeedProgress.java:26-99) with a float-exact mirror
// (struct-packed binary32 for every `* 0.5f`, `* 0.8f`, `0.487f *` and `0.78f *`, a double for `* 1.05`), repeated per case.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PetFeedData.bind.h"
#include "aion/gameserver/dataholders/PetFeedData.h"
#include "aion/gameserver/model/templates/pet/PetFeedResult.h"
#include "aion/gameserver/model/templates/pet/PetFlavour.h"
#include "aion/gameserver/model/templates/pet/PetRewards.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/services/toypet/PetFeedCalculator.h"
#include "aion/gameserver/services/toypet/PetFeedProgress.h"

#include "PetTestSupport.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::toypet {
namespace {

class PetFeedTest : public ::testing::Test {
protected:
	static void SetUpTestSuite() {
		runtime::TaskScope suiteScope(AION_TASK_INFO(runtime::TaskKind::TEST));
		testing::pets::publishFeedDataOnce();
	}

	/** every Field load runs inside a TaskScope (runtime/lifetime/TaskScope.h) */
	runtime::TaskScope scope{AION_TASK_INFO(runtime::TaskKind::TEST)};

	static const model::templates::pet::PetRewards& thorns() {
		return dataholders::DataManager::PET_FEED_DATA->getFlavourById(9)->getFood().at(0);
	}
};

/** PetFeedProgress.java:26-28, :44-46, :52-54, :64-79: the masks, the loved food limit and reset's two arms */
TEST_F(PetFeedTest, TheProgressCountersMaskAndReset) {
	runtime::Ref<PetFeedProgress> progress = PetFeedProgress::create(0x47); // & 0x3F: 7
	EXPECT_EQ(progress->getLovedFoodRemaining(), 7);
	progress->setTotalPoints(0x4001);
	EXPECT_EQ(progress->getTotalPoints(), 1) << "& 0x3FFF";
	progress->setRegularCount(0x1FF);
	EXPECT_EQ(progress->getRegularCount(), 0xFF) << "& 0xFF";
	progress->incrementCount(true);
	EXPECT_EQ(progress->getLovedFoodRemaining(), 6);
	progress->setIsLovedFeeded();
	progress->reset();
	EXPECT_FALSE(progress->isLovedFeeded()) << "a loved feed resets only the flag";
	EXPECT_EQ(progress->getTotalPoints(), 1);
	progress->reset();
	EXPECT_EQ(progress->getTotalPoints(), 0) << "then the points and the regular count";
	EXPECT_EQ(progress->getRegularCount(), 0);
	progress->setRegularCount(32767);
	progress->incrementCount(false);
	EXPECT_EQ(progress->getRegularCount(), 0) << "a Java short wraps to -32768, & 0xFF is 0";
}

/** PetFeedProgress.java:81-99: regular count (8 bits), points >> 2 (14 bits), loved count (6 bits), 4 unknown bits; setData reads it back */
TEST_F(PetFeedTest, TheProgressPacketWordRoundTrips) {
	runtime::Ref<PetFeedProgress> progress = PetFeedProgress::create(10);
	progress->setRegularCount(12);
	progress->setTotalPoints(360);
	progress->incrementCount(true);
	progress->incrementCount(true);
	const int32_t word = progress->getDataForPacket();
	EXPECT_EQ(word, ((12 << 14 | 360 >> 2) << 6 | 2) << 4);
	runtime::Ref<PetFeedProgress> loaded = PetFeedProgress::create(10);
	loaded->setData(word);
	EXPECT_EQ(loaded->getRegularCount(), 12);
	EXPECT_EQ(loaded->getTotalPoints(), 360);
	EXPECT_EQ(loaded->getLovedFoodRemaining(), 8);
	runtime::Ref<PetFeedProgress> top = PetFeedProgress::create(0);
	top->setRegularCount(200);
	EXPECT_LT(top->getDataForPacket(), 0) << "the int wraps into the sign bit as in Java";
	runtime::Ref<PetFeedProgress> back = PetFeedProgress::create(0);
	back->setData(top->getDataForPacket());
	EXPECT_EQ(back->getRegularCount(), 200) << "the arithmetic shift keeps the low byte";
}

/** PetFeedCalculator.java:90-112: the header table's values, feed points 8 per 5 item levels times the full count */
TEST_F(PetFeedTest, ThePointsAreTheHeaderTable) {
	EXPECT_EQ(PetFeedCalculator::getPoints(8, 10), 80);
	EXPECT_EQ(PetFeedCalculator::getPoints(8, 200), 1600);
	EXPECT_EQ(PetFeedCalculator::getPoints(16, 25), 400);
	EXPECT_EQ(PetFeedCalculator::getPoints(88, 200), 17600);
	EXPECT_EQ(PetFeedCalculator::getPoints(0, 100), 0);
	EXPECT_EQ(PetFeedCalculator::getPoints(40, 1), 40);
}

/** PetFeedCalculator.java:38-59: the sorted full counts, twelve level steps of 5 and the point table filled from row 1 */
TEST_F(PetFeedTest, TheStaticTablesFollowTheFlavours) {
	PetFeedCalculator::calculate();
	runtime::Ref<runtime::Array<int16_t>> counts = PetFeedCalculator::fullCounts.get();
	ASSERT_EQ(counts->length(), 2);
	EXPECT_EQ((*counts)[0].get(), 10);
	EXPECT_EQ((*counts)[1].get(), 100);
	runtime::Ref<runtime::Array<int8_t>> levels = PetFeedCalculator::itemLevels.get();
	ASSERT_EQ(levels->length(), 12);
	EXPECT_EQ((*levels)[11].get(), 60);
	runtime::Ref<runtime::Array<runtime::Ref<runtime::Array<int32_t>>>> points = PetFeedCalculator::pointValues.get();
	EXPECT_EQ((*(*points)[0].get())[0].get(), 0) << "row 0 (level 5) is skipped: level < 10";
	EXPECT_EQ((*(*points)[1].get())[0].get(), 80);
	EXPECT_EQ((*(*points)[5].get())[1].get(), 4000) << "items of level 26-30 (40 points) fed 100 times";
	EXPECT_EQ((*(*points)[11].get())[1].get(), 8800);
}

/**
 * PetFeedCalculator.java:114-154: twelve level-30 items into a full count of 10. The mirror's sequence (regular count, points, hungry level):
 * 40/80/.../240 HUNGRY for the first six; the 7th switches to CONTENT keeping 240 (6 > 10 * 0.5f, 6 > 0.487f * 10); 280, 320; the 10th to
 * SEMIFULL (9 > 8.0f); 360; the 12th to FULL (11 > 10.5)
 */
TEST_F(PetFeedTest, AFeedClimbsTheHungryLevels) {
	runtime::Ref<PetFeedProgress> progress = PetFeedProgress::create(0);
	const int32_t expectedPoints[] = {40, 80, 120, 160, 200, 240, 240, 280, 320, 320, 360, 360};
	const PetHungryLevel expectedLevels[] = {PetHungryLevel::HUNGRY, PetHungryLevel::HUNGRY, PetHungryLevel::HUNGRY, PetHungryLevel::HUNGRY,
		PetHungryLevel::HUNGRY, PetHungryLevel::HUNGRY, PetHungryLevel::CONTENT, PetHungryLevel::CONTENT, PetHungryLevel::CONTENT,
		PetHungryLevel::SEMIFULL, PetHungryLevel::SEMIFULL, PetHungryLevel::FULL};
	for (int32_t i = 0; i < 12; i++) {
		PetFeedCalculator::updatePetFeedProgress(*progress, 30, 10);
		EXPECT_EQ(progress->getRegularCount(), i + 1);
		EXPECT_EQ(progress->getTotalPoints(), expectedPoints[i]) << "feed " << i + 1;
		EXPECT_EQ(progress->getHungryLevel(), expectedLevels[i]) << "feed " << i + 1;
	}
	EXPECT_THROW(PetFeedCalculator::updatePetFeedProgress(*PetFeedProgress::create(0), 61, 10), runtime::ArrayIndexOutOfBoundsException)
		<< "itemLevels[60 / 5] of a level-61 item";
}

/** PetFeedCalculator.java:118-124: a loved feed fills up at once while loved food remains, and does nothing once it is used up */
TEST_F(PetFeedTest, ALovedFeedFillsUpAtOnce) {
	runtime::Ref<PetFeedProgress> progress = PetFeedProgress::create(1);
	progress->setIsLovedFeeded();
	PetFeedCalculator::updatePetFeedProgress(*progress, 30, 1);
	EXPECT_EQ(progress->getHungryLevel(), PetHungryLevel::FULL);
	EXPECT_EQ(progress->getLovedFoodRemaining(), 0);
	progress->setHungryLevel(PetHungryLevel::HUNGRY);
	PetFeedCalculator::updatePetFeedProgress(*progress, 30, 1);
	EXPECT_EQ(progress->getHungryLevel(), PetHungryLevel::HUNGRY) << "no loved food left";
	EXPECT_EQ(progress->getRegularCount(), 0);
}

/**
 * PetFeedCalculator.java:156-200: FULL with 360 points of full count 10 (index 0): rows 1-4 (80k <= 360) qualify, the last gives
 * Math.round(5 / 11f * 4) - 1 = 1; not FULL, or a full count not in the table, is null
 */
TEST_F(PetFeedTest, TheRewardFollowsThePoints) {
	runtime::Ref<PetFeedProgress> progress = PetFeedProgress::create(0);
	progress->setTotalPoints(360);
	EXPECT_EQ(PetFeedCalculator::getReward(10, &thorns(), *progress, 10), nullptr) << "not FULL";
	progress->setHungryLevel(PetHungryLevel::FULL);
	const model::templates::pet::PetFeedResult* reward = PetFeedCalculator::getReward(10, &thorns(), *progress, 10);
	ASSERT_NE(reward, nullptr);
	EXPECT_EQ(reward->getItem(), 188050989);
	EXPECT_EQ(PetFeedCalculator::getReward(25, &thorns(), *progress, 10), nullptr) << "25 is no flavour's full count";
	progress->setTotalPoints(0);
	EXPECT_EQ(PetFeedCalculator::getReward(10, &thorns(), *progress, 10)->getItem(), 188050988) << "no row qualifies: index 0";
	progress->setTotalPoints(16383);
	EXPECT_EQ(PetFeedCalculator::getReward(10, &thorns(), *progress, 10)->getItem(), 188050992) << "Math.round(5 / 11f * 11) - 1 = 4";
	EXPECT_THROW(PetFeedCalculator::getReward(10, nullptr, *progress, 10), runtime::NullPointerException);
}

} // namespace
} // namespace aion::gameserver::services::toypet
