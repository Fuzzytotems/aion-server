#pragma once

// Shared fixture of the M5b-3 item service tests (m5b3-plan.md T-07): ItemService, ItemMoveService, ItemRestrictionService, ItemSplitService,
// ItemSocketService.socketGodstone, the item actions and StigmaService.notifyEquipAction, driven against a real Player whose packets a real
// AionConnection captures (tests/cm_ak/InWorldPacketRunSupport.h, included by relative path the way ItemPacketServiceTest.cpp does).
//
// Every item template below is a row of the shipped data copied verbatim (game-server/data/static_data/items/item_templates.xml, line cited).
// Expected packets are spelled out as the bytes Java writes (AionServerPacket.writeOP with the opcode of ServerPacketsOpcodes.java and the
// writeImpl fields the service chooses); packets whose body carries an ItemInfoBlob are compared whole against their own serialization (their
// bytes are pinned by tests/sm_ak and tests/sm_lz) after the fields the service chooses were read from the bytes by hand.

#include "../cm_ak/InWorldPacketRunSupport.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test {

namespace cp = network::aion::clientpackets::testing;
using model::gameobjects::Item;
using model::items::storage::StorageType;
using network::test::PacketReader;
using network::test::PacketWriter;
using runtime::Ptr;
using runtime::Ref;

// ServerPacketsOpcodes.java:43-201
inline constexpr int32_t SM_SYSTEM_MESSAGE_OPCODE = 25;
inline constexpr int32_t SM_INVENTORY_ADD_ITEM_OPCODE = 27;
inline constexpr int32_t SM_DELETE_ITEM_OPCODE = 28;
inline constexpr int32_t SM_INVENTORY_UPDATE_ITEM_OPCODE = 29;
inline constexpr int32_t SM_CUBE_UPDATE_OPCODE = 130;
inline constexpr int32_t SM_WAREHOUSE_ADD_ITEM_OPCODE = 169;
inline constexpr int32_t SM_DELETE_WAREHOUSE_ITEM_OPCODE = 170;
inline constexpr int32_t SM_WAREHOUSE_UPDATE_ITEM_OPCODE = 171;
inline constexpr int32_t SM_ITEM_USAGE_ANIMATION_OPCODE = 183;

// the item ids of the rows below
inline constexpr int32_t TRAINING_SWORD = 100000094;
inline constexpr int32_t TAHABATA_SWORD = 100000768;
inline constexpr int32_t TUNE_RETUNE_TEST_OPTION = 100001276;
inline constexpr int32_t SPARKIE_CANDY = 160001275;
inline constexpr int32_t MINOR_LIFE_POTION = 162000002;
inline constexpr int32_t MINOR_MANA_POTION = 162000007;
inline constexpr int32_t SHISHIRS_POWERSTONE = 164000137;
inline constexpr int32_t VERTERON_HELPER_SCROLL = 164000227;
inline constexpr int32_t EVENT_ACCELEROX = 164002116;
inline constexpr int32_t L1_ENCHANTMENT_STONE = 166000001;
inline constexpr int32_t WEAPON_REIDENTIFY_TEST_ITEM = 166200000;
inline constexpr int32_t FX_TEST_EARTH_GODSTONE = 168000116;
inline constexpr int32_t MINOR_POWER_SHARD = 169000003;
inline constexpr int32_t DYE_REMOVER = 169100000;
inline constexpr int32_t BANDAGE = 169300002;
inline constexpr int32_t CARPET = 170000000;
inline constexpr int32_t OCTAGONAL_BOARD_ROOF = 170000023;
inline constexpr int32_t ODIUM_REFINING_METHOD = 182200558;
inline constexpr int32_t KINAH = 182400001;
inline constexpr int32_t SPARKIE_CARAPACE_FRAGMENT = 182004793;
inline constexpr int32_t PET_CARD_SIBERIAN_TIGER = 190000000;
inline constexpr int32_t CIRRUSPEED = 190100000;

inline constexpr int32_t HEALING_LIGHT_II = 140000001;
inline constexpr int32_t FLAME_CAGE_I = 140000002;
inline constexpr int32_t MERCENARYS_FRUIT_JUICE = 160000001;

