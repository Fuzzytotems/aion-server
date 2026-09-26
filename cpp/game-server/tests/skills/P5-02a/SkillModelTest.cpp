// P5-02a, M5b-2 stage 1 part 2 (m5b2-plan.md S-06, S-07): the chain state a player carries between casts (ChainSkills.java, ChainSkill.java),
// WeaponTypeWrapper (WeaponTypeWrapper.java) and the DashStatus id companion (DashStatus.java). Expectations follow the Java sources.
//
// The chain's time arms read the wall clock (System.currentTimeMillis, TimeUtils.h), so the expiry case waits a few real milliseconds.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/skillengine/model/ChainSkill.h"
#include "aion/gameserver/skillengine/model/ChainSkills.h"
#include "aion/gameserver/skillengine/model/DashStatusInfo.h"
#include "aion/gameserver/skillengine/model/WeaponTypeWrapper.h"

namespace aion::gameserver::skillengine::model::test {
namespace {

using gameserver::model::templates::item::enums::ItemGroup;
using runtime::Ptr;
using runtime::Ref;

class SkillModelTest : public testing::Test {
protected:
	void TearDown() override { runtime::Reclaimer::getInstance().drain(); }
};

TEST_F(SkillModelTest, ChainSkillCountsItsUsesAndClearsBackToEmpty) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<ChainSkill> skill = ChainSkill::create("A_1TH");
	EXPECT_EQ(skill->getCategory(), "A_1TH");
	EXPECT_EQ(skill->getUseCount(), 0);
	EXPECT_EQ(skill->getLastUseTime(), 0);

	int64_t before = commons::utils::currentTimeMillis();
	skill->increaseUseCount();
	skill->increaseUseCount();
	int64_t after = commons::utils::currentTimeMillis();
	EXPECT_EQ(skill->getUseCount(), 2) << "ChainSkill.java: useCount++";
	EXPECT_GE(skill->getLastUseTime(), before);
	EXPECT_LE(skill->getLastUseTime(), after);

	skill->clear();
	EXPECT_EQ(skill->getCategory(), "");
	EXPECT_EQ(skill->getUseCount(), 0);
	EXPECT_EQ(skill->getLastUseTime(), 0);
}

TEST_F(SkillModelTest, UpdateChainFillsAnEmptyChainAndMovesAnotherCategoryToPrevious) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<ChainSkills> chain = ChainSkills::create();
	Ptr<ChainSkill> first = chain->getCurrentChainSkill();

	// ChainSkills.updateChain: an empty current skill takes the category (the same object keeps counting)
	chain->updateChain("A_1TH", 0);
	EXPECT_EQ(chain->getCurrentChainSkill(), first) << "the empty current chain skill is reused, not replaced";
	EXPECT_EQ(first->getCategory(), "A_1TH");
	EXPECT_EQ(chain->getCurrentChainCount("A_1TH"), 1);
	chain->updateChain("A_1TH", 0);
	EXPECT_EQ(chain->getCurrentChainCount("A_1TH"), 2) << "the same category counts on";
	EXPECT_EQ(chain->getCurrentChainCount("B_2TH"), 0) << "another category answers 0";

	// another category: the current skill becomes the previous one and a new skill starts at 1
	chain->updateChain("B_2TH", 0);
	EXPECT_EQ(chain->getPreviousChainSkill(), first);
	EXPECT_EQ(chain->getPreviousChainSkill()->getUseCount(), 2);
	EXPECT_EQ(chain->getCurrentChainSkill()->getCategory(), "B_2TH");
	EXPECT_EQ(chain->getCurrentChainCount("B_2TH"), 1);
	EXPECT_FALSE(chain->isChainExpired()) << "duration 0: expireTime stays 0, which never expires";
}

TEST_F(SkillModelTest, ResetChainClearsBothSkillsInPlaceOnlyWhenAChainIsActive) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<ChainSkills> chain = ChainSkills::create();
	chain->updateChain("A_1TH", 0);
	chain->updateChain("B_2TH", 60000);
	Ptr<ChainSkill> previous = chain->getPreviousChainSkill();
	Ptr<ChainSkill> current = chain->getCurrentChainSkill();

	chain->resetChain();
	EXPECT_EQ(chain->getCurrentChainSkill(), current) << "Java clears the objects, it does not replace them";
	EXPECT_EQ(chain->getPreviousChainSkill(), previous);
	EXPECT_EQ(current->getCategory(), "");
	EXPECT_EQ(current->getUseCount(), 0);
	EXPECT_EQ(previous->getCategory(), "");
	EXPECT_EQ(previous->getUseCount(), 0);

	// an empty chain is left alone: the previous skill is not cleared (resetChain's `if (!chainSkill.getCategory().isEmpty())`)
	Ref<ChainSkills> other = ChainSkills::create();
	other->updateChain("A_1TH", 0);
	other->updateChain("B_2TH", 0);
	other->getCurrentChainSkill()->clear(); // current empty, previous still "A_1TH"
	other->resetChain();
	EXPECT_EQ(other->getPreviousChainSkill()->getCategory(), "A_1TH");
}

TEST_F(SkillModelTest, AChainWithADurationExpires) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<ChainSkills> chain = ChainSkills::create();
	chain->updateChain("A_1TH", 60000);
	EXPECT_FALSE(chain->isChainExpired()) << "a minute from now";

	chain->updateChain("A_1TH", 1);
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	EXPECT_TRUE(chain->isChainExpired()) << "expireTime = now + 1 ms is in the past";

	chain->resetChain();
	EXPECT_FALSE(chain->isChainExpired()) << "resetChain sets expireTime back to 0";
}

