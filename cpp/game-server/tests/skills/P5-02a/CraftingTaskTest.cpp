// C-02 of m5c-plan.md (M5c stage 2, the craft-task lane, chunk P5-02a): the nine bodies of CraftingTask and their C-06 cases - analyzeInteraction
// over a seeded Rnd (the success and failure steps, CRIT_BLUE, the speed, the morph and skillLvlDiff < 0 arms), calculateCrit (the crit and
// combo chances, the ESTATE/PALACE bonus, the used-up combo products), and the packets of the start, a tick, the abort and both finishes (to
// the crafter, and the animations to the players around), also through the scheduler; the enhancement stone's bonus through onSuccessFinish
// into CraftService.finishCrafting (C-01's, merged before this lane). Expectations: CraftingTask.java, AbstractCraftTask.java,
// AbstractInteractionTask.java, CraftService.java and `tools/oracle/oracle.py m5c-craft` (CraftingTaskTestSupport.h says how).

#include "CraftingTaskTestSupport.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aion::gameserver::skillengine::task::crafttest {
namespace {

using configs::main::CraftConfig;
using configs::main::RatesConfig;
using gameserver::model::templates::housing::HouseType;
using CraftType = AbstractCraftTask::CraftType;
using Lines = std::vector<std::string>;

// ------------------------------------------------------------------------------------------------------------------------- the arithmetic

// The functions of CraftingTaskTestSupport.h restate CraftingTask.java:130-172; here they are held to the numbers of
//   python tools/oracle/oracle.py m5c-craft --no-profile --set gameserver.event.service.disabled_events=* --recipe <id> [--skill-level <n>]
// (craft.bars[]: failureThreshold, critBlueThreshold, successStep, failureStep, executionSpeed, showBarDelay) at both ends of multi.
TEST(CraftingTaskArithmeticTest, TheRestatedJavaArithmeticGivesTheOraclesNumbers) {
	const float top = topMulti();
	EXPECT_EQ(top, 1.9999998807907104f) << "the oracle's multi[1]";

	// --recipe 155001381 (COMMON, skill level 1: skillLvlDiff 0)
	EXPECT_EQ(failureThreshold(33, 0), 33.0f);
	EXPECT_EQ(critBlueThreshold(0), 15.0f);
	EXPECT_EQ(successStep(0, false, 1.0f, 1.0f), 75);
	EXPECT_EQ(successStep(0, false, top, 1.0f), 79);
	EXPECT_EQ(successStep(0, true, 1.0f, 1.0f), 175);
	EXPECT_EQ(successStep(0, true, top, 1.0f), 279);
	EXPECT_EQ(failureStep(0, false, 1.0f, 1.0f), 126);
	EXPECT_EQ(failureStep(0, false, top, 1.0f), 133);
	EXPECT_EQ(executionSpeedOf(0, 1.0f), 900);
	EXPECT_EQ(barDelayOf(0, 1.0f), 1200);
	// --recipe 155001381 --skill-level 21 (skillLvlDiff 20: lvlBoni 20)
	EXPECT_EQ(failureThreshold(33, 20), 23.10000228881836f);
	EXPECT_EQ(critBlueThreshold(20), 21.66666603088379f);
	EXPECT_EQ(successStep(20, false, 1.0f, 1.0f), 375);
	EXPECT_EQ(successStep(20, false, top, 1.0f), 679);
	EXPECT_EQ(successStep(20, true, 1.0f, 1.0f), 475);
	EXPECT_EQ(successStep(20, true, top, 1.0f), 879);
	EXPECT_EQ(failureStep(20, false, 1.0f, 1.0f), 260);
	EXPECT_EQ(failureStep(20, false, top, 1.0f), 399);
	EXPECT_EQ(executionSpeedOf(20, 1.0f), 300);
	EXPECT_EQ(barDelayOf(20, 1.0f), 600);
	// --recipe 155001381 --skill-level 60 (skillLvlDiff 59: no failure roll, alwaysSuccess)
	EXPECT_EQ(failureThreshold(33, 59), 8.25f);
	EXPECT_EQ(critBlueThreshold(59), 34.666664123535156f);
	EXPECT_EQ(successStep(59, false, 1.0f, 1.0f), 1350);
	EXPECT_EQ(successStep(59, false, top, 1.0f), 2629);
	EXPECT_EQ(successStep(59, true, 1.0f, 1.0f), 1450);
	EXPECT_EQ(successStep(59, true, top, 1.0f), 2829);
	EXPECT_EQ(failureStep(59, false, 1.0f, 1.0f), 520);
	EXPECT_EQ(failureStep(59, false, top, 1.0f), 919);
	EXPECT_EQ(executionSpeedOf(59, 1.0f), 300);
	EXPECT_EQ(barDelayOf(59, 1.0f), 500);
	// --recipe 155003492 (RARE, max_production_count 1) at skill levels 550 and 560
	EXPECT_EQ(failureStep(0, true, 1.0f, 1.0f), 76);
	EXPECT_EQ(failureStep(0, true, top, 1.0f), 83);
	EXPECT_EQ(failureThreshold(33, 10), 28.05000114440918f);
	EXPECT_EQ(critBlueThreshold(10), 18.33333396911621f);
	EXPECT_EQ(successStep(10, false, 1.0f, 1.0f), 125);
	EXPECT_EQ(successStep(10, false, top, 1.0f), 179);
	EXPECT_EQ(successStep(10, true, 1.0f, 1.0f), 225);
	EXPECT_EQ(successStep(10, true, top, 1.0f), 379);
	EXPECT_EQ(failureStep(10, true, 1.0f, 1.0f), 143);
	EXPECT_EQ(failureStep(10, true, top, 1.0f), 216);
	EXPECT_EQ(executionSpeedOf(10, 1.0f), 600);
	EXPECT_EQ(barDelayOf(10, 1.0f), 900);
	// --recipe 155002856 (LEGEND, bonusModifier 0.9f)
	EXPECT_EQ(successStep(0, false, 1.0f, LEGEND_MODIFIER), 75);
	EXPECT_EQ(successStep(0, false, top, LEGEND_MODIFIER), 78);
	EXPECT_EQ(successStep(0, true, 1.0f, LEGEND_MODIFIER), 165);
	EXPECT_EQ(successStep(0, true, top, LEGEND_MODIFIER), 258);
	EXPECT_EQ(failureStep(0, false, 1.0f, LEGEND_MODIFIER), 125);
	EXPECT_EQ(failureStep(0, false, top, LEGEND_MODIFIER), 132);
	EXPECT_EQ(executionSpeedOf(0, LEGEND_MODIFIER), 990);
	EXPECT_EQ(barDelayOf(0, LEGEND_MODIFIER), 1200);
	// --recipe 155001752 (UNIQUE, 0.7f)
	EXPECT_EQ(successStep(0, false, 1.0f, UNIQUE_MODIFIER), 74);
	EXPECT_EQ(successStep(0, false, top, UNIQUE_MODIFIER), 76);
	EXPECT_EQ(successStep(0, true, 1.0f, UNIQUE_MODIFIER), 144);
	EXPECT_EQ(successStep(0, true, top, UNIQUE_MODIFIER), 216);
	EXPECT_EQ(failureStep(0, false, 1.0f, UNIQUE_MODIFIER), 124);
	EXPECT_EQ(failureStep(0, false, top, UNIQUE_MODIFIER), 129);
	EXPECT_EQ(executionSpeedOf(0, UNIQUE_MODIFIER), 1170);
	// --recipe 155101302 (EPIC, 0.5f)
	EXPECT_EQ(successStep(0, false, 1.0f, EPIC_MODIFIER), 73);
	EXPECT_EQ(successStep(0, false, top, EPIC_MODIFIER), 75);
	EXPECT_EQ(successStep(0, true, 1.0f, EPIC_MODIFIER), 123);
	EXPECT_EQ(successStep(0, true, top, EPIC_MODIFIER), 175);
	EXPECT_EQ(failureStep(0, false, 1.0f, EPIC_MODIFIER), 123);
	EXPECT_EQ(failureStep(0, false, top, EPIC_MODIFIER), 127);
	EXPECT_EQ(executionSpeedOf(0, EPIC_MODIFIER), 1350);
	// --recipe 155101322 (MYTHIC, 0.3f)
	EXPECT_EQ(successStep(0, false, 1.0f, MYTHIC_MODIFIER), 72);
	EXPECT_EQ(successStep(0, false, top, MYTHIC_MODIFIER), 73);
	EXPECT_EQ(successStep(0, true, 1.0f, MYTHIC_MODIFIER), 102);
	EXPECT_EQ(successStep(0, true, top, MYTHIC_MODIFIER), 133);
	EXPECT_EQ(failureStep(0, false, 1.0f, MYTHIC_MODIFIER), 122);
	EXPECT_EQ(failureStep(0, false, top, MYTHIC_MODIFIER), 124);
	EXPECT_EQ(executionSpeedOf(0, MYTHIC_MODIFIER), 1530);
}

// ------------------------------------------------------------------------------------------------------------------------- start, abort

TEST_F(CraftingTaskTest, StartOpensTheBarAtTheStationAndAbortCancelsIt) {
	ASSERT_NE(stationId(), crafterId());
	Ref<CraftingTaskProbe> craft = newTask(ROAST_ININA_RECIPE, 0);
	craft->setBars(400, 300);
	craft->start();

	// onInteractionStart (CraftingTask.java:107-118): both bars back to 0, the full bar with action 0 (no proc yet), the empty bar with action 1,
	// and the two animations of the recipe's skill from the crafter to the station
	EXPECT_EQ(craft->successValue(), 0);
	EXPECT_EQ(craft->failureValue(), 0);
	EXPECT_EQ(sentCraftPackets(), (Lines{update(COOKING, 0, ROAST_ININA, 1000, 1000), update(COOKING, 1, ROAST_ININA, 0, 0),
									  animation(crafterId(), stationId(), COOKING, 0), animation(crafterId(), stationId(), COOKING, 1)}));
	EXPECT_TRUE(craft->isInProgress());
	EXPECT_EQ(crafter.player->getInteractionTask().rawPointer(), static_cast<AbstractInteractionTask*>(craft.get()));
	EXPECT_EQ(craft->firstDelay(), 1000) << "AbstractInteractionTask.java:18; CraftingTask keeps it";
	EXPECT_EQ(craft->period(), 2500) << "AbstractInteractionTask.java:17; CraftService.startCrafting sets the real interval";

	// abort = onInteractionAbort, then stop (AbstractInteractionTask.java:97-100), mid-craft: the cancelled bar is 0/0 whatever the bars hold
	// (CraftingTask.java:98), then the end animation, nothing else; neither touches the bars themselves
	craft->setBars(640, 210);
	clearSent();
	craft->abort();
	EXPECT_EQ(sentCraftPackets(), (Lines{update(COOKING, 4, ROAST_ININA, 0, 0), animation(crafterId(), stationId(), 0, 2)}));
	EXPECT_EQ((*client)->sent().size(), 2u) << "onInteractionFinish sends nothing (CraftingTask.java:103-104)";
	EXPECT_FALSE(craft->isInProgress());
	EXPECT_FALSE(crafter.player->getInteractionTask());
	EXPECT_EQ(craft->successValue(), 640);
	EXPECT_EQ(craft->failureValue(), 210);

	clearSent();
	craft->onInteractionFinish();
	EXPECT_TRUE((*client)->sent().empty());
	EXPECT_EQ(craft->successValue(), 640);
	EXPECT_EQ(craft->failureValue(), 210);
}

TEST_F(CraftingTaskTest, AMorphHasNoStationSoTheCrafterIsItsOwnResponder) {
	// CraftService.startCrafting casts the missing target of a morph to a null StaticObject, and AbstractInteractionTask makes the requester the
	// responder (AbstractInteractionTask.java:29-32); SM_CRAFT_UPDATE always carries the delay 1000 for 40009 (its constructor)
	Ref<CraftingTaskProbe> craft = newMorphTask();
	craft->onInteractionStart();
	EXPECT_EQ(sentCraftPackets(), (Lines{update(MORPH, 0, ARIA, 1000, 1000, 0, 1000), update(MORPH, 1, ARIA, 0, 0, 0, 1000),
									  animation(crafterId(), crafterId(), MORPH, 0), animation(crafterId(), crafterId(), MORPH, 1)}));
	// aborted with the bar the first run fills (the morph arm of analyzeInteraction): still the 0/0 bar
	craft->setBars(1000, 0);
	clearSent();
	craft->onInteractionAbort();
	EXPECT_EQ(sentCraftPackets(), (Lines{update(MORPH, 4, ARIA, 0, 0, 0, 1000), animation(crafterId(), crafterId(), 0, 2)}));
}

TEST_F(CraftingTaskTest, ANullRecipeIsANullPointerExceptionOfTheConstructor) {
	// Java: this.maxCritCount = recipeTemplate.getComboProductSize() (CraftingTask.java:34)
	EXPECT_THROW(CraftingTaskProbe::create(*crafter.player, Ptr<StaticObject>(station), nullptr, 0, 0), runtime::NullPointerException);
	EXPECT_FALSE(crafter.player->getInteractionTask());
}

TEST_F(CraftingTaskTest, AProductMissingFromTheItemDataIsANullPointerExceptionWhereJavaReadsIt) {
	dataholders::DataManager::ITEM_DATA.resetForTests();
	xml::LoadContext context;
	dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, "<item_templates/>"));
	// the constructor stores the null template (CraftingTask.java:36), SM_CRAFT_UPDATE's constructor and the quality switch dereference it
	Ref<CraftingTaskProbe> craft = newTask(ROAST_ININA_RECIPE, 0);
	EXPECT_THROW(craft->onInteractionStart(), runtime::NullPointerException);
	EXPECT_THROW(craft->sendInteractionUpdate(), runtime::NullPointerException);
	EXPECT_THROW(craft->onInteractionAbort(), runtime::NullPointerException);
	EXPECT_THROW(craft->onFailureFinish(), runtime::NullPointerException);
	EXPECT_THROW(craft->analyzeInteraction(), runtime::NullPointerException);
	EXPECT_TRUE(sentCraftPackets().empty());
}