/** item_templates.xml, verbatim rows (the line of each <item_template> in the comment above it) */
inline constexpr std::string_view ITEM_TEMPLATES_XML = R"xml(<item_templates>
	<!-- :739006 -->
	<item_template id="140000001" name="Healing Light II" level="20" cName="item_stigma_heal_g2" mask="4672" item_group="STIGMA" quality="RARE" price="6250" restrict="20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20" desc="728292">
		<stigma gain_skill_group1="STIGMA_HEAL" chargeable="false"/>
	</item_template>
	<!-- :739009 -->
	<item_template id="140000002" name="Flame Cage I" level="20" cName="item_stigma_flamecage_g1" mask="4672" item_group="STIGMA" quality="RARE" price="6250" restrict="20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20" desc="728293">
		<stigma gain_skill_group1="STIGMA_FLAMECAGE" chargeable="false"/>
	</item_template>
	<!-- :821856 -->
	<item_template id="160000001" name="Mercenary's Fruit Juice" level="1" cName="dish_hp_01a" mask="12360" max_stack_count="1000" quality="COMMON" price="5" desc="702368" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10034"/>
		</actions>
		<uselimits usedelay="5000" usedelayid="21"/>
	</item_template>
	<!-- :375 -->
	<item_template id="100000094" name="Training Sword" level="1" cName="sword_n_c_01a" mask="138366" item_group="SWORD" quality="COMMON" price="5" desc="700775" attack_type="PHYSICAL" max_enchant="10" m_slots="1">
		<weapon_stats hit_count="2" attack_range="1500" parry="173" physical_accuracy="52" critical="50" attack_speed="1400" max_damage="20" min_damage="16"/>
		<idian burn_attack="29" burn_defend="12"/>
	</item_template>
	<!-- :4807 -->
	<item_template id="100000768" name="Tahabata's Sword" level="50" cName="sword_n_e1_50a" mask="138316" pack_count="3" item_group="SWORD" quality="EPIC" price="1361300" restrict="50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50" desc="746164" attack_type="PHYSICAL" exceed_enchant_skill="RANK2_SET1_PHYSICAL_WEAPON" can_exceed_enchant="true" max_enchant="15" m_slots="5" temp_exchange_time="10">
		<modifiers>
			<rate name="ATTACK_SPEED" value="-19" bonus="true"/>
			<add name="PHYSICAL_ACCURACY" value="140" bonus="true"/>
			<add name="PHYSICAL_CRITICAL" value="78" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="36" bonus="true"/>
			<add name="MAXHP" value="317" bonus="true"/>
		</modifiers>
		<actions>
			<remodel type="2"/>
		</actions>
		<weapon_stats hit_count="2" attack_range="1500" magical_accuracy="285" parry="780" physical_accuracy="870" critical="50" attack_speed="1400" max_damage="197" min_damage="161"/>
		<disposition id="188950005" count="6"/>
		<uselimits pack_count="3"/>
		<idian burn_attack="29" burn_defend="12"/>
	</item_template>
	<!-- :9614 -->
	<item_template id="100001276" name="Tune, Retune_Test_Option" level="55" cName="test_sword_n_e1_55a" mask="134220" item_group="SWORD" quality="EPIC" price="2499500" rnd_count="1" rnd_bonus="1" restrict="55 55 55 55 55 55 55 55 55 55 55 55 55 55 55 55 55" desc="783931" attack_type="PHYSICAL" can_exceed_enchant="true" max_enchant="15" m_slots="5" temp_exchange_time="10">
		<modifiers>
			<add name="MAXHP" value="500" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="30" bonus="true"/>
			<add name="MAXMP" value="150" bonus="true"/>
		</modifiers>
		<weapon_stats hit_count="2" attack_range="1500" magical_accuracy="311" parry="974" physical_accuracy="936" critical="50" attack_speed="1400" max_damage="211" min_damage="171"/>
		<disposition id="188950006" count="6"/>
		<idian burn_attack="29" burn_defend="12"/>
	</item_template>
	<!-- :823368 -->
	<item_template id="160001275" name="Sparkie Candy" level="20" cName="food_Spaky_6_n" mask="12414" max_stack_count="1000" quality="RARE" price="750" desc="730258" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="10171"/>
		</actions>
		<uselimits usedelay="5000" usedelayid="24"/>
	</item_template>
	<!-- :830724 -->
	<item_template id="162000002" name="Minor Life Potion" level="10" cName="remedy_hp_10a" mask="12414" max_stack_count="1000" quality="COMMON" price="250" desc="702583" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="9889"/>
		</actions>
		<uselimits usedelay="30000" usedelayid="11"/>
	</item_template>
	<!-- :830754 -->
	<item_template id="162000007" name="Minor Mana Potion" level="10" cName="remedy_mp_10a" mask="12414" max_stack_count="1000" quality="COMMON" price="250" desc="702593" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="9894"/>
		</actions>
		<uselimits usedelay="30000" usedelayid="11"/>
	</item_template>
	<!-- :833141 -->
	<item_template id="164000137" name="Shishir's Powerstone" level="1" cName="kaspa_damage_item" mask="20553" quality="LEGEND" price="5" desc="764391" activate_target="STANDALONE" activate_combat="true" activate_count="1000" expire_time="120">
		<actions>
			<skilluse level="4" skillid="9832"/>
		</actions>
		<uselimits usedelay="10000" usedelayid="61" usearea="IDELIM_ITEMUSE"/>
	</item_template>
	<!-- :833680 -->
	<item_template id="164000227" name="Verteron Helper Summoning Scroll" level="1" cName="Item_RecallAid_LF1a_01" casting_delay="4500" mask="4164" quality="COMMON" price="5" race="ELYOS" desc="798864" activate_target="MYMENTO" activate_count="1">
		<actions>
			<skilluse mapid="210030000" level="1" skillid="10490"/>
		</actions>
		<uselimits usedelay="3600000" usedelayid="67"/>
		<inventory id="2"/>
	</item_template>
	<!-- :835126 -->
	<item_template id="164002116" name="[Event] Rx: Accelerox" level="30" cName="world_event_scroll_speed_run_50a" mask="12352" max_stack_count="1000" quality="RARE" price="5" desc="798392" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="3" skillid="10465"/>
		</actions>
		<uselimits usedelay="1000" usedelayid="35"/>
	</item_template>
	<!-- :837022 -->
	<item_template id="166000001" name="L1 Enchantment Stone" level="1" cName="matter_enchant_01" mask="12414" max_stack_count="100" item_group="ENCHANTMENT" quality="COMMON" price="5" desc="701681" activate_count="1">
		<actions>
			<enchant count="1"/>
		</actions>
	</item_template>
	<!-- :839141 -->
	<item_template id="166200000" name="Weapon Reidentify Test Item" level="60" cName="test_reidentify_01" mask="12414" max_stack_count="100" quality="EPIC" price="1000" desc="806540" activate_count="1">
		<actions>
			<tuning no_reduce="false" target="WEAPON"/>
		</actions>
	</item_template>
	<!-- :848006 -->
	<item_template id="168000116" name="Fx Test Earth Godstone" level="1" cName="test_matter_proc_earth_damage_high" mask="12414" max_stack_count="100" quality="COMMON" price="1" desc="753638" activate_count="1">
		<godstone nonbreakcount="0" breakprob="0" probabilityleft="500" probability="1000" skilllvl="1" skillid="8267"/>
	</item_template>
	<!-- :848722 -->
	<item_template id="169000003" name="Minor Power Shard" level="1" cName="battery_01" mask="12414" max_stack_count="10000" item_group="POWER_SHARDS" quality="COMMON" price="5" desc="701811" weapon_boost="10"/>
	<!-- :848736 -->
	<item_template id="169100000" name="Dye Remover" level="1" cName="dye_remover" mask="12414" max_stack_count="100" quality="COMMON" price="500" desc="701823" activate_count="1">
		<actions>
			<dye color="no"/>
		</actions>
	</item_template>
	<!-- :850013 -->
	<item_template id="169300002" name="Bandage" level="1" cName="bandage_01" mask="12414" max_stack_count="10000" quality="COMMON" price="5" desc="701824"/>
	<!-- :861772 -->
	<item_template id="170000000" name="Carpet" level="1" cName="item_furniture_Carpet_Normal_01" mask="20606" quality="COMMON" price="5" desc="763902" activate_target="STANDALONE" activate_count="1000">
		<actions>
			<houseobject id="3000001"/>
		</actions>
		<uselimits usedelayid="91"/>
	</item_template>
	<!-- :861910 -->
	<item_template id="170000023" name="Octagonal Board Roof" level="1" cName="item_housing_add_cp_a002_roof_01" mask="20606" quality="COMMON" price="5" desc="790490" activate_target="STANDALONE" activate_count="1000">
		<actions>
			<housedeco/>
		</actions>
		<uselimits usedelayid="91"/>
	</item_template>
	<!-- :874138 -->
	<item_template id="182004793" name="Sparkie Carapace Fragment" level="5" cName="junk_spaky_05" mask="12414" max_stack_count="1000" quality="JUNK" price="300" desc="718718"/>
	<!-- :877230 -->
	<item_template id="182200558" name="Odium Refining Method" level="12" cName="doc_quest_1197a" mask="20545" quality="COMMON" price="1" desc="1106553" activate_target="STANDALONE" activate_count="1000">
		<actions>
			<queststart questid="1197"/>
			<read/>
		</actions>
		<uselimits usedelay="2000" usedelayid="41"/>
		<inventory id="2"/>
	</item_template>
	<!-- :894666 -->
	<item_template id="182400001" name="Kinah" level="1" cName="gold" mask="12350" quality="COMMON" price="0" desc="701677"/>
	<!-- :930741 -->
	<item_template id="190000000" name="Pet Distribution Card: Siberian Wild Tiger (Purebred)" level="1" cName="test_petcard_01" mask="4222" quality="COMMON" price="5" desc="765502">
		<actions>
			<adoptpet petId="900000"/>
		</actions>
	</item_template>
	<!-- :932552 -->
	<item_template id="190100000" name="Cirruspeed" level="1" cName="ride_cloud_001" casting_delay="3000" mask="4101" quality="EPIC" price="5" restrict="30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30" desc="781281" activate_target="STANDALONE" activate_count="1000">
		<actions>
			<ride npc_id="2000000"/>
		</actions>
	</item_template>
