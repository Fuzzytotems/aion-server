#pragma once

// Shared fixture of the M5b-3 loot tests (m5b3-plan.md L-05): DropRegistrationService (L-01) and DropService's solo path (L-02) driven against
// real Players, real Npcs of a small test world and the rows of the shipped static data.
//
// - The players are tests/cm_ak's (InWorldPacketRunSupport.h, included by relative path as tests/itemsvc and tests/quest do): the real
//   PlayerGameStats that calculateBoostDropRate asks for BOOST_DROP_RATE / DR_BOOST (Player::getGameStats narrows to it) and a TestClient whose
//   send queue holds every packet the services send.
// - calculateBoostDropRate also asks Player::getActiveHouse, which loads the houses through HousingService, i.e. the database: every case that
//   runs createDropModifiers or registerDrop starts with DROP_REQUIRE_DATABASE() (the economy test database of EconomyTestSupport.h, an empty
//   HOUSE_DATA, so no player owns a house).
// - The static data is published once per process and never reset, because the World singleton keeps pointers into the world map templates
//   and a drop item into its item template (the note of tests/ai/AiWorldTestSupport.h): ctest runs each case in its own process, and a
//   manual run of several cases in one process sees the same data.
// - Every row below is copied from the shipped data of the Java tree (game-server/data/static_data), file:line beside it. Item templates are
//   their start tags only (the child elements - actions, modifiers - are read by no loot path); npc templates leave out <equipment>, which no
//   case reads. The two rules whose restriction no shipped rule uses (gd_tribes, gd_excluded_npcs: 0 of the 2,196 rules) are marked as fixture
//   rules; they are bound beside the shipped ones, never published.

#include "../cm_ak/InWorldPacketRunSupport.h"
#include "EconomyTestSupport.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/CustomDrop.bind.h"
#include "aion/gameserver/dataholders/CustomDrop.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GlobalDropData.bind.h"
#include "aion/gameserver/dataholders/GlobalDropData.h"
#include "aion/gameserver/dataholders/GlobalNpcExclusionData.bind.h"
#include "aion/gameserver/dataholders/GlobalNpcExclusionData.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/drop/DropModifiers.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/globaldrops/GlobalRule.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/services/drop/DropService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

