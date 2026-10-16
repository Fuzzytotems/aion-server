#pragma once

// Shared fixture of the M5c stage-2 craft tests (m5c-plan.md C-01, C-06; tests/economy/P5-09c): CraftService, CraftSkillUpdateService,
// RecipeService and the Profession companion (ProfessionInfo.h), driven against a real Player.
//
// - The player is tests/cm_ak's ItemPacketTest "Holder" (710101, ELYOS, WARRIOR level 1, spawned in a Poeta map instance, a real AionConnection
//   whose send queue the cases read), reached through MailTestSupport.h (the P5-09c mail fixture's header: LogCapture, ConfigScope, and the
//   ItemPacketTestSupport.h / EconomyTestSupport.h includes). A craft master is a real Npc of its npc_templates.xml row, and a crafting station a
//   real StaticObject whose template is the station's item_templates.xml row (StaticObjectSpawnManager.java:23: ITEM_DATA.getItemTemplate),
//   both spawned beside the player in his map instance; the station is put into his known list (CraftService.startCrafting finds its target
//   there, CraftService.java:101).
// - ITEM_DATA holds ItemPacketTestSupport.h's rows plus CRAFT_ITEM_ROWS, SKILL_DATA the fixture's row plus CRAFT_SKILL_ROWS, RECIPE_DATA
//   RECIPE_TEMPLATES_XML: every row verbatim from the Java tree's game-server/data/static_data, file:line beside each. SKILL_TREE_DATA is empty
//   (PlayerSkillList.addSkill of a new skill asks it; no craft skill has a skill tree row the cases need).
// - Expectations come from tools/oracle `oracle.py m5c-craft --no-profile --set gameserver.event.service.disabled_events=* ...` (its arguments
//   beside each case) or, where the oracle does not model a value, from the Java arithmetic named beside the case. The fixture sets the shipped
//   craft, rates and logging values the bodies read (config/main/craft.properties:12, :16; rates.properties:67, :82; logging.properties:12, :16).
// - A case that learns or deletes a recipe through RecipeList (PlayerRecipesDAO) uses the economy test database of EconomyTestSupport.h
//   (CRAFT_REQUIRE_DATABASE); without DatabaseFactory the DAO writes return false, so RecipeList.addRecipe / deleteRecipe change nothing.
// - CraftingTask is C-02's (P5-02a, the craft-task lane). Before C-02 merges, its constructor is AION_UNPORTED, so a startCrafting that passes
//   checkCraft ends there with an UnportedException after it has consumed the materials and spent the DP; after C-02 the same call starts the
//   task. pastCheckCraft() accepts both and tells them apart, and every case that passes checkCraft ends a started task with endCraft(), so the
//   cases stay green when C-02 merges. The cases that read the started task (its interval, or its run to the end) skip themselves until then.

#include "MailTestSupport.h"

#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/CraftConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/StaticObjectController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/RecipeData.bind.h"
#include "aion/gameserver/dataholders/RecipeData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/SkillTreeData.bind.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/StaticObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/craft/CraftService.h"
#include "aion/gameserver/skillengine/task/AbstractInteractionTask.h"
#include "aion/gameserver/skillengine/task/CraftingTask.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::economy::test::craft {

namespace cp = network::aion::clientpackets::testing;
namespace itemtest = cp::items;

using model::gameobjects::Npc;
using model::gameobjects::StaticObject;
using model::gameobjects::player::Player;
using network::test::PacketReader;
using network::test::PacketWriter;

// ServerPacketsOpcodes.java:24-260
inline constexpr int32_t SM_STATUPDATE_EXP_OPCODE = 8;
inline constexpr int32_t SM_MESSAGE_OPCODE = 24;
inline constexpr int32_t SM_SYSTEM_MESSAGE_OPCODE = itemtest::SM_SYSTEM_MESSAGE_OPCODE;
inline constexpr int32_t SM_SKILL_LIST_OPCODE = 44;
inline constexpr int32_t SM_QUESTION_WINDOW_OPCODE = itemtest::SM_QUESTION_WINDOW_OPCODE;
inline constexpr int32_t SM_CRAFT_ANIMATION_OPCODE = 180;
inline constexpr int32_t SM_CRAFT_UPDATE_OPCODE = 181;
inline constexpr int32_t SM_LEARN_RECIPE_OPCODE = 241;
inline constexpr int32_t SM_RECIPE_DELETE_OPCODE = 242;

// skills (skill_templates.xml; Profession.java:11-21)
inline constexpr int32_t ESSENCETAPPING = 30002;
inline constexpr int32_t COOKING = 40001;
inline constexpr int32_t WEAPONSMITHING = 40002;
inline constexpr int32_t ARMORSMITHING = 40003;
inline constexpr int32_t TAILORING = 40004;
inline constexpr int32_t ALCHEMY = 40007;
inline constexpr int32_t HANDICRAFTING = 40008;
inline constexpr int32_t MORPH = 40009;
inline constexpr int32_t CONSTRUCTION = 40010;

// the skills' nameId (their l10n id)
inline constexpr int32_t ESSENCETAPPING_NAME = 282933;
inline constexpr int32_t AETHERTAPPING_NAME = 282935;
inline constexpr int32_t COOKING_NAME = 280383;
inline constexpr int32_t WEAPONSMITHING_NAME = 280385;
inline constexpr int32_t MORPH_NAME = 282929;
inline constexpr int32_t CONSTRUCTION_NAME = 298164;

// Profession.getSkillGrade's client strings (Profession.java:65-79)
inline constexpr int32_t AMATEUR = 900797;
inline constexpr int32_t NOVICE = 900798;
inline constexpr int32_t APPRENTICE = 900799;
inline constexpr int32_t JOURNEYMAN = 900800;
inline constexpr int32_t EXPERT = 900801;
inline constexpr int32_t ARTISAN = 902027;
inline constexpr int32_t MASTER = 902028;