// ------------------------------------------------------------------------------------------------------------------------- analyzeInteraction

TEST_F(CraftingTaskTest, TheMorphSkillFillsTheSuccessBarAtOnceAndRollsNothing) {
	AtomicConfigScope<int32_t> alwaysFail(CraftConfig::MAX_CRAFT_FAILURE_CHANCE, 100);
	const Draws draws = drawsOf(11);
	// the morph check comes before the level check (CraftingTask.java:122-128): a morph below its level still succeeds
	Ref<CraftingTaskProbe> craft = newMorphTask(-5);
	craft->setBars(300, 200);
	craft->setType(CraftType::CRIT_BLUE);
	Rnd::seedCurrentThreadForTests(11);
	craft->analyzeInteraction();
	EXPECT_EQ(Rnd::nextFloat(1.0f, 2.0f), draws.multi) << "nothing was drawn";
	EXPECT_EQ(craft->successValue(), 1000);
	EXPECT_EQ(craft->failureValue(), 200);
	EXPECT_EQ(craft->type(), CraftType::CRIT_BLUE) << "the arm returns before craftType = NORMAL";
	craft->sendInteractionUpdate();
	EXPECT_EQ(sentCraftPackets(), (Lines{update(MORPH, 2, ARIA, 1000, 200, 0, 1000)})) << "no speed was computed";
}

