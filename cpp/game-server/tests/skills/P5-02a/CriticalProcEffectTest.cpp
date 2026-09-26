// P5-02a, M5b-2 stage 1 part 2 (m5b2-plan.md S-01, D7/D14 closed): SkillEngine::createCriticalProcEffect, the body
// CreatureController::attackTarget reaches on every critical hit of a Player (CreatureController.cpp:375, Java CreatureController.java:341-345).
//
// Expectations follow SkillEngine.java:193-225, which the port now follows in Java's order: (1) the isUnderNormalShield guard, (2) the
// `skillId != 0` filter over the skill template's type and effect types, (3) the main-hand weapon-group switch, (4) `new Effect(attacker, target,
// template, template.getLvl(), null, null, true, null).initialize()` for 8218 (stumble) or 8217 (Java's comment says stun; the data's only
// effect of 8217 is <stagger>). M5b-1 had reordered (1)-(2) away and left (4) an AION_PARTIAL (m5b-plan.md D14); both are closed.
//
// The two proc templates here carry no <effects>, so Effect.initialize returns at its first line (the stumble and stagger behaviour is the effect
// lanes' work) and what these cases observe is the Effect the engine builds: its skill id, level, effector, effected and sub-effect flag. The
// shield guard needs an effect in a controller whose owner is in a map instance (EffectController::addEffect broadcasts), so its case is
// SkillRulesTest.ATargetUnderANormalShieldIsNeverStumbled.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/detail/ItemSlotMasks.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroupInfo.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::skillengine::test {
namespace {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::templates::item::enums::ItemGroup;
using runtime::Ptr;
using runtime::Ref;

/** skillId filter templates (SkillEngine.java:196-204) */
constexpr int32_t MAGICAL_ATTACK = 60101;  // MAGICAL: "magical skills do not stun"
constexpr int32_t STUNNING_ATTACK = 60102; // PHYSICAL, but its own effects include a STUN
constexpr int32_t NO_ATTACK = 60103;       // PHYSICAL without SKILLATTACKINSTANT / SKILLATKDRAININSTANT
constexpr int32_t PLAIN_ATTACK = 60104;    // PHYSICAL with a plain <skillatk>: passes the filter

inline std::vector<Ref<gameserver::model::gameobjects::player::PetCommonData>> noPets(Player&) {
	return {};
}

class CriticalProcEffectTest : public testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 11));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(&noPets);
		// the two proc skills without their <effects> (skill_templates.xml: 8218 <stumble>, 8217 <stagger>), and the filter templates
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext,
			R"(<skill_data>)"
			R"(<skill_template skill_id="8218" name="stumble" nameId="1" stack="PROC_STUMBLE" lvl="2" skilltype="PHYSICAL" skillsubtype="DEBUFF" activation="PROVOKED" duration="0"/>)"
			R"(<skill_template skill_id="8217" name="stun" nameId="1" stack="PROC_STUN" lvl="3" skilltype="PHYSICAL" skillsubtype="DEBUFF" activation="PROVOKED" duration="0"/>)"
			R"(<skill_template skill_id="60101" name="magical" nameId="1" stack="F1" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK" activation="ACTIVE" duration="0">)"
			R"(<effects><skillatk value="27" e="1"/></effects></skill_template>)"
			R"(<skill_template skill_id="60102" name="stunning" nameId="1" stack="F2" lvl="1" skilltype="PHYSICAL" skillsubtype="ATTACK" activation="ACTIVE" duration="0">)"
			R"(<effects><skillatk value="27" e="1"/><stun duration2="3000" e="2"/></effects></skill_template>)"
			R"(<skill_template skill_id="60103" name="noattack" nameId="1" stack="F3" lvl="1" skilltype="PHYSICAL" skillsubtype="ATTACK" activation="ACTIVE" duration="0"/>)"
			R"(<skill_template skill_id="60104" name="plain" nameId="1" stack="F4" lvl="1" skilltype="PHYSICAL" skillsubtype="ATTACK" activation="ACTIVE" duration="0">)"
			R"(<effects><skillatk value="27" e="1"/></effects></skill_template>)"
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

	/**
	 * Java PlayerService.getPlayer: account, common data, appearance, account data; then the Player, with the effect controller
	 * PlayerService.cpp:204 gives it (the guard of SkillEngine.java:194 reads the target's)
	 */
	static PlayerFixture makePlayer(int32_t objectId) {
		namespace m = gameserver::model;
		PlayerFixture f;
		f.account = m::account::Account::create(9000 + objectId);
		f.commonData = m::gameobjects::player::PlayerCommonData::create(objectId);
		f.commonData->setName("Proc" + std::to_string(objectId));
		f.commonData->setRace(m::Race::ELYOS);
		f.commonData->setPlayerClass(m::PlayerClass::WARRIOR);
		Ref<m::gameobjects::player::PlayerAppearance> appearance = m::gameobjects::player::PlayerAppearance::create();
		f.account->addPlayerAccountData(std::make_unique<m::account::PlayerAccountData>(*f.account, *f.commonData, *appearance));
		f.account->setAccountWarehouse(
			std::make_unique<m::items::storage::PlayerStorage>(*f.account, m::items::storage::StorageType::ACCOUNT_WAREHOUSE));
		f.player = m::gameobjects::VisibleObject::create<Player>(*f.account->getPlayerAccountData(objectId), *f.account);
		f.player->setPosition(world::WorldPosition::create(210010000, 1212.94f, 1044.85f, 140.76f, int8_t{0}));
		f.player->setKnownlist(std::make_unique<world::knownlist::KnownList>(*f.player));
		f.player->setSkillList(m::skill::PlayerSkillList::create());
		f.player->setEffectController(std::make_unique<controllers::effect::PlayerEffectController>(*f.player));
		return f;
	}

	/**
	 * Puts a weapon of the given group into the player's main hand, the way Java's PlayerService loads equipped rows: Equipment.onLoadHandler
	 * (Equipment.cpp:530-546). It first teaches the player the weapon skills the group requires (ItemGroupInfo.h ITEM_GROUP_DATA, Java
	 * ItemGroup's `requiredSkill` arrays), because otherwise checkAvailableEquipSkills sends the item back to the inventory
	 * (Equipment.cpp:395-406). MAIN_HAND is not a left-hand slot, so the dual-wield restriction does not apply.
	 */
	void equipMainHand(Player& player, ItemGroup itemGroup, int32_t itemId, int32_t itemObjId) {
		namespace m = gameserver::model;
		std::vector<Ptr<m::skill::PlayerSkillEntry>> weaponSkills;
		for (int32_t skillId : m::templates::item::enums::getRequiredSkills(itemGroup)) {
			Ref<m::skill::PlayerSkillEntry> entry =
				m::skill::PlayerSkillEntry::create(skillId, 1, 0, m::gameobjects::Persistable_PersistentState::NOACTION);
			weaponSkills.push_back(Ptr<m::skill::PlayerSkillEntry>(entry));
			learnedSkills.push_back(std::move(entry));
		}
		player.setSkillList(m::skill::PlayerSkillList::create(weaponSkills));

		const std::string xmlText = R"(<item_template id=")" + std::to_string(itemId) + R"(" item_group=")"
			+ std::string(xml::EnumTraits<ItemGroup>::names[static_cast<size_t>(itemGroup)]) + R"("/>)";
		const m::templates::item::ItemTemplate* itemTemplate =
			xml::bindString<m::templates::item::ItemTemplate>(itemContext, xmlText).release();
		Ref<m::gameobjects::Item> weapon = m::gameobjects::Item::create(itemObjId, itemTemplate);
		weapon->setEquipmentSlot(m::gameobjects::player::detail::MAIN_HAND);
		player.getEquipment().onLoadHandler(*weapon);
		ASSERT_TRUE(player.getEquipment().getMainHandWeaponType().has_value())
			<< "onLoadHandler put the weapon back into the inventory instead of the main hand";
		equipped.push_back(std::move(weapon));
	}

	runtime::ManualClock clock{0};
	xml::LoadContext itemContext;
	xml::LoadContext skillContext;
	std::vector<Ref<gameserver::model::gameobjects::Item>> equipped;
	std::vector<Ref<gameserver::model::skill::PlayerSkillEntry>> learnedSkills;
};