namespace aion::gameserver::economy::test {

namespace cp = network::aion::clientpackets::testing;

inline constexpr int32_t SANCTUM = 110010000;
inline constexpr int32_t POETA = 210010000;
inline constexpr int32_t ELTNEN = 210020000;
inline constexpr int32_t INGGISON = 210050000;
inline constexpr int32_t CYGNEA = 210070000;
inline constexpr int32_t ISHALGEN = 220010000;
/** instance maps the World of this fixture does not create: an npc there has a position (its map id) and no map instance */
inline constexpr int32_t DREDGION = 300110000;
inline constexpr int32_t TALOCS_HOLLOW = 300190000;

inline constexpr int32_t JUVENILE_SPARKIE = 210663; // level 2, BEAST, SPAKY, DISCIPLINED/NORMAL: the M5b gate's monster
inline constexpr int32_t STRIPED_KERUB = 210133;    // level 1, MAGICALMONSTER, CHERUBIM, DISCIPLINED/NORMAL: the kinah monster
inline constexpr int32_t SAENDUKAL = 211040;        // level 40, KRALL, EXPERT/LEGENDARY
inline constexpr int32_t DATURUM_WRECKHELM = 218082; // level 65, MAGICALMONSTER, EXPERT/HERO, named in "Kahruns Symbol"'s gd_npcs
inline constexpr int32_t SALT_FIN_CELLATU = 214253;  // level 48, BEAST, CELLATU
inline constexpr int32_t LATRI = 203894;             // level 20, ELYOS, tribe and type GENERAL (excluded), NOVICE/JUNK
inline constexpr int32_t SIEGE_WEAPON = 201054;      // level 40, SEASONED/ELITE
inline constexpr int32_t FULLA = 203064;             // level 45, VETERAN/ELITE
inline constexpr int32_t PROTECTOR_DEIMOS = 203338;  // level 43, MASTER/HERO
inline constexpr int32_t BIG_CARGO_BOX = 210524;     // level 17, group_drop FAKEBOX: a chest by its drop group
inline constexpr int32_t CASTLE_GUARD = 250061;      // level 1, abyss_type GUARD
inline constexpr int32_t ELROCO = 210338;            // level 1, Poeta, listed in global_npc_exclusions.xml's npc_ids
inline constexpr int32_t HIGH_PRIEST_YATRI = 212308;  // level 47, NAGA, VETERAN/HERO: a kinah count the float rank x rating product decides
inline constexpr int32_t JEBAL = 278002;              // level 58, ASMODIANS, MASTER/HERO: the same, and a one-word name
inline constexpr int32_t COMMANDER_ASHUTANG = 212266; // level 44, LIZARDMAN, EXPERT/HERO: a kinah count a float factor would change
inline constexpr int32_t FARM_WORKER = 203051;        // level 2, type GENERAL (excluded), tribe FARMER_HKERUBIM_LF1 (not excluded)
inline constexpr int32_t CASTLE_DEFENSE_NAMED = 250102; // level 1, abyss_type DEFENDER
inline constexpr int32_t AETHERIC_FIELD_ENGINEER = 250076; // level 1, abyss_type SHIELDNPC_ON (excluded), type ABYSS_GUARD, tribe GUARD

inline constexpr int32_t KINAH = 182400001;
inline constexpr int32_t MINOR_RALLY_SERUM = 160003551;
inline constexpr int32_t MINOR_FOCUS_AGENT = 160003557;
inline constexpr int32_t LESSER_RALLY_SERUM = 160003552;
inline constexpr int32_t LESSER_FOCUS_AGENT = 160003558;
inline constexpr int32_t GREATER_RALLY_SERUM = 160003554;
inline constexpr int32_t GREATER_FOCUS_AGENT = 160003560;
inline constexpr int32_t MINOR_POWER_SHARD = 169000003;
inline constexpr int32_t OMEGA_ENCHANTMENT_STONE = 166020000;
inline constexpr int32_t KERUB_SCALE_FRAGMENT = 182003943;
inline constexpr int32_t SPARKIE_CARAPACE_FRAGMENT = 182004793;
inline constexpr int32_t KAHRUNS_SYMBOL = 186000143;
inline constexpr int32_t NOBLE_DRACONUTE_ARMOR = 182003802;
inline constexpr int32_t CERAMIUM_FRAGMENT = 152012578;
inline constexpr int32_t BRONZE_COIN = 186000002;
inline constexpr int32_t ASMODIAN_BRONZE_COIN = 186000007; // "Bronze Coin", cName coin_d_02: the Asmodian twin of BRONZE_COIN
inline constexpr int32_t CELLATU_MEAT = 152015011;
inline constexpr int32_t NAMUS_DIARY = 182200214; // mask 20545: LIMIT_ONE (ItemMask.java:7), a Poeta quest item of rules_map_poeta.xml
inline constexpr int32_t MOTTLED_EGG = 186000175; // "[Event] Mottled Egg", the item of the Easter event's drop rule

/** world_maps.xml:3, :11, :12, :15, :17, :20 */
inline constexpr const char* DROP_WORLD_MAPS_XML =
	R"(<world_maps>)"
	R"(<map id="110010000" cName="LC1" name="Sanctum" name_id="400437" water_level="16" death_level="400" world_type="ELYSEA" world_size="3072" drop_type="NONE" flags="RECALL GLIDE RIDE PVP DUEL_SAME_RACE" pve_attack_ratio="150" pve_defend_ratio="50"/>)"
	R"(<map id="210010000" cName="LF1" name="Poeta" name_id="400234" twin_count="5" beginner_twin_count="6" max_user="200" water_level="100" death_level="0" world_type="ELYSEA" world_size="3072" drop_type="ELYSEA" flags="BIND RECALL GLIDE PVP DUEL_SAME_RACE" pve_attack_ratio="150" pve_defend_ratio="50"/>)"
	R"(<map id="210020000" cName="LF2" name="Eltnen" name_id="400262" beginner_twin_count="5" max_user="200" water_level="6" death_level="0" world_type="ELYSEA" world_size="3072" drop_type="ELYSEA" flags="BIND RECALL GLIDE RIDE PVP DUEL_SAME_RACE" pve_attack_ratio="150" pve_defend_ratio="50"/>)"
	R"(<map id="210050000" cName="LF4" name="Inggison" name_id="401340" water_level="59" death_level="0" world_type="BALAUREA" world_size="3072" drop_type="BALAUREA" flags="BIND RECALL GLIDE RIDE PVP DUEL_SAME_RACE" pve_attack_ratio="150" pve_defend_ratio="50"/>)"
	R"(<map id="210070000" cName="LF5" name="Cygnea" name_id="404616" max_user="200" water_level="1" death_level="0" world_type="BALAUREA" world_size="3072" drop_type="BALAUREA_HIGH" flags="BIND RECALL GLIDE RIDE PVP DUEL_SAME_RACE"/>)"
	R"(<map id="220010000" cName="DF1" name="Ishalgen" name_id="400259" twin_count="5" beginner_twin_count="6" max_user="200" water_level="248" death_level="0" world_type="ASMODAE" world_size="3072" drop_type="ASMODAE" flags="BIND RECALL GLIDE PVP DUEL_SAME_RACE" pve_attack_ratio="150" pve_defend_ratio="50"/>)"
	R"(</world_maps>)";

inline constexpr const char* DROP_NPC_TEMPLATES_XML =
	R"(<npc_templates>)"
	// npc_templates.xml:57809-57814
	R"(<npc_template npc_id="210663" level="2" name="juvenile sparkie" name_id="301022" height="1.02" group_drop="SPAKY" rank="DISCIPLINED" rating="NORMAL" race="BEAST" tribe="MONSTER" ai="aggressive" srange="8" sangle="270" arange="2" attack_speed="2142" hpgauge="3" floatcorpse="true"><stats maxHp="199"><speeds walk="2" group_walk="2" run="7" run_fight="5" group_run_fight="7" /></stats><bound_radius front="0.55" side="0.56" upper="2.82" /></npc_template>)"
	// npc_templates.xml:54505-54510
	R"(<npc_template npc_id="210133" level="1" name="striped kerub" name_id="300110" height="1.372" group_drop="CHERUBIM" rank="DISCIPLINED" rating="NORMAL" race="MAGICALMONSTER" tribe="MONSTER" type="MONSTER" ai="aggressive" srange="7" sangle="240" arange="2" attack_speed="2100" hpgauge="3"><stats maxHp="143"><speeds walk="0.6" group_walk="0.6" run="7" run_fight="5.5" group_run_fight="7" /></stats><bound_radius front="0.525" side="0.275" upper="1.372" /></npc_template>)"
	// npc_templates.xml:60600-60608 (without <equipment>)
	R"(<npc_template npc_id="211040" level="40" name="grand chieftain saendukal" name_id="301359" height="5.25" group_drop="ORCSHANDURA" rank="EXPERT" rating="LEGENDARY" race="KRALL" tribe="KRALL" type="MONSTER" ai="aggressive" srange="15" sangle="240" arange="2" attack_speed="2625" hpgauge="26" cancel_level="0"><stats maxHp="2689332"><speeds walk="1.44" group_walk="1.44" run="7" run_fight="14" group_run_fight="7" /></stats><bound_radius front="1.5" side="1.5" upper="5.25" /></npc_template>)"
	// npc_templates.xml:117172-117177
	R"(<npc_template npc_id="218082" level="65" name="daturum wreckhelm" name_id="324108" height="13.2" title_id="314452" group_drop="NEPILIM" rank="EXPERT" rating="HERO" race="MAGICALMONSTER" tribe="LDF4A_NEPILIM" ai="aggressive" srange="10" sangle="270" arange="3" attack_speed="2756" hpgauge="20" cancel_level="0"><stats maxHp="1954800" msup="147"><speeds walk="2.63" group_walk="2.63" run="8" run_fight="10" group_run_fight="8" /></stats><bound_radius front="6.4" side="3.8" upper="13.2" /></npc_template>)"
	// npc_templates.xml:85730-85735
	R"(<npc_template npc_id="214253" level="48" name="salt fin cellatu" name_id="317365" height="2.88" group_drop="CELLATU" rank="DISCIPLINED" rating="NORMAL" race="BEAST" tribe="AGGRESSIVESINGLEMONSTER" ai="aggressive" srange="8" sangle="240" arange="2" attack_speed="2040" hpgauge="3"><stats maxHp="10865"><speeds walk="1.45" group_walk="1.125" run="7" run_fight="7" group_run_fight="7" /></stats><bound_radius front="1.26" side="0.9" upper="2.88" /></npc_template>)"
	// npc_templates.xml:12005-12020 (without <equipment>)
	R"(<npc_template npc_id="203894" level="20" name="latri" name_id="351407" height="2" title_id="350613" group_drop="LIGHT" rank="NOVICE" rating="JUNK" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="10" sangle="300" arange="2" attack_speed="2000" hpgauge="1"><stats maxHp="888"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:319-324
	R"(<npc_template npc_id="201054" level="40" name="siege weapon" name_id="392028" height="4.32" group_drop="SIEGEWEAPON" rank="SEASONED" rating="ELITE" race="ELEMENTAL" tribe="PET" type="ABYSS_GUARD" ai="siege_weapon" srange="15" sangle="240" attack_speed="2000" hpgauge="12" cancel_level="30"><stats maxHp="41191" attack="0" pdef="647" mresist="814" accuracy="1013" macc="852" pcrit="18" mcrit="18" evasion="1013" parry="0"><speeds walk="2" group_walk="1.5" run="8" run_fight="8" group_run_fight="6" /></stats><bound_radius front="2.48" side="1.28" upper="4.32" /></npc_template>)"
	// npc_templates.xml:2057-2066 (without <equipment>)
	R"(<npc_template npc_id="203064" level="45" name="fulla" name_id="351007" height="1.8" title_id="350412" group_drop="NONE" rank="VETERAN" rating="ELITE" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" arange="10" attack_speed="2000" hpgauge="14" cancel_level="20"><stats maxHp="108866"><speeds walk="2.1" group_walk="2.1" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius front="0.25" side="0.35" upper="1.8" /><talk_info distance="5" is_dialog="true" func_dialogs="35" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:5871-5883 (without <equipment>)
	R"(<npc_template npc_id="203338" level="43" name="protector deimos" name_id="351858" height="2.4" group_drop="LIGHT" rank="MASTER" rating="HERO" race="ELYOS" tribe="GUARD" type="ABYSS_GUARD" ai="simple_abyssguard" srange="10" arange="37" attack_speed="2300" hpgauge="22" cancel_level="0"><stats maxHp="1261743" /><bound_radius front="0.3" side="0.42" upper="2.4" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:56745-56748
	R"(<npc_template npc_id="210524" level="17" name="big cargo box" name_id="300663" height="0.6" group_drop="FAKEBOX" rank="DISCIPLINED" rating="NORMAL" race="CONSTRUCT" tribe="MONSTER" ai="aggressive" srange="8" sangle="240" arange="2" attack_speed="2415" hpgauge="3" floatcorpse="true"><stats maxHp="23" /><bound_radius front="0.43" side="0.43" upper="1.4" /></npc_template>)"
	// npc_templates.xml:199236-199249 (without <equipment>)
	R"(<npc_template npc_id="250061" level="1" name="castle guard npc  elyos level 1" name_id="301485" height="3" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GUARD" type="ABYSS_GUARD" abyss_type="GUARD" ai="aggressive" srange="10" sangle="270" arange="2" attack_speed="2100" cast_speed="100" hpgauge="3"><stats maxHp="3981"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4" group_run_fight="4.2" /></stats><bound_radius front="0.375" side="0.525" upper="3" /></npc_template>)"
	// npc_templates.xml:55404-55409
	R"(<npc_template npc_id="210338" level="1" name="elroco" name_id="300429" height="0.42" group_drop="MINX" rank="NOVICE" rating="NORMAL" race="BEAST" tribe="MINX_HZAIF" ai="aggressive" srange="8" sangle="270" arange="2" attack_speed="2142" hpgauge="2"><stats maxHp="5"><speeds walk="1.132" group_walk="1.132" run="6" run_fight="5" group_run_fight="6" /></stats><bound_radius front="0.25" side="0.75" upper="0.42" /></npc_template>)"
	// npc_templates.xml:70122-70130 (without <equipment>)
	R"(<npc_template npc_id="212308" level="47" name="high priest yatri" name_id="305120" height="4.25" title_id="350637" group_drop="NAGACLERIC" rank="VETERAN" rating="HERO" race="NAGA" tribe="NNAGA_PRIESTBOSS" ai="aggressive" srange="12" sangle="240" arange="2" attack_speed="2448" hpgauge="21" cancel_level="0"><stats maxHp="706005"><speeds walk="1" group_walk="1" run="7" run_fight="14" group_run_fight="7" /></stats><bound_radius front="1.7" side="3.23" upper="4.25" /></npc_template>)"
	// npc_templates.xml:336543-336557 (without <equipment>)
	R"(<npc_template npc_id="278002" level="58" name="jebal" name_id="313002" height="2.4" title_id="314304" group_drop="DARK" rank="MASTER" rating="HERO" race="ASMODIANS" tribe="GUARD_DARK" type="ABYSS_GUARD" ai="simple_abyssguard" srange="10" arange="4" attack_speed="2100" hpgauge="22" cancel_level="0"><stats maxHp="3214671"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius front="0.3" side="0.42" upper="2.4" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:69758-69767 (without <equipment>)
	R"(<npc_template npc_id="212266" level="44" name="commander ashutang" name_id="305078" height="1.8" title_id="350637" group_drop="LIZARDMANASSASSIN" rank="EXPERT" rating="HERO" race="LIZARDMAN" tribe="NLIZARDMAN" ai="aggressive" srange="12" sangle="240" arange="2" attack_speed="2280" hpgauge="20" cancel_level="0"><stats maxHp="108540"><speeds walk="1.76" group_walk="1.76" run="7" run_fight="14" group_run_fight="7" /></stats><bound_radius front="0.9425" side="2.1125" upper="1.8" /></npc_template>)"
	// npc_templates.xml:1870-1884 (without <equipment>)
	R"(<npc_template npc_id="203051" level="2" name="farm worker" name_id="351127" height="2" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="FARMER_HKERUBIM_LF1" type="GENERAL" ai="general" srange="10" sangle="355" arange="2" attack_speed="2100" hpgauge="3"><stats maxHp="28396"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius front="0.25" side="0.35" upper="2" /></npc_template>)"
	// npc_templates.xml:199695-199708 (without <equipment>)
	R"(<npc_template npc_id="250102" level="1" name="castle defense special named - elyos" name_id="301527" height="4" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GUARD" type="ABYSS_GUARD" abyss_type="DEFENDER" ai="aggressive" srange="10" sangle="240" arange="2" attack_speed="2100" hpgauge="3"><stats maxHp="3981"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4" group_run_fight="4.2" /></stats><bound_radius front="0.5" side="0.7" upper="4" /></npc_template>)"
	// npc_templates.xml:199421-199434 (without <equipment>)
	R"(<npc_template npc_id="250076" level="1" name="aetheric field engineer npc - pvpon, elyos only" name_id="301500" height="3" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GUARD" type="ABYSS_GUARD" abyss_type="SHIELDNPC_ON" ai="siege_shieldnpc" srange="10" sangle="240" arange="2" attack_speed="2100" hpgauge="3"><stats maxHp="3981"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4" group_run_fight="4.2" /></stats><bound_radius front="0.375" side="0.525" upper="3" /><talk_info distance="5" can_talk_invisible="false" /></npc_template>)"
	R"(</npc_templates>)";

/** Every gd_item of the rules below: the start tags of item_templates.xml */
inline constexpr const char* DROP_ITEM_TEMPLATES_XML =
	R"(<item_templates>)"
	R"(<item_template id="152012578" name="Ceramium Fragment" level="61" cName="LDF5ab_all_material_U_61a" mask="12414" max_stack_count="1000" quality="UNIQUE" price="7936" desc="812471"/>)" // item_templates.xml:744688
	R"(<item_template id="152015011" name="Cellatu Meat" level="50" cName="food_material_50e" mask="12414" max_stack_count="1000" quality="COMMON" price="300" race="ELYOS" desc="743898"/>)" // item_templates.xml:744841
	R"(<item_template id="160003551" name="Minor Rally Serum" level="10" cName="shop_food_2stat_10a" mask="12414" max_stack_count="1000" quality="COMMON" price="200" desc="770106" activate_target="STANDALONE" activate_count="1"/>)" // item_templates.xml:827571
	R"(<item_template id="160003552" name="Lesser Rally Serum" level="20" cName="shop_food_2stat_20a" mask="12414" max_stack_count="1000" quality="COMMON" price="300" desc="770107" activate_target="STANDALONE" activate_count="1"/>)" // item_templates.xml:827577
	R"(<item_template id="160003553" name="Rally Serum" level="30" cName="shop_food_2stat_30a" mask="12414" max_stack_count="1000" quality="COMMON" price="600" desc="770108" activate_target="STANDALONE" activate_count="1"/>)" // item_templates.xml:827583
	R"(<item_template id="160003554" name="Greater Rally Serum" level="40" cName="shop_food_2stat_40a" mask="12414" max_stack_count="1000" quality="COMMON" price="1000" desc="770109" activate_target="STANDALONE" activate_count="1"/>)" // item_templates.xml:827589
	R"(<item_template id="160003555" name="Major Rally Serum" level="50" cName="shop_food_2stat_50a" mask="12414" max_stack_count="1000" quality="COMMON" price="1500" desc="770110" activate_target="STANDALONE" activate_count="1"/>)" // item_templates.xml:827595
	R"(<item_template id="160003556" name="Fine Rally Serum" level="60" cName="shop_food_2stat_60a" mask="12414" max_stack_count="1000" quality="COMMON" price="2000" restrict="50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50" desc="770111" activate_target="STANDALONE" activate_count="1"/>)" // item_templates.xml:827601
	R"(<item_template id="160003557" name="Minor Focus Agent" level="10" cName="shop_food_2stat_10b" mask="12414" max_stack_count="1000" quality="COMMON" price="200" desc="770112" activate_target="STANDALONE" activate_count="1"/>)" // item_templates.xml:827607
	R"(<item_template id="160003558" name="Lesser Focus Agent" level="20" cName="shop_food_2stat_20b" mask="12414" max_stack_count="1000" quality="COMMON" price="300" desc="770113" activate_target="STANDALONE" activate_count="1"/>)" // item_templates.xml:827613
	R"(<item_template id="160003559" name="Focus Agent" level="30" cName="shop_food_2stat_30b" mask="12414" max_stack_count="1000" quality="COMMON" price="600" desc="770114" activate_target="STANDALONE" activate_count="1"/>)" // item_templates.xml:827619
	R"(<item_template id="160003560" name="Greater Focus Agent" level="40" cName="shop_food_2stat_40b" mask="12414" max_stack_count="1000" quality="COMMON" price="1000" desc="770115" activate_target="STANDALONE" activate_count="1"/>)" // item_templates.xml:827625
	R"(<item_template id="160003561" name="Major Focus Agent" level="50" cName="shop_food_2stat_50b" mask="12414" max_stack_count="1000" quality="COMMON" price="1500" desc="770116" activate_target="STANDALONE" activate_count="1"/>)" // item_templates.xml:827631
	R"(<item_template id="160003562" name="Fine Focus Agent" level="60" cName="shop_food_2stat_60b" mask="12414" max_stack_count="1000" quality="COMMON" price="2000" restrict="50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50" desc="770117" activate_target="STANDALONE" activate_count="1"/>)" // item_templates.xml:827637
	R"(<item_template id="166020000" name="Omega Enchantment Stone" level="65" cName="matter_enchant_exceed_01" mask="12414" max_stack_count="100" item_group="ENCHANTMENT" quality="MYTHIC" price="885" desc="841577" activate_count="1"/>)" // item_templates.xml:838027
	R"(<item_template id="169000003" name="Minor Power Shard" level="1" cName="battery_01" mask="12414" max_stack_count="10000" item_group="POWER_SHARDS" quality="COMMON" price="5" desc="701811" weapon_boost="10"/>)" // item_templates.xml:848722
	R"(<item_template id="169000004" name="Lesser Power Shard" level="10" cName="battery_10" mask="12414" max_stack_count="10000" item_group="POWER_SHARDS" quality="COMMON" price="15" desc="701812" weapon_boost="15"/>)" // item_templates.xml:848723
	R"(<item_template id="169000005" name="Power Shard" level="20" cName="battery_20" mask="12414" max_stack_count="10000" item_group="POWER_SHARDS" quality="COMMON" price="45" desc="701813" weapon_boost="20"/>)" // item_templates.xml:848724
	R"(<item_template id="169000007" name="Greater Power Shard" level="30" cName="battery_30" mask="12414" max_stack_count="10000" item_group="POWER_SHARDS" quality="COMMON" price="85" desc="701814" weapon_boost="25"/>)" // item_templates.xml:848726
	R"(<item_template id="169000008" name="Fine Power Shard" level="40" cName="battery_40" mask="12414" max_stack_count="10000" item_group="POWER_SHARDS" quality="COMMON" price="145" desc="701815" weapon_boost="30"/>)" // item_templates.xml:848727
	R"(<item_template id="169000009" name="Premium Power Shard" level="50" cName="battery_50" mask="12414" max_stack_count="10000" item_group="POWER_SHARDS" quality="COMMON" price="220" desc="701816" weapon_boost="35"/>)" // item_templates.xml:848728
	R"(<item_template id="169000010" name="Superior Power Shard" level="60" cName="battery_60" mask="12414" max_stack_count="10000" item_group="POWER_SHARDS" quality="COMMON" price="367" desc="812379" weapon_boost="40"/>)" // item_templates.xml:848729
	R"(<item_template id="182003802" name="Noble Draconute Armor" level="50" cName="junk_d3_lizardman_50" mask="12414" max_stack_count="1000" quality="JUNK" price="6600" desc="715670"/>)" // item_templates.xml:873147
	R"(<item_template id="182003943" name="Kerub Scale Fragment" level="5" cName="junk_cherubim1_05" mask="12414" max_stack_count="1000" quality="JUNK" price="300" desc="717868"/>)" // item_templates.xml:873288
	R"(<item_template id="182003944" name="Tattered Kerub Scale" level="10" cName="junk_cherubim1_10" mask="12414" max_stack_count="1000" quality="JUNK" price="600" desc="717869"/>)" // item_templates.xml:873289
	R"(<item_template id="182003945" name="Thin Kerub Scale" level="15" cName="junk_cherubim1_15" mask="12414" max_stack_count="1000" quality="JUNK" price="1000" desc="717870"/>)" // item_templates.xml:873290
	R"(<item_template id="182003946" name="Scratched Kerub Scale" level="20" cName="junk_cherubim1_20" mask="12414" max_stack_count="1000" quality="JUNK" price="1500" desc="717871"/>)" // item_templates.xml:873291
	R"(<item_template id="182003947" name="Kerub Scale" level="25" cName="junk_cherubim1_25" mask="12414" max_stack_count="1000" quality="JUNK" price="2100" desc="717872"/>)" // item_templates.xml:873292
	R"(<item_template id="182003948" name="Intact Kerub Scale" level="30" cName="junk_cherubim1_30" mask="12414" max_stack_count="1000" quality="JUNK" price="2800" desc="717873"/>)" // item_templates.xml:873293
	R"(<item_template id="182003949" name="Thick Kerub Scale" level="35" cName="junk_cherubim1_35" mask="12414" max_stack_count="1000" quality="JUNK" price="3600" desc="717874"/>)" // item_templates.xml:873294
	R"(<item_template id="182003950" name="Glossy Kerub Scale" level="40" cName="junk_cherubim1_40" mask="12414" max_stack_count="1000" quality="JUNK" price="4500" desc="717875"/>)" // item_templates.xml:873295
	R"(<item_template id="182003951" name="Hazy Kerub Scale" level="45" cName="junk_cherubim1_45" mask="12414" max_stack_count="1000" quality="JUNK" price="5500" desc="717876"/>)" // item_templates.xml:873296
	R"(<item_template id="182003952" name="Noble Kerub Scale" level="50" cName="junk_cherubim1_50" mask="12414" max_stack_count="1000" quality="JUNK" price="6600" desc="717877"/>)" // item_templates.xml:873297
	R"(<item_template id="182004793" name="Sparkie Carapace Fragment" level="5" cName="junk_spaky_05" mask="12414" max_stack_count="1000" quality="JUNK" price="300" desc="718718"/>)" // item_templates.xml:874138
	R"(<item_template id="182004794" name="Tattered Sparkie Carapace" level="10" cName="junk_spaky_10" mask="12414" max_stack_count="1000" quality="JUNK" price="600" desc="718719"/>)" // item_templates.xml:874139
	R"(<item_template id="182004795" name="Thin Sparkie Carapace" level="15" cName="junk_spaky_15" mask="12414" max_stack_count="1000" quality="JUNK" price="1000" desc="718720"/>)" // item_templates.xml:874140
	R"(<item_template id="182004796" name="Scratched Sparkie Carapace" level="20" cName="junk_spaky_20" mask="12414" max_stack_count="1000" quality="JUNK" price="1500" desc="718721"/>)" // item_templates.xml:874141
	R"(<item_template id="182004797" name="Sparkie Carapace" level="25" cName="junk_spaky_25" mask="12414" max_stack_count="1000" quality="JUNK" price="2100" desc="718722"/>)" // item_templates.xml:874142
	R"(<item_template id="182004798" name="Intact Sparkie Carapace" level="30" cName="junk_spaky_30" mask="12414" max_stack_count="1000" quality="JUNK" price="2800" desc="718723"/>)" // item_templates.xml:874143
	R"(<item_template id="182004799" name="Thick Sparkie Carapace" level="35" cName="junk_spaky_35" mask="12414" max_stack_count="1000" quality="JUNK" price="3600" desc="718724"/>)" // item_templates.xml:874144
	R"(<item_template id="182004800" name="Glossy Sparkie Carapace" level="40" cName="junk_spaky_40" mask="12414" max_stack_count="1000" quality="JUNK" price="4500" desc="718725"/>)" // item_templates.xml:874145
	R"(<item_template id="182004801" name="Hazy Sparkie Carapace" level="45" cName="junk_spaky_45" mask="12414" max_stack_count="1000" quality="JUNK" price="5500" desc="718726"/>)" // item_templates.xml:874146
	R"(<item_template id="182004802" name="Noble Sparkie Carapace" level="50" cName="junk_spaky_50" mask="12414" max_stack_count="1000" quality="JUNK" price="6600" desc="718727"/>)" // item_templates.xml:874147
	R"(<item_template id="182005581" name="Beautiful Kerub Scale" level="55" cName="junk_cherubim1_55" mask="12414" max_stack_count="1000" quality="JUNK" price="7800" desc="752038"/>)" // item_templates.xml:874926
	R"(<item_template id="182005582" name="Rainbow Kerub Scale" level="60" cName="junk_cherubim1_60" mask="12414" max_stack_count="1000" quality="JUNK" price="9100" desc="752039"/>)" // item_templates.xml:874927
	R"(<item_template id="182005751" name="Beautiful Sparkie Carapace" level="55" cName="junk_spaky_55" mask="12414" max_stack_count="1000" quality="JUNK" price="7800" desc="752208"/>)" // item_templates.xml:875096
	R"(<item_template id="182005752" name="Mutated Sparkie Carapace" level="60" cName="junk_spaky_60" mask="12414" max_stack_count="1000" quality="JUNK" price="9100" desc="752209"/>)" // item_templates.xml:875097
	R"(<item_template id="182005959" name="Taloc's Root Fragment" level="55" cName="junk_idelim3f_55" mask="12414" max_stack_count="1000" quality="JUNK" price="3900" desc="760530"/>)" // item_templates.xml:875304
	R"(<item_template id="182200214" name="Namus's Diary" level="1" cName="quest_1114a" mask="20545" item_group="QUEST" quality="COMMON" price="1" desc="1106419" activate_target="STANDALONE" activate_count="1000"/>)" // item_templates.xml:876974
	R"(<item_template id="182400001" name="Kinah" level="1" cName="gold" mask="12350" quality="COMMON" price="0" desc="701677"/>)" // item_templates.xml:894666
	R"(<item_template id="186000002" name="Bronze Coin" level="20" cName="coin_02" mask="12410" max_stack_count="10000" item_group="COINS" quality="COMMON" price="8208" desc="703679"/>)" // item_templates.xml:896236
	R"(<item_template id="186000007" name="Bronze Coin" level="20" cName="coin_d_02" mask="12410" max_stack_count="10000" item_group="COINS" quality="COMMON" price="8208" desc="704946"/>)" // item_templates.xml:896251
	R"(<item_template id="186000143" name="Kahrun's Symbol" level="1" cName="karon_coin_01" mask="12364" max_stack_count="10000" quality="RARE" price="5" desc="790366"/>)" // item_templates.xml:896546
	R"(<item_template id="186000175" name="[Event] Mottled Egg" level="1" cName="world_event_coin_easter_01" mask="12360" max_stack_count="10000" quality="RARE" price="5" desc="796879"/>)" // item_templates.xml:896632
	R"(</item_templates>)";

/**
 * The published GLOBAL_DROP_DATA of the fixture, seven shipped rules in this order (the order registerDrop evaluates them and numbers the
 * entries in): Kinah, Buff Food, Power Shards, Omega Enchantment Stone, JUNK_CHERUBIM1_MATERIAL, JUNK_SPAKY_MATERIAL, Kahruns Symbol.
 */
inline constexpr const char* DROP_REGISTRATION_RULES_XML =
	R"(<global_rules>)"
	// rules_commons.xml:4-28
	R"(<gd_rule rule_name="Kinah" chance="50" dynamic_chance="true">)"
	R"(<gd_races>)"
	R"(<gd_race race="ELYOS" />)"
	R"(<gd_race race="ASMODIANS" />)"
	R"(<gd_race race="BROWNIE" />)"
	R"(<gd_race race="CONSTRUCT" />)"
	R"(<gd_race race="DEMIHUMANOID" />)"
	R"(<gd_race race="DRAGON" />)"
	R"(<gd_race race="DRAKAN" />)"
	R"(<gd_race race="ELEMENTAL" />)"
	R"(<gd_race race="KRALL" />)"
	R"(<gd_race race="LIZARDMAN" />)"
	R"(<gd_race race="LYCAN" />)"
	R"(<gd_race race="MAGICALMONSTER" />)"
	R"(<gd_race race="NAGA" />)"
	R"(<gd_race race="RATMAN" />)"
	R"(<gd_race race="NAGA" />)"
	R"(<gd_race race="SHULACK" />)"
	R"(<gd_race race="UNDEAD" />)"
	R"(</gd_races>)"
	R"(<gd_items>)"
	R"(<!-- Level 1 -->)"
	R"(<gd_item id="182400001" min_count="5" max_count="25" /><!-- Kinah -->)"
	R"(</gd_items>)"
	R"(</gd_rule>)"
	// rules_commons.xml:30-51
	R"(<gd_rule rule_name="Buff Food" chance="3.5" dynamic_chance="true" min_diff="-9" max_diff="0">)"
	R"(<gd_items>)"
	R"(<!-- Level 10 -->)"
	R"(<gd_item id="160003551" /><!-- Minor Rally Serum -->)"
	R"(<gd_item id="160003557" /><!-- Minor Focus Agent -->)"
	R"(<!-- Level 20 -->)"
	R"(<gd_item id="160003552" /><!-- Lesser Rally Serum -->)"
	R"(<gd_item id="160003558" /><!-- Lesser Focus Agent -->)"
	R"(<!-- Level 30 -->)"
	R"(<gd_item id="160003553" /><!-- Rally Serum -->)"
	R"(<gd_item id="160003559" /><!-- Focus Agent -->)"
	R"(<!-- Level 40 -->)"
	R"(<gd_item id="160003554" /><!-- Greater Rally Serum -->)"
	R"(<gd_item id="160003560" /><!-- Greater Focus Agent -->)"
	R"(<!-- Level 50 -->)"
	R"(<gd_item id="160003555" /><!-- Major Rally Serum -->)"
	R"(<gd_item id="160003561" /><!-- Major Focus Agent -->)"
	R"(<!-- Level 60 -->)"
	R"(<gd_item id="160003556" /><!-- Fine Rally Serum -->)"
	R"(<gd_item id="160003562" /><!-- Fine Focus Agent -->)"
	R"(</gd_items>)"
	R"(</gd_rule>)"
	// rules_commons.xml:169-190
	R"(<gd_rule rule_name="Power Shards" chance="6" dynamic_chance="true" min_diff="0" max_diff="9">)"
	R"(<gd_ratings>)"
	R"(<gd_rating rating="NORMAL" />)"
	R"(<gd_rating rating="ELITE" />)"
	R"(</gd_ratings>)"
	R"(<gd_items>)"
	R"(<!-- Level 1 -->)"
	R"(<gd_item id="169000003" min_count="2" max_count="15" /><!-- Minor Power Shard -->)"
	R"(<!-- Level 10 -->)"
	R"(<gd_item id="169000004" min_count="2" max_count="15" /><!-- Lesser Power Shard -->)"
	R"(<!-- Level 20 -->)"
	R"(<gd_item id="169000005" min_count="2" max_count="15" /><!-- Power Shard -->)"
	R"(<!-- Level 30 -->)"
	R"(<gd_item id="169000007" min_count="2" max_count="15" /><!-- Greater Power Shard -->)"
	R"(<!-- Level 40 -->)"
	R"(<gd_item id="169000008" min_count="2" max_count="15" /><!-- Fine Power Shard -->)"
	R"(<!-- Level 50 -->)"
	R"(<gd_item id="169000009" min_count="2" max_count="15" /><!-- Premium Power Shard -->)"
	R"(<!-- Level 60 -->)"
	R"(<gd_item id="169000010" min_count="2" max_count="15" /><!-- Superior Power Shard -->)"
	R"(</gd_items>)"
	R"(</gd_rule>)"
	// rules_commons.xml:676-692
	R"(<gd_rule rule_name="Omega Enchantment Stone" chance="1" level_based_chance_reduction="true">)"
	R"(<gd_ratings>)"
	R"(<gd_rating rating="NORMAL" />)"
	R"(<gd_rating rating="ELITE" />)"
	R"(<gd_rating rating="HERO" />)"
	R"(<gd_rating rating="LEGENDARY" />)"
	R"(</gd_ratings>)"
	R"(<gd_worlds>)"
	R"(<gd_world wd_type="BALAUREA" />)"
	R"(<gd_world wd_type="BALAUREA_HIGH" />)"
	R"(<gd_world wd_type="BALAUREA_HIGH_INSTANCE" />)"
	R"(<gd_world wd_type="BALAUREA_INSTANCE" />)"
	R"(</gd_worlds>)"
	R"(<gd_items>)"
	R"(<gd_item id="166020000" /><!-- Omega Enchantment Stone -->)"
	R"(</gd_items>)"
	R"(</gd_rule>)"
	// rules_junk_materials.xml:427-445
	R"(<gd_rule rule_name="JUNK_CHERUBIM1_MATERIAL" chance="40" min_diff="-4" max_diff="5">)"
	R"(<gd_items>)"
	R"(<gd_item id="182003943" /><!-- Kerub Scale Fragment -->)"
	R"(<gd_item id="182003944" /><!-- Tattered Kerub Scale -->)"
	R"(<gd_item id="182003945" /><!-- Thin Kerub Scale -->)"
	R"(<gd_item id="182003946" /><!-- Scratched Kerub Scale -->)"
	R"(<gd_item id="182003947" /><!-- Kerub Scale -->)"
	R"(<gd_item id="182003948" /><!-- Intact Kerub Scale -->)"
	R"(<gd_item id="182003949" /><!-- Thick Kerub Scale -->)"
	R"(<gd_item id="182003950" /><!-- Glossy Kerub Scale -->)"
	R"(<gd_item id="182003951" /><!-- Hazy Kerub Scale -->)"
	R"(<gd_item id="182003952" /><!-- Noble Kerub Scale -->)"
	R"(<gd_item id="182005581" /><!-- Beautiful Kerub Scale -->)"
	R"(<gd_item id="182005582" /><!-- Rainbow Kerub Scale -->)"
	R"(</gd_items>)"
	R"(<gd_npc_groups>)"
	R"(<gd_npc_group group="CHERUBIM" />)"
	R"(</gd_npc_groups>)"
	R"(</gd_rule>)"
	// rules_junk_materials.xml:3927-3945
	R"(<gd_rule rule_name="JUNK_SPAKY_MATERIAL" chance="40" min_diff="-4" max_diff="5">)"
	R"(<gd_items>)"
	R"(<gd_item id="182004793" /><!-- Sparkie Carapace Fragment -->)"
	R"(<gd_item id="182004794" /><!-- Tattered Sparkie Carapace -->)"
	R"(<gd_item id="182004795" /><!-- Thin Sparkie Carapace -->)"
	R"(<gd_item id="182004796" /><!-- Scratched Sparkie Carapace -->)"
	R"(<gd_item id="182004797" /><!-- Sparkie Carapace -->)"
	R"(<gd_item id="182004798" /><!-- Intact Sparkie Carapace -->)"
	R"(<gd_item id="182004799" /><!-- Thick Sparkie Carapace -->)"
	R"(<gd_item id="182004800" /><!-- Glossy Sparkie Carapace -->)"
	R"(<gd_item id="182004801" /><!-- Hazy Sparkie Carapace -->)"
	R"(<gd_item id="182004802" /><!-- Noble Sparkie Carapace -->)"
	R"(<gd_item id="182005751" /><!-- Beautiful Sparkie Carapace -->)"
	R"(<gd_item id="182005752" /><!-- Mutated Sparkie Carapace -->)"
	R"(</gd_items>)"
	R"(<gd_npc_groups>)"
	R"(<gd_npc_group group="SPAKY" />)"
	R"(</gd_npc_groups>)"
	R"(</gd_rule>)"
	// rules_sandstorm_targets.xml:17-25
	R"(<gd_rule rule_name="Kahruns Symbol" chance="100" member_limit="6">)"
	R"(<gd_npcs>)"
	R"(<gd_npc npc_id="218082" /><!-- Daturum Wreckhelm -->)"
	R"(<gd_npc npc_id="218080" /><!-- Nubale the Glacier -->)"
	R"(</gd_npcs>)"
	R"(<gd_items>)"
	R"(<gd_item id="186000143" min_count="60" max_count="90" /><!-- Kahrun's Symbol -->)"
	R"(</gd_items>)"
	R"(</gd_rule>)"
	R"(</global_rules>)";

/**
 * The rules of the restriction table (collectDrops, which the table calls directly): one shipped rule per restriction, bound but not published,
 * and the two fixture rules for the restrictions no shipped rule uses.
 */
inline constexpr const char* DROP_RESTRICTION_RULES_XML =
	R"(<global_rules>)"
	// instances/rules_map_dredgion.xml:4-11
	R"(<gd_rule rule_name="Noble Draconute Armor" chance="35">)"
	R"(<gd_maps>)"
	R"(<gd_map map_id="300110000" /><!-- Dredgion -->)"
	R"(</gd_maps>)"
	R"(<gd_items>)"
	R"(<gd_item id="182003802" /><!-- Noble Draconute Armor -->)"
	R"(</gd_items>)"
	R"(</gd_rule>)"
	// rules_crafting_materials.xml:1768-1775
	R"(<gd_rule rule_name="Ceramium Fragment" chance="1.5" dynamic_chance="true">)"
	R"(<gd_worlds>)"
	R"(<gd_world wd_type="BALAUREA_HIGH" />)"
	R"(</gd_worlds>)"
	R"(<gd_items>)"
	R"(<gd_item id="152012578" /><!-- Ceramium Fragment -->)"
	R"(</gd_items>)"
	R"(</gd_rule>)"
	// open_worlds/rules_map_eltnen.xml:4-11
	R"(<gd_rule rule_name="Bronze Coin" chance="5" restriction_race="ELYOS">)"
	R"(<gd_maps>)"
	R"(<gd_map map_id="210020000" /><!-- Eltnen -->)"
	R"(</gd_maps>)"
	R"(<gd_items>)"
	R"(<gd_item id="186000002" /><!-- Bronze Coin -->)"
	R"(</gd_items>)"
	R"(</gd_rule>)"
	// open_worlds/rules_map_morheim.xml:4-11 - the Asmodian twin, with the same name and (as shipped) Eltnen's map id
	R"(<gd_rule rule_name="Bronze Coin" chance="5" restriction_race="ASMODIANS">)"
	R"(<gd_maps>)"
	R"(<gd_map map_id="210020000" /><!-- Eltnen -->)"
	R"(</gd_maps>)"
	R"(<gd_items>)"
	R"(<gd_item id="186000007" /><!-- Bronze Coin -->)"
	R"(</gd_items>)"
	R"(</gd_rule>)"
	// rules_crafting_materials.xml:1381-1392
	R"(<gd_rule rule_name="Cellatu Meat" chance="24" dynamic_chance="true" min_diff="-2" max_diff="2">)"
	R"(<gd_worlds>)"
	R"(<gd_world wd_type="ELYSEA" />)"
	R"(</gd_worlds>)"
	R"(<gd_npc_groups>)"
	R"(<gd_npc_group group="CELLATU" />)"
	R"(</gd_npc_groups>)"
	R"(<gd_items>)"
	R"(<!-- Level 50 -->)"
	R"(<gd_item id="152015011" /><!-- Cellatu Meat -->)"
	R"(</gd_items>)"
	R"(</gd_rule>)"
	// instances/rules_map_talocs_hollow.xml:37-48
	R"(<gd_rule rule_name="Junk Taloc's Root Fragment" chance="40">)"
	R"(<gd_maps>)"
	R"(<gd_map map_id="300190000" /><!-- Taloc's Hollow -->)"
	R"(</gd_maps>)"
	R"(<gd_zones>)"
	R"(<gd_zone zone="TALOCS_HEART_300190000" />)"
	R"(<gd_zone zone="BLIGHTROOT_300190000" />)"
	R"(</gd_zones>)"
	R"(<gd_items>)"
	R"(<gd_item id="182005959" /><!-- Taloc's Root Fragment -->)"
	R"(</gd_items>)"
	R"(</gd_rule>)"
	// fixture rule: "Kahruns Symbol" (rules_sandstorm_targets.xml:17-25) with gd_tribes in place of its gd_npcs - no shipped rule has gd_tribes
	R"(<gd_rule rule_name="Kahruns Symbol, fixture gd_tribes" chance="100">)"
	R"(<gd_tribes><gd_tribe tribe="MONSTER" /></gd_tribes>)"
	R"(<gd_items><gd_item id="186000143" min_count="60" max_count="90" /></gd_items>)"
	R"(</gd_rule>)"
	// fixture rule: the same with gd_excluded_npcs - no shipped rule has gd_excluded_npcs
	R"(<gd_rule rule_name="Kahruns Symbol, fixture gd_excluded_npcs" chance="100">)"
	R"(<gd_excluded_npcs npc_ids="210663 218080" />)"
	R"(<gd_items><gd_item id="186000143" min_count="60" max_count="90" /></gd_items>)"
	R"(</gd_rule>)"
	R"(</global_rules>)";

/**
 * The drop rule of a shipped event (events/timed_events/custom_events.xml:2954-2964, the Easter event's <event_drops>), bound as a rule list: a
 * case puts it into EventService's active event rules, which Event.start would do (Event::start is AION_UNPORTED, services/event/Event.cpp)
 */
inline constexpr const char* DROP_EVENT_RULES_XML =
	R"(<global_rules>)"
	R"(<gd_rule rule_name="Easter Event Drop" chance="15" member_limit="12">)"
	R"(<gd_ratings>)"
	R"(<gd_rating rating="NORMAL" />)"
	R"(<gd_rating rating="ELITE" />)"
	R"(<gd_rating rating="HERO" />)"
	R"(<gd_rating rating="LEGENDARY" />)"
	R"(</gd_ratings>)"
	R"(<gd_items>)"
	R"(<gd_item id="186000175" /><!-- [Event] Mottled Egg -->)"
	R"(</gd_items>)"
	R"(</gd_rule>)"
	R"(</global_rules>)";

/** global_drops/global_npc_exclusions.xml:2-10 */
inline constexpr const char* GLOBAL_NPC_EXCLUSIONS_XML =
	R"(<global_npc_exclusions>)"
	R"(<npc_ids>219501 280603 280604 280605 280606 280607 217301 283001 283000 701413 217243 282604 216951 700835 256694 210338 210342 214803</npc_ids>)"
	R"(<npc_types>SUMMON_PET GENERAL MERCENARY HOUSING</npc_types>)"
	R"(<npc_tribes>PET PET_DARK QUESTGUARD_DARK QUESTGUARD_LIGHT DUMMY DUMMY2 DUMMY2_DGUARD DUMMY2_LGUARD DUMMY_DGUARD DUMMY_LGUARD HOLYSERVANT)"
	R"( HOLYSERVANT_DEBUFFER HOLYSERVANT_DESPAWN USEALL TEST_LIGHT_AETC TEST_DARK_ADRAGON TEST_DARK_AETC TEST_DARK_ALIGHT TEST_DRAGON_ADARK)"
	R"( TEST_DRAGON_AETC TEST_DRAGON_ALIGHT TEST_ETC_ADARK TEST_ETC_ADRAGON TEST_ETC_ALIGHT TEST_LIGHT_ADARK TEST_LIGHT_ADRAGON GENERAL)"
	R"( GENERAL_DARK GENERAL_ADADR GENERAL_DRAGON GENERAL_DARK_LYCAN GENERAL_KRALL FIELD_OBJECT_ALL</npc_tribes>)"
	R"(<npc_abyss_types>DOOR ARTIFACT ARTIFACT_EFFECT_CORE DOORREPAIR SHIELDNPC_OFF SHIELDNPC_ON</npc_abyss_types>)"
	R"(</global_npc_exclusions>)";

/**
 * The load context of the item templates and of the rules: GlobalDropItem.afterUnmarshal finds its item template by XmlID in the context of the
 * load (GlobalDropItem.cpp), as the static data loader binds every file of static_data.xml in one context
 */
inline xml::LoadContext& dropLoadContext() {
	static xml::LoadContext context;
	return context;
}

/** Publishes the fixture's static data once per process (see the header comment: the World and the drop items keep pointers into it) */
inline void publishDropStaticDataOnce();

/** The rules of DROP_RESTRICTION_RULES_XML, bound once per process after the item templates */
inline const dataholders::GlobalDropData& restrictionRules() {
	publishDropStaticDataOnce();
	static const std::unique_ptr<dataholders::GlobalDropData> rules =
		xml::bindString<dataholders::GlobalDropData>(dropLoadContext(), DROP_RESTRICTION_RULES_XML);
	return *rules;
}

/** The rules of DROP_EVENT_RULES_XML, bound once per process after the item templates */
inline const dataholders::GlobalDropData& eventRules() {
	publishDropStaticDataOnce();
	static const std::unique_ptr<dataholders::GlobalDropData> rules =
		xml::bindString<dataholders::GlobalDropData>(dropLoadContext(), DROP_EVENT_RULES_XML);
	return *rules;
}

inline void publishDropStaticDataOnce() {
	static const bool published = [] {
		configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
		configs::main::WorldConfig::WORLD_MAX_TWINS_USUAL.store(0);
		configs::main::WorldConfig::WORLD_MAX_TWINS_BEGINNER.store(0);
		static std::deque<xml::LoadContext> contexts;
		dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(contexts.emplace_back(), DROP_WORLD_MAPS_XML));
		dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(contexts.emplace_back(), "<zones/>"));
		dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(contexts.emplace_back(), "<shields/>"));
		dataholders::DataManager::MATERIAL_DATA.publish(xml::bindString<dataholders::MaterialData>(contexts.emplace_back(), "<material_templates/>"));
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), DROP_NPC_TEMPLATES_XML));
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(dropLoadContext(), DROP_ITEM_TEMPLATES_XML));
		// the item info blob of the storage packets asks it (GeneralInfoBlobEntry); no cleanup entries
		dataholders::DataManager::ITEM_CLEAN_UP.publish(std::make_unique<dataholders::ItemRestrictionCleanupData>());
		dataholders::DataManager::GLOBAL_DROP_DATA.publish(xml::bindString<dataholders::GlobalDropData>(dropLoadContext(), DROP_REGISTRATION_RULES_XML));
		dataholders::DataManager::GLOBAL_EXCLUSION_DATA.publish(
			xml::bindString<dataholders::GlobalNpcExclusionData>(contexts.emplace_back(), GLOBAL_NPC_EXCLUSIONS_XML));
		// custom_drop.xml has no <npc_drop> for any fixture npc
		dataholders::DataManager::CUSTOM_NPC_DROP.publish(xml::bindString<dataholders::CustomDrop>(contexts.emplace_back(), "<custom_drop/>"));
		dataholders::DataManager::HOUSE_DATA.publish(xml::bindString<dataholders::HouseData>(contexts.emplace_back(), "<house_lands/>"));
		// a player seeing an npc asks the tribe relations (the free-for-all cases pair them): the GENERAL and MONSTER rows,
		// tribe_relations.xml:719-721 and :2170-2173
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(xml::bindString<dataholders::TribeRelationsData>(contexts.emplace_back(),
			R"(<tribe_relations><tribe name="GENERAL"><none>NEUTRAL_DGUARD YDUMMY_DGUARD YDUMMY2_DGUARD LDF4B_SPARRING_DGUARD LDF4B_SPARRING_DGUARD2)"
			R"( LDF5_DUMMY1_DGUARD LDF5_DUMMY2_DGUARD LDF5_SPARRING1_DGUARD LDF5_SPARRING2_DGUARD</none></tribe>)"
			R"(<tribe name="MONSTER"><hostile>YUN_GUARD</hostile>)"
			R"(<friend>POLYMORPHPARROT USEALL_TELEPORTER_LI USEALL_TELEPORTER_DA</friend></tribe></tribe_relations>)"));
		return true;
	}();
	static_cast<void>(published);
}

