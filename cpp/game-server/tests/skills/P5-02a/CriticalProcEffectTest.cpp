// P5-02, M5b-1 item E-05 (m5b-plan.md D14): SkillEngine::createCriticalProcEffect, the body CreatureController::attackTarget reaches on every
// critical hit of a Player (CreatureController.cpp:375, Java CreatureController.java:341-345).
//
// Expectations follow SkillEngine.java:193-225. The ported body evaluates the main-hand weapon-group switch of :206-213 first and answers null
// for every group outside {POLEARM, STAFF, GREATSWORD, BOW}, which is exactly Java's answer; for those four it marks an AION_PARTIAL and answers
// null, because building the Effect needs the effect engine (M5b-2, work item O-01). The isUnderNormalShield guard of :194-195 is skipped
// (docs/deviations/P5-02.md).
//
// What these cases prove and what they cannot: they pin the membership of the switch - which groups take the proc arm and which do not - and
// that the gate's own weapon (SWORD, item 100000094 "Training Sword") answers null without marking the partial. They cannot tell 8218 (stumble)
// from 8217 (stun): both arms funnel into the same partial site and both answer null, so the two skill ids first become observable when M5b-2
// creates the Effect. Nothing else in the body is observable, because the target and skillId parameters are unused by the ported arms.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

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

using gameserver::model::gameobjects::player::Player;
using gameserver::model::templates::item::enums::ItemGroup;
using runtime::Ptr;
using runtime::Ref;

/** The reason the AION_PARTIAL of the id != 0 arm carries (SkillEngine.cpp) */
constexpr std::string_view PROC_PARTIAL = "critical proc stumble/stun needs the effect engine (M5b-2)";

/** The hit count of one AION_PARTIAL site, found by the reason its macro carries (runtime/base/Unported.h) */
uint64_t partialHitsFor(std::string_view reason) {
	for (const runtime::PartialHit& hit : runtime::partialHits()) {
		if (hit.reason == reason)
			return hit.hits;
	}
	return 0;
}

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
	}

	void TearDown() override {
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

	/** Java PlayerService.getPlayer: account, common data, appearance, account data; then the Player */
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
	std::vector<Ref<gameserver::model::gameobjects::Item>> equipped;
	std::vector<Ref<gameserver::model::skill::PlayerSkillEntry>> learnedSkills;
};

TEST_F(CriticalProcEffectTest, TheGatesSwordAnswersNullWithoutMarkingThePartial) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture attacker = makePlayer(7101);
	PlayerFixture target = makePlayer(7102);
	uint64_t before = partialHitsFor(PROC_PARTIAL);

	// no weapon at all: Java's getMainHandWeaponType() is null and the switch is skipped (SkillEngine.java:208)
	ASSERT_FALSE(attacker.player->getEquipment().getMainHandWeaponType().has_value());
	EXPECT_FALSE(SkillEngine::getInstance().createCriticalProcEffect(*attacker.player, *target.player, 0));

	// the M5b gate's Elyos Warrior starts with item 100000094 "Training Sword", item_group="SWORD" (player_initial_data.xml:8)
	equipMainHand(*attacker.player, ItemGroup::SWORD, 100000094, 8101);
	ASSERT_EQ(attacker.player->getEquipment().getMainHandWeaponType(), ItemGroup::SWORD);
	EXPECT_FALSE(SkillEngine::getInstance().createCriticalProcEffect(*attacker.player, *target.player, 0)) << "Java: id stays 0, so null";

	EXPECT_EQ(partialHitsFor(PROC_PARTIAL), before)
		<< "m5b-plan.md §6.1 §B: the gate asserts this site's hit count is 0, so a SWORD must not reach the partial arm";
}

TEST_F(CriticalProcEffectTest, PolearmStaffAndGreatswordTakeTheStumbleArm) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture target = makePlayer(7201);

	// SkillEngine.java:210: case POLEARM, STAFF, GREATSWORD -> id = 8218 (stumble)
	int32_t objectId = 7202;
	int32_t itemId = 100000200;
	int32_t itemObjId = 8201;
	for (ItemGroup group : {ItemGroup::POLEARM, ItemGroup::STAFF, ItemGroup::GREATSWORD}) {
		const std::string_view name = xml::EnumTraits<ItemGroup>::names[static_cast<size_t>(group)];
		PlayerFixture attacker = makePlayer(objectId++);
		ASSERT_NO_FATAL_FAILURE(equipMainHand(*attacker.player, group, itemId++, itemObjId++)) << name;
		ASSERT_EQ(attacker.player->getEquipment().getMainHandWeaponType(), group) << name;

		uint64_t before = partialHitsFor(PROC_PARTIAL);
		EXPECT_FALSE(SkillEngine::getInstance().createCriticalProcEffect(*attacker.player, *target.player, 0))
			<< name << ": the Effect needs the effect engine (M5b-2), so the partial answers null";
		EXPECT_EQ(partialHitsFor(PROC_PARTIAL), before + 1) << name << ": the proc arm is taken and marked";
	}
}

TEST_F(CriticalProcEffectTest, BowTakesTheStunArm) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture attacker = makePlayer(7301);
	PlayerFixture target = makePlayer(7302);
	ASSERT_NO_FATAL_FAILURE(equipMainHand(*attacker.player, ItemGroup::BOW, 100000300, 8301));
	ASSERT_EQ(attacker.player->getEquipment().getMainHandWeaponType(), ItemGroup::BOW);

	// SkillEngine.java:211: case BOW -> id = 8217 (stun)
	uint64_t before = partialHitsFor(PROC_PARTIAL);
	EXPECT_FALSE(SkillEngine::getInstance().createCriticalProcEffect(*attacker.player, *target.player, 0));
	EXPECT_EQ(partialHitsFor(PROC_PARTIAL), before + 1);

	// the partial counts every call, so a gate run's hit count is the number of procs, not the number of sites
	EXPECT_FALSE(SkillEngine::getInstance().createCriticalProcEffect(*attacker.player, *target.player, 0));
	EXPECT_EQ(partialHitsFor(PROC_PARTIAL), before + 2);
}

TEST_F(CriticalProcEffectTest, EveryOtherWeaponGroupAnswersNullWithoutMarkingThePartial) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	PlayerFixture target = makePlayer(7401);
	uint64_t before = partialHitsFor(PROC_PARTIAL);

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
	EXPECT_EQ(partialHitsFor(PROC_PARTIAL), before) << "none of these groups is in the switch";
}

} // namespace
} // namespace aion::gameserver::skillengine::test
