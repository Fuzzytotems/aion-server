// Player skill list and effect controller of P5-02 (wave 5a, work items B-05 and B-06): PlayerSkillEntry predicates and persistent state rules,
// PlayerSkillList over existing entries (the DAO load path), and the effect controller queries and logout paths of a player without effects.
// Expectations follow the Java sources.

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/CraftConfig.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/SkillTreeData.bind.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/DispelSlotType.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlotInfo.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::model::skill::test {
namespace {

using gameobjects::Persistable_PersistentState;
using runtime::Ptr;
using runtime::Ref;

TEST(PlayerSkillEntryTest, KindPredicates) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<PlayerSkillEntry> normal = PlayerSkillEntry::create(1001, 1, 0, Persistable_PersistentState::NOACTION);
	EXPECT_TRUE(normal->isNormalSkill());
	EXPECT_TRUE(normal->isNormalOrStigmaSkill());
	EXPECT_FALSE(normal->isStigmaSkill());
	EXPECT_FALSE(normal->isProfessionSkill());
	EXPECT_EQ(normal->getProfessionFlag(), 0);
	EXPECT_GT(normal->getFlag(), 0) << "a normal skill's flag is the (not stored) learn date";
	EXPECT_EQ(normal->getProfessionSkillBarSize(), 0);

	Ref<PlayerSkillEntry> stigma = PlayerSkillEntry::create(1002, 1, 1, Persistable_PersistentState::NOACTION);
	EXPECT_TRUE(stigma->isStigmaSkill());
	EXPECT_TRUE(stigma->isNormalStigmaSkill());
	EXPECT_FALSE(stigma->isLinkedStigmaSkill());
	EXPECT_FALSE(stigma->isNormalSkill());
	EXPECT_EQ(stigma->getFlag(), 0);
	Ref<PlayerSkillEntry> linked = PlayerSkillEntry::create(1003, 1, 3, Persistable_PersistentState::NOACTION);
	EXPECT_TRUE(linked->isLinkedStigmaSkill());
	EXPECT_FALSE(linked->isNormalStigmaSkill());

	Ref<PlayerSkillEntry> tapping = PlayerSkillEntry::create(30002, 520, 0, Persistable_PersistentState::NOACTION);
	EXPECT_TRUE(tapping->isTappingSkill());
	EXPECT_TRUE(tapping->isProfessionSkill());
	EXPECT_FALSE(tapping->isNormalOrStigmaSkill());
	EXPECT_EQ(tapping->getProfessionFlag(), 1);
	EXPECT_EQ(tapping->getProfessionSkillBarSize(), 4) << "min(520 / 100, 4)";

	Ref<PlayerSkillEntry> crafting = PlayerSkillEntry::create(40001, 460, 0, Persistable_PersistentState::NOACTION);
	crafting->setCurrentXp(77);
	EXPECT_TRUE(crafting->isCraftingSkill());
	EXPECT_EQ(crafting->getProfessionFlag(), 77);
	EXPECT_EQ(crafting->getProfessionSkillBarSize(), 5) << "460 / 100 + (460 - 350) / 100";
	Ref<PlayerSkillEntry> morph = PlayerSkillEntry::create(40009, 100, 0, Persistable_PersistentState::NOACTION);
	EXPECT_TRUE(morph->isMorphSkill());
	EXPECT_FALSE(morph->isCraftingSkill());
	EXPECT_EQ(morph->getProfessionFlag(), 1);
	Ref<PlayerSkillEntry> action = PlayerSkillEntry::create(50001, 1, 0, Persistable_PersistentState::NOACTION);
	EXPECT_FALSE(action->isProfessionSkill());
	EXPECT_FALSE(action->isNormalSkill());
}

TEST(PlayerSkillEntryTest, PersistentStateTransitions) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<PlayerSkillEntry> entry = PlayerSkillEntry::create(1001, 1, 0, Persistable_PersistentState::NEW);
	entry->setPersistentState(Persistable_PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(entry->getPersistentState(), Persistable_PersistentState::NEW) << "a new entry stays new";
	entry->setSkillLvl(2);
	EXPECT_EQ(entry->getSkillLevel(), 2);
	EXPECT_EQ(entry->getPersistentState(), Persistable_PersistentState::NEW);
	entry->setPersistentState(Persistable_PersistentState::DELETED);
	EXPECT_EQ(entry->getPersistentState(), Persistable_PersistentState::NOACTION) << "deleting a new entry needs no DB action";

	Ref<PlayerSkillEntry> stored = PlayerSkillEntry::create(1002, 1, 0, Persistable_PersistentState::UPDATED);
	stored->setSkillLvl(3);
	EXPECT_EQ(stored->getPersistentState(), Persistable_PersistentState::UPDATE_REQUIRED);
	stored->setPersistentState(Persistable_PersistentState::NOACTION);
	EXPECT_EQ(stored->getPersistentState(), Persistable_PersistentState::UPDATE_REQUIRED) << "NOACTION is ignored";
	stored->setPersistentState(Persistable_PersistentState::DELETED);
	EXPECT_EQ(stored->getPersistentState(), Persistable_PersistentState::DELETED);

	Ref<PlayerSkillEntry> temporary = PlayerSkillEntry::create(1003, 1, 0, Persistable_PersistentState::NOACTION);
	temporary->setSkillLvl(4);
	EXPECT_EQ(temporary->getPersistentState(), Persistable_PersistentState::NOACTION) << "temporary skills are never stored";
}

TEST(PlayerSkillListTest, LoadedEntriesAndRemoval) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<PlayerSkillEntry> first = PlayerSkillEntry::create(1001, 1, 0, Persistable_PersistentState::UPDATED);
	Ref<PlayerSkillEntry> second = PlayerSkillEntry::create(1002, 5, 0, Persistable_PersistentState::NEW);
	Ref<PlayerSkillList> list = PlayerSkillList::create(std::vector<Ptr<PlayerSkillEntry>>{first, second});
	EXPECT_EQ(list->size(), 2);
	EXPECT_TRUE(list->isSkillPresent(1001));
	EXPECT_FALSE(list->isSkillPresent(1003));
	EXPECT_EQ(list->getSkillEntry(1002).get(), second.get());
	EXPECT_FALSE(list->getSkillEntry(1003));
	EXPECT_EQ(list->getSkillLevel(1002), 5);
	EXPECT_THROW(static_cast<void>(list->getSkillLevel(1003)), runtime::NullPointerException);
	EXPECT_EQ(list->getAllSkills().size(), 2u);
	EXPECT_TRUE(list->getDeletedSkills().empty());

	EXPECT_TRUE(list->removeSkill(1001));
	EXPECT_FALSE(list->removeSkill(1001));
	EXPECT_TRUE(list->removeSkill(1002));
	EXPECT_EQ(list->size(), 0);
	std::vector<Ptr<PlayerSkillEntry>> deleted = list->getDeletedSkills();
	ASSERT_EQ(deleted.size(), 2u);
	EXPECT_EQ(deleted[0]->getPersistentState(), Persistable_PersistentState::DELETED);
	EXPECT_EQ(deleted[1]->getPersistentState(), Persistable_PersistentState::NOACTION) << "the new entry was never stored";
	EXPECT_TRUE(PlayerSkillList::create()->getAllSkills().empty());
}

