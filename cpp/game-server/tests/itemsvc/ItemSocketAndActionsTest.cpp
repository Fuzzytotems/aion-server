// P5-07 ItemSocketService.socketGodstone with its ItemUseObserver and 2 s task (m5b3-plan.md T-05), SkillUseAction.canAct/act and the
// ItemActions lookups (T-04), StigmaService.notifyEquipAction (T-06), against ItemSocketService.java:153-207, SkillUseAction.java:51-106,
// ItemActions.java:150-262 and StigmaService.java:42-86. The task runs on the fixture's DeterministicExecutor.

#include "ItemServicesTestSupport.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/configs/main/PricesConfig.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/dataholders/MotionData.bind.h"
#include "aion/gameserver/dataholders/MotionData.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/items/GodStone.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/AbstractItemAction.h"
#include "aion/gameserver/model/templates/item/actions/AdoptPetAction.h"
#include "aion/gameserver/model/templates/item/actions/DecorateAction.h"
#include "aion/gameserver/model/templates/item/actions/DyeAction.h"
#include "aion/gameserver/model/templates/item/actions/EnchantItemAction.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/QuestStartAction.h"
#include "aion/gameserver/model/templates/item/actions/ReadAction.h"
#include "aion/gameserver/model/templates/item/actions/RemodelAction.h"
#include "aion/gameserver/model/templates/item/actions/RideAction.h"
#include "aion/gameserver/model/templates/item/actions/SkillUseAction.h"
#include "aion/gameserver/model/templates/item/actions/SummonHouseObjectAction.h"
#include "aion/gameserver/model/templates/item/actions/TuningAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/StigmaService.h"
#include "aion/gameserver/services/item/ItemMoveService.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemSocketService.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test {
namespace {

using namespace std::chrono_literals;
using model::templates::item::actions::ItemActions;
using model::templates::item::actions::SkillUseAction;
using network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using ItemUpdateType = ItemPacketService::ItemUpdateType;

/** the fixture player's object id (ItemServicesTest::SetUp) */
constexpr int32_t PLAYER_OBJECT_ID = 700101;

/** ItemSlot.MAIN_HAND, STIGMA1, STIGMA2 (ItemSlot.java: 1, 1 << 30, 1 << 31) */
constexpr int64_t MAIN_HAND = 1LL;
constexpr int64_t STIGMA1 = 1LL << 30;
constexpr int64_t STIGMA2 = 1LL << 31;

class ItemSocketAndActionsTest : public ItemServicesTest {
protected:
	const ItemActions& actionsOf(int32_t itemId) {
		const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
		EXPECT_TRUE(itemTemplate && itemTemplate->getActions()) << itemId;
		return *itemTemplate->getActions();
	}

