#pragma once

// Shared fixture of the M5b-3 item client packet tests (m5b3-plan.md P-06): CM_EQUIP_ITEM and CM_DELETE_ITEM (tests/cm_ak), CM_USE_ITEM,
// CM_MOVE_ITEM, CM_SPLIT_ITEM, CM_REPLACE_ITEM, CM_MANASTONE, CM_START_LOOT and CM_LOOT_ITEM (tests/cm_lz, which includes this header by
// relative path as it includes InWorldPacketRunSupport.h), and the equipment tests of tests/player and tests/stats.
//
// - The player stands spawned in a Poeta map instance, so a cast or an appearance change can broadcast (WorldPosition.getWorldMapInstance). The
//   world holders (a map row, empty zones, shields and materials) are published once per process if no other fixture of the executable has
//   (ZoneService and the map instances cache theirs; the guarded pattern of tests/skills/P5-02b/EffectTestSupport.h publishWorldStaticDataOnce,
//   which tests/cm_lz also runs: whichever runs first publishes).
// - ITEM_DATA and SKILL_DATA hold the rows below, copied verbatim from the shipped data (game-server/data/static_data, file:line beside each).
// - Expected packets are Java's bytes where the packet's fields are the packet's own choice (writeOP + the writeImpl fields, opcodes of
//   ServerPacketsOpcodes.java); packets whose body carries an item info blob or a localized message are compared against the server's own
//   serialization of the packet Java constructs there (their bytes are pinned by tests/sm_ak and tests/sm_lz).

#include "InWorldPacketRunSupport.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/ItemSetData.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/MotionData.bind.h"
#include "aion/gameserver/dataholders/MotionData.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {

using model::gameobjects::Item;
using model::items::storage::StorageType;
using network::test::PacketReader;
using network::test::PacketWriter;

// ServerPacketsOpcodes.java:19-224
inline constexpr int32_t SM_STATS_INFO_OPCODE = 1;
inline constexpr int32_t SM_SYSTEM_MESSAGE_OPCODE = 25;
inline constexpr int32_t SM_INVENTORY_ADD_ITEM_OPCODE = 27;
inline constexpr int32_t SM_DELETE_ITEM_OPCODE = 28;
inline constexpr int32_t SM_INVENTORY_UPDATE_ITEM_OPCODE = 29;
inline constexpr int32_t SM_UPDATE_PLAYER_APPEARANCE_OPCODE = 36;
inline constexpr int32_t SM_EMOTION_OPCODE = 37;
inline constexpr int32_t SM_QUESTION_WINDOW_OPCODE = 52;
inline constexpr int32_t SM_CUBE_UPDATE_OPCODE = 130;
inline constexpr int32_t SM_WAREHOUSE_ADD_ITEM_OPCODE = 169;
inline constexpr int32_t SM_DELETE_WAREHOUSE_ITEM_OPCODE = 170;
inline constexpr int32_t SM_ITEM_USAGE_ANIMATION_OPCODE = 183;
inline constexpr int32_t SM_LOOT_STATUS_OPCODE = 205;
inline constexpr int32_t SM_LOOT_ITEMLIST_OPCODE = 206;

// the item ids of the rows below
inline constexpr int32_t TRAINING_SWORD = 100000094;
inline constexpr int32_t SOUL_BOUND_TEST_SWORD = 100000377;
inline constexpr int32_t FABLED_TEST_SWORD = 100000379;
inline constexpr int32_t SET_TEST_SWORD_01 = 100000714;
inline constexpr int32_t TRAINING_HAUBERK = 110500003;
inline constexpr int32_t SURE_STRIKE_STIGMA = 140001103;
inline constexpr int32_t SPITE_STRIKE_STIGMA = 140001104;
inline constexpr int32_t MERCENARYS_FRUIT_JUICE = 160000001;
inline constexpr int32_t MINOR_LIFE_POTION = 162000002;
inline constexpr int32_t MANASTONE_HP_20 = 167000226;
inline constexpr int32_t FX_TEST_EARTH_GODSTONE = 168000116;
inline constexpr int32_t SPARKIE_CARAPACE_FRAGMENT = 182004793;
inline constexpr int32_t LESSER_ANCIENT_KINAH = 182006985;
inline constexpr int32_t KINAH = 182400001;

