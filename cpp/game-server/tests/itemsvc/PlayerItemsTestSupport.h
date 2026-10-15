#pragma once

// Shared fixture of the M5c stage-1 player-items tests (m5c-plan.md P-06): RepurchaseService.repurchaseFromShop, TemporaryTradeTimeTask,
// CubeExpandService and ExpandInventoryAction, ItemActionService (identification) and TuningAction, driven against the real Player and
// AionConnection of ItemServicesTestSupport.h.
//
// Every item template, npc template, cube expander and random bonus row below is copied verbatim from the shipped data
// (game-server/data/static_data/items/item_templates.xml, items/item_random_bonuses.xml, npcs/npc_templates.xml and
// storage_expander/cube_expander.xml; the line of each row in the comment above it). The configuration values are the shipped profile's
// (config/main/custom.properties), set explicitly because the unit tests load no properties.
//
// The names live in a namespace of their own (`playeritems`): EnchantTestSupport.h defines helpers of the same names in the parent namespace,
// and both headers end up in one test executable.

#include "ItemServicesTestSupport.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/configuration/ConfigValue.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/CubeExpandData.bind.h"
#include "aion/gameserver/dataholders/CubeExpandData.h"
#include "aion/gameserver/dataholders/ItemRandomBonusData.bind.h"
#include "aion/gameserver/dataholders/ItemRandomBonusData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test::playeritems {

// the item ids of the rows below (the ids the base fixture names are reused from ItemServicesTestSupport.h)
inline constexpr int32_t MODORS_SWORD = 100001551;
inline constexpr int32_t MODORS_TUNIC = 110101616;
inline constexpr int32_t WEAPON_TUNING_SCROLL = 166200009;
inline constexpr int32_t ARMOR_TUNING_SCROLL = 166200010;
inline constexpr int32_t ENDURING_WEAPON_TUNING_SCROLL = 166200011;
inline constexpr int32_t EQUIPMENT_REIDENTIFY_TEST_ITEM = 166200002;
inline constexpr int32_t EXPAND_CUBE_TICKET_1 = 169630000;
inline constexpr int32_t EXPAND_CUBE_TICKET_2 = 169630002;
inline constexpr int32_t EXPAND_WAREHOUSE_TICKET_1 = 169640000;
inline constexpr int32_t MANASTONE_HP_20 = 167000226; // the same row and id as EnchantTestSupport.h's (a name of this namespace, same value)

// the npc ids of the rows below
inline constexpr int32_t BAEVRUNERK = 798008;  // Poeta's cube expander (cube_expander.xml: levels 1)
inline constexpr int32_t TARAERINERK = 798009; // the next row of npc_templates.xml: no cube expander
inline constexpr int32_t JARUMONERK = 279022;  // Tigraki Island's cube expander (cube_expander.xml: level 5 only)