</item_templates>)xml";

/** item_random_bonuses.xml (game-server/data/static_data/items), the verbatim INVENTORY set 1 (:3-13) of Tune, Retune_Test_Option's rnd_bonus */
inline constexpr std::string_view ITEM_RANDOM_BONUSES_XML = R"xml(<random_bonuses>
	<random_bonus type="INVENTORY" id="1">
		<modifiers chance="50.0">
			<add name="MAXHP" value="100" bonus="true"/>
			<add name="MAXMP" value="-50" bonus="true"/>
		</modifiers>
		<modifiers chance="20.0">
			<add name="MAXHP" value="-100" bonus="true"/>
			<add name="MAXMP" value="-50" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="10" bonus="true"/>
		</modifiers>
	</random_bonus>
</random_bonuses>)xml";

/** skill_templates.xml (game-server/data/static_data/skills), verbatim rows of the item skills the SkillUseAction cases use */
inline constexpr std::string_view SKILL_TEMPLATES_XML = R"xml(<skill_data>
	<!-- :91124, Shishir's Powerstone (164000137): an enemy target is needed -->
	<skill_template skill_id="9832" name="Shishir's Powerstone" nameId="296415" stack="ITEM_DEVA_DAMAGE_IDELIM" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0" ammospeed="40" hostile_type="DIRECT">
		<properties first_target="TARGET" first_target_range="20" target_relation="ENEMY" target_type="ONLYONE" />
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<effects>
			<noreducespellatk value="7000" e="1" element="FIRE" hoptype="DAMAGE" />
		</effects>
		<motion name="pointfire3" />
	</skill_template>
	<!-- :91905, Minor Life Potion (162000002) -->
	<skill_template skill_id="9889" name="Healing" nameId="702131" stack="ITEM_REMEDY_HP_10" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE" cooldown="0" duration="0">
		<properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" />
		<startconditions>
			<weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" />
		</startconditions>
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<effects>
			<prochealinstant value="37" e="1" noresist="true" element="WATER" />
			<heal checktime="2000" value="37" duration2="20000" effectid="209582" e="2" noresist="true" element="WATER" preeffect="1" />
		</effects>
	</skill_template>
	<!-- :91970, Minor Mana Potion (162000007) -->
	<skill_template skill_id="9894" name="Mana Treatment" nameId="702132" stack="ITEM_REMEDY_MP_10" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE" cooldown="0" duration="0">
		<properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" />
		<startconditions>
			<weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" />
		</startconditions>
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<effects>
			<procmphealinstant value="59" e="1" noresist="true" element="WATER" />
			<mpheal checktime="2000" value="59" duration2="20000" effectid="218232" e="2" noresist="true" element="WATER" preeffect="1" />
		</effects>
	</skill_template>
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
	<!-- :96934, Sparkie Candy (160001275) -->
	<skill_template skill_id="10171" name="Sparkie Candy" nameId="729354" stack="FOOD_SHAPE_SPAKY_6_N" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE" cooldown="0" duration="0">
		<properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" />
		<startconditions>
			<weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" />
		</startconditions>
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<effects>
			<shapechange model="210119" type="NONE" cantUseSkills="true" duration2="600000" effectid="275" e="1" noresist="true" />
			<statup duration2="600000" e="2" noresist="true" element="FIRE" preeffect="1">
				<change stat="FLY_TIME" func="ADD" value="10" />
			</statup>
		</effects>
	</skill_template>