/** the warrior's sword skills (ItemGroup.java:16, SWORD requires 37 or 44); Equipment.checkAvailableEquipSkills asks for one of them */
inline constexpr int32_t SWORD_SKILL = 37;

/** item_templates.xml, verbatim rows (the line of each <item_template> in the comment above it) */
inline constexpr std::string_view ITEM_TEMPLATES_XML = R"xml(<item_templates>
	<!-- :375 -->
	<item_template id="100000094" name="Training Sword" level="1" cName="sword_n_c_01a" mask="138366" item_group="SWORD" quality="COMMON" price="5" desc="700775" attack_type="PHYSICAL" max_enchant="10" m_slots="1">
		<weapon_stats hit_count="2" attack_range="1500" parry="173" physical_accuracy="52" critical="50" attack_speed="1400" max_damage="20" min_damage="16"/>
		<idian burn_attack="29" burn_defend="12"/>
	</item_template>
	<!-- :2062 -->
	<item_template id="100000377" name="Manastone Slot Test Superior Sword" level="1" cName="test_sword_option_slot_rare" mask="138494" item_group="SWORD" quality="RARE" price="5" option_slot_bonus="4" desc="717582" attack_type="PHYSICAL" max_enchant="10" m_slots="2">
		<weapon_stats hit_count="2" attack_range="1500" parry="173" physical_accuracy="118" critical="50" attack_speed="1400" max_damage="20" min_damage="16"/>
		<idian burn_attack="29" burn_defend="12"/>
	</item_template>
	<!-- :2070 -->
	<item_template id="100000379" name="Manastone Slot Test Fabled Sword" level="1" cName="test_sword_option_slot_unique" mask="138366" item_group="SWORD" quality="UNIQUE" price="5" option_slot_bonus="2" desc="717584" attack_type="PHYSICAL" max_enchant="15" m_slots="4">
		<modifiers>
			<add name="MAXHP" value="500" bonus="true"/>
		</modifiers>
		<weapon_stats hit_count="2" attack_range="1500" magical_accuracy="42" parry="173" physical_accuracy="250" critical="50" attack_speed="1400" max_damage="20" min_damage="16"/>
		<idian burn_attack="29" burn_defend="12"/>
	</item_template>
	<!-- :4443 -->
	<item_template id="100000714" name="Set Test Sword 01" level="30" cName="test_set_sword_01" mask="138366" item_group="SWORD" quality="COMMON" price="5" restrict="30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30" desc="743693" attack_type="PHYSICAL" can_exceed_enchant="true" enchant_type="1" max_enchant_bonus="5">
		<modifiers>
			<add name="MAXHP" value="135" bonus="true"/>
			<add name="PHYSICAL_ACCURACY" value="60" bonus="true"/>
		</modifiers>
		<weapon_stats hit_count="2" attack_range="1500" parry="173" physical_accuracy="52" critical="50" attack_speed="1400" max_damage="20" min_damage="16"/>
		<idian burn_attack="29" burn_defend="12"/>
	</item_template>
	<!-- :232833 -->
	<item_template id="110500003" name="Training Hauberk" level="1" cName="ch_torso_n_c_01a" mask="36990" item_group="CH_TORSO" quality="COMMON" price="5" desc="700977" max_enchant="10" m_slots="1">
		<modifiers>
			<add name="EVASION" value="21"/>
			<add name="MAGICAL_RESIST" value="10"/>
			<add name="PHYSICAL_DEFENSE" value="24"/>
		</modifiers>
	</item_template>
	<!-- :742368 -->
	<item_template id="140001103" name="Sure Strike" level="45" cName="STIGMA_N_FI_burserklance_g1" mask="4222" item_group="STIGMA" quality="LEGEND" price="3970" restrict="0 45 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" desc="844617">
		<stigma gain_skill_group1="FI_BURSERKLANCE" chargeable="true"/>
	</item_template>
	<!-- :742371 -->
	<item_template id="140001104" name="Spite Strike" level="45" cName="STIGMA_N_FI_technicalcounter_g1" mask="4222" item_group="STIGMA" quality="LEGEND" price="3970" restrict="0 45 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" desc="844618">
		<stigma gain_skill_group1="FI_TECHNICALCOUNTER" chargeable="true"/>
	</item_template>
	<!-- :821856 -->
	<item_template id="160000001" name="Mercenary's Fruit Juice" level="1" cName="dish_hp_01a" mask="12360" max_stack_count="1000" quality="COMMON" price="5" desc="702368" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10034"/>
		</actions>
		<uselimits usedelay="5000" usedelayid="21"/>
	</item_template>
	<!-- :830724 -->
	<item_template id="162000002" name="Minor Life Potion" level="10" cName="remedy_hp_10a" mask="12414" max_stack_count="1000" quality="COMMON" price="250" desc="702583" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="9889"/>
		</actions>
		<uselimits usedelay="30000" usedelayid="11"/>
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
	<!-- :848006 -->
	<item_template id="168000116" name="Fx Test Earth Godstone" level="1" cName="test_matter_proc_earth_damage_high" mask="12414" max_stack_count="100" quality="COMMON" price="1" desc="753638" activate_count="1">
		<godstone nonbreakcount="0" breakprob="0" probabilityleft="500" probability="1000" skilllvl="1" skillid="8267"/>
	</item_template>
	<!-- :874138 -->
	<item_template id="182004793" name="Sparkie Carapace Fragment" level="5" cName="junk_spaky_05" mask="12414" max_stack_count="1000" quality="JUNK" price="300" desc="718718"/>
	<!-- :876350 -->
	<item_template id="182006985" name="Lesser Ancient Kinah" level="1" cName="junk_OwnerTree_housing_gold_01" mask="28684" max_stack_count="1000" quality="JUNK" price="42857" desc="801258"/>
	<!-- :894666 -->
	<item_template id="182400001" name="Kinah" level="1" cName="gold" mask="12350" quality="COMMON" price="0" desc="701677"/>