/** item_templates.xml, verbatim rows added to the base fixture's (the line of each <item_template> in the comment above it) */
inline constexpr std::string_view PLAYER_ITEMS_TEMPLATES_XML = R"xml(
	<!-- :13282 -->
	<item_template id="100001551" name="Modor's Sword" level="65" cName="sword_n_e_idunderrune_65a" mask="136268" item_group="SWORD" quality="EPIC" price="2687303" rnd_count="3" rnd_bonus="119" option_slot_bonus="1" restrict="65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65" desc="819871" attack_type="PHYSICAL" exceed_enchant_skill="RANK3_SET1_PHYSICAL_WEAPON" can_exceed_enchant="true" max_enchant_bonus="2" max_enchant="13" m_slots="5" s_slots="1">
		<modifiers>
			<add name="PARRY" value="138" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="34" bonus="true"/>
			<add name="PHYSICAL_CRITICAL" value="50" bonus="true"/>
			<add name="PHYSICAL_ACCURACY" value="123" bonus="true"/>
			<rate name="ATTACK_SPEED" value="-17" bonus="true"/>
		</modifiers>
		<actions>
			<remodel type="0"/>
		</actions>
		<weapon_stats hit_count="2" attack_range="1500" magical_accuracy="371" parry="1124" physical_accuracy="1096" critical="50" attack_speed="1400" max_damage="258" min_damage="211"/>
		<disposition id="188950015" count="6"/>
		<idian burn_attack="34" burn_defend="14"/>
	</item_template>
	<!-- :204906 -->
	<item_template id="110101616" name="Modor's Tunic" level="65" cName="rb_torso_n_e_idunderrune_65a" mask="36940" item_group="RB_TORSO" quality="EPIC" price="1791535" rnd_count="3" rnd_bonus="161" option_slot_bonus="1" restrict="0 0 0 0 0 0 0 65 65 0 0 0 0 0 0 0 0" desc="819923" exceed_enchant_skill="RANK3_SET1_MAGICAL_TORSO" can_exceed_enchant="true" max_enchant_bonus="2" max_enchant="13" m_slots="5" s_slots="1" temp_exchange_time="10">
		<modifiers>
			<add name="EVASION" value="279"/>
			<add name="MAGICAL_RESIST" value="166"/>
			<add name="PHYSICAL_DEFENSE" value="236"/>
			<add name="MAGIC_SKILL_BOOST_RESIST" value="214"/>
			<add name="MAXMP" value="1170" bonus="true"/>
			<add name="PHYSICAL_DEFENSE" value="94" bonus="true"/>
			<add name="MAGIC_SKILL_BOOST_RESIST" value="46" bonus="true"/>
			<rate name="BOOST_HATE" value="-38" bonus="true"/>
			<add name="BOOST_MAGICAL_SKILL" value="62" bonus="true"/>
		</modifiers>
		<actions>
			<remodel type="0"/>
		</actions>
		<disposition id="188950015" count="4"/>
	</item_template>
	<!-- :839151 -->
	<item_template id="166200002" name="Equipment Reidentify Test Item" level="60" cName="test_reidentify_03" mask="12414" max_stack_count="100" quality="EPIC" price="1000" desc="806542" activate_count="1">
		<actions>
			<tuning no_reduce="false" target="EQUIPMENT"/>
		</actions>
	</item_template>
	<!-- :839186 -->
	<item_template id="166200009" name="Mythic Weapon Tuning Scroll" level="65" cName="cash_weapon_reidentify_m_65a" mask="12414" max_stack_count="100" quality="MYTHIC" price="5" desc="825736" activate_count="1">
		<actions>
			<tuning no_reduce="false" target="WEAPON"/>
		</actions>
	</item_template>
	<!-- :839191 -->
	<item_template id="166200010" name="Mythic Armor Tuning Scroll" level="65" cName="cash_armor_reidentify_m_65a" mask="12414" max_stack_count="100" quality="MYTHIC" price="5" desc="825737" activate_count="1">
		<actions>
			<tuning no_reduce="false" target="ARMOR"/>
		</actions>
	</item_template>
	<!-- :839196 -->
	<item_template id="166200011" name="Enduring Eternal Weapon Tuning Scroll" level="65" cName="cash_weapon_reidentify_e_65b" mask="12414" max_stack_count="100" quality="EPIC" price="5" desc="831748" activate_count="1">
		<actions>
			<tuning no_reduce="true" target="WEAPON"/>
		</actions>
	</item_template>
	<!-- :839282 -->
	<item_template id="167000226" name="Manastone: HP +20" level="10" cName="matter_option_c_hp_10" mask="12414" max_stack_count="10000" item_group="MANASTONE" quality="COMMON" price="10" desc="719037" activate_count="1">
		<modifiers>
			<add name="MAXHP" value="20" bonus="true"/>
		</modifiers>
		<actions>
			<enchant count="1"/>
		</actions>
	</item_template>
	<!-- :859188 -->
	<item_template id="169630000" name="[Expand Card] Expand Cube Ticket (lvl 1)" level="1" cName="cash_extend_cube_01" mask="4168" quality="COMMON" price="0" desc="725545" activate_count="1">
		<actions>
			<expandinventory level="1" storage="CUBE"/>
		</actions>
	</item_template>
	<!-- :859198 -->
	<item_template id="169630002" name="[Expand Card] Expand Cube Ticket (lvl 2)" level="1" cName="event_extend_cube_02" mask="4168" quality="COMMON" price="0" desc="752374" activate_count="1">
		<actions>
			<expandinventory level="2" storage="CUBE"/>
		</actions>
	</item_template>
	<!-- :859234 -->
	<item_template id="169640000" name="[Expand Card] Expand Warehouse Ticket (lvl 1)" level="1" cName="cash_extend_warehouse_01" mask="4168" quality="COMMON" price="0" desc="725546" activate_count="1">
		<actions>
			<expandinventory level="1" storage="WAREHOUSE"/>
		</actions>
	</item_template>
)xml";