	const SkillUseAction& skillUseOf(int32_t itemId) {
		const SkillUseAction* action = actionsOf(itemId).getSkillUseAction();
		EXPECT_TRUE(action) << itemId;
		return *action;
	}
};

// ------------------------------------------------------------------------------------------------------------------------- socketGodstone

TEST_F(ItemSocketAndActionsTest, AGodstoneIsSocketedAfterTwoSecondsAndTheStoneIsUsedUp) {
	// ItemSocketService.java:177-206: the observer is attached, SM_ITEM_USAGE_ANIMATION(time 2000) is broadcast at once, and a 2,000 ms ITEM_USE
	// task removes the observer, sends the closing animation (end 1), takes one stone (decreaseByObjectId: DEC_ITEM_USE), adds the godstone,
	// STR_GIVE_ITEM_PROC_ENCHANTED_TARGET_ITEM and SM_INVENTORY_UPDATE_ITEM of the weapon (m5b3-plan.md Y6). 168000116: item_templates.xml:848006;
	// the Training Sword's mask 138366 has CAN_PROC_ENCHANT (item_templates.xml:375, ItemMask.java: 1 << 10)
	Item& stones = stored(811101, FX_TEST_EARTH_GODSTONE, 2);
	Item& sword = stored(811102, TRAINING_SWORD, 1);
	ItemSocketService::socketGodstone(player(), Ptr<Item>(sword), 811101);
	EXPECT_EQ(sent(), cp::exactly({itemUsageAnimation(PLAYER_OBJECT_ID, 811101, FX_TEST_EARTH_GODSTONE, 2000, 0, 0)}));

	clearSent();
	executor->advance(1999ms);
	EXPECT_TRUE(sent().empty()) << "the task runs 2,000 ms later";
	EXPECT_FALSE(sword.getGodStone());

	executor->advance(1ms);
	ASSERT_TRUE(sword.getGodStone());
	EXPECT_EQ(sword.getGodStone()->getItemId(), FX_TEST_EARTH_GODSTONE);
	EXPECT_EQ(sword.getGodStone()->getActivatedCount(), 0);
	EXPECT_EQ(stones.getItemCount(), 1);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 4u);
	EXPECT_EQ(packets[0], itemUsageAnimation(PLAYER_OBJECT_ID, 811101, FX_TEST_EARTH_GODSTONE, 0, 1, 0));
	EXPECT_EQ(trailingMask(packets[1]), 0x16) << "DEC_ITEM_USE";
	EXPECT_EQ(packets[1], serialized(SM_INVENTORY_UPDATE_ITEM(player(), stones, ItemUpdateType::DEC_ITEM_USE)));
	EXPECT_EQ(packets[2], serialized(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_PROC_ENCHANTED_TARGET_ITEM(sword.getL10n())));
	EXPECT_EQ(PacketReader(cp::bodyOf(packets[3])).D(), 811102);
	EXPECT_EQ(packets[3], serialized(SM_INVENTORY_UPDATE_ITEM(player(), sword)));

	// the task removed its observer: a later move cancels nothing
	clearSent();
	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty());
}

TEST_F(ItemSocketAndActionsTest, AMoveDuringTheTwoSecondsAbortsTheSocketing) {
	// ItemSocketService.java:177-187: any disturbing event of the ItemUseObserver (ItemUseObserver.java: moved -> abort) removes the observer,
	// cancels the ITEM_USE task, sends STR_MSG_GIVE_PROC_CANCEL and the aborted animation (end 3); the stone stays, the weapon gets nothing
	Item& stones = stored(811103, FX_TEST_EARTH_GODSTONE, 2);
	Item& sword = stored(811104, TRAINING_SWORD, 1);
	ItemSocketService::socketGodstone(player(), Ptr<Item>(sword), 811103);
	clearSent();
	executor->advance(1000ms);
	player().getObserveController()->notifyMoveObservers();
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_GIVE_PROC_CANCEL(sword.getL10n())),
						  itemUsageAnimation(PLAYER_OBJECT_ID, 811103, FX_TEST_EARTH_GODSTONE, 0, 3, 0)}));

	clearSent();
	executor->advance(3000ms);
	EXPECT_TRUE(sent().empty()) << "the task was cancelled";
	EXPECT_EQ(stones.getItemCount(), 2);
	EXPECT_FALSE(sword.getGodStone());
	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty()) << "the observer is gone";
}

TEST_F(ItemSocketAndActionsTest, AStoneThatLeftTheCubeDuringTheTwoSecondsIsNotSocketed) {
	// ItemSocketService.java:199-200: `if (!player.getInventory().decreaseByObjectId(stoneId, 1)) return;` - a storage move is no event of the
	// ItemUseObserver, so the stone can go to the warehouse (mask 12414 has STORABLE_IN_WH, item_templates.xml:848006) while the task waits. The
	// task still removes its observer and sends the closing animation (end 1), then finds no stone in the cube: no godstone, no message
	Item& stones = stored(811108, FX_TEST_EARTH_GODSTONE, 2);
	Item& sword = stored(811109, TRAINING_SWORD, 1);
	ItemSocketService::socketGodstone(player(), Ptr<Item>(sword), 811108);
	executor->advance(1000ms);
	model::items::storage::Storage& warehouse = storage(StorageType::REGULAR_WAREHOUSE);
	ItemMoveService::moveItem(player(), 811108, /*CUBE*/ 0, /*REGULAR_WAREHOUSE*/ 1, 0);
	ASSERT_TRUE(warehouse.getItemByObjId(811108));
	ASSERT_FALSE(player().getInventory().getItemByObjId(811108));

	clearSent();
	executor->advance(1000ms);
	EXPECT_EQ(sent(), cp::exactly({itemUsageAnimation(PLAYER_OBJECT_ID, 811108, FX_TEST_EARTH_GODSTONE, 0, 1, 0)}));
	EXPECT_FALSE(sword.getGodStone());
	EXPECT_EQ(stones.getItemCount(), 2);

	clearSent();
	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty()) << "the task removed its observer";
}