TEST(SkillTargetSlotInfoTest, IdsAndDispelSlots) {
	using skillengine::model::SkillTargetSlot;
	EXPECT_EQ(getId(SkillTargetSlot::BUFF), 1);
	EXPECT_EQ(getId(SkillTargetSlot::NOSHOW), 64);
	EXPECT_EQ(getId(SkillTargetSlot::NONE), 128);
	EXPECT_EQ(skillengine::model::SKILL_TARGET_SLOT_FULLSLOTS, 127);
	EXPECT_EQ(of(skillengine::model::DispelSlotType::SPECIAL2), SkillTargetSlot::SPEC2);
	EXPECT_EQ(of(skillengine::model::DispelSlotType::DEBUFF), SkillTargetSlot::DEBUFF);
}

std::vector<Ref<gameobjects::player::PetCommonData>> noPets(gameobjects::player::Player&) {
	return {};
}

class PlayerEffectControllerTest : public testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 13));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		gameobjects::player::PetList::setPlayerPetsLoaderForTests(&noPets);
	}

	void TearDown() override {
		gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	runtime::ManualClock clock{0};
};

TEST_F(PlayerEffectControllerTest, QueriesAndLogoutWithoutEffects) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<account::Account> account = account::Account::create(7001);
	Ref<gameobjects::player::PlayerCommonData> commonData = gameobjects::player::PlayerCommonData::create(3001);
	commonData->setName("Effects");
	commonData->setRace(Race::ASMODIANS);
	Ref<gameobjects::player::PlayerAppearance> appearance = gameobjects::player::PlayerAppearance::create();
	account->addPlayerAccountData(std::make_unique<account::PlayerAccountData>(*account, *commonData, *appearance));
	account->setAccountWarehouse(std::make_unique<items::storage::PlayerStorage>(*account, items::storage::StorageType::ACCOUNT_WAREHOUSE));
	Ref<gameobjects::player::Player> player =
		gameobjects::VisibleObject::create<gameobjects::player::Player>(*account->getPlayerAccountData(3001), *account);
	player->setPosition(world::WorldPosition::create(220010000, 571.04f, 2787.34f, 299.875f, int8_t{0}));
	player->setKnownlist(std::make_unique<world::knownlist::KnownList>(*player));
	player->setEffectController(std::make_unique<controllers::effect::PlayerEffectController>(*player));
	Ptr<controllers::effect::PlayerEffectController> effects = player->getEffectController();
	ASSERT_TRUE(effects);

	using skillengine::effect::AbnormalState;
	EXPECT_TRUE(effects->isEmpty());
	EXPECT_EQ(effects->getAbnormals(), 0);
	EXPECT_TRUE(effects->isAbnormalSet(AbnormalState::NONE)) << "NONE: no abnormal state at all";
	EXPECT_TRUE(effects->isInAnyAbnormalState(AbnormalState::NONE));
	EXPECT_FALSE(effects->isAbnormalSet(AbnormalState::DISEASE));
	EXPECT_FALSE(effects->isInAnyAbnormalState(AbnormalState::CANT_MOVE_STATE));
	EXPECT_FALSE(effects->isUnderFear());
	EXPECT_FALSE(effects->isConfused());
	EXPECT_TRUE(effects->getAllEffects().empty());
	EXPECT_TRUE(effects->getAbnormalEffects().empty());
	EXPECT_TRUE(effects->getAbnormalEffectsToShow().empty());

	// enter world: SM_ABNORMAL_STATE of all slots (the offline player has no connection); logout: nothing to end
	EXPECT_NO_THROW(effects->updatePlayerEffectIcons(nullptr));
	EXPECT_NO_THROW(effects->broadCastEffects(nullptr));
	EXPECT_NO_THROW(effects->removeNonStorableEffectsForLogout());
	EXPECT_NO_THROW(effects->removeAllEffects(true));
	EXPECT_NO_THROW(effects->removeAllEffects()) << "die: the (empty) abnormal effects, then SM_ABNORMAL_EFFECT and the icons";
	EXPECT_NO_THROW(effects->clearEffectMapsWithoutNotify());
	EXPECT_TRUE(effects->isEmpty());

	// SkillEngine.applyEffectDirectly(SkillTemplate, ...) of the passive skills, M5a O-09, closed in part 3 (m5b2-plan.md D11): the template is
	// not checked, so a null one reaches new Effect(...), whose skillTemplate.getReqDispelCount() throws (Effect.java:154), and no partial is hit
	uint64_t partialHitsBefore = runtime::partialHitCount();
	EXPECT_THROW(skillengine::SkillEngine::getInstance().applyEffectDirectly(nullptr, 1, *player, *player), runtime::NullPointerException);
	EXPECT_EQ(runtime::partialHitCount(), partialHitsBefore);
}