</item_templates>)xml";

/** skill_templates.xml, verbatim rows */
inline constexpr std::string_view SKILL_TEMPLATES_XML = R"xml(<skill_data>
	<!-- :94394, Mercenary's Fruit Juice (160000001) -->
	<skill_template skill_id="10034" name="Increase Natural Healing" nameId="729310" stack="SHOP_FOOD_HPREGEN" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" conflict_id="21" activation="ACTIVE" cooldown="0" duration="0">
		<properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" />
		<startconditions>
			<weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" />
		</startconditions>
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<effects>
			<statup duration2="900000" effectid="10013" e="1" noresist="true" element="FIRE">
				<change stat="REGEN_HP" func="ADD" delta="2" value="0" />
			</statup>
		</effects>
	</skill_template>
</skill_data>)xml";

/** One server packet as Java writes it: AionServerPacket.writeOP ([H op][C 0x44][H ~op], op = Crypt.encodeServerPacketOpcode) and the body */
inline std::vector<uint8_t> javaPacket(int32_t opcode, const PacketWriter& body) {
	int32_t op = (opcode + 207) ^ 0xDF; // Crypt.encodeServerPacketOpcode: (opcode + SM_VERSION_CHECK.INTERNAL_VERSION) ^ 0xDF
	PacketWriter packet;
	packet.H(op).C(0x44).H(~op);
	packet.B(body.data);
	return packet.data;
}

/** The opcode Java wrote into the header of a captured packet */
inline int32_t javaOpcodeOf(const std::vector<uint8_t>& bytes) {
	PacketReader reader(bytes);
	return ((static_cast<uint16_t>(reader.H()) ^ 0xDF) - 207) & 0xFFFF;
}

/** The opcodes of the captured packets, in order */
inline std::vector<int32_t> opcodesOf(const std::vector<std::vector<uint8_t>>& packets) {
	std::vector<int32_t> opcodes;
	for (const std::vector<uint8_t>& packet : packets)
		opcodes.push_back(javaOpcodeOf(packet));
	return opcodes;
}