TEST_F(ItemSocketAndActionsTest, TheSocketingRefusalsSendTheirMessageAndStartNothing) {
	// ItemSocketService.java:153-175, in order: no weapon; a weapon without CAN_PROC_ENCHANT (the potion's mask 12414, item_templates.xml:830724);
	// no such stone in the cube; a "stone" without <godstone>
	Item& stones = stored(811105, FX_TEST_EARTH_GODSTONE, 2);
	Item& sword = stored(811106, TRAINING_SWORD, 1);
	Item& potions = stored(811107, MINOR_LIFE_POTION, 5);

	ItemSocketService::socketGodstone(player(), nullptr, 811105);
	ItemSocketService::socketGodstone(player(), Ptr<Item>(potions), 811105);
	ItemSocketService::socketGodstone(player(), Ptr<Item>(sword), 999999);
	ItemSocketService::socketGodstone(player(), Ptr<Item>(sword), 811107);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_PROC_NO_TARGET_ITEM()),
						  serialized(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_PROC_NOT_PROC_GIVABLE_ITEM(potions.getL10n())),
						  serialized(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_PROC_NO_PROC_GIVE_ITEM()),
						  serialized(SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_PROC_NO_PROC_GIVE_ITEM())}));

	clearSent();
	executor->advance(3000ms);
	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty()) << "no task, no observer";
	EXPECT_EQ(stones.getItemCount(), 2);
	EXPECT_EQ(potions.getItemCount(), 5);
	EXPECT_FALSE(sword.getGodStone());
}

// ------------------------------------------------------------------------------------------------------------------------- SkillUseAction

TEST_F(ItemSocketAndActionsTest, AnItemBoundToAnotherMapCannotBeUsedHere) {
	// SkillUseAction.java:53-56: `mapid != 0 && player.getWorldId() != mapid` -> STR_SKILL_CAN_NOT_USE_ITEM_IN_CURRENT_POSITION. The Verteron
	// Helper Summoning Scroll is bound to 210030000 (item_templates.xml:833680); the player stands in Poeta (210010000)
	Item& scroll = stored(811201, VERTERON_HELPER_SCROLL, 1);
	const SkillUseAction& action = skillUseOf(VERTERON_HELPER_SCROLL);
	EXPECT_EQ(action.getMapId(), 210030000);
	EXPECT_FALSE(action.canAct(player(), Ptr<Item>(scroll), nullptr));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_USE_ITEM_IN_CURRENT_POSITION())}));

	// in Verteron the map check passes; skill 10490 is not in this skill data, so getSkill returns null: false without a message (:57-59)
	clearSent();
	player().setPosition(world::WorldPosition::create(210030000, 100.0f, 100.0f, 50.0f, int8_t{0}));
	EXPECT_FALSE(action.canAct(player(), Ptr<Item>(scroll), nullptr));
	EXPECT_TRUE(sent().empty());
}