/**
 * The character-creation half of B-05 (PlayerSkillList.addSkill / addTemporarySkill, reached from PlayerService.getPlayer ->
 * SkillLearnService.learnNewSkills -> autoLearnSkills) and the saved-effect path of B-06 (PlayerEffectsDAO calls addSavedEffect once per stored
 * row while the character is loaded). Expectations follow PlayerSkillList.java:47-77, SkillLearnService.java:34-60 and
 * PlayerEffectController.java:97-117.
 */
class PlayerSkillLearnTest : public testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 17));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		gameobjects::player::PetList::setPlayerPetsLoaderForTests(&noPets);
		// skills 1 and 2 are active, skill 3 is passive (SkillLearnService applies its effect through SkillEngine.applyEffectDirectly)
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, R"(<skill_data>)"
			R"(<skill_template skill_id="1" name="s1" nameId="1" skilltype="PHYSICAL" skillsubtype="ATTACK" activation="ACTIVE" duration="0" stack="S1"/>)"
			R"(<skill_template skill_id="2" name="s2" nameId="1" skilltype="PHYSICAL" skillsubtype="ATTACK" activation="ACTIVE" duration="0" stack="S2"/>)"
			R"(<skill_template skill_id="3" name="s3" nameId="1" skilltype="PHYSICAL" skillsubtype="BUFF" activation="PASSIVE" duration="0" stack="S3"/>)"
			R"(</skill_data>)"));
		// skill 2 learns from skill 1 (skillLearn), so learning it while skill 1 is known is not a new skill
		dataholders::DataManager::SKILL_TREE_DATA.publish(xml::bindString<dataholders::SkillTreeData>(treeContext, R"(<skill_tree>)"
			R"(<skill skillId="1" minLevel="0" race="ELYOS" classId="WARRIOR"/>)"
			R"(<skill skillId="2" minLevel="0" skillLearn="1" race="ELYOS" classId="WARRIOR"/>)"
			R"(<skill skillId="3" minLevel="0" race="ELYOS" classId="WARRIOR"/>)"
			R"(</skill_tree>)"));
	}

	void TearDown() override {
		dataholders::DataManager::SKILL_TREE_DATA.resetForTests();
		dataholders::DataManager::SKILL_DATA.resetForTests();
		gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	struct PlayerFixture {
		Ref<account::Account> account;
		Ref<gameobjects::player::PlayerCommonData> commonData;
		Ref<gameobjects::player::Player> player;
	};

	/** Java PlayerService.getPlayer: the player, its (empty) skill list and the effect controller */
	static PlayerFixture makePlayer(int32_t objectId) {
		PlayerFixture f;
		f.account = account::Account::create(9000 + objectId);
		f.commonData = gameobjects::player::PlayerCommonData::create(objectId);
		f.commonData->setName("Learn" + std::to_string(objectId));
		f.commonData->setRace(Race::ELYOS);
		f.commonData->setPlayerClass(PlayerClass::WARRIOR);
		Ref<gameobjects::player::PlayerAppearance> appearance = gameobjects::player::PlayerAppearance::create();
		f.account->addPlayerAccountData(std::make_unique<account::PlayerAccountData>(*f.account, *f.commonData, *appearance));
		f.account->setAccountWarehouse(
			std::make_unique<items::storage::PlayerStorage>(*f.account, items::storage::StorageType::ACCOUNT_WAREHOUSE));
		f.player = gameobjects::VisibleObject::create<gameobjects::player::Player>(*f.account->getPlayerAccountData(objectId), *f.account);
		f.player->setPosition(world::WorldPosition::create(210010000, 1212.94f, 1044.85f, 140.76f, int8_t{0}));
		f.player->setKnownlist(std::make_unique<world::knownlist::KnownList>(*f.player));
		f.player->setSkillList(PlayerSkillList::create());
		f.player->setEffectController(std::make_unique<controllers::effect::PlayerEffectController>(*f.player));
		return f;
	}

	runtime::ManualClock clock{0};
	xml::LoadContext skillContext;
	xml::LoadContext treeContext;
};