// recipes (RECIPE_TEMPLATES_XML)
inline constexpr int32_t ROAST_ININA_RECIPE = 155001381;      // cooking 1, ELYOS, autolearn, combo Tasty Roast Inina: the gate's C19 recipe
inline constexpr int32_t ASMODIAN_COOKING_RECIPE = 155006386; // cooking 1, ASMODIANS, autolearn
inline constexpr int32_t ARIA_MORPH_RECIPE = 155000001;       // morph 1, ELYOS, dp 200, autolearn
inline constexpr int32_t NOT_AUTOLEARN_MORPH_RECIPE = 155000003; // morph 1, ELYOS, dp 200, not autolearn; its product has no ITEM_DATA row here
inline constexpr int32_t STEEL_INGOT_RECIPE = 155000075;      // weaponsmithing 1, ELYOS, autolearn
inline constexpr int32_t WEAPONSMITH_450_RECIPE = 155001752;  // weaponsmithing 450, ELYOS, autolearn
inline constexpr int32_t PISTOL_RECIPE = 155090009;           // weaponsmithing 1, PC_ALL, an EPIC gun, two alternatives of 8 components
inline constexpr int32_t LIMITED_SUPPLEMENT_RECIPE = 155090012; // weaponsmithing 1, PC_ALL, max_production_count 1, two alternatives
inline constexpr int32_t DELAYED_SUPPLEMENT_RECIPE = 155090013; // weaponsmithing 1, PC_ALL, craft_delay_id 1 for 30 s, two alternatives
inline constexpr int32_t METAL_PLATE_RECIPE = 155004042;      // armorsmithing 1
inline constexpr int32_t THIN_LEATHER_RECIPE = 155000586;     // tailoring 1
inline constexpr int32_t STONE_POWDER_RECIPE = 155001261;     // alchemy 1
inline constexpr int32_t BETUA_WOOD_RECIPE = 155001120;       // handicrafting 1
inline constexpr int32_t WOODEN_SHELF_RECIPE = 155004247;     // construction 1
inline constexpr int32_t KATALIUM_SHIELD_RECIPE = 155100649;  // armorsmithing 500, a MYTHIC shield
inline constexpr int32_t VEGETABLE_DISH_RECIPE = 155002239;   // cooking 499, ELYOS, max_production_count 1, combo Tasty Vegetable Dish

// items (CRAFT_ITEM_ROWS; the ids of ItemPacketTestSupport.h through itemtest)
inline constexpr int32_t OVEN = 150000009;
inline constexpr int32_t ARIA = 152000401;
inline constexpr int32_t AETHER_POWDER = 152000901;
inline constexpr int32_t ININA = 152001001;
inline constexpr int32_t SALT = 169400096;
inline constexpr int32_t ROAST_ININA = 160001001;
inline constexpr int32_t TASTY_ROAST_ININA = 160001051;
inline constexpr int32_t PURE_KATALIUM = 152000222;
inline constexpr int32_t PURE_ANCIENT_AETHER = 152000918;
inline constexpr int32_t SIEGE_WEAPON_FUEL_A = 169405264; // "Enhanced Siege Weapon Fuel" (shot material), the first alternative's first item
inline constexpr int32_t SIEGE_WEAPON_FUEL_B = 169405263; // "Enhanced Siege Weapon Fuel" (fuel material), the second alternative's first item
inline constexpr int32_t SIEGE_WEAPON_SUPPLEMENT = 169405266;
inline constexpr int32_t NOBLE_PREMIUM_OPHIDAN_PISTOL = 101801009;
inline constexpr int32_t EXALTED_KATALIUM_SHIELD = 115001586; // item_group SHIELD: an armor (ItemSubType.SHIELD, EquipType.ARMOR)
inline constexpr int32_t VEGETABLE_DISH = 182206769;          // "Eremitia's Vegetable Dish", a quest item
inline constexpr int32_t TASTY_VEGETABLE_DISH = 182206773;    // "Eremitia's Tasty Vegetable Dish": _19038MasterCooksPotential.java:26 waits for it
inline constexpr int32_t COOKING_STONE = 169401081;
inline constexpr int32_t WEAPONSMITHING_STONE = 169401076;
inline constexpr int32_t ARMORSMITHING_STONE = 169401077;
inline constexpr int32_t TAILORING_STONE = 169401078;
inline constexpr int32_t HANDICRAFTING_STONE = 169401079;
inline constexpr int32_t ALCHEMY_STONE = 169401080;
inline constexpr int32_t CONSTRUCTION_STONE = 169401082;
inline constexpr int32_t TRAINING_SWORD = itemtest::TRAINING_SWORD;
inline constexpr int32_t KINAH = itemtest::KINAH;

// the items' desc (their l10n id)
inline constexpr int32_t OVEN_NAME = 700104;
inline constexpr int32_t ININA_NAME = 702027;
inline constexpr int32_t SALT_NAME = 703774;
inline constexpr int32_t PURE_ANCIENT_AETHER_NAME = 811647;
inline constexpr int32_t COOKING_STONE_NAME = 789759;
inline constexpr int32_t WEAPONSMITHING_STONE_NAME = 789754;
inline constexpr int32_t ARMORSMITHING_STONE_NAME = 789755;
inline constexpr int32_t TAILORING_STONE_NAME = 789756;
inline constexpr int32_t HANDICRAFTING_STONE_NAME = 789757;
inline constexpr int32_t ALCHEMY_STONE_NAME = 789758;
inline constexpr int32_t CONSTRUCTION_STONE_NAME = 789760;

// npcs (CRAFT_NPC_TEMPLATES_XML)
inline constexpr int32_t CORNELIUS = 203780; // the Elyos essencetapping master (CraftSkillUpdateService.java:56)
inline constexpr int32_t HESTIA = 203784;    // the Elyos cooking master (:61), the gate's C19 master
inline constexpr int32_t LUELAS = 203785;    // a Sanctum merchant who teaches nothing
inline constexpr int32_t LAINITA = 204100;   // the Asmodian cooking master (:40)