TEST_F(ItemSocketAndActionsTest, ATransformItemIsRefusedWhileTransformed) {
	// SkillUseAction.java:62-66: a TransformEffect (Sparkie Candy's shapechange, skill_templates.xml:96934, ShapeChangeEffect extends
	// TransformEffect) while player.isTransformed() -> STR_SKILL_CAN_NOT_CAST_IN_SHAPECHANGE
	Item& candy = stored(811202, SPARKIE_CANDY, 3);
	const SkillUseAction& action = skillUseOf(SPARKIE_CANDY);
	EXPECT_TRUE(action.canAct(player(), Ptr<Item>(candy), nullptr)) << "not transformed";
	EXPECT_TRUE(sent().empty());

	player().getTransformModel().apply(210119);
	ASSERT_TRUE(player().isTransformed());
	clearSent();
	EXPECT_FALSE(action.canAct(player(), Ptr<Item>(candy), nullptr));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_CAST_IN_SHAPECHANGE())}));
}

TEST_F(ItemSocketAndActionsTest, ASkillThatCannotBeCastIsRefused) {
	// SkillUseAction.java:73-74: skill.canUseSkill(CAST_START) false -> false. Shishir's Powerstone casts 9832 (skill_templates.xml:91124,
	// first_target TARGET, target_relation ENEMY) and the player has no target, so Properties.validate fails
	Item& stone = stored(811203, SHISHIRS_POWERSTONE, 1);
	ASSERT_FALSE(player().getTarget());
	EXPECT_FALSE(skillUseOf(SHISHIRS_POWERSTONE).canAct(player(), Ptr<Item>(stone), nullptr));
}

TEST_F(ItemSocketAndActionsTest, APotionAtFullHealthIsRefusedOnlyWithIgnorePotionsAtFullHealth) {
	// SkillUseAction.java:75-78 and isIneffectiveHealSkill (:82-97): with gameserver.items.ignore_potions_at_full_health (CustomConfig.java:
	// 243-244, default false) a skill of heal effects only (9889: prochealinstant + heal, skill_templates.xml:91905) whose effected are all at full
	// HP -> STR_NOTHING_HAPPEN; any other effect (the candy's shapechange) makes it effective
	Item& potions = stored(811204, MINOR_LIFE_POTION, 10);
	Item& candy = stored(811205, SPARKIE_CANDY, 3);
	const SkillUseAction& potion = skillUseOf(MINOR_LIFE_POTION);
	ASSERT_TRUE(player().getLifeStats()->isFullyRestoredHp());

	EXPECT_TRUE(potion.canAct(player(), Ptr<Item>(potions), nullptr)) << "the option is off by default";
	EXPECT_TRUE(sent().empty());

	AtomicConfigScope<bool> ignore(configs::main::CustomConfig::IGNORE_POTIONS_AT_FULL_HEALTH, true);
	EXPECT_FALSE(potion.canAct(player(), Ptr<Item>(potions), nullptr));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_NOTHING_HAPPEN())}));

	clearSent();
	EXPECT_TRUE(skillUseOf(SPARKIE_CANDY).canAct(player(), Ptr<Item>(candy), nullptr)) << "a statup is no heal effect";
	EXPECT_TRUE(sent().empty());

	player().getLifeStats()->setCurrentHp(player().getLifeStats()->getMaxHp() - 10);
	ASSERT_FALSE(player().getLifeStats()->isFullyRestoredHp());
	clearSent();
	EXPECT_TRUE(potion.canAct(player(), Ptr<Item>(potions), nullptr)) << "the player misses HP";
	EXPECT_TRUE(sent().empty());

	// the MP arm: 9894 is procmphealinstant + mpheal (skill_templates.xml:91970): refused at full MP whatever the HP, allowed with MP missing
	Item& manaPotions = stored(811207, MINOR_MANA_POTION, 10);
	const SkillUseAction& manaPotion = skillUseOf(MINOR_MANA_POTION);
	ASSERT_TRUE(player().getLifeStats()->isFullyRestoredMp());
	clearSent();
	EXPECT_FALSE(manaPotion.canAct(player(), Ptr<Item>(manaPotions), nullptr)) << "full MP, although HP is missing";
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_NOTHING_HAPPEN())}));
	player().getLifeStats()->setCurrentMp(player().getLifeStats()->getMaxMp() - 10);
	ASSERT_FALSE(player().getLifeStats()->isFullyRestoredMp());
	clearSent();
	EXPECT_TRUE(manaPotion.canAct(player(), Ptr<Item>(manaPotions), nullptr)) << "the player misses MP";
	EXPECT_TRUE(sent().empty());
}