TEST_F(PlayerSkillLearnTest, AddSkillCreatesUpdatesAndRejectsEntries) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(4101);
	Ptr<PlayerSkillList> skills = f.player->getSkillList();
	ASSERT_TRUE(skills);
	runtime::resetUnportedHitsForTests();

	// a first skill: a new entry with PersistentState.NEW (isNew is true, nothing of its skill tree is known yet)
	EXPECT_TRUE(skills->addSkill(*f.player, 1, 1));
	Ptr<PlayerSkillEntry> first = skills->getSkillEntry(1);
	ASSERT_TRUE(first);
	EXPECT_EQ(first->getSkillLevel(), 1);
	EXPECT_EQ(first->getPersistentState(), Persistable_PersistentState::NEW);
	EXPECT_EQ(skills->size(), 1);

	// the same level again: rejected without touching the entry (skillLevel <= existingSkill.getSkillLevel())
	EXPECT_FALSE(skills->addSkill(*f.player, 1, 1));
	EXPECT_EQ(skills->getSkillEntry(1).get(), first.get());
	EXPECT_EQ(first->getSkillLevel(), 1);
	EXPECT_EQ(skills->size(), 1);

	// a higher level updates the existing entry (isNew false); a NEW entry stays NEW
	EXPECT_TRUE(skills->addSkill(*f.player, 1, 3));
	EXPECT_EQ(skills->getSkillEntry(1).get(), first.get());
	EXPECT_EQ(first->getSkillLevel(), 3);
	EXPECT_EQ(first->getPersistentState(), Persistable_PersistentState::NEW);

	// skill 2 learns from skill 1, which is known: the entry is created, but the skill counts as not new
	EXPECT_TRUE(skills->addSkill(*f.player, 2, 1));
	Ptr<PlayerSkillEntry> second = skills->getSkillEntry(2);
	ASSERT_TRUE(second);
	EXPECT_EQ(second->getPersistentState(), Persistable_PersistentState::NEW);
	EXPECT_EQ(skills->size(), 2);
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "the create path reaches no unported body";
}