/** item_random_bonuses.xml:4623-4682, the verbatim INVENTORY set 119 of Modor's Sword's rnd_bonus (ten modifier groups) */
inline constexpr std::string_view MODORS_SWORD_RANDOM_BONUSES_XML = R"xml(<random_bonuses>
	<random_bonus type="INVENTORY" id="119">
		<modifiers chance="7.0">
			<add name="PHYSICAL_ACCURACY" value="40" bonus="true"/>
			<add name="PHYSICAL_CRITICAL" value="30" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="20" bonus="true"/>
			<rate name="ATTACK_SPEED" value="-2" bonus="true"/>
		</modifiers>
		<modifiers chance="4.0">
			<add name="PHYSICAL_ACCURACY" value="37" bonus="true"/>
			<add name="PHYSICAL_CRITICAL" value="27" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="20" bonus="true"/>
			<rate name="ATTACK_SPEED" value="-2" bonus="true"/>
		</modifiers>
		<modifiers chance="4.0">
			<add name="PHYSICAL_ACCURACY" value="33" bonus="true"/>
			<add name="PHYSICAL_CRITICAL" value="30" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="18" bonus="true"/>
			<rate name="ATTACK_SPEED" value="-2" bonus="true"/>
		</modifiers>
		<modifiers chance="14.0">
			<add name="PHYSICAL_ACCURACY" value="22" bonus="true"/>
			<add name="PHYSICAL_CRITICAL" value="24" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="13" bonus="true"/>
		</modifiers>
		<modifiers chance="15.0">
			<add name="PHYSICAL_ACCURACY" value="26" bonus="true"/>
			<add name="PHYSICAL_CRITICAL" value="20" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="11" bonus="true"/>
			<rate name="ATTACK_SPEED" value="-2" bonus="true"/>
		</modifiers>
		<modifiers chance="14.0">
			<add name="PHYSICAL_ACCURACY" value="30" bonus="true"/>
			<add name="PHYSICAL_CRITICAL" value="14" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="9" bonus="true"/>
			<rate name="ATTACK_SPEED" value="-2" bonus="true"/>
		</modifiers>
		<modifiers chance="12.0">
			<add name="PHYSICAL_ACCURACY" value="18" bonus="true"/>
			<add name="PHYSICAL_CRITICAL" value="17" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="15" bonus="true"/>
			<rate name="ATTACK_SPEED" value="-2" bonus="true"/>
		</modifiers>
		<modifiers chance="10.0">
			<add name="PHYSICAL_ACCURACY" value="14" bonus="true"/>
			<add name="PHYSICAL_CRITICAL" value="10" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="4" bonus="true"/>
			<rate name="ATTACK_SPEED" value="-2" bonus="true"/>
		</modifiers>
		<modifiers chance="10.0">
			<add name="PHYSICAL_ACCURACY" value="10" bonus="true"/>
			<add name="PHYSICAL_CRITICAL" value="5" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="3" bonus="true"/>
		</modifiers>
		<modifiers chance="10.0">
			<add name="PHYSICAL_ACCURACY" value="2" bonus="true"/>
			<add name="PHYSICAL_CRITICAL" value="2" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="1" bonus="true"/>
			<rate name="ATTACK_SPEED" value="-2" bonus="true"/>
		</modifiers>
	</random_bonus>
</random_bonuses>)xml";

/** The chances of set 119's ten modifier groups, in order (the lines above) */
inline constexpr float MODORS_SWORD_BONUS_CHANCES[] = {7.0f, 4.0f, 4.0f, 14.0f, 15.0f, 14.0f, 12.0f, 10.0f, 10.0f, 10.0f};

/**
 * Java ItemRandomBonusData.selectRandomBonusNumber of set 119 with the draw `Rnd.nextFloat(sum of the chances)` taken by the caller: the 1-based
 * number of the first group whose running chance sum reaches the draw (ItemRandomBonusData.java), computed here from the chances of the data
 */
inline int32_t modorsSwordBonusNumber(float draw) {
	float chanceSum = 0;
	for (int32_t i = 0; i < 10; i++) {
		chanceSum += MODORS_SWORD_BONUS_CHANCES[i];
		if (chanceSum >= draw)
			return i + 1;
	}
	return 0;
}

/** Java ItemRandomBonusData.calculateSumOfChances of set 119: the float sum of its chances (100) */
inline float modorsSwordBonusChanceSum() {
	float sum = 0;
	for (float chance : MODORS_SWORD_BONUS_CHANCES)
		sum += chance;
	return sum;
}