/**
 * The fixture player placed in a Poeta map instance, so a cast can broadcast (WorldPosition.getWorldMapInstance): the map row of world_maps.xml:11
 * and the empty zone, shield and material lists a region reads, published once per process and never reset (ZoneService and the map instance
 * cache theirs; the pattern of tests/skills/P5-02a/CastTestSupport.h), and the motion holder a cast asks.
 */
class ItemSkillCastTest : public ItemSocketAndActionsTest {
protected:
	void SetUp() override {
		ItemSocketAndActionsTest::SetUp();
		static const bool published = [] {
			// Java's defaults (WorldConfig.java:15 gameserver.world.region.size = 128); cansee stays off: the unit tests load no geo data
			configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
			configs::main::GeoDataConfig::CANSEE_ENABLE.store(false);
			static std::deque<xml::LoadContext> contexts;
			dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(contexts.emplace_back(),
				R"(<world_maps><map id="210010000" cName="LF1" name="Poeta" name_id="400234" twin_count="5" beginner_twin_count="6" max_user="200" water_level="100" death_level="0" world_type="ELYSEA" world_size="3072" drop_type="ELYSEA" flags="BIND RECALL GLIDE PVP DUEL_SAME_RACE" pve_attack_ratio="150" pve_defend_ratio="50"/></world_maps>)"));
			dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(contexts.emplace_back(), "<zones/>"));
			dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(contexts.emplace_back(), "<shields/>"));
			return true;
		}();
		static_cast<void>(published);
		xml::LoadContext context;
		dataholders::DataManager::MOTION_DATA.publish(xml::bindString<dataholders::MotionData>(context, "<motion_times/>"));
		map = world::WorldMap::create(dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(210010000));
		mapInstance = world::WorldMap2DInstance::create(*map, 1, 0, 0, [](world::WorldMapInstance& instance) {
			return runtime::Ref<instance::handlers::InstanceHandler>(instance::handlers::GeneralInstanceHandler::create(instance));
		});
		player().setPosition(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(100.0f, 100.0f, 50.0f)));
		player().getPosition()->setIsSpawned(true);
	}

	void TearDown() override {
		// the player first, then the map instance its position names, then the base fixture (whose own releases are then no-ops)
		if (f.player) {
			f.player->setCasting(nullptr); // a cast in progress holds the caster (Skill.effector)
			f.player->setClientConnection(nullptr);
		}
		client.reset();
		items.clear();
		f = {};
		mapInstance = nullptr;
		map = nullptr;
		ItemSocketAndActionsTest::TearDown();
		dataholders::DataManager::MOTION_DATA.resetForTests();
	}

	runtime::Ref<world::WorldMap> map;
	runtime::Ref<world::WorldMapInstance> mapInstance;
};