TEST_F(CraftingTaskTest, BelowTheRecipeLevelTheFailureBarFillsAtOnceAndNothingIsRolled) {
	const Draws draws = drawsOf(12);
	Ref<CraftingTaskProbe> craft = newTask(ROAST_ININA_RECIPE, -1);
	craft->setBars(300, 200);
	craft->setType(CraftType::CRIT_BLUE);
	Rnd::seedCurrentThreadForTests(12);
	craft->analyzeInteraction();
	EXPECT_EQ(Rnd::nextFloat(1.0f, 2.0f), draws.multi) << "nothing was drawn";
	EXPECT_EQ(craft->successValue(), 300);
	EXPECT_EQ(craft->failureValue(), 1000);
	EXPECT_EQ(craft->type(), CraftType::CRIT_BLUE) << "the arm returns before craftType = NORMAL";
	craft->sendInteractionUpdate();
	EXPECT_EQ(sentCraftPackets(), (Lines{update(COOKING, 2, ROAST_ININA, 300, 1000, 0, 0)})) << "no speed or delay was computed";
}

TEST_F(CraftingTaskTest, ASuccessfulTickAddsTheLevelStepAndSometimesTheBlueCrit) {
	// failure chance 0: `Rnd.chance() >= 0 * failReduction` holds for every roll (CraftingTask.java:133), so every tick succeeds
	AtomicConfigScope<int32_t> noFailure(CraftConfig::MAX_CRAFT_FAILURE_CHANCE, 0);
	int32_t blueTicks = 0;
	int32_t normalTicks = 0;
	for (uint64_t seed = 1; seed <= 150; seed++) {
		const Draws draws = drawsOf(seed);
		const bool blue = draws.second < critBlueThreshold(0); // draws.first was the failure roll
		const int32_t step = successStep(0, blue, draws.multi, 1.0f);
		Ref<CraftingTaskProbe> craft = newTask(ROAST_ININA_RECIPE, 0);
		craft->setBars(300, 200);
		craft->setType(CraftType::CRIT_BLUE); // the tick before was a blue one: this one starts from NORMAL
		Rnd::seedCurrentThreadForTests(seed);
		craft->analyzeInteraction();
		EXPECT_EQ(Rnd::chance(), draws.third) << "seed " << seed << ": multi, the failure roll and the CRIT_BLUE roll, in that order";
		EXPECT_EQ(craft->successValue(), 300 + step) << "seed " << seed;
		EXPECT_EQ(craft->failureValue(), 200) << "seed " << seed;
		EXPECT_EQ(craft->type(), blue ? CraftType::CRIT_BLUE : CraftType::NORMAL) << "seed " << seed;
		// the oracle's bounds of the step: normal [75, 79], critBlue [175, 279]
		if (blue) {
			blueTicks++;
			EXPECT_TRUE(step >= 175 && step <= 279) << step;
		} else {
			normalTicks++;
			EXPECT_TRUE(step >= 75 && step <= 79) << step;
		}
		clearSent();
		craft->sendInteractionUpdate();
		EXPECT_EQ(sentCraftPackets(), (Lines{update(COOKING, blue ? 2 : 1, ROAST_ININA, 300 + step, 200, 900, 1200)})) << "seed " << seed;
	}
	EXPECT_GT(blueTicks, 0);
	EXPECT_GT(normalTicks, 0);
}