/** item_templates.xml, verbatim: the craft cases' rows that ItemPacketTestSupport.h lacks */
inline constexpr std::string_view CRAFT_ITEM_ROWS = R"xml(
	<!-- :743547 -->
	<item_template id="150000009" name="Oven" level="1" cName="cooking" mask="4190" quality="COMMON" price="100" restrict="0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" desc="700104"/>
	<!-- :744358 -->
	<item_template id="152001001" name="Inina" level="10" cName="shell_n_c_10a" mask="4222" max_stack_count="10000" item_group="GATHERABLE" quality="COMMON" price="5" race="ELYOS" desc="702027"/>
	<!-- :850125 -->
	<item_template id="169400096" name="Salt" level="10" cName="shopmaterial_co_01a" mask="12414" max_stack_count="1000" quality="COMMON" price="50" desc="703774"/>
	<!-- :821940 -->
	<item_template id="160001001" name="Roast Inina" level="10" cName="food_phyattack_20a" mask="12414" max_stack_count="1000" quality="COMMON" price="300" desc="729414" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="2" skillid="10054"/>
		</actions>
		<uselimits usedelay="5000" usedelayid="22"/>
	</item_template>
	<!-- :822180 -->
	<item_template id="160001051" name="Tasty Roast Inina" level="10" cName="food_r_phyattack_20a" mask="12414" max_stack_count="1000" quality="COMMON" price="300" desc="729464" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="2" skillid="10086"/>
		</actions>
		<uselimits usedelay="5000" usedelayid="22"/>
	</item_template>
	<!-- :850246 -->
	<item_template id="169401081" name="Cooking Enhancement Stone" level="1" cName="co_boost_material_c_01a" mask="12414" max_stack_count="1000" quality="COMMON" price="3000" desc="789759"/>
	<!-- :850241 -->
	<item_template id="169401076" name="Weaponsmithing Enhancement Stone" level="1" cName="ws_boost_material_c_01a" mask="12414" max_stack_count="1000" quality="COMMON" price="3000" desc="789754"/>
	<!-- :850242 -->
	<item_template id="169401077" name="Armorsmithing Enhancement Stone" level="1" cName="as_boost_material_c_01a" mask="12414" max_stack_count="1000" quality="COMMON" price="3000" desc="789755"/>
	<!-- :850243 -->
	<item_template id="169401078" name="Tailoring Enhancement Stone" level="1" cName="ta_boost_material_c_01a" mask="12414" max_stack_count="1000" quality="COMMON" price="3000" desc="789756"/>
	<!-- :850244 -->
	<item_template id="169401079" name="Handicrafting Enhancement Stone" level="1" cName="ha_boost_material_c_01a" mask="12414" max_stack_count="1000" quality="COMMON" price="3000" desc="789757"/>
	<!-- :850245 -->
	<item_template id="169401080" name="Alchemy Enhancement Stone" level="1" cName="al_boost_material_c_01a" mask="12414" max_stack_count="1000" quality="COMMON" price="3000" desc="789758"/>
	<!-- :850247 -->
	<item_template id="169401082" name="Construction Enhancement Stone" level="1" cName="me_boost_material_c_01a" mask="12414" max_stack_count="1000" quality="COMMON" price="3000" desc="789760"/>
	<!-- :744285 -->
	<item_template id="152000901" name="Aether Powder" level="10" cName="od_n_c_10a" mask="4222" max_stack_count="10000" item_group="GATHERABLE" quality="COMMON" price="5" desc="702030"/>
	<!-- :743911 -->
	<item_template id="152000401" name="Aria" level="10" cName="herb_n_c_10a" mask="4222" max_stack_count="10000" item_group="GATHERABLE" quality="COMMON" price="5" race="ELYOS" desc="702016"/>
	<!-- :850545 -->
	<item_template id="169405264" name="Enhanced Siege Weapon Fuel" level="65" cName="shot_material_shop_r_61a" mask="28794" max_stack_count="10000" quality="RARE" price="5000" desc="829465">
		<acquisition type="AP" ap="200"/>
	</item_template>
	<!-- :850542 -->
	<item_template id="169405263" name="Enhanced Siege Weapon Fuel" level="65" cName="fuel_material_shop_r_61a" mask="28794" max_stack_count="10000" quality="RARE" price="5000" desc="829464">
		<acquisition type="AP" ap="200"/>
	</item_template>
	<!-- :743760 -->
	<item_template id="152000222" name="Pure Katalium" level="60" cName="metal_L_60a" casting_delay="3000" mask="4222" max_stack_count="10000" item_group="GATHERABLE" quality="LEGEND" price="3780" desc="811607" activate_target="STANDALONE" activate_count="1">
		<actions>
			<decompose/>
		</actions>
		<uselimits usedelayid="85"/>
	</item_template>
	<!-- :744352 -->
	<item_template id="152000918" name="Pure Ancient Aether" level="60" cName="od_all_L_60a" casting_delay="3000" mask="4222" max_stack_count="10000" item_group="GATHERABLE" quality="LEGEND" price="11338" desc="811647" activate_target="STANDALONE" activate_count="1">
		<actions>
			<decompose/>
		</actions>
		<uselimits usedelayid="85"/>
	</item_template>
	<!-- :850549 -->
	<item_template id="169405266" name="Siege Weapon Supplement" level="65" cName="shot_combineskill_make_l_61a" mask="28798" max_stack_count="10000" quality="LEGEND" price="50000" desc="829467"/>
	<!-- :145032 -->
	<item_template id="101801009" name="Noble Premium Ophidan Pistol" level="65" cName="gun_PvP_E_p_65a" mask="138494" item_group="GUN" quality="EPIC" price="16795644" restrict="65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65" desc="814986" attack_type="MAGICAL_FIRE" exceed_enchant_skill="RANK3_SET2_MAGICAL_WEAPON" can_exceed_enchant="true" max_enchant="10" m_slots="6">
		<modifiers>
			<add name="MAXHP" value="546" bonus="true"/>
			<add name="BOOST_MAGICAL_SKILL" value="87" bonus="true"/>
			<add name="MAGICAL_CRITICAL" value="26" bonus="true"/>
			<add name="MAGICAL_ACCURACY" value="61" bonus="true"/>
			<rate name="ATTACK_SPEED" value="-19" bonus="true"/>
			<add name="PVP_ATTACK_RATIO" value="72" bonus="true"/>
		</modifiers>
		<actions>
			<remodel type="0"/>
		</actions>
		<weapon_stats hit_count="1" attack_range="20000" boost_magical_skill="964" magical_accuracy="504" attack_speed="1800" max_damage="290" min_damage="262"/>
		<disposition id="188950015" count="6"/>
		<idian burn_attack="38" burn_defend="21"/>
	</item_template>
	<!-- :891837 -->
	<item_template id="182290358" name="Metal Plate" level="1" cName="item_as_q5100" mask="28736" max_stack_count="100" quality="COMMON" price="1" desc="1192042">
		<inventory id="2"/>
	</item_template>
	<!-- :744920 -->
	<item_template id="152020033" name="Thin Leather" level="10" cName="ta_part_lea1_01a" mask="12414" max_stack_count="1000" quality="COMMON" price="80" race="ELYOS" desc="707476"/>
	<!-- :744998 -->
	<item_template id="152020111" name="Lesser Elemental Stone Powder" level="10" cName="al_part_stone1_01a" mask="12414" max_stack_count="1000" quality="COMMON" price="20" race="ELYOS" desc="707533"/>
	<!-- :744974 -->
	<item_template id="152020087" name="Betua Wood" level="10" cName="jr_part_birke_01a" mask="12414" max_stack_count="1000" quality="COMMON" price="60" race="ELYOS" desc="707524"/>
	<!-- :892590 -->
	<item_template id="182290609" name="Wooden Shelf" level="1" cName="item_me_q5541" mask="28736" max_stack_count="100" quality="COMMON" price="1" desc="1192247">
		<inventory id="2"/>
	</item_template>
	<!-- :881749 -->
	<item_template id="182206769" name="Eremitia's Vegetable Dish" level="1" cName="quest_skill_cook01_450" mask="20544" item_group="QUEST" quality="LEGEND" price="1" race="ELYOS" desc="760211">
		<inventory id="0"/>
	</item_template>
	<!-- :881761 -->
	<item_template id="182206773" name="Eremitia's Tasty Vegetable Dish" level="1" cName="quest_skill_cook01_500" mask="20544" item_group="QUEST" quality="LEGEND" price="1" race="ELYOS" desc="760032">
		<inventory id="2"/>
	</item_template>
	<!-- :745068 -->
	<item_template id="152020181" name="Red Heliotrope Crystal" level="55" cName="combineskill_all_01a" mask="12414" max_stack_count="1000" quality="UNIQUE" price="300000" race="ELYOS" desc="759267"/>
	<!-- :622554 -->
	<item_template id="115001586" name="Exalted Katalium Shield" level="65" cName="shield_PvE_M_p_65a_down" mask="37118" item_group="SHIELD" quality="MYTHIC" price="12289155" restrict="65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65" desc="815104" exceed_enchant_skill="RANK4_SET2_PHYSICAL_WEAPON" can_exceed_enchant="true" max_enchant_bonus="0" max_enchant="12" m_slots="4">
		<modifiers>
			<add name="BLOCK" value="1257"/>
			<rate name="DAMAGE_REDUCE" value="50"/>
			<add name="BLOCK" value="162" bonus="true"/>
			<add name="MAXHP" value="461" bonus="true"/>
			<add name="PHYSICAL_DEFENSE" value="131" bonus="true"/>
			<add name="MAGICAL_RESIST" value="57" bonus="true"/>
			<rate name="BOOST_HATE" value="70" bonus="true"/>
			<add name="PHYSICAL_ATTACK" value="30" bonus="true"/>
			<add name="PHYSICAL_ACCURACY" value="163" bonus="true"/>
		</modifiers>
		<actions>
			<remodel type="0"/>
		</actions>
		<weapon_stats reduce_max="1843"/>
		<disposition id="188950019" count="4"/>
	</item_template>
)xml";