TEST_F(PlayerSkillLearnTest, TemporarySkillsAreNotStoredAndPassiveSkillsApplyTheirEffect) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(4102);
	Ptr<PlayerSkillList> skills = f.player->getSkillList();
	runtime::resetUnportedHitsForTests();

	// addTemporarySkill: the same path with PersistentState.NOACTION, so the DAO never writes the skill
	EXPECT_TRUE(skills->addTemporarySkill(*f.player, 1, 2));
	Ptr<PlayerSkillEntry> temporary = skills->getSkillEntry(1);
	ASSERT_TRUE(temporary);
	EXPECT_EQ(temporary->getSkillLevel(), 2);
	EXPECT_EQ(temporary->getPersistentState(), Persistable_PersistentState::NOACTION);
	EXPECT_TRUE(skills->getDeletedSkills().empty());

	// a temporary entry follows the same level rules
	EXPECT_FALSE(skills->addTemporarySkill(*f.player, 1, 2));
	EXPECT_TRUE(skills->addSkill(*f.player, 1, 5));
	EXPECT_EQ(temporary->getSkillLevel(), 5);
	EXPECT_EQ(temporary->getPersistentState(), Persistable_PersistentState::NOACTION) << "a temporary skill is never stored";

	// SkillLearnService.onLearnSkill applies the effect of a passive skill through SkillEngine.applyEffectDirectly(SkillTemplate, ...), the M5a
	// O-09 warn stub until part 3 closed it (m5b2-plan.md D11): the Effect is applied, and without <effects> nothing is added and no partial hit
	uint64_t partialsBefore = runtime::partialHitCount();
	EXPECT_TRUE(skills->addSkill(*f.player, 3, 1));
	EXPECT_EQ(runtime::partialHitCount(), partialsBefore) << "the O-09 warn stub is gone";
	EXPECT_TRUE(f.player->getEffectController()->getAllEffects().empty());
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

/**
 * PlayerSkillList.addSkillXp (PlayerSkillList.java:84-126), the last body of the list: no experience past 40 levels above the object's level;
 * the gathering and crafting caps (30001 stops at 49; the tapping skills break out at 449 and, with the cap disabled, at 499 and above; every
 * listed skill stops at the mastery levels 99..549); otherwise the experience accumulates until (int) (0.23 * (level + 17.2)^2), which raises
 * the level by one, resets the experience and tells SkillLearnService.
 */