/** npc_templates.xml, verbatim rows (the lines of each row in the comment above it) */
inline constexpr std::string_view CUBE_EXPANDER_NPC_TEMPLATES_XML = R"xml(<npc_templates>
	<!-- :341406-341412 -->
	<npc_template npc_id="279022" level="40" name="jarumonerk" name_id="314013" height="1.16875" title_id="314333" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="USEALL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2000" hpgauge="3">
		<stats maxHp="9419">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="47" can_talk_invisible="false" />
	</npc_template>
	<!-- :461611-461617 -->
	<npc_template npc_id="798008" level="9" name="baevrunerk" name_id="351141" height="1.16875" title_id="350421" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2100" hpgauge="3">
		<stats maxHp="2568">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="47" can_talk_invisible="false" />
	</npc_template>
	<!-- :461618-461624 -->
	<npc_template npc_id="798009" level="15" name="taraerinerk" name_id="351370" height="1.16875" title_id="350419" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2000" hpgauge="3">
		<stats maxHp="2256">
			<speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" />
		</stats>
		<bound_radius front="0.595" side="0.3774" upper="1.16875" />
		<talk_info distance="5" is_dialog="true" func_dialogs="33" can_talk_invisible="false" />
	</npc_template>
</npc_templates>)xml";

/** storage_expander/cube_expander.xml:1-17, verbatim (the whole file) */
inline constexpr std::string_view CUBE_EXPANDER_XML = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<cube_expander xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="storage_expander.xsd">
	<!-- Poeta / Ishalgen -->
	<expansion_npc ids="798008 798037">
		<expand level="1" price="1000" />
	</expansion_npc>
	<!-- Sanctum / Pandaemonium -->
	<expansion_npc ids="798011 798012 798058 798059">
		<expand level="1" price="1000" />
		<expand level="2" price="12000" />
		<expand level="3" price="80000" />
		<expand level="4" price="180000" />
	</expansion_npc>
	<!-- Abyss, Tigraki Island -->
	<expansion_npc ids="279022">
		<expand level="5" price="360000" />
	</expansion_npc>
</cube_expander>)xml";

/** Sets a ConfigValue for the scope and restores the previous value */
template <class T>
class PlayerItemsConfigValueScope {
public:
	PlayerItemsConfigValueScope(commons::configuration::ConfigValue<T>& configValue, T value) : config(configValue), previous(configValue.get()) {
		config.set(std::move(value));
	}
	~PlayerItemsConfigValueScope() { config.set(previous ? *previous : T{}); }
	PlayerItemsConfigValueScope(const PlayerItemsConfigValueScope&) = delete;
	PlayerItemsConfigValueScope& operator=(const PlayerItemsConfigValueScope&) = delete;

private:
	commons::configuration::ConfigValue<T>& config;
	const std::shared_ptr<const T> previous;
};

class PlayerItemsSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	PlayerItemsSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/**
 * ItemServicesTest with this lane's item rows added to the base fixture's, the random bonus set of Modor's Sword, the cube expanders and the
 * shipped cube limits (config/main/custom.properties:154, :158). Rnd's generator is restored after each case, since the cases seed it.
 */