/** skill_templates.xml, verbatim: the professions' skills (:204328-204389; 30003 for Aethertapping, 40003-40008 for the other crafts) */
inline constexpr std::string_view CRAFT_SKILL_ROWS = R"xml(
	<!-- :204328 -->
	<skill_template skill_id="30002" name="Essencetapping" nameId="282933" stack="GATHERING_B" lvl="1" skilltype="NONE" skillsubtype="NONE" tslot="NONE" activation="NONE" cooldown="0" duration="0">
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<motion name="axe" />
	</skill_template>
	<!-- :204334 -->
	<skill_template skill_id="30003" name="Aethertapping" nameId="282935" stack="AERIAL_GATHERING" lvl="1" skilltype="NONE" skillsubtype="NONE" tslot="NONE" activation="NONE" cooldown="0" duration="0">
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<motion name="mine" />
	</skill_template>
	<!-- :204340 -->
	<skill_template skill_id="40001" name="Cooking" nameId="280383" stack="COOKING" lvl="1" skilltype="NONE" skillsubtype="NONE" tslot="NONE" activation="NONE" cooldown="0" duration="0">
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
	</skill_template>
	<!-- :204345 -->
	<skill_template skill_id="40002" name="Weaponsmithing" nameId="280385" stack="WEAPONSMITH" lvl="1" skilltype="NONE" skillsubtype="NONE" tslot="NONE" activation="NONE" cooldown="0" duration="0">
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
	</skill_template>
	<!-- :204350 -->
	<skill_template skill_id="40003" name="Armorsmithing" nameId="280387" stack="ARMORSMITH" lvl="1" skilltype="NONE" skillsubtype="NONE" tslot="NONE" activation="NONE" cooldown="0" duration="0">
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
	</skill_template>
	<!-- :204355 -->
	<skill_template skill_id="40004" name="Tailoring" nameId="280389" stack="TAILORING" lvl="1" skilltype="NONE" skillsubtype="NONE" tslot="NONE" activation="NONE" cooldown="0" duration="0">
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
	</skill_template>
	<!-- :204370 -->
	<skill_template skill_id="40007" name="Alchemy" nameId="280395" stack="ALCHEMY" lvl="1" skilltype="NONE" skillsubtype="NONE" tslot="NONE" activation="NONE" cooldown="0" duration="0">
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
	</skill_template>
	<!-- :204375 -->
	<skill_template skill_id="40008" name="Handicrafting" nameId="280397" stack="HANDIWORK" lvl="1" skilltype="NONE" skillsubtype="NONE" tslot="NONE" activation="NONE" cooldown="0" duration="0">
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
	</skill_template>
	<!-- :204380 -->
	<skill_template skill_id="40009" name="Morph Substances" nameId="282929" stack="CONVERT" lvl="1" skilltype="NONE" skillsubtype="NONE" tslot="NONE" activation="NONE" cooldown="0" duration="0">
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
	</skill_template>
	<!-- :204385 -->
	<skill_template skill_id="40010" name="Construction" nameId="298164" stack="MENUISIER" lvl="1" skilltype="NONE" skillsubtype="NONE" tslot="NONE" activation="NONE" cooldown="0" duration="0">
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
	</skill_template>
)xml";