TEST_F(PlayerSkillLearnTest, AddSkillXpAccumulatesUntilTheLevelUpAndStopsAtTheCaps) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(4104);
	using gameobjects::Persistable_PersistentState;
	Ref<PlayerSkillEntry> learned = PlayerSkillEntry::create(2, 1, 0, Persistable_PersistentState::NOACTION);
	Ref<PlayerSkillEntry> human = PlayerSkillEntry::create(30001, 49, 0, Persistable_PersistentState::NOACTION);
	Ref<PlayerSkillEntry> tapping = PlayerSkillEntry::create(30002, 449, 0, Persistable_PersistentState::NOACTION);
	Ref<PlayerSkillEntry> crafting = PlayerSkillEntry::create(40001, 199, 0, Persistable_PersistentState::NOACTION);
	f.player->setSkillList(PlayerSkillList::create({learned, human, tapping, crafting}));
	Ptr<PlayerSkillList> skills = f.player->getSkillList();

	EXPECT_THROW(skills->addSkillXp(*f.player, 1, 10, 1), runtime::NullPointerException) << "a skill the list does not hold";

	// level 1: (int) (0.23 * 18.2 * 18.2) = (int) 76.1852 = 76
	EXPECT_TRUE(skills->addSkillXp(*f.player, 2, 50, 1));
	EXPECT_EQ(learned->getCurrentXp(), 50);
	EXPECT_EQ(learned->getSkillLevel(), 1);
	EXPECT_TRUE(skills->addSkillXp(*f.player, 2, 25, 1));
	EXPECT_EQ(learned->getCurrentXp(), 75) << "75 < 76";
	EXPECT_TRUE(skills->addSkillXp(*f.player, 2, 1, 1));
	EXPECT_EQ(learned->getSkillLevel(), 2) << "76 >= 76: the next level";
	EXPECT_EQ(learned->getCurrentXp(), 0);

	learned->setSkillLvl(45);
	EXPECT_FALSE(skills->addSkillXp(*f.player, 2, 10, 4)) << "45 - 4 = 41 > 40";
	EXPECT_EQ(learned->getCurrentXp(), 0);
	EXPECT_TRUE(skills->addSkillXp(*f.player, 2, 10, 5)) << "45 - 5 = 40 is not above 40";
	EXPECT_EQ(learned->getCurrentXp(), 10);

	EXPECT_FALSE(skills->addSkillXp(*f.player, 30001, 10, 49)) << "human gathering is capped at 49";
	EXPECT_EQ(human->getCurrentXp(), 0);
	human->setSkillLvl(99);
	EXPECT_FALSE(skills->addSkillXp(*f.player, 30001, 10, 99)) << "30001 falls through into the mastery levels";
	EXPECT_TRUE(skills->addSkillXp(*f.player, 30002, 10, 449)) << "a tapping skill at 449 breaks out of the switch";
	EXPECT_EQ(tapping->getCurrentXp(), 10);
	tapping->setSkillLvl(499);
	EXPECT_FALSE(skills->addSkillXp(*f.player, 30002, 10, 499)) << "499 with the cap enabled: the mastery level";
	configs::main::CraftConfig::DISABLE_AETHER_AND_ESSENCE_TAPPING_CAP.store(true);
	EXPECT_TRUE(skills->addSkillXp(*f.player, 30002, 10, 499)) << "with the cap disabled 499 breaks out";
	configs::main::CraftConfig::DISABLE_AETHER_AND_ESSENCE_TAPPING_CAP.store(false);
	EXPECT_EQ(tapping->getCurrentXp(), 20);
	EXPECT_FALSE(skills->addSkillXp(*f.player, 40001, 10, 199)) << "a crafting skill at the mastery level 199";
	crafting->setSkillLvl(150);
	EXPECT_TRUE(skills->addSkillXp(*f.player, 40001, 10, 150));
	EXPECT_EQ(crafting->getCurrentXp(), 10);
}

/**
 * M5b-2 closed the M5a AION_PARTIAL of addSavedEffect: a row Java restores is restored (the whole restore is asserted by
 * EffectControllerTest.ASavedEffectIsRestoredWithItsRemainingTime). What is left for this fixture, whose skills carry no <effects>: a row without
 * remaining time is still dropped before anything is created, and a restorable row now runs Java's restore - put, then addAllEffectToSucess,
 * which reads the template's <effects> and throws Java's NullPointerException for a skill without them (PlayerEffectController.java:110-115).
 */
TEST_F(PlayerSkillLearnTest, SavedEffectsWithoutRemainingTimeAreDroppedAndTheOthersAreRestored) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(4103);
	Ptr<controllers::effect::PlayerEffectController> effects = f.player->getEffectController();
	ASSERT_TRUE(effects);
	runtime::resetUnportedHitsForTests();
	uint64_t partialsBefore = runtime::partialHitCount();

	// PlayerEffectsDAO passes one row per stored effect. Java drops a row without remaining time before creating the Effect, so does the port
	EXPECT_NO_THROW(effects->addSavedEffect(1, 1, 0, 0, nullptr, nullptr));
	EXPECT_NO_THROW(effects->addSavedEffect(1, 1, -5, 0, nullptr, nullptr));
	EXPECT_TRUE(effects->isEmpty()) << "an expired row is dropped, as in Java";

	// a row Java would restore reaches the restore: no partial any more, and the missing <effects> is Java's NullPointerException
	EXPECT_THROW(effects->addSavedEffect(1, 1, 60000, 0, nullptr, nullptr), runtime::NullPointerException);
	EXPECT_EQ(runtime::partialHitCount(), partialsBefore) << "the M5a partial is closed";
	EXPECT_FALSE(effects->isEmpty()) << "Java puts the effect before addAllEffectToSucess throws";
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "enter-world never reaches an unported body because of a stored effect";
	effects->clearEffectMapsWithoutNotify(); // the unstarted effect holds the player (LogoutBreakers D3 cuts it in the server)
}

} // namespace
} // namespace aion::gameserver::model::skill::test