TEST_F(CriticalProcEffectTest, TheGatesSwordAndAnEmptyHandAnswerNull) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture attacker = makePlayer(7101);
	PlayerFixture target = makePlayer(7102);

	// no weapon at all: Java's getMainHandWeaponType() is null and the switch is skipped (SkillEngine.java:208)
	ASSERT_FALSE(attacker.player->getEquipment().getMainHandWeaponType().has_value());
	EXPECT_FALSE(SkillEngine::getInstance().createCriticalProcEffect(*attacker.player, *target.player, 0));

	// the M5b gate's Elyos Warrior starts with item 100000094 "Training Sword", item_group="SWORD" (player_initial_data.xml:8)
	equipMainHand(*attacker.player, ItemGroup::SWORD, 100000094, 8101);
	ASSERT_EQ(attacker.player->getEquipment().getMainHandWeaponType(), ItemGroup::SWORD);
	EXPECT_FALSE(SkillEngine::getInstance().createCriticalProcEffect(*attacker.player, *target.player, 0)) << "Java: id stays 0, so null";
	EXPECT_EQ(runtime::partialHitCount(), 0u) << "the D14 partial is gone";
}

TEST_F(CriticalProcEffectTest, PolearmStaffAndGreatswordCreateTheStumble) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture target = makePlayer(7201);

	// SkillEngine.java:210: case POLEARM, STAFF, GREATSWORD -> id = 8218 (stumble); :218-222: new Effect(attacker, target, template,
	// template.getLvl(), null, null, true, null), initialize()
	int32_t objectId = 7202;
	int32_t itemId = 100000200;
	int32_t itemObjId = 8201;
	for (ItemGroup group : {ItemGroup::POLEARM, ItemGroup::STAFF, ItemGroup::GREATSWORD}) {
		const std::string_view name = xml::EnumTraits<ItemGroup>::names[static_cast<size_t>(group)];
		PlayerFixture attacker = makePlayer(objectId++);
		ASSERT_NO_FATAL_FAILURE(equipMainHand(*attacker.player, group, itemId++, itemObjId++)) << name;
		ASSERT_EQ(attacker.player->getEquipment().getMainHandWeaponType(), group) << name;

		Ref<model::Effect> effect = SkillEngine::getInstance().createCriticalProcEffect(*attacker.player, *target.player, 0);
		ASSERT_TRUE(effect) << name;
		EXPECT_EQ(effect->getSkillId(), 8218) << name << ": stumble";
		EXPECT_EQ(effect->getSkillLevel(), 2) << name << ": the template's lvl";
		EXPECT_EQ(effect->getEffector(), Ptr<Creature>(attacker.player)) << name;
		EXPECT_EQ(effect->getEffected(), Ptr<Creature>(target.player)) << name;
		EXPECT_TRUE(effect->isSubEffect()) << name << ": the constructor's `true`";
		EXPECT_EQ(effect->getForceType(), nullptr) << name << ": force type null, not DEFAULT";
	}
}