TEST_F(CraftingTaskTest, TheFailureRollIsTheConfiguredChanceScaledDownByTheLevelDifference) {
	AtomicConfigScope<int32_t> shipped(CraftConfig::MAX_CRAFT_FAILURE_CHANCE, 33); // config/main/craft.properties: gameserver.craft.fail.chance
	for (int32_t diff : {0, 10, 20}) {
		const float threshold = failureThreshold(33, diff);
		int32_t failures = 0;
		int32_t successes = 0;
		for (uint64_t seed = 1; seed <= 300; seed++) {
			const Draws draws = drawsOf(seed);
			Ref<CraftingTaskProbe> craft = newTask(ROAST_ININA_RECIPE, diff);
			Rnd::seedCurrentThreadForTests(seed);
			craft->analyzeInteraction();
			if (draws.first >= threshold) {
				successes++;
				const bool blue = draws.second < critBlueThreshold(diff);
				EXPECT_EQ(craft->successValue(), successStep(diff, blue, draws.multi, 1.0f)) << "diff " << diff << " seed " << seed;
				EXPECT_EQ(craft->failureValue(), 0) << "diff " << diff << " seed " << seed;
				EXPECT_EQ(craft->type(), blue ? CraftType::CRIT_BLUE : CraftType::NORMAL) << "diff " << diff << " seed " << seed;
				EXPECT_EQ(Rnd::chance(), draws.third) << "a success rolls CRIT_BLUE; diff " << diff << " seed " << seed;
			} else {
				failures++;
				EXPECT_EQ(craft->successValue(), 0) << "diff " << diff << " seed " << seed;
				EXPECT_EQ(craft->failureValue(), failureStep(diff, false, draws.multi, 1.0f)) << "diff " << diff << " seed " << seed;
				EXPECT_EQ(craft->type(), CraftType::NORMAL) << "diff " << diff << " seed " << seed;
				EXPECT_EQ(Rnd::chance(), draws.second) << "a failure rolls no CRIT_BLUE; diff " << diff << " seed " << seed;
			}
		}
		EXPECT_GT(failures, 0) << diff;
		EXPECT_GT(successes, 0) << diff;
	}
}

TEST_F(CraftingTaskTest, ARecipeWithAProductionLimitFailsInSmallerSteps) {
	AtomicConfigScope<int32_t> alwaysFail(CraftConfig::MAX_CRAFT_FAILURE_CHANCE, 100); // `chance() >= 100` never holds below 41 levels
	for (uint64_t seed = 21; seed <= 60; seed++) {
		const Draws draws = drawsOf(seed);
		Ref<CraftingTaskProbe> limited = newTask(OFFERING_RECIPE, 0); // max_production_count="1": minStep 70 (CraftingTask.java:160)
		Rnd::seedCurrentThreadForTests(seed);
		limited->analyzeInteraction();
		const int32_t limitedStep = failureStep(0, true, draws.multi, 1.0f);
		EXPECT_EQ(limited->failureValue(), limitedStep) << seed;
		EXPECT_TRUE(limitedStep >= 76 && limitedStep <= 83) << limitedStep << ": the oracle's bounds for 155003492";

		Ref<CraftingTaskProbe> unlimited = newTask(ROAST_ININA_RECIPE, 0); // minStep 120
		Rnd::seedCurrentThreadForTests(seed);
		unlimited->analyzeInteraction();
		EXPECT_EQ(unlimited->failureValue(), limitedStep + 50) << seed;
	}
}

TEST_F(CraftingTaskTest, FromFortyOneLevelsAboveTheRecipeNothingFailsAndTheBlueRollComesFirst) {
	AtomicConfigScope<int32_t> alwaysFail(CraftConfig::MAX_CRAFT_FAILURE_CHANCE, 100);
	int32_t blueTicks = 0;
	int32_t normalTicks = 0;
	for (uint64_t seed = 1; seed <= 150; seed++) {
		const Draws draws = drawsOf(seed);
		// `skillLvlDiff >= 41 || Rnd.chance() >= ...` (CraftingTask.java:133): no failure roll, so the CRIT_BLUE roll is the second draw
		const bool blue = draws.first < critBlueThreshold(41);
		Ref<CraftingTaskProbe> craft = newTask(ROAST_ININA_RECIPE, 41);
		Rnd::seedCurrentThreadForTests(seed);
		craft->analyzeInteraction();
		EXPECT_EQ(craft->successValue(), std::min(1000, successStep(41, blue, draws.multi, 1.0f))) << seed;
		EXPECT_EQ(craft->failureValue(), 0) << seed;
		EXPECT_EQ(craft->type(), blue ? CraftType::CRIT_BLUE : CraftType::NORMAL) << seed;
		EXPECT_EQ(Rnd::chance(), draws.second) << "two draws: multi and the CRIT_BLUE roll; seed " << seed;
		(blue ? blueTicks : normalTicks)++;
	}
	EXPECT_GT(blueTicks, 0);
	EXPECT_GT(normalTicks, 0);

	// one level less: the roll is made, against 100 * (1 - 40 * 0.015f)
	int32_t failures = 0;
	for (uint64_t seed = 1; seed <= 150; seed++) {
		const Draws draws = drawsOf(seed);
		Ref<CraftingTaskProbe> craft = newTask(ROAST_ININA_RECIPE, 40);
		Rnd::seedCurrentThreadForTests(seed);
		craft->analyzeInteraction();
		const bool success = draws.first >= failureThreshold(100, 40);
		EXPECT_EQ(craft->failureValue() == 0, success) << seed;
		failures += success ? 0 : 1;
	}
	EXPECT_GT(failures, 0);
}

TEST_F(CraftingTaskTest, TheItemQualityScalesTheStepAndSetsTheSpeed) {
	struct Quality {
		int32_t recipeId;
		int32_t skillId;
		int32_t productId;
		float modifier;
		int32_t speed; // the oracle's executionSpeed at skillLvlDiff 0
	};
	const Quality qualities[] = {
		{LEATHER_PAD_RECIPE, TAILORING, LEATHER_PAD, LEGEND_MODIFIER, 990},
		{HELIOTROPE_RECIPE, WEAPONSMITHING, HELIOTROPE_CRYSTAL, UNIQUE_MODIFIER, 1170},
		{RARIFIED_ROD_RECIPE, WEAPONSMITHING, RARIFIED_ROD, EPIC_MODIFIER, 1350},
		{HAIRPIN_RECIPE, 40008, HAIRPIN, MYTHIC_MODIFIER, 1530},
		{OFFERING_RECIPE, WEAPONSMITHING, OFFERING, 1.0f, 900}, // RARE: no modifier
	};
	for (const Quality& quality : qualities) {
		for (int32_t diff : {0, 20}) {
			for (int32_t failureChance : {0, 100}) {
				AtomicConfigScope<int32_t> chance(CraftConfig::MAX_CRAFT_FAILURE_CHANCE, failureChance);
				for (uint64_t seed = 31; seed <= 50; seed++) {
					const Draws draws = drawsOf(seed);
					Ref<CraftingTaskProbe> craft = newTask(quality.recipeId, diff);
					Rnd::seedCurrentThreadForTests(seed);
					craft->analyzeInteraction();
					// failure chance 0 never fails; 100 always fails at 0 levels and fails below 70 at 20 levels (100 * (1 - 20 * 0.015f))
					const bool succeeded = draws.first >= failureThreshold(failureChance, diff);
					const bool blue = succeeded && draws.second < critBlueThreshold(diff);
					const bool limited = quality.recipeId == OFFERING_RECIPE;
					const int32_t success = succeeded ? successStep(diff, blue, draws.multi, quality.modifier) : 0;
					const int32_t failure = succeeded ? 0 : failureStep(diff, limited, draws.multi, quality.modifier);
					// a quality above RARE sets the speed whatever the level difference; below it the difference does (CraftingTask.java:170-172)
					const int32_t speed = quality.modifier < 1.0f ? quality.speed : (diff == 0 ? 900 : 300);
					const int32_t delay = quality.modifier < 1.0f ? 1200 : (diff == 0 ? 1200 : 600);
					EXPECT_EQ(speed, executionSpeedOf(diff, quality.modifier));
					EXPECT_EQ(delay, barDelayOf(diff, quality.modifier));
					const int32_t progress = blue ? 2 : 1;
					clearSent();
					craft->sendInteractionUpdate();
					EXPECT_EQ(sentCraftPackets(), (Lines{update(quality.skillId, progress, quality.productId, success, failure, speed, delay)}))
						<< quality.recipeId << " diff " << diff << " failure chance " << failureChance << " seed " << seed;
				}
			}
		}
	}
}

