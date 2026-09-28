// P5-02 Skill construction (wave 5a stage 2, item X-01a): the three Skill constructors of skillengine/model/Skill.java:105-124 and
// initializeSkillMethod (Skill.java:126-135). Expectations follow the Java sources.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
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
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::skillengine::model::test {
namespace {

using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;
using runtime::Ref;

inline std::vector<Ref<gameserver::model::gameobjects::player::PetCommonData>> noPets(Player&) {
	return {};
}

class SkillConstructionTest : public testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 29));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(&noPets);
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, R"(<skill_data>)"
			R"(<skill_template skill_id="1" name="active" nameId="1" skilltype="PHYSICAL" skillsubtype="ATTACK" activation="ACTIVE" duration="1500" stack="A1"/>)"
			R"(<skill_template skill_id="2" name="passive" nameId="1" skilltype="PHYSICAL" skillsubtype="BUFF" activation="PASSIVE" duration="0" stack="A2"/>)"
			R"(<skill_template skill_id="3" name="provoked" nameId="1" skilltype="PHYSICAL" skillsubtype="ATTACK" activation="PROVOKED" duration="0" stack="A3"/>)"
			R"(</skill_data>)"));
	}

	void TearDown() override {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	struct PlayerFixture {
		Ref<gameserver::model::account::Account> account;
		Ref<gameserver::model::gameobjects::player::PlayerCommonData> commonData;
		Ref<Player> player;
	};

	static PlayerFixture makePlayer(int32_t objectId) {
		namespace m = gameserver::model;
		PlayerFixture f;
		f.account = m::account::Account::create(8000 + objectId);
		f.commonData = m::gameobjects::player::PlayerCommonData::create(objectId);
		f.commonData->setName("Caster" + std::to_string(objectId));
		f.commonData->setRace(m::Race::ELYOS);
		f.commonData->setPlayerClass(m::PlayerClass::WARRIOR);
		Ref<m::gameobjects::player::PlayerAppearance> appearance = m::gameobjects::player::PlayerAppearance::create();
		f.account->addPlayerAccountData(std::make_unique<m::account::PlayerAccountData>(*f.account, *f.commonData, *appearance));
		f.account->setAccountWarehouse(
			std::make_unique<m::items::storage::PlayerStorage>(*f.account, m::items::storage::StorageType::ACCOUNT_WAREHOUSE));
		f.player = m::gameobjects::VisibleObject::create<m::gameobjects::player::Player>(*f.account->getPlayerAccountData(objectId), *f.account);
		f.player->setPosition(world::WorldPosition::create(210010000, 1212.94f, 1044.85f, 140.76f, int8_t{0}));
		f.player->setKnownlist(std::make_unique<world::knownlist::KnownList>(*f.player));
		f.player->setSkillList(m::skill::PlayerSkillList::create());
		return f;
	}

	runtime::ManualClock clock{0};
	xml::LoadContext skillContext;
};

TEST_F(SkillConstructionTest, ThePlayerConstructorTakesTheSkillLevelFromTheSkillList) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(6101);
	Ref<gameserver::model::skill::PlayerSkillEntry> known = gameserver::model::skill::PlayerSkillEntry::create(1, 7, 0,
		gameserver::model::gameobjects::Persistable_PersistentState::NOACTION);
	f.player->setSkillList(gameserver::model::skill::PlayerSkillList::create(std::vector<Ptr<gameserver::model::skill::PlayerSkillEntry>>{
		Ptr<gameserver::model::skill::PlayerSkillEntry>(known)}));
	const SkillTemplate* active = dataholders::DataManager::SKILL_DATA->getSkillTemplate(1);
	ASSERT_NE(active, nullptr);

	// Java: this(skillTemplate, effector, effector.getSkillList().getSkillLevel(skillTemplate.getSkillId()), firstTarget, null)
	Ref<Skill> skill = Skill::create(active, *f.player, nullptr);
	EXPECT_EQ(skill->getSkillLevel(), 7);
	EXPECT_EQ(skill->getSkillTemplate(), active);
	EXPECT_EQ(skill->getEffector().rawPointer(), static_cast<gameserver::model::gameobjects::Creature*>(f.player.get()));
	EXPECT_FALSE(skill->getFirstTarget());
	EXPECT_EQ(skill->getSkillMethod(), Skill::SkillMethod::CAST);
	EXPECT_EQ(skill->getSkillId(), 1);
	EXPECT_FALSE(skill->isPassive());

	// Java: skills.get(skillId).getSkillLevel() on a skill the player does not know throws
	const SkillTemplate* provoked = dataholders::DataManager::SKILL_DATA->getSkillTemplate(3);
	EXPECT_THROW(static_cast<void>(Skill::create(provoked, *f.player, nullptr)), runtime::NullPointerException);
	Ref<Skill> known3 = Skill::create(provoked, *f.player, nullptr, 1);
	EXPECT_EQ(known3->getSkillMethod(), Skill::SkillMethod::PROVOKED);
}

TEST_F(SkillConstructionTest, InitializeSkillMethodFollowsTheTemplate) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture f = makePlayer(6102);
	const SkillTemplate* active = dataholders::DataManager::SKILL_DATA->getSkillTemplate(1);
	const SkillTemplate* passive = dataholders::DataManager::SKILL_DATA->getSkillTemplate(2);

	// Java: the explicit level constructor
	Ref<Skill> cast = Skill::create(active, *f.player, nullptr, 3);
	EXPECT_EQ(cast->getSkillLevel(), 3);
	EXPECT_EQ(cast->getSkillMethod(), Skill::SkillMethod::CAST);

	Ref<Skill> passiveSkill = Skill::create(passive, *f.player, nullptr, 1);
	EXPECT_EQ(passiveSkill->getSkillMethod(), Skill::SkillMethod::PASSIVE);
	EXPECT_TRUE(passiveSkill->isPassive());

	// the Creature constructor without an item template ends in CAST as well
	Ref<Skill> creatureCast = Skill::create(active, static_cast<gameserver::model::gameobjects::Creature&>(*f.player), 2, nullptr, nullptr);
	EXPECT_EQ(creatureCast->getSkillLevel(), 2);
	EXPECT_EQ(creatureCast->getSkillMethod(), Skill::SkillMethod::CAST);
}

} // namespace
} // namespace aion::gameserver::skillengine::model::test