TEST_F(CriticalProcEffectTest, BowCreatesTheStagger) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture attacker = makePlayer(7301);
	PlayerFixture target = makePlayer(7302);
	ASSERT_NO_FATAL_FAILURE(equipMainHand(*attacker.player, ItemGroup::BOW, 100000300, 8301));

	// SkillEngine.java:211: case BOW -> id = 8217 ("stun" in Java's comment, but 8217 "Stunned" has a single <stagger> effect in
	// skill_templates.xml: the proc is a StaggerEffect, docs/deviations/P5-02a.md)
	Ref<model::Effect> effect = SkillEngine::getInstance().createCriticalProcEffect(*attacker.player, *target.player, 0);
	ASSERT_TRUE(effect);
	EXPECT_EQ(effect->getSkillId(), 8217);
	EXPECT_EQ(effect->getSkillLevel(), 3);
}

TEST_F(CriticalProcEffectTest, EveryOtherWeaponGroupAnswersNull) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture target = makePlayer(7401);

	// Java's arrow switch has no default arm, so every group it does not name leaves id at 0 and the method returns null
	// (SkillEngine.java:209-216). EXTRACT_SWORD sits next to GREATSWORD in the enum and HARP next to BOW, so both are worth naming.
	int32_t objectId = 7402;
	int32_t itemId = 100000400;
	int32_t itemObjId = 8401;
	for (ItemGroup group : {ItemGroup::NONE, ItemGroup::NOWEAPON, ItemGroup::SWORD, ItemGroup::EXTRACT_SWORD, ItemGroup::DAGGER, ItemGroup::MACE,
			 ItemGroup::ORB, ItemGroup::SPELLBOOK, ItemGroup::HARP, ItemGroup::GUN, ItemGroup::CANNON, ItemGroup::KEYBLADE, ItemGroup::SHIELD}) {
		const std::string_view name = xml::EnumTraits<ItemGroup>::names[static_cast<size_t>(group)];
		PlayerFixture attacker = makePlayer(objectId++);
		ASSERT_NO_FATAL_FAILURE(equipMainHand(*attacker.player, group, itemId++, itemObjId++)) << name;
		ASSERT_EQ(attacker.player->getEquipment().getMainHandWeaponType(), group) << name;
		EXPECT_FALSE(SkillEngine::getInstance().createCriticalProcEffect(*attacker.player, *target.player, 0)) << name;
	}
}

TEST_F(CriticalProcEffectTest, ASkillIdFiltersMagicalStunningAndNonAttackSkills) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture attacker = makePlayer(7501);
	PlayerFixture target = makePlayer(7502);
	ASSERT_NO_FATAL_FAILURE(equipMainHand(*attacker.player, ItemGroup::POLEARM, 100000500, 8501));
	SkillEngine& engine = SkillEngine::getInstance();

	// SkillEngine.java:196-204, evaluated before the weapon switch: each of these answers null although the polearm would stumble
	EXPECT_FALSE(engine.createCriticalProcEffect(*attacker.player, *target.player, MAGICAL_ATTACK)) << "magical skills do not stun";
	EXPECT_FALSE(engine.createCriticalProcEffect(*attacker.player, *target.player, STUNNING_ATTACK)) << "the skill stuns on its own";
	EXPECT_FALSE(engine.createCriticalProcEffect(*attacker.player, *target.player, NO_ATTACK)) << "no SKILLATTACKINSTANT effect";

	Ref<model::Effect> effect = engine.createCriticalProcEffect(*attacker.player, *target.player, PLAIN_ATTACK);
	ASSERT_TRUE(effect) << "a plain physical attack skill passes the filter and the polearm stumbles";
	EXPECT_EQ(effect->getSkillId(), 8218);

	// Java dereferences the template of an unknown skill id unchecked (SkillEngine.java:197-198)
	EXPECT_THROW(engine.createCriticalProcEffect(*attacker.player, *target.player, 99999), runtime::NullPointerException);
}

} // namespace
} // namespace aion::gameserver::skillengine::test