TEST_F(SkillModelTest, WeaponTypeWrapperFoldsADualWieldPairToItsOneTimesKey) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	// WeaponTypeWrapper.java:13-45: DAGGER, SWORD, MACE, TOOLHOES, GUN in both hands keep the main hand group twice
	for (ItemGroup group : {ItemGroup::DAGGER, ItemGroup::SWORD, ItemGroup::MACE, ItemGroup::TOOLHOES, ItemGroup::GUN}) {
		Ref<WeaponTypeWrapper> pair = WeaponTypeWrapper::create(group, ItemGroup::SHIELD);
		EXPECT_EQ(pair->getMainHand(), group);
		EXPECT_EQ(pair->getOffHand(), group) << "the off hand becomes the main hand's group";
	}
	// any other main hand with an off hand: the off hand is dropped
	Ref<WeaponTypeWrapper> polearm = WeaponTypeWrapper::create(ItemGroup::POLEARM, ItemGroup::SWORD);
	EXPECT_EQ(polearm->getMainHand(), ItemGroup::POLEARM);
	EXPECT_EQ(polearm->getOffHand(), std::nullopt);
	// a missing hand: both kept as given
	Ref<WeaponTypeWrapper> single = WeaponTypeWrapper::create(ItemGroup::SWORD, std::nullopt);
	EXPECT_EQ(single->getMainHand(), ItemGroup::SWORD);
	EXPECT_EQ(single->getOffHand(), std::nullopt);
	Ref<WeaponTypeWrapper> offOnly = WeaponTypeWrapper::create(std::nullopt, ItemGroup::SHIELD);
	EXPECT_EQ(offOnly->getMainHand(), std::nullopt);
	EXPECT_EQ(offOnly->getOffHand(), ItemGroup::SHIELD);
}

TEST_F(SkillModelTest, WeaponTypeWrapperEqualsHashCodeToStringAndCompareTo) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<WeaponTypeWrapper> daggers = WeaponTypeWrapper::create(ItemGroup::DAGGER, ItemGroup::SWORD);
	Ref<WeaponTypeWrapper> daggers2 = WeaponTypeWrapper::create(ItemGroup::DAGGER, ItemGroup::DAGGER);
	Ref<WeaponTypeWrapper> sword = WeaponTypeWrapper::create(ItemGroup::SWORD, std::nullopt);
	Ref<WeaponTypeWrapper> bow = WeaponTypeWrapper::create(ItemGroup::BOW, std::nullopt);
	Ref<WeaponTypeWrapper> none = WeaponTypeWrapper::create(std::nullopt, std::nullopt);

	EXPECT_TRUE(daggers->equals(*daggers2)) << "both fold to (DAGGER, DAGGER)";
	EXPECT_EQ(daggers->hashCode(), daggers2->hashCode()) << "equal wrappers hash equally";
	EXPECT_FALSE(daggers->equals(*sword));
	EXPECT_FALSE(sword->equals(*bow));
	EXPECT_TRUE(none->equals(*WeaponTypeWrapper::create(std::nullopt, std::nullopt)));
	EXPECT_EQ(none->hashCode(), 31 * 31) << "prime * (prime * 1 + 0) + 0 for two nulls";

	EXPECT_EQ(daggers->toString(), "mainHand=\"DAGGER\" offHand=\"DAGGER\"");
	EXPECT_EQ(sword->toString(), "mainHand=\"SWORD\" offHand=\"null\"") << "Java's string concatenation of null";

	// WeaponTypeWrapper.java:77-88
	EXPECT_EQ(none->compareTo(*sword), 0) << "a null main hand compares 0";
	EXPECT_EQ(daggers->compareTo(*daggers2), 0) << "two off hands compare 0";
	EXPECT_EQ(daggers->compareTo(*sword), 1) << "only this one has an off hand";
	EXPECT_EQ(sword->compareTo(*daggers), -1) << "only the other one has an off hand";
	// String.compareTo of the names: 'B' - 'S' for BOW against SWORD, and the length difference for a prefix
	EXPECT_EQ(bow->compareTo(*sword), 'B' - 'S');
	EXPECT_EQ(sword->compareTo(*bow), 'S' - 'B');
	Ref<WeaponTypeWrapper> gun = WeaponTypeWrapper::create(ItemGroup::GUN, std::nullopt);
	Ref<WeaponTypeWrapper> sword2 = WeaponTypeWrapper::create(ItemGroup::SWORD, std::nullopt);
	EXPECT_EQ(sword->compareTo(*sword2), 0);
	EXPECT_EQ(gun->compareTo(*sword), 'G' - 'S');
}

TEST_F(SkillModelTest, DashStatusIdsAreJavasConstructorArguments) {
	// DashStatus.java:6-12: RANDOMMOVELOC_NEW is 6, not its ordinal 5
	EXPECT_EQ(getId(DashStatus::NONE), 0);
	EXPECT_EQ(getId(DashStatus::RANDOMMOVELOC), 1);
	EXPECT_EQ(getId(DashStatus::DASH), 2);
	EXPECT_EQ(getId(DashStatus::BACKDASH), 3);
	EXPECT_EQ(getId(DashStatus::MOVEBEHIND), 4);
	EXPECT_EQ(getId(DashStatus::RANDOMMOVELOC_NEW), 6);
}

} // namespace
} // namespace aion::gameserver::skillengine::model::test