/** recipe_templates.xml, verbatim rows */
inline constexpr std::string_view RECIPE_TEMPLATES_XML = R"xml(<recipe_templates>
	<!-- :3 -->
	<recipe_template id="155000001" nameid="730278" skillid="40009" race="ELYOS" skillpoint="1" dp="200" autolearn="1" productid="152000401" quantity="3">
		<components_data>
			<component quantity="1" itemid="152000901"/>
		</components_data>
	</recipe_template>
	<!-- :8 -->
	<recipe_template id="155000002" nameid="730279" skillid="40009" race="ELYOS" skillpoint="1" dp="200" autolearn="1" productid="152001001" quantity="3">
		<components_data>
			<component quantity="1" itemid="152000901"/>
		</components_data>
	</recipe_template>
	<!-- :13 -->
	<recipe_template id="155000003" nameid="730280" skillid="40009" race="ELYOS" skillpoint="1" dp="200" productid="152000801" quantity="3">
		<components_data>
			<component quantity="1" itemid="152000901"/>
		</components_data>
	</recipe_template>
	<!-- :23 -->
	<recipe_template id="155000005" nameid="730282" skillid="40009" race="ELYOS" skillpoint="1" dp="200" autolearn="1" productid="152000201" quantity="2">
		<components_data>
			<component quantity="1" itemid="152000901"/>
		</components_data>
	</recipe_template>
	<!-- :373 -->
	<recipe_template id="155000075" nameid="730352" skillid="40002" race="ELYOS" skillpoint="1" autolearn="1" productid="152020001" quantity="1">
		<components_data>
			<component quantity="1" itemid="152000201"/>
			<component quantity="1" itemid="169400010"/>
		</components_data>
	</recipe_template>
	<!-- :5191 -->
	<recipe_template id="155000586" nameid="730863" skillid="40004" race="ELYOS" skillpoint="1" autolearn="1" productid="152020033" quantity="1">
		<components_data>
			<component quantity="2" itemid="152010310"/>
		</components_data>
	</recipe_template>
	<!-- :10104 -->
	<recipe_template id="155001120" nameid="731382" skillid="40008" race="ELYOS" skillpoint="1" autolearn="1" productid="152020087" quantity="1">
		<components_data>
			<component quantity="2" itemid="152000501"/>
		</components_data>
	</recipe_template>
	<!-- :11175 -->
	<recipe_template id="155001261" nameid="731533" skillid="40007" race="ELYOS" skillpoint="1" autolearn="1" productid="152020111" quantity="2">
		<components_data>
			<component quantity="1" itemid="152010314"/>
		</components_data>
	</recipe_template>
	<!-- :12124 -->
	<recipe_template id="155001381" nameid="731656" skillid="40001" race="ELYOS" skillpoint="1" autolearn="1" productid="160001001" quantity="2">
		<components_data>
			<component quantity="1" itemid="152001001"/>
			<component quantity="2" itemid="169400096"/>
		</components_data>
		<comboproduct itemid="160001051"/>
	</recipe_template>
	<!-- :15101 -->
	<recipe_template id="155001752" nameid="754522" skillid="40002" race="ELYOS" skillpoint="450" autolearn="6" productid="152020181" quantity="1" craft_delay_id="915501" craft_delay_time="79200">
		<components_data>
			<component quantity="1" itemid="169405023"/>
			<component quantity="1" itemid="169400132"/>
		</components_data>
	</recipe_template>
	<!-- :19638 -->
	<recipe_template id="155002239" nameid="755008" skillid="40001" race="ELYOS" skillpoint="499" productid="182206769" quantity="1" max_production_count="1">
		<components_data>
			<component quantity="10" itemid="152000010"/>
			<component quantity="10" itemid="152000011"/>
			<component quantity="16" itemid="152000012"/>
			<component quantity="12" itemid="169400106"/>
			<component quantity="3" itemid="169400108"/>
			<component quantity="10" itemid="152000911"/>
		</components_data>
		<comboproduct itemid="182206773"/>
	</recipe_template>
	<!-- :34095 -->
	<recipe_template id="155004042" nameid="704246" skillid="40003" race="ELYOS" skillpoint="1" productid="182290358" quantity="1">
		<components_data>
			<component quantity="1" itemid="182290041"/>
		</components_data>
	</recipe_template>
	<!-- :35660 -->
	<recipe_template id="155004247" nameid="793258" skillid="40010" race="ELYOS" skillpoint="1" productid="182290609" quantity="1">
		<components_data>
			<component quantity="1" itemid="182290563"/>
		</components_data>
	</recipe_template>
	<!-- :35967 -->
	<recipe_template id="155005001" nameid="734715" skillid="40009" race="ASMODIANS" skillpoint="1" dp="200" autolearn="1" productid="152000451" quantity="3">
		<components_data>
			<component quantity="1" itemid="152000901"/>
		</components_data>
	</recipe_template>
	<!-- :35972 -->
	<recipe_template id="155005002" nameid="734716" skillid="40009" race="ASMODIANS" skillpoint="1" dp="200" autolearn="1" productid="152001051" quantity="3">
		<components_data>
			<component quantity="1" itemid="152000901"/>
		</components_data>
	</recipe_template>
	<!-- :35987 -->
	<recipe_template id="155005005" nameid="734719" skillid="40009" race="ASMODIANS" skillpoint="1" dp="200" autolearn="1" productid="152000201" quantity="2">
		<components_data>
			<component quantity="1" itemid="152000901"/>
		</components_data>
	</recipe_template>
	<!-- :48134 -->
	<recipe_template id="155006386" nameid="736098" skillid="40001" race="ASMODIANS" skillpoint="1" autolearn="1" productid="160002001" quantity="2">
		<components_data>
			<component quantity="1" itemid="152001051"/>
			<component quantity="2" itemid="169400096"/>
		</components_data>
		<comboproduct itemid="160002051"/>
	</recipe_template>
	<!-- :72306 -->
	<recipe_template id="155090009" nameid="830631" skillid="40002" race="PC_ALL" skillpoint="1" productid="101801009" quantity="1">
		<components_data>
			<component quantity="1" itemid="169400096"/>
			<component quantity="2" itemid="169400097"/>
			<component quantity="3" itemid="169400098"/>
			<component quantity="4" itemid="169400099"/>
			<component quantity="5" itemid="169400100"/>
			<component quantity="6" itemid="169400101"/>
			<component quantity="7" itemid="169400102"/>
			<component quantity="8" itemid="169400103"/>
		</components_data>
		<components_data>
			<component quantity="1" itemid="169400104"/>
			<component quantity="2" itemid="169400105"/>
			<component quantity="3" itemid="169400106"/>
			<component quantity="4" itemid="169400107"/>
			<component quantity="5" itemid="169400108"/>
			<component quantity="6" itemid="169400109"/>
			<component quantity="7" itemid="169400110"/>
			<component quantity="8" itemid="169400111"/>
		</components_data>
	</recipe_template>
	<!-- :72402 -->
	<recipe_template id="155090012" nameid="831554" skillid="40002" race="PC_ALL" skillpoint="1" productid="169405266" quantity="1" max_production_count="1">
		<components_data>
			<component quantity="1" itemid="169405264"/>
			<component quantity="1" itemid="152000222"/>
		</components_data>
		<components_data>
			<component quantity="1" itemid="169405263"/>
			<component quantity="1" itemid="152000918"/>
		</components_data>
	</recipe_template>
	<!-- :72412 -->
	<recipe_template id="155090013" nameid="831555" skillid="40002" race="PC_ALL" skillpoint="1" productid="169405266" quantity="1" craft_delay_id="1" craft_delay_time="30">
		<components_data>
			<component quantity="1" itemid="169405264"/>
			<component quantity="1" itemid="152000222"/>
		</components_data>
		<components_data>
			<component quantity="1" itemid="169405263"/>
			<component quantity="1" itemid="152000918"/>
		</components_data>
	</recipe_template>
	<!-- :78376 -->
	<recipe_template id="155100649" nameid="815645" skillid="40003" race="ELYOS" skillpoint="500" productid="115001586" quantity="1">
		<components_data>
			<component quantity="1" itemid="115001583"/>
			<component quantity="90" itemid="169405233"/>
			<component quantity="30" itemid="152000918"/>
			<component quantity="30" itemid="152011105"/>
			<component quantity="60" itemid="152000222"/>
			<component quantity="8" itemid="152012590"/>
			<component quantity="1" itemid="169405248"/>
			<component quantity="4" itemid="152012593"/>
		</components_data>
		<comboproduct itemid="115001585"/>
	</recipe_template>
</recipe_templates>)xml";