TEST_F(CraftingTaskTest, TheSpeedAndTheBarDelayFollowTheLevelDifference) {
	AtomicConfigScope<int32_t> noFailure(CraftConfig::MAX_CRAFT_FAILURE_CHANCE, 0);
	struct Speed {
		int32_t diff;
		int32_t speed;
		int32_t delay;
	};
	// the oracle's executionSpeed / showBarDelay for 155001381 at skill levels 1, 11, 21 and 60; 5 is the Java arithmetic's (900 - 150, 1200 - 150)
	for (const Speed& expected : {Speed{0, 900, 1200}, Speed{5, 750, 1050}, Speed{10, 600, 900}, Speed{20, 300, 600}, Speed{59, 300, 500}}) {
		const Draws draws = drawsOf(41);
		Ref<CraftingTaskProbe> craft = newTask(ROAST_ININA_RECIPE, expected.diff);
		Rnd::seedCurrentThreadForTests(41);
		craft->analyzeInteraction();
		const bool blue = (expected.diff >= 41 ? draws.first : draws.second) < critBlueThreshold(expected.diff);
		const int32_t success = std::min(1000, successStep(expected.diff, blue, draws.multi, 1.0f));
		clearSent();
		craft->sendInteractionUpdate();
		EXPECT_EQ(sentCraftPackets(), (Lines{update(COOKING, blue ? 2 : 1, ROAST_ININA, success, 0, expected.speed, expected.delay)}))
			<< "diff " << expected.diff;
	}
}

TEST_F(CraftingTaskTest, TheBarsStopAtTheFullBar) {
	{
		AtomicConfigScope<int32_t> noFailure(CraftConfig::MAX_CRAFT_FAILURE_CHANCE, 0);
		Ref<CraftingTaskProbe> craft = newTask(ROAST_ININA_RECIPE, 0);
		craft->setBars(990, 995);
		Rnd::seedCurrentThreadForTests(3);
		craft->analyzeInteraction();
		EXPECT_EQ(craft->successValue(), 1000);
		EXPECT_EQ(craft->failureValue(), 995);
		// `else if` (CraftingTask.java:165-168): a failure value above the bar (never reached in play) is left alone when the success bar was capped
		craft->setBars(990, 1500);
		craft->analyzeInteraction();
		EXPECT_EQ(craft->successValue(), 1000);
		EXPECT_EQ(craft->failureValue(), 1500);
	}
	AtomicConfigScope<int32_t> alwaysFail(CraftConfig::MAX_CRAFT_FAILURE_CHANCE, 100);
	Ref<CraftingTaskProbe> craft = newTask(ROAST_ININA_RECIPE, 0);
	craft->setBars(995, 990);
	Rnd::seedCurrentThreadForTests(4);
	craft->analyzeInteraction();
	EXPECT_EQ(craft->successValue(), 995);
	EXPECT_EQ(craft->failureValue(), 1000);
}

// ------------------------------------------------------------------------------------------------------------------------- the finishes

TEST_F(CraftingTaskTest, OnFailureFinishSendsTheFailedBarAndTheFailureAnimation) {
	Ref<CraftingTaskProbe> craft = newTask(ROAST_ININA_RECIPE, 0);
	craft->setBars(200, 1000);
	craft->onFailureFinish();
	EXPECT_EQ(sentCraftPackets(), (Lines{update(COOKING, 6, ROAST_ININA, 200, 1000), animation(crafterId(), stationId(), 0, 3)}));
	EXPECT_EQ(craft->successValue(), 200);
	EXPECT_EQ(craft->failureValue(), 1000);
}

TEST_F(CraftingTaskTest, WithoutAComboProductAFullBarEndsInFinishCrafting) {
	// maxCritCount = getComboProductSize() = 0: calculateCrit answers false before it asks the house or rolls (CraftingTask.java:61-62) - no
	// database here, so a house lookup would throw
	const Draws draws = drawsOf(51);
	Ref<CraftingTaskProbe> craft = newTask(STEEL_NAIL_RECIPE, 3);
	craft->setBars(1000, 40);
	Rnd::seedCurrentThreadForTests(51);
	const SuccessFinish finish = successFinish(*craft, STEEL_NAIL);
	EXPECT_EQ(Rnd::nextFloat(1.0f, 2.0f), draws.multi) << "nothing was rolled";
	EXPECT_EQ(sentCraftPackets(), (Lines{update(WEAPONSMITHING, 5, STEEL_NAIL, 1000, 40), animation(crafterId(), stationId(), 0, 2)}));
	expectFinishCrafting(finish, 1);

	clearSent();
	Ref<CraftingTaskProbe> morph = newMorphTask();
	morph->setBars(1000, 0);
	const SuccessFinish morphFinish = successFinish(*morph, ARIA);
	EXPECT_EQ(sentCraftPackets(), (Lines{update(MORPH, 5, ARIA, 1000, 0, 0, 1000), animation(crafterId(), crafterId(), 0, 2)}));
	expectFinishCrafting(morphFinish, 3);
}