TEST_F(ItemSkillCastTest, ActCastsTheItemSkillWhichUsesUpOneItemOfTheParentStack) {
	// SkillUseAction.java:99-106: skill.setItemObjectId(parentItem.getObjectId()), useSkill(); the instant item cast pays its cost with
	// inventory.decreaseByObjectId(itemObjectId, 1, DEC_ITEM_USE) (Skill.java payCastCosts) -> SM_INVENTORY_UPDATE_ITEM(0x16). Mercenary's
	// Fruit Juice casts 10034, a statup (item_templates.xml:821856, skill_templates.xml:94394)
	Item& juice = stored(811206, MERCENARYS_FRUIT_JUICE, 12);
	const SkillUseAction& action = skillUseOf(MERCENARYS_FRUIT_JUICE);
	ASSERT_TRUE(action.canAct(player(), Ptr<Item>(juice), nullptr));
	runtime::resetUnportedHitsForTests();
	action.act(player(), Ptr<Item>(juice), nullptr);
	EXPECT_EQ(juice.getItemCount(), 11);
	std::vector<std::vector<uint8_t>> updates;
	for (const std::vector<uint8_t>& packet : sent()) {
		if (javaOpcodeOf(packet) == SM_INVENTORY_UPDATE_ITEM_OPCODE)
			updates.push_back(packet);
	}
	ASSERT_EQ(updates.size(), 1u);
	EXPECT_EQ(PacketReader(cp::bodyOf(updates[0])).D(), 811206);
	EXPECT_EQ(trailingMask(updates[0]), 0x16);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

// ------------------------------------------------------------------------------------------------------------------------- ItemActions

TEST_F(ItemSocketAndActionsTest, EachTypedLookupFindsTheActionOfItsTypeAndNothingElse) {
	// ItemActions.java:150-262: getItemActions returns the bound list; each typed lookup returns the first action of its type, null otherwise.
	// One shipped item per action type (the rows of ItemServicesTestSupport.h)
	using namespace model::templates::item::actions;
	const ItemActions& enchant = actionsOf(L1_ENCHANTMENT_STONE);
	ASSERT_TRUE(enchant.getEnchantAction());
	EXPECT_EQ(enchant.getEnchantAction()->getCount(), 1);
	const ItemActions& carpet = actionsOf(CARPET);
	ASSERT_TRUE(carpet.getHouseObjectAction());
	EXPECT_EQ(carpet.getHouseObjectAction()->getTemplateId(), 3000001);
	EXPECT_TRUE(actionsOf(OCTAGONAL_BOARD_ROOF).getDecorateAction());
	EXPECT_TRUE(actionsOf(DYE_REMOVER).getDyeAction());
	const ItemActions& petCard = actionsOf(PET_CARD_SIBERIAN_TIGER);
	ASSERT_TRUE(petCard.getAdoptPetAction());
	EXPECT_EQ(petCard.getAdoptPetAction()->getPetId(), 900000);
	const ItemActions& tahabata = actionsOf(TAHABATA_SWORD);
	ASSERT_TRUE(tahabata.getRemodelAction());
	EXPECT_EQ(tahabata.getRemodelAction()->getExtractType(), 2);
	EXPECT_TRUE(actionsOf(WEAPON_REIDENTIFY_TEST_ITEM).getTuningAction());
	EXPECT_TRUE(actionsOf(CIRRUSPEED).getRideAction());
	EXPECT_TRUE(actionsOf(MINOR_LIFE_POTION).getSkillUseAction());

	// every lookup of a single-action item but its own is null
	const std::vector<int32_t> itemIds{L1_ENCHANTMENT_STONE, CARPET, OCTAGONAL_BOARD_ROOF, DYE_REMOVER, PET_CARD_SIBERIAN_TIGER, TAHABATA_SWORD,
		WEAPON_REIDENTIFY_TEST_ITEM, CIRRUSPEED, MINOR_LIFE_POTION};
	for (size_t i = 0; i < itemIds.size(); i++) {
		SCOPED_TRACE(itemIds[i]);
		const ItemActions& actions = actionsOf(itemIds[i]);
		ASSERT_EQ(actions.getItemActions().size(), 1u);
		const int found = (actions.getEnchantAction() != nullptr) + (actions.getHouseObjectAction() != nullptr) +
			(actions.getDecorateAction() != nullptr) + (actions.getDyeAction() != nullptr) + (actions.getAdoptPetAction() != nullptr) +
			(actions.getRemodelAction() != nullptr) + (actions.getTuningAction() != nullptr) + (actions.getRideAction() != nullptr) +
			(actions.getSkillUseAction() != nullptr) + (actions.getCraftLearnAction() != nullptr) + (actions.getPolishAction() != nullptr);
		EXPECT_EQ(found, 1);
	}

	// Odium Refining Method (item_templates.xml:877230): two actions in document order, neither of a looked-up type
	const ItemActions& odium = actionsOf(ODIUM_REFINING_METHOD);
	const std::vector<std::unique_ptr<AbstractItemAction>>& list = odium.getItemActions();
	ASSERT_EQ(list.size(), 2u);
	EXPECT_EQ(list[0]->javaClassName(), "QuestStartAction");
	EXPECT_EQ(list[1]->javaClassName(), "ReadAction");
	EXPECT_FALSE(odium.getSkillUseAction());
	EXPECT_FALSE(odium.getEnchantAction());

	// an item without <actions> has no ItemActions at all (ItemTemplate.getActions() null)
	EXPECT_FALSE(dataholders::DataManager::ITEM_DATA->getItemTemplate(SPARKIE_CARAPACE_FRAGMENT)->getActions());
}

// ------------------------------------------------------------------------------------------------------------------------- StigmaService

TEST_F(ItemSocketAndActionsTest, NotifyEquipActionPassesAnyItemButAStigma) {
	// StigmaService.java:42-86: only a stigma template enters the body; any other item (the sword, main hand) passes at once and costs nothing
	Item& sword = stored(811301, TRAINING_SWORD, 1);
	Item& kinah = stored(811302, KINAH, 100000);
	EXPECT_TRUE(StigmaService::notifyEquipAction(player(), sword, MAIN_HAND));
	EXPECT_EQ(kinah.getItemCount(), 100000);
	EXPECT_TRUE(sent().empty());
}

TEST_F(ItemSocketAndActionsTest, AStigmaOfAnotherNameCannotReplaceTheEquippedOne) {
	// StigmaService.java:48-56: an equipped stigma in the slot whose name (spaces removed, upper case) differs refuses the equip before anything
	// is removed or paid. Healing Light II and Flame Cage I: item_templates.xml:739006, :739009
	Ref<Item> equipped = loadedItem(811303, HEALING_LIGHT_II, 1, StorageType::CUBE, STIGMA1, true);
	items.push_back(equipped);
	player().getEquipment().onLoadHandler(*equipped);
	ASSERT_EQ(player().getEquipment().getEquippedItemsAllStigma().size(), 1u);
	Item& flameCage = stored(811304, FLAME_CAGE_I, 1);
	Item& kinah = stored(811305, KINAH, 100000);

	EXPECT_FALSE(StigmaService::notifyEquipAction(player(), flameCage, STIGMA1));
	EXPECT_EQ(kinah.getItemCount(), 100000);
	EXPECT_TRUE(sent().empty());

	// a free regular stigma slot counts the sockets first (:57-61), before any kinah is charged (:79): StigmaService.getPossibleStigmaCount
	// (:274) is still unported (M5c), so the call ends there - the only unported site reached, the kinah untouched and no packet sent. The
	// prices are Java's defaults (PricesConfig.java:13-25, config/main/prices.properties: 100 each), so a charge would not be 0
	AtomicConfigScope<int32_t> prices(configs::main::PricesConfig::DEFAULT_PRICES, 100);
	AtomicConfigScope<int32_t> modifier(configs::main::PricesConfig::DEFAULT_MODIFIER, 100);
	AtomicConfigScope<int32_t> taxes(configs::main::PricesConfig::DEFAULT_TAXES, 100);
	runtime::resetUnportedHitsForTests();
	EXPECT_THROW(StigmaService::notifyEquipAction(player(), flameCage, STIGMA2), runtime::UnportedException);
	std::vector<runtime::UnportedHit> hits;
	for (const runtime::UnportedHit& hit : runtime::unportedHits()) {
		if (hit.hits > 0)
			hits.push_back(hit);
	}
	ASSERT_EQ(hits.size(), 1u);
	EXPECT_NE(hits[0].function.find("StigmaService::getPossibleStigmaCount"), std::string::npos) << hits[0].function;
	EXPECT_EQ(kinah.getItemCount(), 100000);
	EXPECT_TRUE(sent().empty());
}

} // namespace
} // namespace aion::gameserver::services::item::test