/** npc_templates.xml, verbatim: the craft masters and a merchant */
inline constexpr std::string_view CRAFT_NPC_TEMPLATES_XML = R"xml(<npc_templates>
	<!-- :10376 -->
	<npc_template npc_id="203780" level="40" name="cornelius" name_id="351271" height="2" title_id="350399" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="10" sangle="300" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="9426">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>113500310</item>
			<item>110500326</item>
			<item>112500302</item>
			<item>111500318</item>
			<item>114500322</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" is_dialog="true" func_dialogs="45" can_talk_invisible="false" />
	</npc_template>
	<!-- :10432 -->
	<npc_template npc_id="203784" level="40" name="hestia" name_id="351275" height="2" title_id="350401" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="10" sangle="300" attack_speed="2000" hpgauge="3">
		<stats maxHp="9426">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>113100277</item>
			<item>110100339</item>
			<item>111100278</item>
			<item>114100295</item>
			<item>125000652</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" is_dialog="true" func_dialogs="46 79 58 80" can_talk_invisible="false" />
	</npc_template>
	<!-- :10446 -->
	<npc_template npc_id="203785" level="10" name="luelas" name_id="351276" height="2" title_id="350368" group_drop="LIGHT" rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="10" sangle="300" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="1392">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>113100270</item>
			<item>110100316</item>
			<item>112100250</item>
			<item>111100271</item>
			<item>114100288</item>
			<item>125000652</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" is_dialog="true" func_dialogs="2 3" can_talk_invisible="false" />
	</npc_template>
	<!-- :14943 -->
	<npc_template npc_id="204100" level="40" name="lainita" name_id="352342" height="2" title_id="350401" group_drop="DARK" rank="DISCIPLINED" rating="NORMAL" race="ASMODIANS" tribe="GENERAL_DARK" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" attack_speed="2000" hpgauge="3">
		<stats maxHp="9426">
			<speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" />
		</stats>
		<equipment>
			<item>113100285</item>
			<item>110100347</item>
			<item>111100286</item>
			<item>114100303</item>
			<item>125000652</item>
		</equipment>
		<bound_radius front="0.25" side="0.35" upper="2" />
		<talk_info distance="5" is_dialog="true" func_dialogs="46 79 58 80" can_talk_invisible="false" />
	</npc_template>
</npc_templates>)xml";

/** ItemPacketTestSupport.h's rows and CRAFT_ITEM_ROWS in one <item_templates> document */
inline std::string craftItemTemplatesXml() {
	std::string xml(itemtest::ITEM_TEMPLATES_XML);
	xml.erase(xml.rfind("</item_templates>"));
	xml += CRAFT_ITEM_ROWS;
	xml += "</item_templates>";
	return xml;
}

/** ItemPacketTestSupport.h's skill row and CRAFT_SKILL_ROWS in one <skill_data> document */
inline std::string craftSkillTemplatesXml() {
	std::string xml(itemtest::SKILL_TEMPLATES_XML);
	xml.erase(xml.rfind("</skill_data>"));
	xml += CRAFT_SKILL_ROWS;
	xml += "</skill_data>";
	return xml;
}

/** The npc rows, bound through NpcData as the server loads npc_templates.xml; kept for the process as DataManager keeps its own */
inline const dataholders::NpcData& craftNpcData() {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		return xml::bindString<dataholders::NpcData>(context, CRAFT_NPC_TEMPLATES_XML).release();
	}();
	return *holder;
}

/**
 * Java ChatUtil.l10n(id) (ChatUtil.java:96-102): "$" and the two UTF-16 units of `id << 1 | 1`, the low one first - written out here from the
 * Java source rather than taken from the C++ ChatUtil, and kept as WTF-8 like the C++ strings (ChatUtil.h)
 */
inline std::string javaL10n(int32_t l10nId) {
	const uint32_t id = (static_cast<uint32_t>(l10nId) << 1) | 1u;
	return "$" + commons::utils::StringUtils::toWtf8(std::u16string{static_cast<char16_t>(id & 0xFFFF), static_cast<char16_t>(id >> 16)});
}

/** The number of hits of the AION_UNPORTED sites whose function (the compiler's spelling) names `function` since the last reset */
inline uint64_t unportedHitsOf(std::string_view function) {
	uint64_t hits = 0;
	for (const runtime::UnportedHit& hit : runtime::unportedHits()) {
		if (hit.function.find(function) != std::string::npos)
			hits += hit.hits;
	}
	return hits;
}

/** The action byte of every captured SM_CRAFT_UPDATE (SM_CRAFT_UPDATE.java writeImpl: H(skillId), C(action), ...), in order */
inline std::vector<int32_t> craftUpdateActions(const std::vector<std::vector<uint8_t>>& packets) {
	std::vector<int32_t> actions;
	for (const std::vector<uint8_t>& packet : itemtest::packetsOf(packets, SM_CRAFT_UPDATE_OPCODE)) {
		PacketReader reader(cp::bodyOf(packet));
		reader.H();
		actions.push_back(reader.C());
	}
	return actions;
}

/** The SM_SYSTEM_MESSAGE packets of a capture, in order */
inline std::vector<std::vector<uint8_t>> systemMessagesOf(const std::vector<std::vector<uint8_t>>& packets) {
	return itemtest::packetsOf(packets, SM_SYSTEM_MESSAGE_OPCODE);
}

/** The materials map of CM_CRAFT (item id -> count; checkCraft reads only the keys, CraftService.java:200, :224) */
using Materials = std::unordered_map<int32_t, int64_t>;

class CraftSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	CraftSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** How a startCrafting that passed checkCraft ended (see the file comment) */
enum class PastCheckCraft {
	TASK_CONSTRUCTOR_UNPORTED, // before C-02: CraftingTask's constructor threw its UnportedException
	TASK_STARTED               // after C-02: the task started and is the player's interaction task
};

/**
 * ItemPacketTest with the craft rows published, an empty recipe list, the shipped craft, rates and logging values, craft masters and stations
 * to spawn, and the player's skills and items set the way the DAOs load them (no packet).
 */