TEST_F(CraftingTaskTest, TheEnhancementStoneBonusReachesFinishCraftingAsFifteenPercentMoreXp) {
	// CM_CRAFT's craft type 1 (the enhancement stone) makes CraftService.startCrafting create the task with bonus 15 (CraftService.java:123); the
	// constructor keeps it (CraftingTask.java:35) and onSuccessFinish hands it to finishCrafting (:55), which adds 15% to the xp reward
	// (CraftService.java:55-56). `oracle.py m5c-craft --no-profile --recipe 155000076 --skill-level 20`: --craft-type 0 gainedSkillXp 148,
	// playerExp 148; --craft-type 1 xpRewardWithBonus 170, gainedSkillXp 170, playerExp 170 (requiredExp 318: no level-up). Steel Nail has no
	// combo product, so calculateCrit answers false before the house lookup (no database).
	struct Bonus {
		int32_t bonus;
		int32_t xp;
	};
	for (const Bonus& expected : {Bonus{0, 148}, Bonus{15, 170}}) {
		learn(crafter, 20); // the skills again, at 0 xp
		ASSERT_EQ(skillXp(WEAPONSMITHING), 0);
		const int64_t exp = characterExp();
		Ref<CraftingTaskProbe> craft = newTask(STEEL_NAIL_RECIPE, 15, expected.bonus);
		craft->setBars(1000, 0);
		expectFinishCrafting(successFinish(*craft, STEEL_NAIL), 1);
		EXPECT_EQ(skillXp(WEAPONSMITHING), expected.xp) << "bonus " << expected.bonus;
		EXPECT_EQ(characterExp(), exp + expected.xp) << "bonus " << expected.bonus;
	}
}

TEST_F(CraftingTaskTest, ThePlayersAroundSeeTheAnimationsButNotTheBars) {
	// every SM_CRAFT_ANIMATION goes out with broadcastPacket(requester, ..., true): the crafter and every player he knows
	// (PacketSendUtility.java:88-93: toSelf, then the known list's players); every SM_CRAFT_UPDATE with sendPacket, to the crafter alone
	cp::PlayerFixture onlooker = cp::makePlayer(5310, 9540, "Onlooker");
	cp::TestClient onlookerClient;
	onlookerClient.enterWorld(onlooker);
	ASSERT_TRUE(crafter.knownList().addForTest(*onlooker.player)) << "the crafter knows the onlooker (the see notification is counted, not sent)";
	onlookerClient->clearSent();
	auto seen = [&onlookerClient] {
		Lines lines = craftPackets(onlookerClient->sent());
		onlookerClient->clearSent();
		return lines;
	};

	Ref<CraftingTaskProbe> craft = newTask(STEEL_NAIL_RECIPE, 3);
	craft->onInteractionStart();
	EXPECT_EQ(seen(), (Lines{animation(crafterId(), stationId(), WEAPONSMITHING, 0), animation(crafterId(), stationId(), WEAPONSMITHING, 1)}))
		<< "the start (CraftingTask.java:114-117)";
	craft->setBars(420, 130);
	craft->sendInteractionUpdate();
	EXPECT_TRUE(seen().empty()) << "a tick's update is the crafter's (:92-93)";
	craft->onInteractionAbort();
	EXPECT_EQ(seen(), (Lines{animation(crafterId(), stationId(), 0, 2)})) << "the abort (:98-99)";
	craft->setBars(420, 1000);
	craft->onFailureFinish();
	EXPECT_EQ(seen(), (Lines{animation(crafterId(), stationId(), 0, 3)})) << "the failure (:41-43)";
	craft->setBars(1000, 130);
	const SuccessFinish finish = successFinish(*craft, STEEL_NAIL);
	EXPECT_EQ(seen(), (Lines{animation(crafterId(), stationId(), 0, 2)})) << "the success (:52-54)";
	expectFinishCrafting(finish, 1);

	// the crafter got every one of them, the bars included
	EXPECT_EQ(sentCraftPackets(),
		(Lines{update(WEAPONSMITHING, 0, STEEL_NAIL, 1000, 1000), update(WEAPONSMITHING, 1, STEEL_NAIL, 0, 0),
			animation(crafterId(), stationId(), WEAPONSMITHING, 0), animation(crafterId(), stationId(), WEAPONSMITHING, 1),
			update(WEAPONSMITHING, 1, STEEL_NAIL, 420, 130), update(WEAPONSMITHING, 4, STEEL_NAIL, 0, 0), animation(crafterId(), stationId(), 0, 2),
			update(WEAPONSMITHING, 6, STEEL_NAIL, 420, 1000), animation(crafterId(), stationId(), 0, 3), update(WEAPONSMITHING, 5, STEEL_NAIL, 1000, 130),
			animation(crafterId(), stationId(), 0, 2)}));
	onlooker.player->setClientConnection(nullptr);
}

// ------------------------------------------------------------------------------------------------------------------------- the scheduler

TEST_F(CraftingTaskTest, AStartedCraftBelowTheRecipeLevelFailsOnItsSecondRun) {
	ASSERT_TRUE(crafter.player->isOnline()) << "the scheduled body is `!requester.isOnline() || onInteraction()` (AbstractInteractionTask.java:72)";
	Ref<CraftingTaskProbe> craft = newTask(ROAST_ININA_RECIPE, -1);
	craft->start();
	clearSent();

	executor->advance(std::chrono::milliseconds(999));
	EXPECT_TRUE(sentCraftPackets().empty()) << "the first run is due after the delay of 1000 ms";
	executor->advance(std::chrono::milliseconds(1));
	// run 1: AbstractCraftTask.onInteraction analyzes (the failure bar fills) and sends the update; no speed was computed (the early return)
	EXPECT_EQ(sentCraftPackets(), (Lines{update(COOKING, 1, ROAST_ININA, 0, 1000)}));
	EXPECT_TRUE(craft->isInProgress());

	clearSent();
	executor->advance(std::chrono::milliseconds(2500));
	// run 2: the failure bar is full: onFailureFinish, then stop()
	EXPECT_EQ(sentCraftPackets(), (Lines{update(COOKING, 6, ROAST_ININA, 0, 1000), animation(crafterId(), stationId(), 0, 3)}));
	EXPECT_FALSE(craft->isInProgress());
	EXPECT_FALSE(crafter.player->getInteractionTask());

	clearSent();
	executor->advance(std::chrono::milliseconds(10000));
	EXPECT_TRUE(sentCraftPackets().empty()) << "a stopped task never runs again";
}

TEST_F(CraftingTaskTest, AStartedMorphFinishesOnItsSecondRun) {
	Ref<CraftingTaskProbe> craft = newMorphTask();
	craft->start();
	clearSent();

	executor->advance(std::chrono::milliseconds(1000));
	EXPECT_EQ(sentCraftPackets(), (Lines{update(MORPH, 1, ARIA, 1000, 0, 0, 1000)}));
	clearSent();
	executor->advance(std::chrono::milliseconds(2500));
	EXPECT_EQ(sentCraftPackets(), (Lines{update(MORPH, 5, ARIA, 1000, 0, 0, 1000), animation(crafterId(), crafterId(), 0, 2)}));
	EXPECT_FALSE(craft->isInProgress()) << "onSuccessFinish answered true: stop()";
	EXPECT_EQ(crafter.player->getInventory().getItemCountByItemId(ARIA), 3) << "CraftService.finishCrafting added the product";
	EXPECT_FALSE(crafter.player->getInteractionTask());
}