/** A rule of the published registration rules or of the restriction rules, by name */
inline const model::templates::globaldrops::GlobalRule* ruleNamed(std::string_view name) {
	for (const model::templates::globaldrops::GlobalRule& rule : dataholders::DataManager::GLOBAL_DROP_DATA->getAllRules())
		if (rule.getRuleName() == name)
			return &rule;
	for (const model::templates::globaldrops::GlobalRule& rule : restrictionRules().getAllRules())
		if (rule.getRuleName() == name)
			return &rule;
	return nullptr;
}

/** A spawn template of the group, like the spawn data of a map */
class DropSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	explicit DropSpawnTemplate(model::templates::spawns::SpawnGroup& group)
		: SpawnTemplate(group, 100.0f, 100.0f, 50.0f, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** Exposes the protected static KnownList::addPair, so an npc and a player know each other without the spawn machinery */
struct DropKnownListPairing : world::knownlist::KnownList {
	static bool pair(model::gameobjects::VisibleObject& a, model::gameobjects::VisibleObject& b) { return addPair(a, b); }
};

/** The packets of one client, as the byte vectors the connection queued (header included) */
struct SentPacket {
	int32_t opcode;
	std::vector<uint8_t> body;
};

class DropTest : public cp::InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		// the base fixture's DeterministicExecutor again, with a handle the timer cases advance (nothing is scheduled yet); it also seeds this
		// thread's Rnd, so every case draws the same sequence
		utils::ThreadPoolManager::installBackend(nullptr);
		auto backend = std::make_unique<runtime::DeterministicExecutor>(clock, 29);
		executor = backend.get();
		utils::ThreadPoolManager::installBackend(std::move(backend));
		// the npc templates name their AIs and this executable links no AI handler: the warn mode puts AIEngine's substitute in place
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		configs::main::GeoDataConfig::CANSEE_ENABLE.store(false);
		// Player::postConstruct loads the toy pets from the database; the tests have none
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(
			[](model::gameobjects::player::Player&) { return std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>>(); });
		savedDropRates = configs::main::RatesConfig::DROP_RATES.get();
		publishDropStaticDataOnce();
	}

	void TearDown() override {
		for (const runtime::Ref<model::gameobjects::Npc>& npc : npcs) {
			services::drop::DropService::getInstance().unregisterDrop(*npc);
			if (world::World::getInstance().findVisibleObject(npc->getObjectId()).get() == npc.get())
				world::World::getInstance().removeObject(*npc);
		}
		for (cp::PlayerFixture& f : players) {
			if (f.player)
				f.player->setClientConnection(nullptr);
		}
		clients.clear();
		npcs.clear();
		spawnGroups.clear();
		players.clear();
		if (savedDropRates)
			configs::main::RatesConfig::DROP_RATES.set(*savedDropRates);
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		executor = nullptr;
		InWorldPacketTest::TearDown();
	}

	/** @return false (and marks the test skipped) without the test database */
	bool requireDatabase() {
		if (!isDatabaseEnabled())
			return false;
		setUpDatabaseOnce();
		return true;
	}

	/** gameserver.rates.drop, one value for every membership */
	static void setDropRate(float rate) { configs::main::RatesConfig::DROP_RATES.set(std::vector<float>{rate}); }

	/** An npc of the fixture's templates, at (100, 100, 50) of the map: in its instance 1 where the World has the map, else on a bare position */
	model::gameobjects::Npc& spawnNpc(int32_t npcId, int32_t mapId = POETA) {
		const model::templates::npc::NpcTemplate* template_ = dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId);
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(mapId, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<DropSpawnTemplate>(*group));
		runtime::Ref<model::gameobjects::Npc> npc = model::gameobjects::VisibleObject::create<model::gameobjects::Npc>(
			std::make_unique<controllers::NpcController>(), spawn, template_);
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		if (world::World::getInstance().getWorldMap(mapId))
			npc->setPosition(world::World::getInstance().createPosition(mapId, 100.0f, 100.0f, 50.0f, int8_t{0}, 1));
		else
			npc->setPosition(world::WorldPosition::create(mapId, 100.0f, 100.0f, 50.0f, int8_t{0}));
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		return *npc;
	}

	/** A player with a connection whose sent packets the case reads (tests/cm_ak's makePlayer and TestClient) */
	model::gameobjects::player::Player& newPlayer(int32_t objectId, std::string_view name, model::Race race = model::Race::ELYOS) {
		cp::PlayerFixture& f = players.emplace_back(cp::makePlayer(objectId, 9000 + objectId % 1000, name, race));
		std::unique_ptr<cp::TestClient>& client = clients.emplace_back(std::make_unique<cp::TestClient>());
		client->enterWorld(f);
		(*client)->clearSent();
		return *f.player;
	}

	/** The packets the player's connection queued since the last call (cleared) */
	std::vector<SentPacket> takeSent(model::gameobjects::player::Player& player) {
		for (size_t i = 0; i < players.size(); i++) {
			if (players[i].player.get() == &player) {
				std::vector<SentPacket> packets;
				for (const std::vector<uint8_t>& bytes : (*clients[i])->sentBytes()) {
					network::test::PacketReader reader(bytes);
					int32_t opcode = ((static_cast<uint16_t>(reader.H()) ^ 0xDF) - 207) & 0xFFFF; // AionServerPacket.writeOP / Crypt
					packets.push_back({opcode, cp::bodyOf(bytes)});
				}
				(*clients[i])->clearSent();
				return packets;
			}
		}
		ADD_FAILURE() << "not a fixture player";
		return {};
	}

	/** The serialization of a packet for the player's connection, to compare a queued packet with (its header stripped) */
	std::vector<uint8_t> bodyFor(model::gameobjects::player::Player& player, network::aion::AionServerPacket&& packet) {
		for (size_t i = 0; i < players.size(); i++)
			if (players[i].player.get() == &player)
				return cp::bodyOf(cp::serialized(std::move(packet), clients[i]->con()));
		ADD_FAILURE() << "not a fixture player";
		return {};
	}

	static std::vector<runtime::Ptr<model::drop::DropItem>> entriesOf(model::gameobjects::Npc& npc) {
		runtime::Ptr<runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>> set =
			services::drop::DropRegistrationService::getInstance().getCurrentDropMap().get(npc.getObjectId());
		return set ? set->snapshot() : std::vector<runtime::Ptr<model::drop::DropItem>>();
	}

	/** The entry of that index (loot list order is not Java's, m5b3-plan.md D10: entries are matched by index), or null */
	static runtime::Ptr<model::drop::DropItem> entryAt(model::gameobjects::Npc& npc, int32_t index) {
		for (const runtime::Ptr<model::drop::DropItem>& entry : entriesOf(npc))
			if (entry->getIndex() == index)
				return entry;
		return nullptr;
	}

	/** Modifiers as createDropModifiers makes them for a killer without boost (rate 1, no repose, salvation or palace) */
	static model::drop::DropModifiers plainModifiers(model::Race race = model::Race::ELYOS) {
		model::drop::DropModifiers modifiers;
		modifiers.setDropRace(race);
		modifiers.setBoostDropRate(1.0f);
		return modifiers;
	}

	static std::vector<int32_t> itemIdsOf(const std::vector<const model::templates::globaldrops::GlobalDropItem*>& drops) {
		std::vector<int32_t> ids;
		for (const model::templates::globaldrops::GlobalDropItem* drop : drops)
			ids.push_back(drop->getId());
		return ids;
	}

	runtime::DeterministicExecutor* executor = nullptr;
	std::shared_ptr<const std::vector<float>> savedDropRates;
	std::shared_ptr<const std::string> savedMissingAiHandlers;
	std::deque<cp::PlayerFixture> players;
	std::vector<std::unique_ptr<cp::TestClient>> clients;
	std::vector<runtime::Ref<model::gameobjects::Npc>> npcs;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
};

/** Skips the case without the test database (the first statement of a TEST_F body that runs createDropModifiers or registerDrop) */
#define DROP_REQUIRE_DATABASE()                                                                                                                       \
	if (!requireDatabase())                                                                                                                           \
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: calculateBoostDropRate reads the killer's houses (HousingService, the database)";

} // namespace aion::gameserver::economy::test