class CraftTest : public itemtest::ItemPacketTest {
protected:
	void SetUp() override {
		ItemPacketTest::SetUp();
		// the npc rows name ai="general", whose handler lives in the handler library this executable does not link: AIEngine then substitutes a
		// DummyNpcAI (TradeTestSupport.h does the same; the craft bodies never ask the AI)
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		savedSkillXpCraftingRates = configs::main::RatesConfig::SKILL_XP_CRAFTING_RATES.get();
		savedXpCraftingRates = configs::main::RatesConfig::XP_CRAFTING_RATES.get();
		setRates(configs::main::RatesConfig::SKILL_XP_CRAFTING_RATES, {1.0f, 2.0f}); // rates.properties:82
		setRates(configs::main::RatesConfig::XP_CRAFTING_RATES, {1.0f, 2.0f});       // rates.properties:67
		xml::LoadContext context;
		dataholders::DataManager::ITEM_DATA.resetForTests(); // the base rows, republished with the craft rows
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, craftItemTemplatesXml()));
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the base row, republished with the craft skills (the base TearDown resets it)
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(context, craftSkillTemplatesXml()));
		dataholders::DataManager::RECIPE_DATA.publish(xml::bindString<dataholders::RecipeData>(context, RECIPE_TEMPLATES_XML));
		dataholders::DataManager::SKILL_TREE_DATA.publish(xml::bindString<dataholders::SkillTreeData>(context, "<skill_tree/>"));
		player().setRecipeList(model::gameobjects::player::RecipeList::create()); // PlayerRecipesDAO.load of a character without recipes
		setSkills({});
		clearSent();
	}

	void TearDown() override {
		endCraft();
		for (const runtime::Ref<Player>& online : inWorld) {
			online->getCommonData()->setOnline(false);
			world::World::getInstance().removeObject(*online);
		}
		inWorld.clear();
		given.clear();
		npcs.clear(); // before the map instance their positions name
		stations.clear();
		spawnGroups.clear();
		ItemPacketTest::TearDown();
		dataholders::DataManager::SKILL_TREE_DATA.resetForTests();
		dataholders::DataManager::RECIPE_DATA.resetForTests();
		restoreRates(configs::main::RatesConfig::XP_CRAFTING_RATES, savedXpCraftingRates);
		restoreRates(configs::main::RatesConfig::SKILL_XP_CRAFTING_RATES, savedSkillXpCraftingRates);
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
	}

	static void setRates(commons::configuration::ConfigValue<std::vector<float>>& config, std::vector<float> rates) { config.set(std::move(rates)); }

	static void restoreRates(commons::configuration::ConfigValue<std::vector<float>>& config,
		const std::shared_ptr<const std::vector<float>>& saved) {
		config.set(saved ? *saved : std::vector<float>{});
	}

	model::gameobjects::player::PlayerCommonData& commonData() { return *player().getCommonData(); }

	/** The player's skills: the fixture's sword skill and `skills` (id, level), loaded like PlayerSkillListDAO does (no packet) */
	void setSkills(std::initializer_list<std::pair<int32_t, int32_t>> skills) {
		std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>> entries;
		std::vector<runtime::Ref<model::skill::PlayerSkillEntry>> owned;
		constexpr auto loaded = model::gameobjects::Persistable_PersistentState::UPDATED;
		owned.push_back(model::skill::PlayerSkillEntry::create(itemtest::SWORD_SKILL, 1, 0, loaded));
		for (const auto& [skillId, level] : skills)
			owned.push_back(model::skill::PlayerSkillEntry::create(skillId, level, 0, loaded));
		for (const runtime::Ref<model::skill::PlayerSkillEntry>& entry : owned)
			entries.emplace_back(entry);
		player().setSkillList(model::skill::PlayerSkillList::create(entries));
	}

	model::skill::PlayerSkillEntry& skill(int32_t skillId) {
		runtime::Ptr<model::skill::PlayerSkillEntry> entry = player().getSkillList()->getSkillEntry(skillId);
		if (!entry)
			throw runtime::NullPointerException("the player has no skill " + std::to_string(skillId));
		return *entry;
	}

	/** The player's recipes, loaded like PlayerRecipesDAO.load (no packet) */
	void setRecipes(std::initializer_list<int32_t> recipeIds) {
		player().setRecipeList(model::gameobjects::player::RecipeList::create(std::unordered_set<int32_t>(recipeIds)));
	}

	bool knowsRecipe(int32_t recipeId) { return player().getRecipeList()->isRecipePresent(recipeId); }

	/** An item loaded into the cube the way the DAO does (onLoadHandler: no packet) */
	model::gameobjects::Item& give(int32_t objId, int32_t itemId, int64_t count) {
		runtime::Ref<model::gameobjects::Item> item = itemtest::loadedItem(objId, itemId, count, model::items::storage::StorageType::CUBE);
		player().getInventory().onLoadHandler(*item);
		given.push_back(item);
		return *item;
	}

	int64_t countOf(int32_t itemId) { return player().getInventory().getItemCountByItemId(itemId); }

	int64_t kinah() { return player().getInventory().getKinah(); }

	/** Fills the cube with Training Swords until Storage.isFull (the cube of a character without expansions holds 27) */
	void fillCube(int32_t firstObjId) {
		for (int32_t objId = firstObjId; !player().getInventory().isFull(); objId++)
			give(objId, TRAINING_SWORD, 1);
	}

	/** Java PlayerCommonData.setLevel of a Daeva (PlayerCommonData.java:276-281): levels past 9 need the flag */
	void setDaevaLevel(int32_t level) {
		commonData().setDaeva(true);
		commonData().setLevel(level);
		ASSERT_EQ(player().getLevel(), level);
	}

	/** Java PlayerEnterWorldService for the rates' purposes: the player is in the World and his common data online (getPlayer answers him) */
	void goOnline() {
		world::World::getInstance().storeObject(player());
		commonData().setOnline(true);
		inWorld.emplace_back(player());
	}

	/** The npc of a CRAFT_NPC_TEMPLATES_XML row, spawned once 2 m beside the player (Java VisibleObjectSpawner.spawnNpc) */
	Npc& npc(int32_t npcId) {
		for (const runtime::Ref<Npc>& spawned : npcs)
			if (spawned->getNpcId() == npcId)
				return *spawned;
		const model::templates::npc::NpcTemplate* objectTemplate = craftNpcData().getNpcTemplate(npcId);
		if (objectTemplate == nullptr)
			throw runtime::NullPointerException("no npc row " + std::to_string(npcId));
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<CraftSpawnTemplate>(*group, 102.0f, 100.0f, 50.0f));
		runtime::Ref<Npc> created =
			model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn, objectTemplate);
		created->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*created));
		created->setEffectController(std::make_unique<controllers::effect::EffectController>(*created));
		created->setPosition(
			world::WorldPosition::create(210010000, 102.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(102.0f, 100.0f, 50.0f)));
		created->getPosition()->setIsSpawned(true);
		spawnGroups.push_back(group);
		npcs.push_back(created);
		return *created;
	}

	/** An npc of the rows, known to the player (a known object that is not a StaticObject) */
	Npc& knownNpc(int32_t npcId) {
		Npc& known = npc(npcId);
		f.knownList().addForTest(known);
		clearSent(); // the see notification's packets, if any
		return known;
	}

	/**
	 * A crafting station: a StaticObject of the station's item template (StaticObjectSpawnManager.java:23, 31), spawned `dx` metres east of the
	 * player (both at y 100, z 50) and known to him (PlayerController.see sends SM_GATHERABLE_INFO, cleared here)
	 */
	StaticObject& station(float dx, int32_t templateId = OVEN) {
		const model::templates::item::ItemTemplate* objectTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(templateId);
		if (objectTemplate == nullptr)
			throw runtime::NullPointerException("no item row " + std::to_string(templateId));
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, templateId, 0, nullptr);
		const float x = player().getX() + dx;
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<CraftSpawnTemplate>(*group, x, 100.0f, 50.0f));
		runtime::Ref<StaticObject> created =
			model::gameobjects::VisibleObject::create<StaticObject>(std::make_unique<controllers::StaticObjectController>(), spawn, objectTemplate);
		created->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(x, 100.0f, 50.0f)));
		created->getPosition()->setIsSpawned(true);
		spawnGroups.push_back(group);
		stations.push_back(created);
		f.knownList().addForTest(*created);
		clearSent();
		return *created;
	}

	/** Java CM_CRAFT.runImpl's call (CM_CRAFT.java:60) */
	void craft(int32_t recipeId, int32_t targetObjId, int32_t craftType = 0, const Materials& materials = {}) {
		services::craft::CraftService::startCrafting(player(), recipeId, targetObjId, craftType, materials);
	}

	/** @return true if an SM_CRAFT_UPDATE of action 4 (cancelled: sendCancelCraft, or an aborted task) was sent */
	bool sentCancel() {
		for (int32_t action : craftUpdateActions(sent()))
			if (action == 4)
				return true;
		return false;
	}

	/**
	 * A startCrafting that must pass checkCraft (see the file comment): the CraftingTask constructor's UnportedException, or the started task;
	 * either way no cancel pair
	 */
	PastCheckCraft pastCheckCraft(int32_t recipeId, int32_t targetObjId, int32_t craftType, const Materials& materials) {
		runtime::resetUnportedHitsForTests();
		clearSent();
		PastCheckCraft outcome = PastCheckCraft::TASK_STARTED;
		try {
			craft(recipeId, targetObjId, craftType, materials);
		} catch (const runtime::UnportedException&) {
			EXPECT_EQ(unportedHitsOf("CraftingTask::CraftingTask"), 1u) << "before C-02, CraftingTask's constructor is the only unported site";
			EXPECT_EQ(runtime::unportedHitCount(), 1u);
			outcome = PastCheckCraft::TASK_CONSTRUCTOR_UNPORTED;
		}
		EXPECT_FALSE(sentCancel()) << "checkCraft passed: no sendCancelCraft";
		if (outcome == PastCheckCraft::TASK_STARTED) {
			runtime::Ptr<skillengine::task::CraftingTask> task = runtime::as<skillengine::task::CraftingTask>(player().getInteractionTask());
			EXPECT_TRUE(task && task->isInProgress()) << "after C-02, startCrafting starts the CraftingTask (CraftService.java:131)";
		}
		return outcome;
	}

	/**
	 * Runs a started craft (after C-02) on the fixture's executor, one due tick after the other, until the task has left the player (the tick
	 * after a full bar ends it, AbstractCraftTask.java:45-53, AbstractInteractionTask.java:72-74) or nothing is due; at most `maxTicks` ticks.
	 * The case asserts what the end left behind.
	 */
	void runCraftToItsEnd(int32_t maxTicks = 20) {
		for (int32_t tick = 0; tick < maxTicks && player().getInteractionTask(); tick++) {
			std::optional<std::chrono::steady_clock::time_point> due = executor->nextDueTime();
			if (!due)
				return;
			executor->advance(std::chrono::duration_cast<std::chrono::milliseconds>(*due - clock.now()));
		}
	}

	/** The last two bytes of a packet as Java's writeH wrote them (little endian), -1 for a packet shorter than that */
	static int32_t closingH(const std::vector<uint8_t>& packet) {
		return packet.size() < 2 ? -1 : packet[packet.size() - 2] | (packet[packet.size() - 1] << 8);
	}

	/** Aborts a craft a case started (only after C-02 is there one), and forgets its packets */
	void endCraft() {
		if (!f.player)
			return;
		if (runtime::Ptr<skillengine::task::AbstractInteractionTask> task = player().getInteractionTask())
			task->abort();
		clearSent();
	}

	/** SM_CRAFT_UPDATE and SM_CRAFT_ANIMATION of sendCancelCraft (CraftService.java:235-238) as Java writes them */
	std::vector<std::vector<uint8_t>> cancelPair(int32_t skillId, int32_t productId, int32_t targetObjId) {
		// SM_CRAFT_UPDATE.java: delay 1000 for the morph skill, else the given 0; action 4 writes D(1330051) and writeS(null)
		std::vector<uint8_t> update = itemtest::javaPacket(SM_CRAFT_UPDATE_OPCODE,
			PacketWriter().H(skillId).C(4).D(productId).D(0).D(0).D(0).D(skillId == MORPH ? 1000 : 0).D(1330051).H(0));
		// SM_CRAFT_ANIMATION.java: D(player), D(target), H(skillId 0), C(action 2)
		std::vector<uint8_t> animation =
			itemtest::javaPacket(SM_CRAFT_ANIMATION_OPCODE, PacketWriter().D(player().getObjectId()).D(targetObjId).H(0).C(2));
		return {update, animation};
	}

	/** One expected packet list: `first` then the packets of `rest` */
	static std::vector<std::vector<uint8_t>> then(std::vector<uint8_t> first, std::vector<std::vector<uint8_t>> rest) {
		rest.insert(rest.begin(), std::move(first));
		return rest;
	}

	std::vector<uint8_t> systemMessage(network::aion::serverpackets::SM_SYSTEM_MESSAGE&& message) { return serializedFor(std::move(message)); }

	/** @return false (and the case skips) without the test database; creates it once per process and inserts the player's players row */
	bool requireDatabase() {
		if (!isDatabaseEnabled())
			return false;
		setUpDatabaseOnce();
		insertPlayer(player().getObjectId(), "Holder", 9901);
		return true;
	}

	/** The recipe ids of the player's player_recipes rows, ascending */
	std::vector<int32_t> storedRecipes() {
		std::vector<int32_t> ids;
		auto con = commons::database::DatabaseFactory::getConnection();
		auto rs = con->prepareStatement("SELECT recipe_id FROM player_recipes WHERE player_id = " + std::to_string(player().getObjectId()) +
									  " ORDER BY recipe_id")
					  ->executeQuery();
		while (rs->next())
			ids.push_back(rs->getInt(1));
		return ids;
	}

	std::shared_ptr<const std::string> savedMissingAiHandlers;
	std::shared_ptr<const std::vector<float>> savedSkillXpCraftingRates;
	std::shared_ptr<const std::vector<float>> savedXpCraftingRates;
	std::vector<runtime::Ref<Player>> inWorld;
	std::vector<runtime::Ref<model::gameobjects::Item>> given;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<Npc>> npcs;
	std::vector<runtime::Ref<StaticObject>> stations;
	// the shipped values the bodies read (craft.properties:12, :16; logging.properties:12, :16) and no auto ban (the AuditLogger's punishment)
	mail::ConfigScope<int32_t> maxExpert{configs::main::CraftConfig::MAX_EXPERT_CRAFTING_SKILLS, 2};
	mail::ConfigScope<int32_t> maxMaster{configs::main::CraftConfig::MAX_MASTER_CRAFTING_SKILLS, 1};
	mail::ConfigScope<bool> logAudit{configs::main::LoggingConfig::LOG_AUDIT, true};
	mail::ConfigScope<bool> logCraft{configs::main::LoggingConfig::LOG_CRAFT, false};
	mail::ConfigScope<bool> punishment{configs::main::PunishmentConfig::PUNISHMENT_ENABLE, false};
};

/** Skips the case without the test database (the start of a TEST_F body) */
#define CRAFT_REQUIRE_DATABASE()                                                                                                                      \
	if (!requireDatabase())                                                                                                                           \
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";

} // namespace aion::gameserver::economy::test::craft