// ------------------------------------------------------------------------------------------------------------------------- calculateCrit

class CraftingTaskDatabaseTest : public CraftingTaskTest {
protected:
	/** @return false (and the case skips itself) without the test database */
	static bool requireDatabase() {
		if (!isDatabaseEnabled())
			return false;
		setUpDatabaseOnce();
		return true;
	}

	/** Another crafter at the station, with its own connection */
	struct OtherCrafter {
		cp::PlayerFixture fixture;
		std::unique_ptr<cp::TestClient> client;

		Player& player() const { return *fixture.player; }
		int32_t id() const { return fixture.player->getObjectId(); }
		Lines sent() const { return craftPackets((*client)->sent()); }
		void clearSent() const { (*client)->clearSent(); }
	};

	OtherCrafter otherCrafter(int32_t objectId, int32_t accountId, std::string_view name) {
		OtherCrafter other{cp::makePlayer(objectId, accountId, name), std::make_unique<cp::TestClient>()};
		learn(other.fixture, 20);
		other.client->enterWorld(other.fixture);
		other.clearSent();
		return other;
	}

	static void leave(OtherCrafter& other) { other.fixture.player->setClientConnection(nullptr); }
};

TEST_F(CraftingTaskDatabaseTest, CalculateCritRollsTheCritChanceThenTheComboChanceAndAnEstateOrPalaceAddsFive) {
	if (!requireDatabase())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";
	// HousingService keeps the houses, and with them pointers into these holders, for the rest of the process: published once, never reset
	if (!dataholders::DataManager::HOUSE_BUILDING_DATA) {
		xml::LoadContext context;
		dataholders::DataManager::HOUSE_BUILDING_DATA.publish(xml::bindString<dataholders::HouseBuildingData>(context, CRAFT_HOUSE_BUILDINGS_XML));
	}
	if (!dataholders::DataManager::HOUSE_DATA) {
		xml::LoadContext context;
		dataholders::DataManager::HOUSE_DATA.publish(xml::bindString<dataholders::HouseData>(context, CRAFT_HOUSE_LANDS_XML));
	}
	constexpr int32_t ESTATE_OWNER = 5302;
	constexpr int32_t PALACE_OWNER = 5303;
	constexpr int32_t MANSION_OWNER = 5304;
	constexpr int32_t TENANT = 5305;
	constexpr int32_t HOUSE_OWNER = 5306;
	constexpr int32_t STUDIO_OWNER = 5307;
	insertPlayer(crafterId(), "Crafter", 9531);
	insertPlayer(ESTATE_OWNER, "EstateOwner", 9532);
	insertPlayer(PALACE_OWNER, "PalaceOwner", 9533);
	insertPlayer(MANSION_OWNER, "MansionOwner", 9534);
	insertPlayer(TENANT, "Tenant", 9535);
	insertPlayer(HOUSE_OWNER, "HouseOwner", 9536);
	insertPlayer(STUDIO_OWNER, "StudioOwner", 9537);
	insertHouse(900001, ESTATE_OWNER, 351000, 6101);   // land 326002, building 351000: ESTATE
	insertHouse(900002, PALACE_OWNER, 350000, 10001);  // land 325001, building 350000: PALACE
	insertHouse(900003, MANSION_OWNER, 352000, 6002);  // land 327002, building 352000: MANSION
	insertHouse(900004, HOUSE_OWNER, 353000, 6005);    // land 328002, building 353000: HOUSE
	insertHouse(900005, STUDIO_OWNER, 355000, 2001);   // land 329001, building 355000: STUDIO (HousesDAO loads address 2001 as a studio)
	OtherCrafter estate = otherCrafter(ESTATE_OWNER, 9532, "EstateOwner");
	OtherCrafter palace = otherCrafter(PALACE_OWNER, 9533, "PalaceOwner");
	OtherCrafter mansion = otherCrafter(MANSION_OWNER, 9534, "MansionOwner");
	OtherCrafter tenant = otherCrafter(TENANT, 9535, "Tenant");
	OtherCrafter house = otherCrafter(HOUSE_OWNER, 9536, "HouseOwner");
	OtherCrafter studio = otherCrafter(STUDIO_OWNER, 9537, "StudioOwner");
	OtherCrafter self{crafter, nullptr}; // the fixture's crafter (no house) with the fixture's connection
	auto sentOf = [&](const OtherCrafter& other) { return other.client ? other.sent() : sentCraftPackets(); };
	auto clearSentOf = [&](const OtherCrafter& other) {
		if (other.client)
			other.clearSent();
		else
			clearSent();
	};

	// 1. Roast Inina (one combo product) at a full bar, for a crafter without a house and the owner of each size of house. CRAFT_CRIT_CHANCES is
	//    the crafter's roll less 4.99 in one run and less 5.01 in the other: without a bonus the roll misses both (`chance() >= chance`,
	//    CraftingTask.java:82), and with the +5 of an ESTATE or a PALACE (:73-80) it hits the first and misses the second - so only these two
	//    sizes get a bonus, and it is 5 to within 0.01. CRAFT_COMBO_CHANCES is 100: the first roll must not read it.
	struct HouseCase {
		OtherCrafter* crafter;
		std::optional<HouseType> house; // the crafter's active house
		uint64_t seed;
	};
	for (const HouseCase& houseCase : {HouseCase{&self, std::nullopt, 7001}, HouseCase{&estate, HouseType::ESTATE, 7002},
			 HouseCase{&palace, HouseType::PALACE, 7003}, HouseCase{&mansion, HouseType::MANSION, 7004}, HouseCase{&house, HouseType::HOUSE, 7006},
			 HouseCase{&studio, HouseType::STUDIO, 7007}}) {
		OtherCrafter& other = *houseCase.crafter;
		// the fixture: the house HousingService loaded for the crafter (a missing one would make a no-bonus run pass for the wrong reason)
		const Ptr<gameserver::model::house::House> active = other.player().getActiveHouse();
		ASSERT_EQ(active ? std::optional<HouseType>(active->getHouseType()) : std::nullopt, houseCase.house) << other.id();
		const bool bonus = houseCase.house == HouseType::ESTATE || houseCase.house == HouseType::PALACE;
		for (const float belowTheRoll : {4.99f, 5.01f}) {
			const uint64_t seed = houseCase.seed + (belowTheRoll < 5.0f ? 0 : 100);
			const std::vector<float> rolls = chancesOf(seed, 2);
			RatesScope crit(RatesConfig::CRAFT_CRIT_CHANCES, {rolls[0] - belowTheRoll, rolls[0] - belowTheRoll});
			RatesScope combo(RatesConfig::CRAFT_COMBO_CHANCES, {100.0f, 100.0f});
			const bool proc = bonus && belowTheRoll < 5.0f;
			Ref<CraftingTaskProbe> craft = newTaskOf(other.player(), ROAST_ININA_RECIPE, 0);
			craft->setBars(1000, 0);
			clearSentOf(other);
			Rnd::seedCurrentThreadForTests(seed);
			const SuccessFinish first = successFinish(*craft, other.player(), proc ? TASTY_ROAST_ININA : ROAST_ININA);
			if (!proc) {
				EXPECT_EQ(sentOf(other), (Lines{update(COOKING, 5, ROAST_ININA, 1000, 0), animation(other.id(), stationId(), 0, 2)}))
					<< other.id() << " crit chance = roll - " << belowTheRoll;
				expectFinishCrafting(first, 2);
				continue;
			}
			// the proc: the combo product becomes the bar's item and the bar starts again, with action 3 (CraftingTask.java:85-87, 49, 112)
			expectProc(first);
			EXPECT_EQ(sentOf(other), (Lines{update(COOKING, 3, TASTY_ROAST_ININA, 1000, 1000), update(COOKING, 1, TASTY_ROAST_ININA, 0, 0),
										  animation(other.id(), stationId(), COOKING, 0), animation(other.id(), stationId(), COOKING, 1)}))
				<< other.id();
			EXPECT_EQ(craft->successValue(), 0);
			EXPECT_EQ(craft->failureValue(), 0);

			// 2. the only combo product is used: critCount 1 >= maxCritCount 1 answers false before any roll (:61-62), and finishCrafting gets
			//    the crit count 1, whose product is the combo product (CraftService.java:70)
			craft->setBars(1000, 0);
			clearSentOf(other);
			const SuccessFinish second = successFinish(*craft, other.player(), TASTY_ROAST_ININA);
			EXPECT_EQ(Rnd::chance(), rolls[1]) << "no second roll";
			EXPECT_EQ(sentOf(other), (Lines{update(COOKING, 5, TASTY_ROAST_ININA, 1000, 0), animation(other.id(), stationId(), 0, 2)}))
				<< other.id();
			expectFinishCrafting(second, 2);
		}
	}

	// 3. a roll equal to the chance misses it: `if (Rnd.chance() >= chance) return false`
	{
		const std::vector<float> rolls = chancesOf(7005, 1);
		RatesScope crit(RatesConfig::CRAFT_CRIT_CHANCES, {rolls[0], rolls[0]});
		Ref<CraftingTaskProbe> craft = newTaskOf(tenant.player(), ROAST_ININA_RECIPE, 0);
		craft->setBars(1000, 0);
		tenant.clearSent();
		Rnd::seedCurrentThreadForTests(7005);
		const SuccessFinish finish = successFinish(*craft, tenant.player(), ROAST_ININA);
		EXPECT_EQ(tenant.sent(), (Lines{update(COOKING, 5, ROAST_ININA, 1000, 0), animation(TENANT, stationId(), 0, 2)}));
		expectFinishCrafting(finish, 2);
	}

	// 4. Sobi Textile (two combo products, RARE then LEGEND): the first roll reads CRAFT_CRIT_CHANCES, every later one CRAFT_COMBO_CHANCES
	//    (CraftingTask.java:67-72). A seed whose second roll is above its first: the crit chance lies between them, the combo chance is 100, so
	//    the second roll hits only by the combo chance.
	uint64_t seed = 9001;
	std::vector<float> rolls = chancesOf(seed, 3);
	while (rolls[1] < rolls[0] + 1.0f)
		rolls = chancesOf(++seed, 3);
	{
		const float between = (rolls[0] + rolls[1]) / 2.0f;
		RatesScope crit(RatesConfig::CRAFT_CRIT_CHANCES, {between, between});
		RatesScope combo(RatesConfig::CRAFT_COMBO_CHANCES, {100.0f, 100.0f});
		Ref<CraftingTaskProbe> craft = newTask(SOBI_TEXTILE_RECIPE, 0);
		craft->setBars(1000, 0);
		clearSent();
		Rnd::seedCurrentThreadForTests(seed);
		expectProc(successFinish(*craft, SOBI_TEXTILE));
		EXPECT_EQ(sentCraftPackets(), (Lines{update(TAILORING, 3, FINE_SOBI_TEXTILE, 1000, 1000), update(TAILORING, 1, FINE_SOBI_TEXTILE, 0, 0),
										  animation(crafterId(), stationId(), TAILORING, 0), animation(crafterId(), stationId(), TAILORING, 1)}));
		craft->setBars(1000, 0);
		clearSent();
		expectProc(successFinish(*craft, SOBI_TEXTILE));
		EXPECT_EQ(sentCraftPackets(), (Lines{update(TAILORING, 3, SUPERB_SOBI_TEXTILE, 1000, 1000), update(TAILORING, 1, SUPERB_SOBI_TEXTILE, 0, 0),
										  animation(crafterId(), stationId(), TAILORING, 0), animation(crafterId(), stationId(), TAILORING, 1)}));
		EXPECT_EQ(Rnd::chance(), rolls[2]) << "one roll per proc";

		// 5. the bar after the procs is the LEGEND combo product's: analyzeInteraction's quality switch reads the current item template
		//    (bonusModifier 0.9f, speed 990, delay 1200), not the product's (COMMON)
		AtomicConfigScope<int32_t> noFailure(CraftConfig::MAX_CRAFT_FAILURE_CHANCE, 0);
		const Draws draws = drawsOf(61);
		const bool blue = draws.second < critBlueThreshold(0);
		const int32_t step = successStep(0, blue, draws.multi, LEGEND_MODIFIER);
		Rnd::seedCurrentThreadForTests(61);
		craft->analyzeInteraction();
		clearSent();
		craft->sendInteractionUpdate();
		EXPECT_EQ(sentCraftPackets(), (Lines{update(TAILORING, blue ? 2 : 1, SUPERB_SOBI_TEXTILE, step, 0, 990, 1200)}));
	}

	for (OtherCrafter* other : {&estate, &palace, &mansion, &tenant, &house, &studio})
		leave(*other);
}

} // namespace
} // namespace aion::gameserver::skillengine::task::crafttest