</skill_data>)xml";

/** Sets an atomic configuration value for the scope and restores the previous value */
template <class T>
class AtomicConfigScope {
public:
	AtomicConfigScope(std::atomic<T>& configValue, T value) : config(configValue), previous(configValue.load()) { config.store(value); }
	~AtomicConfigScope() { config.store(previous); }
	AtomicConfigScope(const AtomicConfigScope&) = delete;
	AtomicConfigScope& operator=(const AtomicConfigScope&) = delete;

private:
	std::atomic<T>& config;
	const T previous;
};

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

/** The last two body bytes of a captured packet as Java's final writeH (the update type mask of a sendable type), -1 for a shorter body */
inline int32_t trailingMask(const std::vector<uint8_t>& packet) {
	std::vector<uint8_t> body = cp::bodyOf(packet);
	return body.size() < 2 ? -1 : body[body.size() - 2] | body[body.size() - 1] << 8;
}

/** The opcodes of the captured packets, in order */
inline std::vector<int32_t> opcodesOf(const std::vector<std::vector<uint8_t>>& packets) {
	std::vector<int32_t> opcodes;
	for (const std::vector<uint8_t>& packet : packets)
		opcodes.push_back(javaOpcodeOf(packet));
	return opcodes;
}

/**
 * SM_CUBE_UPDATE.cubeSize(type, player) of a character without expansions (SM_CUBE_UPDATE.java:29-80): action 0, type.ordinal(), the item count
 * of the cube or the regular warehouse (0 for every other storage) and three zero bytes
 */