/** The captured packets of one opcode, in order */
inline std::vector<std::vector<uint8_t>> packetsOf(const std::vector<std::vector<uint8_t>>& packets, int32_t opcode) {
	std::vector<std::vector<uint8_t>> matching;
	for (const std::vector<uint8_t>& packet : packets) {
		if (javaOpcodeOf(packet) == opcode)
			matching.push_back(packet);
	}
	return matching;
}

/** SM_DELETE_ITEM (SM_DELETE_ITEM.java writeImpl): D(itemObjectId), C(deleteType mask; ItemDeleteType MOVE 0x14, DISCARD 0x15) */
inline std::vector<uint8_t> deleteItem(int32_t objectId, int32_t deleteMask) {
	return javaPacket(SM_DELETE_ITEM_OPCODE, PacketWriter().D(objectId).C(deleteMask));
}

/**
 * SM_CUBE_UPDATE.cubeSize(type, player) of a character without expansions (SM_CUBE_UPDATE.java:29-50, writeImpl :69-80): C(action 0),
 * C(type.ordinal()), D(the item count of the cube or the regular warehouse), C(npc expands 0), C(quest expands 0), C(item expands 0)
 */
inline std::vector<uint8_t> cubeSize(StorageType type, int32_t itemsCount) {
	return javaPacket(SM_CUBE_UPDATE_OPCODE, PacketWriter().C(0).C(static_cast<int32_t>(type)).D(itemsCount).C(0).C(0).C(0));
}

/** An item row as the inventory DAO loads it: count, location and slot, everything else 0 */
inline runtime::Ref<Item> loadedItem(int32_t objId, int32_t itemId, int64_t count, StorageType location, int64_t slot = 0, bool equipped = false) {
	return Item::create(objId, itemId, count, std::nullopt, 0, "", 0, 0, equipped, false, slot, model::items::storage::getId(location), 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, false, 0, 0);
}

/** The world holders a map instance reads, published once per process by whichever fixture of the executable runs first */
inline void publishPoetaWorldDataOnce() {
	static const bool published = [] {
		configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
		// the unit tests load no geo data: with cansee off GeoService::canSee answers true
		configs::main::GeoDataConfig::CANSEE_ENABLE.store(false);
		static std::deque<xml::LoadContext> contexts;
		// the Poeta row of world_maps.xml:11 reduced to the attributes a map instance reads - character for character the row
		// tests/skills/P5-02b/EffectTestSupport.h publishes, so the executable sees the same map whichever fixture publishes first
		if (!dataholders::DataManager::WORLD_MAPS_DATA)
			dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(contexts.emplace_back(),
				R"(<world_maps><map id="210010000" cName="LF1" name="Poeta" name_id="1" water_level="16" death_level="0")"
				R"( world_type="ELYSEA" world_size="1024" flags="FLY GLIDE RECALL"/></world_maps>)"));
		if (!dataholders::DataManager::ZONE_DATA)
			dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(contexts.emplace_back(), "<zones/>"));
		if (!dataholders::DataManager::SHIELD_DATA)
			dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(contexts.emplace_back(), "<shields/>"));
		if (!dataholders::DataManager::MATERIAL_DATA)
			dataholders::DataManager::MATERIAL_DATA.publish(
				xml::bindString<dataholders::MaterialData>(contexts.emplace_back(), "<material_templates/>"));
		return true;
	}();
	static_cast<void>(published);
}

/**
 * InWorldPacketTest with the item rows above, a spawned WARRIOR in a Poeta map instance that knows the sword skill, and a TestClient that
 * holds every packet sent to him. Each case adds the items it needs to his storages the way the DAO loads them (onLoadHandler: no packet).
 */
class ItemPacketTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		// the base fixture's DeterministicExecutor again, with a handle the timed cases advance (nothing is scheduled yet)
		utils::ThreadPoolManager::installBackend(nullptr);
		auto backend = std::make_unique<runtime::DeterministicExecutor>(clock, 17);
		executor = backend.get(); // owned by ThreadPoolManager until the base TearDown installs no backend
		utils::ThreadPoolManager::installBackend(std::move(backend));
		publishPoetaWorldDataOnce();
		xml::LoadContext context;
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, ITEM_TEMPLATES_XML));
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the base fixture's empty holder; its TearDown resets this one
		xml::LoadContext skillContext;
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, SKILL_TEMPLATES_XML));
		// the item info blob's GENERAL_INFO entry asks it (hasAccountOrLegionWhStorabilityDisabled); no cleanup entries
		dataholders::DataManager::ITEM_CLEAN_UP.publish(std::make_unique<dataholders::ItemRestrictionCleanupData>());
		xml::LoadContext motionContext;
		dataholders::DataManager::MOTION_DATA.publish(xml::bindString<dataholders::MotionData>(motionContext, "<motion_times/>"));
		// Equipment.notifyItemEquipped / itemSetPartsEquipped ask the item sets; none of the rows belongs to one
		dataholders::DataManager::ITEM_SET_DATA.publish(std::make_unique<dataholders::ItemSetData>());
		map = world::WorldMap::create(dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(210010000));
		mapInstance = world::WorldMap2DInstance::create(*map, 1, 0, 0, [](world::WorldMapInstance& instance) {
			return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(
				::aion::gameserver::instance::handlers::GeneralInstanceHandler::create(instance));
		});
		f = makePlayer(710101, 9901, "Holder");
		f.player->setPosition(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(100.0f, 100.0f, 50.0f)));
		f.player->getPosition()->setIsSpawned(true);
		f.player->setSkillList(model::skill::PlayerSkillList::create({model::skill::PlayerSkillEntry::create(SWORD_SKILL, 1, 0,
			model::gameobjects::Persistable_PersistentState::UPDATED)}));
		client = std::make_unique<TestClient>();
		client->enterWorld(f);
		clearSent();
	}

	void TearDown() override {
		// the player first, then the map instance his position names, then the base fixture
		if (f.player) {
			f.player->setCasting(nullptr); // a cast in progress holds the caster (Skill.effector)
			f.player->setTarget(nullptr);
			f.player->setClientConnection(nullptr);
		}
		client.reset();
		items.clear();
		f = {};
		mapInstance = nullptr;
		map = nullptr;
		InWorldPacketTest::TearDown();
		dataholders::DataManager::ITEM_SET_DATA.resetForTests();
		dataholders::DataManager::MOTION_DATA.resetForTests();
		dataholders::DataManager::ITEM_CLEAN_UP.resetForTests();
		dataholders::DataManager::ITEM_DATA.resetForTests();
	}

	model::gameobjects::player::Player& player() { return *f.player; }

	model::items::storage::Storage& storage(StorageType type) { return *player().getStorage(model::items::storage::getId(type)); }

	/** An item of the template, loaded into the storage of `location` the way the DAO does (onLoadHandler: no packet) */
	Item& stored(int32_t objId, int32_t itemId, int64_t count, StorageType location = StorageType::CUBE, int64_t slot = 0) {
		runtime::Ref<Item> item = loadedItem(objId, itemId, count, location, slot);
		storage(location).onLoadHandler(*item);
		items.push_back(item);
		return *item;
	}

	/** An item loaded as equipped in `slot` (Equipment.onLoadHandler, as PlayerService.loadPlayer does) */
	Item& equipped(int32_t objId, int32_t itemId, int64_t slot) {
		runtime::Ref<Item> item = loadedItem(objId, itemId, 1, StorageType::CUBE, slot, true);
		player().getEquipment().onLoadHandler(*item);
		items.push_back(item);
		return *item;
	}

	std::vector<std::vector<uint8_t>> sent() { return (*client)->sentBytes(); }

	void clearSent() { (*client)->clearSent(); }

	std::vector<uint8_t> serializedFor(AionServerPacket&& packet) { return serialized(std::move(packet), client->con()); }

	runtime::DeterministicExecutor* executor = nullptr;
	runtime::Ref<world::WorldMap> map;
	runtime::Ref<world::WorldMapInstance> mapInstance;
	PlayerFixture f;
	std::unique_ptr<TestClient> client;
	std::vector<runtime::Ref<Item>> items;
};

} // namespace aion::gameserver::network::aion::clientpackets::testing::items