class PlayerItemsTest : public ItemServicesTest {
protected:
	void SetUp() override {
		ItemServicesTest::SetUp();
		std::string itemRows(ITEM_TEMPLATES_XML);
		itemRows.insert(itemRows.rfind("</item_templates>"), PLAYER_ITEMS_TEMPLATES_XML);
		dataholders::DataManager::ITEM_DATA.resetForTests();
		xml::LoadContext context;
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, itemRows));
		dataholders::DataManager::ITEM_RANDOM_BONUSES.publish(
			xml::bindString<dataholders::ItemRandomBonusData>(context, MODORS_SWORD_RANDOM_BONUSES_XML));
		dataholders::DataManager::CUBEEXPANDER_DATA.publish(xml::bindString<dataholders::CubeExpandData>(context, CUBE_EXPANDER_XML));
		savedGenerator = std::make_unique<commons::utils::Rnd::Xoshiro256PlusPlus>(commons::utils::Rnd::generator());
		savedCubeExpansionLimit = configs::main::CustomConfig::CUBE_EXPANSION_LIMIT.exchange(11);
		savedNpcCubeExpandsLimit = configs::main::CustomConfig::NPC_CUBE_EXPANDS_SIZE_LIMIT.exchange(5);
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		// the npcs' ai="general" needs no handler for these services: an unregistered one gets the DummyNpcAI instead of the throw
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
	}

	void TearDown() override {
		if (f.player)
			f.player->getResponseRequester().denyAll(); // a pending question holds its npc
		npcs.clear();
		spawnGroups.clear();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		configs::main::CustomConfig::NPC_CUBE_EXPANDS_SIZE_LIMIT.store(savedNpcCubeExpandsLimit);
		configs::main::CustomConfig::CUBE_EXPANSION_LIMIT.store(savedCubeExpansionLimit);
		commons::utils::Rnd::generator() = *savedGenerator;
		ItemServicesTest::TearDown();
		dataholders::DataManager::CUBEEXPANDER_DATA.resetForTests();
		dataholders::DataManager::ITEM_RANDOM_BONUSES.resetForTests();
	}

	const model::templates::item::ItemTemplate& templateOf(int32_t itemId) {
		const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
		EXPECT_TRUE(itemTemplate) << itemId;
		return *itemTemplate;
	}

	/** The only bound action of the item, as the action class its XML element binds */
	template <class A>
	const A& onlyActionOf(int32_t itemId) {
		const auto& list = templateOf(itemId).getActions()->getItemActions();
		EXPECT_EQ(list.size(), 1u) << itemId;
		const A* action = dynamic_cast<const A*>(list.front().get());
		EXPECT_TRUE(action) << itemId;
		return *action;
	}

	/** An item row of the template with the columns the DAO would load (tune_count as given), not yet in any storage */
	Ref<Item> itemRow(int32_t objId, int32_t itemId, int64_t count, int32_t tuneCount = 0, bool equipped = false) {
		Ref<Item> item = Item::create(objId, itemId, count, std::nullopt, 0, "", 0, 0, equipped, false, 0,
			model::items::storage::getId(StorageType::CUBE), 0, 0, 0, 0, 0, 0, 0, tuneCount, 0, 0, 0, 0, false, 0, 0);
		items.push_back(item);
		return item;
	}

	/** itemRow, loaded into the cube the way the DAO does (onLoadHandler: no packet) */
	Item& inCube(int32_t objId, int32_t itemId, int64_t count, int32_t tuneCount = 0) {
		Ref<Item> item = itemRow(objId, itemId, count, tuneCount);
		storage(StorageType::CUBE).onLoadHandler(*item);
		return *item;
	}

	/** Kinah loaded into the cube the way the DAO does (the Kinah item row of the base fixture) */
	void giveKinah(int32_t objId, int64_t amount) { storage(StorageType::CUBE).onLoadHandler(*itemRow(objId, KINAH, amount)); }

	/** Every captured packet with the given Java opcode */
	std::vector<std::vector<uint8_t>> sentWithOpcode(int32_t opcode) {
		std::vector<std::vector<uint8_t>> matching;
		for (const std::vector<uint8_t>& packet : sent()) {
			if (javaOpcodeOf(packet) == opcode)
				matching.push_back(packet);
		}
		return matching;
	}

	/** An npc of CUBE_EXPANDER_NPC_TEMPLATES_XML, as VisibleObjectSpawner creates it (no known list or position is needed by these services) */
	model::gameobjects::Npc& npc(int32_t npcId) {
		static const dataholders::NpcData* npcData = [] {
			static xml::LoadContext npcContext;
			return xml::bindString<dataholders::NpcData>(npcContext, CUBE_EXPANDER_NPC_TEMPLATES_XML).release();
		}();
		const model::templates::npc::NpcTemplate* objectTemplate = npcData->getNpcTemplate(npcId);
		EXPECT_NE(objectTemplate, nullptr) << npcId;
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn =
			group->addSpawnTemplate(std::make_unique<PlayerItemsSpawnTemplate>(*group, 103.0f, 100.0f, 50.0f));
		runtime::Ref<model::gameobjects::Npc> created = model::gameobjects::VisibleObject::create<model::gameobjects::Npc>(
			std::make_unique<controllers::NpcController>(), spawn, objectTemplate);
		spawnGroups.push_back(group);
		npcs.push_back(created);
		return *created;
	}

	std::vector<Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<Ref<model::gameobjects::Npc>> npcs;
	std::unique_ptr<commons::utils::Rnd::Xoshiro256PlusPlus> savedGenerator;
	int32_t savedCubeExpansionLimit = 0;
	int32_t savedNpcCubeExpandsLimit = 0;
	std::shared_ptr<const std::string> savedMissingAiHandlers;
};

} // namespace aion::gameserver::services::item::test::playeritems