inline std::vector<uint8_t> cubeSize(StorageType type, int32_t itemsCount) {
	return javaPacket(SM_CUBE_UPDATE_OPCODE, PacketWriter().C(0).C(static_cast<int32_t>(type)).D(itemsCount).C(0).C(0).C(0));
}

/** SM_DELETE_ITEM (SM_DELETE_ITEM.java): writeD(itemObjectId), writeC(deleteType mask) */
inline std::vector<uint8_t> deleteItem(int32_t objectId, int32_t deleteMask) {
	return javaPacket(SM_DELETE_ITEM_OPCODE, PacketWriter().D(objectId).C(deleteMask));
}

/** SM_DELETE_WAREHOUSE_ITEM (SM_DELETE_WAREHOUSE_ITEM.java): writeC(warehouseType id), writeD(itemObjId), writeC(deleteType mask) */
inline std::vector<uint8_t> deleteWarehouseItem(int32_t warehouseType, int32_t objectId, int32_t deleteMask) {
	return javaPacket(SM_DELETE_WAREHOUSE_ITEM_OPCODE, PacketWriter().C(warehouseType).D(objectId).C(deleteMask));
}

/**
 * SM_ITEM_USAGE_ANIMATION(playerObjId, itemObjId, itemId, time, end, unk) (SM_ITEM_USAGE_ANIMATION.java): D(player) D(target = player)
 * D(itemObjId) D(itemId) D(time) C(end) C(unk 0) C(unk1 0) C(unk2 1) D(unk3 = the sixth argument)
 */
inline std::vector<uint8_t> itemUsageAnimation(int32_t playerObjId, int32_t itemObjId, int32_t itemId, int32_t time, int32_t end, int32_t unk3) {
	return javaPacket(SM_ITEM_USAGE_ANIMATION_OPCODE,
		PacketWriter().D(playerObjId).D(playerObjId).D(itemObjId).D(itemId).D(time).C(end).C(0).C(0).C(1).D(unk3));
}

/** An item row as the inventory DAO loads it: count, location and slot, everything else 0 */
inline Ref<Item> loadedItem(int32_t objId, int32_t itemId, int64_t count, StorageType location, int64_t slot = 0, bool equipped = false) {
	return Item::create(objId, itemId, count, std::nullopt, 0, "", 0, 0, equipped, false, slot, model::items::storage::getId(location), 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, false, 0, 0);
}

class ItemServicesTest : public cp::InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		// the base fixture's DeterministicExecutor again, with a handle the timed cases advance (nothing is scheduled yet)
		utils::ThreadPoolManager::installBackend(nullptr);
		auto backend = std::make_unique<runtime::DeterministicExecutor>(clock, 17);
		executor = backend.get(); // owned by ThreadPoolManager until the base TearDown installs no backend
		utils::ThreadPoolManager::installBackend(std::move(backend));
		xml::LoadContext context;
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, ITEM_TEMPLATES_XML));
		// the base fixture's empty SKILL_DATA, replaced by the item skills (the base TearDown resets it)
		dataholders::DataManager::SKILL_DATA.resetForTests();
		xml::LoadContext skillContext;
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, SKILL_TEMPLATES_XML));
		// TargetRelationProperty asks isMaterialSkill (TargetRelationProperty.java:22): an empty material list, published once per process and
		// never reset (the pattern of tests/skills/P5-02a/CastTestSupport.h)
		if (!dataholders::DataManager::MATERIAL_DATA) {
			static xml::LoadContext materialContext;
			dataholders::DataManager::MATERIAL_DATA.publish(xml::bindString<dataholders::MaterialData>(materialContext, "<material_templates/>"));
		}
		// the item info blob's GENERAL_INFO entry asks it (GeneralInfoBlobEntry: hasAccountOrLegionWhStorabilityDisabled); no cleanup entries
		dataholders::DataManager::ITEM_CLEAN_UP.publish(std::make_unique<dataholders::ItemRestrictionCleanupData>());
		f = cp::makePlayer(700101, 9801, "Looter");
		client = std::make_unique<cp::TestClient>();
		client->enterWorld(f);
		(*client)->clearSent();
	}

	void TearDown() override {
		if (f.player)
			f.player->setClientConnection(nullptr);
		client.reset();
		items.clear();
		f = {};
		InWorldPacketTest::TearDown();
		dataholders::DataManager::ITEM_CLEAN_UP.resetForTests();
		dataholders::DataManager::ITEM_DATA.resetForTests();
	}

	model::gameobjects::player::Player& player() { return *f.player; }

	model::items::storage::Storage& storage(StorageType type) { return *player().getStorage(model::items::storage::getId(type)); }

	/** An item of the template, loaded into the storage of `location` the way the DAO does (onLoadHandler: no packet) */
	Item& stored(int32_t objId, int32_t itemId, int64_t count, StorageType location = StorageType::CUBE, int64_t slot = 0) {
		Ref<Item> item = loadedItem(objId, itemId, count, location, slot);
		storage(location).onLoadHandler(*item);
		items.push_back(item);
		return *item;
	}

	/** An item that is in no storage */
	Item& loose(int32_t objId, int32_t itemId, int64_t count, StorageType location = StorageType::CUBE) {
		Ref<Item> item = loadedItem(objId, itemId, count, location);
		items.push_back(item);
		return *item;
	}

	std::vector<std::vector<uint8_t>> sent() { return (*client)->sentBytes(); }

	void clearSent() { (*client)->clearSent(); }

	std::vector<uint8_t> serialized(network::aion::AionServerPacket&& packet) { return cp::serialized(std::move(packet), client->con()); }

	runtime::DeterministicExecutor* executor = nullptr;
	cp::PlayerFixture f;
	std::unique_ptr<cp::TestClient> client;
	std::vector<Ref<Item>> items;
};

} // namespace aion::gameserver::services::item::test
