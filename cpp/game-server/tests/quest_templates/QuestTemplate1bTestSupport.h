#pragma once

// P5-06c, M5d T-04's stage-1b part (m5d-plan.md §7, §18.3): the fixture of the eleven template kinds T-01b and T-03 ported -
// crafting_rewards, fountain_rewards, item_order, kill_in_world, kill_in_zone, kill_spawned, mentor_monster_hunt, relic_rewards, skill_use,
// work_order and xml_quest - on top of QuestTemplateTestSupport.h's (the five stage-1a kinds).
//
// - SetUp publishes the stage-1a holders again with this file's rows added: QUEST_DATA, NPC_DATA, ITEM_DATA, XML_QUESTS, SKILL_DATA (the base
//   fixture's skill rows and Weaponsmithing) and the experience table with all 66 levels (level-65 players for 13818 and 15220), and publishes
//   RECIPE_DATA with the one recipe of 5000, and ITEM_GROUPS_DATA with the bonus rows the finishes of 15205 and 5000 draw from (bound in the
//   item templates' load, whose XmlIDs the rows look up). CraftConfig's two crafting-skill limits and RatesConfig's GP and quest-AP rates are
//   set to Java's defaults (the unit tests load no configuration) and restored in TearDown.
// - useStartItem runs a start item's <queststart> action as CM_USE_ITEM does (E-10's QuestStartAction.act); SeededRnd fixes the random draws of
//   a case (BonusService's Chance.selectElement) and restores the generator after it.
// - Rows are copied from the shipped data (file:line beside each; whitespace between tags removed, an npc's <equipment> left out as the
//   comment says: no case reads it). The two <mentor_monster_hunt> rows are fabricated: no quest of the data uses the element
//   (m5d-plan.md T-04), so they name two real mentor quests of quest_data.xml, 37101 (MENTOR) and 37000 (MENTE).
// - Expected pages and effects come from the templates (handlers/template/*.java, handlers/models/xmlQuest/**) and
//   `oracle.py m5d-quest --no-profile --quest ID` (rewards after rates, registrations).

#include "QuestTemplateTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/CraftConfig.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/dataholders/ItemGroupsData.bind.h"
#include "aion/gameserver/dataholders/ItemGroupsData.h"
#include "aion/gameserver/dataholders/RecipeData.bind.h"
#include "aion/gameserver/dataholders/RecipeData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/SkillTreeData.bind.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/QuestStartAction.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::templates {

inline constexpr const char* T1B_QUESTS_XML =
	// quest_data.xml:1102-1110
	R"(<quest id="1127" name="Ancient Cube" nameId="1102227" quest_zone="Poeta" minlevel_permitted="2" max_repeat_count="1" can_report="true" )"
	R"(cannot_share="true" race_permitted="ELYOS" category="SIGNIFICANT"><collect_items><collect_item item_id="182200215" )"
	R"(count="1"/></collect_items><rewards gold="2400" exp="4015"/><quest_work_items><quest_work_item )"
	R"(item_id="182200215"/></quest_work_items></quest>)"
	// quest_data.xml:8293-8311
	R"(<quest id="1941" name="[Expert] Weaponsmithing Expert" nameId="1102941" quest_zone="Sanctum" minlevel_permitted="29" max_repeat_count="1" )"
	R"(cannot_share="true" race_permitted="ELYOS" category="SIGNIFICANT"><rewards exp="58283"><selectable_reward_item item_id="152200129" )"
	R"(count="1"/><selectable_reward_item item_id="152200130" count="1"/><selectable_reward_item item_id="152200131" )"
	R"(count="1"/><selectable_reward_item item_id="152200132" count="1"/><selectable_reward_item item_id="152200133" )"
	R"(count="1"/><selectable_reward_item item_id="152220446" count="1"/><selectable_reward_item item_id="152220447" )"
	R"(count="1"/><selectable_reward_item item_id="152221540" count="1"/><reward_item item_id="125001837" )"
	R"(count="1"/></rewards><start_conditions><finished quest_id="1973"/></start_conditions><quest_work_items><quest_work_item )"
	R"(item_id="182206009"/></quest_work_items></quest>)"
	// quest_data.xml:8569-8577 (1941's <finished>)
	R"(<quest id="1973" name="[Expert] Expert of Weaponsmithing" nameId="1102973" quest_zone="Sanctum" minlevel_permitted="29" )"
	R"(max_repeat_count="1" cannot_share="true" race_permitted="ELYOS" category="SIGNIFICANT"><collect_items><collect_item item_id="182206893" )"
	R"(count="1"/></collect_items><rewards exp="291412"/><start_conditions><finished quest_id="1972"/></start_conditions></quest>)"
	// quest_data.xml:2127-2134
	R"(<quest id="1323" name="Lost Jewel Box" nameId="1102423" quest_zone="Eltnen" minlevel_permitted="22" max_repeat_count="1" )"
	R"(cannot_share="true" race_permitted="ELYOS" category="QUEST"><rewards gold="2550" exp="100097"><reward_item item_id="186000002" )"
	R"(count="1"/></rewards><quest_work_items><quest_work_item item_id="182201309"/></quest_work_items></quest>)"
	// quest_data.xml:41527-41532
	R"(<quest id="13818" name="[Weekly] Indirect Warfare" nameId="1800167" quest_zone="Kaldor" minlevel_permitted="65" max_repeat_count="255" )"
	R"(race_permitted="ELYOS" category="IMPORTANT" repeat_cycle="SAT" data_driven="true"><rewards exp="4825175"><reward_item item_id="186000242" )"
	R"(count="1"/><reward_item item_id="186000236" count="4"/></rewards></quest>)"
	// quest_data.xml:21743-21746
	R"(<quest id="3910" name="Meaning of Life" nameId="1103009" quest_zone="Sanctum" minlevel_permitted="17" max_repeat_count="1" )"
	R"(cannot_share="true" race_permitted="ELYOS" category="IMPORTANT"><rewards gold="3000" exp="64850"/><class_permitted>WARRIOR SCOUT MAGE )"
	R"(GLADIATOR TEMPLAR ASSASSIN RANGER SORCERER SPIRIT_MASTER GUNNER BARD RIDER</class_permitted></quest>)"
	// quest_data.xml:25964-25967
	R"(<quest id="4922" name="The Gladiator Preceptor's Test" nameId="1104212" quest_zone="Pandaemonium" minlevel_permitted="31" )"
	R"(max_repeat_count="1" cannot_share="true" race_permitted="ASMODIANS" category="IMPORTANT" restricted="true"><rewards exp="633457" )"
	R"(title="88"/><class_permitted>WARRIOR GLADIATOR</class_permitted></quest>)"
	// quest_data.xml:68304-68309
	R"(<quest id="36504" name="[Daily] The Avian Flew" nameId="1126090" quest_zone="Fortuneers" minlevel_permitted="50" maxlevel_permitted="52" )"
	R"(max_repeat_count="255" cannot_share="true" race_permitted="ELYOS" category="FACTION" npcfaction_id="4" restricted="true"><rewards )"
	R"(exp="1239645"><reward_item item_id="186000102" count="2"/><reward_item item_id="188050737" count="1"/></rewards></quest>)"
	// quest_data.xml:51920-51931
	R"(<quest id="21281" name="[Relic Reward] Ancient Icon" nameId="1129724" quest_zone="Gelkmaros" minlevel_permitted="50" max_repeat_count="255" )"
	R"(race_permitted="ASMODIANS" extra_category="COIN_QUEST" category="NON_COUNT"><collect_items start_check="true"><collect_item )"
	R"(item_id="186000066" count="1"/><collect_item item_id="186000065" count="1"/><collect_item item_id="186000064" count="1"/><collect_item )"
	R"(item_id="186000063" count="1"/></collect_items><rewards ap="300" ccheck="0"/><rewards ap="600" ccheck="1"/><rewards ap="900" )"
	R"(ccheck="2"/><rewards ap="1200" ccheck="3"/></quest>)"
	// quest_data.xml:43577-43586
	R"(<quest id="15205" name="Cygnea Fountain of Luck" nameId="1801259" quest_zone="Cygnea" minlevel_permitted="55" max_repeat_count="255" )"
	R"(cannot_share="true" race_permitted="PC_ALL" extra_category="COIN_QUEST" category="QUEST"><collect_items><collect_item item_id="186000096" )"
	R"(count="1"/></collect_items><inventory_items><inventory_item item_id="186000096" count="1"/></inventory_items><rewards/><bonus level="2" )"
	R"(type="MEDAL"/></quest>)"
	// quest_data.xml:43695-43699
	R"(<quest id="15220" name="[Daily] Help in Henor" nameId="1801250" quest_zone="Cygnea" minlevel_permitted="65" max_repeat_count="255" )"
	R"(race_permitted="ELYOS" category="PUBLIC" repeat_cycle="ALL" data_driven="true"><rewards exp="3618881" gp="4"><reward_item )"
	R"(item_id="186000236" count="5"/></rewards></quest>)"
	// quest_data.xml:26739-26748
	R"(<quest id="5000" name="Steel Chisel Supplies" nameId="1190000" minlevel_permitted="9" maxlevel_permitted="65" max_repeat_count="255" )"
	R"(cannot_share="true" race_permitted="ELYOS" combineskill="40002" combine_skillpoint="1" category="TASK"><collect_items><collect_item )"
	R"(item_id="182290317" count="3"/></collect_items><rewards/><bonus type="TASK"/><quest_work_items><quest_work_item item_id="182290000" )"
	R"(count="4"/></quest_work_items></quest>)"
	// quest_data.xml:68750-68755
	R"(<quest id="37101" name="[Daily] No Bad Deed..." nameId="1126149" quest_zone="Orichalcum Key" minlevel_permitted="99" )"
	R"(maxlevel_permitted="49" max_repeat_count="255" cannot_share="true" race_permitted="ELYOS" category="FACTION" npcfaction_id="9" )"
	R"(mentor_type="MENTOR" restricted="true"><rewards exp="1333800"><reward_item item_id="186000114" count="1"/><reward_item item_id="188051126" )"
	R"(count="1"/></rewards></quest>)"
	// quest_data.xml:68623-68632
	R"(<quest id="37000" name="[Daily] Toxic Instruction" nameId="1126139" quest_zone="Kaisinel Academy" minlevel_permitted="99" )"
	R"(maxlevel_permitted="19" max_repeat_count="255" cannot_share="true" race_permitted="ELYOS" category="FACTION" npcfaction_id="8" )"
	R"(mentor_type="MENTE" restricted="true"><collect_items><collect_item item_id="182210034" count="5"/></collect_items><rewards )"
	R"(exp="18900"><reward_item item_id="186000002" count="10"/><reward_item item_id="188051123" count="1"/><reward_item item_id="164000227" )"
	R"(count="1"/></rewards></quest>)"
	// quest_data.xml:69081-69086
	R"(<quest id="39006" name="[Daily] A Handful of Problems" nameId="1140006" quest_zone="Brusthonin" minlevel_permitted="60" )"
	R"(max_repeat_count="255" race_permitted="ELYOS" category="PUBLIC" repeat_cycle="ALL" target="AREA"><rewards exp="283476"><reward_item )"
	R"(item_id="188051844" count="1"/><reward_item item_id="164000131" count="5"/></rewards></quest>)"
	// quest_data.xml:43572-43576
	R"(<quest id="15204" name="[Daily/Repeat] Sending a Message" nameId="1801153" quest_zone="Cygnea" minlevel_permitted="58" )"
	R"(max_repeat_count="255" race_permitted="ELYOS" category="QUEST" repeat_cycle="ALL" data_driven="true"><rewards exp="2948155"><reward_item )"
	R"(item_id="186000147" count="2"/></rewards></quest>)"
	// quest_data.xml:22242-22259
	R"(<quest id="3943" name="[Expert] Expert Weaponsmith" nameId="1103064" quest_zone="Sanctum" minlevel_permitted="29" max_repeat_count="1" )"
	R"(cannot_share="true" race_permitted="ELYOS" category="SIGNIFICANT"><rewards exp="58283" title="36"><selectable_reward_item )"
	R"(item_id="152200129" count="1"/><selectable_reward_item item_id="152200130" count="1"/><selectable_reward_item item_id="152200131" )"
	R"(count="1"/><selectable_reward_item item_id="152200132" count="1"/><selectable_reward_item item_id="152200133" )"
	R"(count="1"/><selectable_reward_item item_id="152220446" count="1"/><selectable_reward_item item_id="152220447" )"
	R"(count="1"/><selectable_reward_item item_id="152221540" count="1"/></rewards><start_conditions><finished )"
	R"(quest_id="3942"/></start_conditions><quest_work_items><quest_work_item item_id="182206102"/></quest_work_items></quest>)"
	// quest_data.xml:47558-47576
	R"(<quest id="19009" name="[Master] Weaponsmithing Master" nameId="1124509" quest_zone="Sanctum" minlevel_permitted="29" max_repeat_count="1" )"
	R"(cannot_share="true" race_permitted="ELYOS" category="SIGNIFICANT"><rewards exp="291412"><selectable_reward_item item_id="152201696" )"
	R"(count="1"/><selectable_reward_item item_id="152201697" count="1"/><selectable_reward_item item_id="152201698" )"
	R"(count="1"/><selectable_reward_item item_id="152201699" count="1"/><selectable_reward_item item_id="152201700" )"
	R"(count="1"/><selectable_reward_item item_id="152220522"/><selectable_reward_item item_id="152220523"/><selectable_reward_item )"
	R"(item_id="152221556"/><reward_item item_id="110900069"/></rewards><start_conditions><finished )"
	R"(quest_id="19008"/></start_conditions><quest_work_items><quest_work_item item_id="182206129"/></quest_work_items></quest>)"
	// quest_data.xml:44907-44917
	R"(<quest id="18214" name="Continued Training" nameId="1129632" quest_zone="Empyrean Crucible" minlevel_permitted="46" maxlevel_permitted="55" )"
	R"(max_repeat_count="255" cannot_share="true" race_permitted="PC_ALL" category="QUEST" restricted="true"><rewards )"
	R"(exp="3773237"><selectable_reward_item item_id="164000154" count="6"/><selectable_reward_item item_id="164000158" )"
	R"(count="6"/><selectable_reward_item item_id="164000156" count="6"/><reward_item item_id="188051489" )"
	R"(count="1"/></rewards><start_conditions><finished quest_id="18212"/></start_conditions></quest>)"
	// quest_data.xml:44996-45006
	R"(<quest id="18224" name="Fight Another Day" nameId="1199937" quest_zone="Empyrean Crucible" minlevel_permitted="61" max_repeat_count="255" )"
	R"(cannot_share="true" race_permitted="PC_ALL" category="QUEST"><rewards exp="4128210"><selectable_reward_item item_id="164000154" )"
	R"(count="7"/><selectable_reward_item item_id="164000158" count="7"/><selectable_reward_item item_id="164000156" count="7"/><reward_item )"
	R"(item_id="188051605" count="1"/></rewards><start_conditions><finished quest_id="18212"/></start_conditions></quest>)"
	// quest_data.xml:41390-41394
	R"(<quest id="13801" name="[Urgent Order] Minus One" nameId="1111675" quest_zone="Kaldor" minlevel_permitted="65" max_repeat_count="255" )"
	R"(race_permitted="ELYOS" category="PUBLIC" repeat_cycle="ALL" data_driven="true"><rewards exp="4825175" gp="3"><reward_item )"
	R"(item_id="186000236" count="5"/></rewards></quest>)"
	// quest_data.xml:46756-46761
	R"(<quest id="18738" name="Test Bomb, Test Bomb" nameId="1800936" quest_zone="Raksang Ruins" minlevel_permitted="60" max_repeat_count="1" )"
	R"(race_permitted="ELYOS" category="IMPORTANT" data_driven="true"><rewards exp="9492173"><selectable_reward_item item_id="160003584" )"
	R"(count="5"/><selectable_reward_item item_id="160003585" count="5"/></rewards></quest>)"
	// quest_data.xml:1566-1571
	R"(<quest id="1182" name="Ancient Stone Fragment" nameId="1102352" quest_zone="Verteron" minlevel_permitted="17" max_repeat_count="1" )"
	R"(cannot_share="true" race_permitted="ELYOS" category="QUEST"><rewards gold="14200" exp="50400"/><quest_work_items><quest_work_item )"
	R"(item_id="182200549"/></quest_work_items></quest>)"
	// quest_data.xml:11040-11047
	R"(<quest id="2274" name="Black Claw Baton" nameId="1103374" quest_zone="Altgard" minlevel_permitted="16" max_repeat_count="1" )"
	R"(cannot_share="true" race_permitted="ASMODIANS" category="QUEST"><rewards gold="10150" exp="36300"><reward_item item_id="166000191" )"
	R"(count="2"/></rewards><quest_work_items><quest_work_item item_id="182203249"/></quest_work_items></quest>)"
	// quest_data.xml:4288-4295
	R"(<quest id="1514" name="The Ettin's Necklace" nameId="1102714" quest_zone="Heiron" minlevel_permitted="31" max_repeat_count="1" )"
	R"(cannot_share="true" race_permitted="ELYOS" category="QUEST" restricted="true"><rewards exp="404557"><reward_item item_id="166000192" )"
	R"(count="3"/></rewards><quest_work_items><quest_work_item item_id="182201710"/></quest_work_items></quest>)";

/** The quest_script_data rows of the eleven kinds (quest_script_data/<file>:<line>), and the two fabricated mentor rows */
inline constexpr const char* T1B_SCRIPTS_XML =
	// poeta.xml:69-111
	R"(<xml_quest id="1127" start_npc_ids="798008"><on_talk_event ids="700001"><conditions operate="AND"><quest_status value="START" )"
	R"(op="EQUAL"/></conditions><var value="0"><npc id="700001"><dialog id="-1"><operations><npc_use><finish><give_item item_id="182200215" )"
	R"(count="1"/><set_quest_var var_id="0" value="1"/></finish></npc_use></operations></dialog></npc></var><var value="1"><npc )"
	R"(id="798008"><dialog id="31"><operations><npc_dialog id="2375"/></operations></dialog><dialog )"
	R"(id="39"><operations><collect_items><true><set_quest_status status="REWARD"/><npc_dialog id="5"/></true><false><npc_dialog )"
	R"(id="2716"/></false></collect_items></operations></dialog></npc></var></on_talk_event></xml_quest>)"
	// sanctum.xml:547
	R"(<crafting_rewards id="1941" start_npc_id="203788" end_npc_id="203700" movie="93" skill_id="40002" level_reward="400"/>)"
	// eltnen.xml:399
	R"(<item_order id="1323" talk_npc_id1="730019" end_npc_id="203939"/>)"
	// kaldor.xml:122
	R"(<kill_in_world id="13818" start_npc_ids="804588" worlds="600090000" amount="6"/>)"
	// sanctum.xml:392-394
	R"(<skill_use id="3910" start_npc_ids="203707"><skill end_var="100" ids="9912"/><!-- Elemental Stone of Resurrection --></skill_use>)"
	// pandaemonium.xml:359-361
	R"(<skill_use id="4922" start_npc_ids="204056"><skill end_var="10" ids="599"/><!-- Gladiator: Daevic Fury --></skill_use>)"
	// fortuneers.xml:104-106
	R"(<kill_spawned id="36504" end_npc_ids="799837 799838"><monster end_var="1" spawner_object_id="700759" npc_ids="216608"/></kill_spawned>)"
	// gelkmaros.xml:416
	R"(<relic_rewards id="21281" start_npc_ids="799949 205621"/>)"
	// cygnea.xml:179
	R"(<fountain_rewards id="15205" start_npc_ids="730556 804788"/>)"
	// cygnea.xml:182
	R"(<kill_in_zone id="15220" start_npc_ids="804876" zones="CRIMSON_HILLS_210070000 CORAL_RISE_210070000" amount="2" level_diff="5"/>)"
	// work_order.xml:4-6
	R"(<work_order id="5000" start_npc_ids="203788 830062" recipe_id="155004001"><give_component item_id="182290000" count="4"/></work_order>)"
	// brusthonin.xml:227
	R"(<kill_in_world id="39006" end_npc_ids="800500" worlds="220050000" amount="5" invasion_world="220050000"/>)"
	// cygnea.xml:190
	R"(<kill_in_world id="15204" start_npc_ids="804704" worlds="210070000" amount="5" end_npc_ids="804704" level_diff="5"/>)"
	// fabricated (m5d-plan.md T-04): mentor_monster_hunt is mapped (XMLQuests.java:19-27) but no quest of the data uses it
	R"(<mentor_monster_hunt id="37101" start_npc_ids="799837" min_mente_level="10" max_mente_level="20"/>)"
	R"(<mentor_monster_hunt id="37000" end_npc_ids="799838"/>)"
	// sanctum.xml:540
	R"(<crafting_rewards id="3943" start_npc_id="203788" end_npc_id="203700" skill_id="40002" level_reward="400"/>)"
	// sanctum.xml:554
	R"(<crafting_rewards id="19009" start_npc_id="203788" end_npc_id="798600" movie="109" skill_id="40002" level_reward="500"/>)"
	// empyrean_crucible.xml:83
	R"(<kill_in_world id="18214" start_npc_ids="205985" worlds="300350000 300360000" amount="5"/>)"
	// empyrean_crucible.xml:85
	R"(<kill_in_world id="18224" start_npc_ids="205985" start_dialog_id="4762" end_dialog_id="10002" worlds="300350000 300360000" amount="5"/>)"
	// kaldor.xml:121
	R"(<kill_in_world id="13801" end_npc_ids="802431" worlds="600090000"/>)"
	// raksang_ruins.xml:47-49
	R"(<skill_use id="18738" start_npc_ids="206378 206379 206380" end_npc_ids="804965"><skill ids="10981" var_num="1" end_var="10"/></skill_use>)"
	// verteron.xml:198
	R"(<item_order id="1182" end_npc_id="203099"/>)"
	// heiron.xml:441
	R"(<item_order id="1514" talk_npc_id1="204582" talk_npc_id2="204505" end_npc_id="203831"/>)"
	// altgard.xml:154-157
	R"(<report_to_many id="2274" start_item_id="182203249"><npc_infos npc_ids="203668"/><npc_infos npc_ids="203560"/></report_to_many>)";

inline constexpr const char* T1B_NPCS_XML =
	// npc_templates.xml:461611-461617
	R"(<npc_template npc_id="798008" level="9" name="baevrunerk" name_id="351141" height="1.16875" title_id="350421" group_drop="NONE" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" attack_speed="2100" )"
	R"(hpgauge="3"><stats maxHp="2568"><speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23" group_run_fight="4.23" )"
	R"(/></stats><bound_radius front="0.595" side="0.3774" upper="1.16875" /><talk_info distance="5" is_dialog="true" func_dialogs="47" )"
	R"(can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:439522-439526
	R"(<npc_template npc_id="700001" level="1" name="ancient cube" name_id="350700" height="0.3" group_drop="NONE" rank="DISCIPLINED" )"
	R"(rating="NORMAL" tribe="FIELD_OBJECT_LIGHT" type="GENERAL" ai="quest_use_item" sangle="0" attack_speed="2000" hpgauge="3"><stats maxHp="172" )"
	R"(/><bound_radius front="0.375" side="0.525" upper="0.3" /><talk_info distance="3" delay="3" is_dialog="true" can_talk_invisible="false" )"
	R"(/></npc_template>)"
	// npc_templates.xml:10489-10502 without its <equipment> (:10493-10499)
	R"(<npc_template npc_id="203788" level="40" name="anteros" name_id="351279" height="1.3" title_id="350402" group_drop="LIGHT" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="10" sangle="300" arange="2" )"
	R"(attack_speed="2000" hpgauge="3"><stats maxHp="9426"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" )"
	R"(/></stats><bound_radius front="0.1625" side="0.2275" upper="1.3" /><talk_info distance="5" is_dialog="true" func_dialogs="46 79 58 80" )"
	R"(can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:9190-9204 without its <equipment> (:9194-9201)
	R"(<npc_template npc_id="203700" level="60" name="fasimedes" name_id="351200" height="2" title_id="350335" group_drop="LIGHT" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GUARD" type="ABYSS_GUARD" ai="simple_abyssguard" srange="7" sangle="300" arange="2" )"
	R"(attack_speed="2000" hpgauge="3" state="6"><stats maxHp="23691"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" )"
	R"(group_run_fight="4.2" /></stats><bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" )"
	R"(can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:452208-452214
	R"(<npc_template npc_id="730019" level="50" name="lodas" name_id="351826" height="8" title_id="350513" group_drop="NONE" rank="DISCIPLINED" )"
	R"(rating="NORMAL" tribe="USEALL" type="GENERAL" ai="general" srange="12" attack_speed="2000" hpgauge="3"><stats maxHp="14614"><speeds )"
	R"(walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius front="0.375" side="0.525" upper="8" )"
	R"(/><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:12672-12688 without its <equipment> (:12676-12685)
	R"(<npc_template npc_id="203939" level="45" name="justachys" name_id="351738" height="2.4" title_id="350300" group_drop="LIGHT" rank="VETERAN" )"
	R"(rating="ELITE" race="ELYOS" tribe="GUARD" type="ABYSS_GUARD" ai="simple_abyssguard" srange="15" arange="2" attack_speed="2100" hpgauge="14" )"
	R"(cancel_level="20"><stats maxHp="127590"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="8" group_run_fight="4.2" )"
	R"(/></stats><bound_radius front="0.3" side="0.42" upper="2.4" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" )"
	R"(/></npc_template>)"
	// npc_templates.xml:521417-521431 without its <equipment> (:521421-521428)
	R"(<npc_template npc_id="804588" level="65" name="leton" name_id="465625" height="2.16" title_id="465619" group_drop="LIGHT" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" arange="2" attack_speed="2100" )"
	R"(hpgauge="3"><stats maxHp="31089"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.3" side="0.42" upper="2.16" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:9298-9312 without its <equipment> (:9302-9309)
	R"(<npc_template npc_id="203707" level="53" name="thrasymedes" name_id="351210" height="2" title_id="350318" group_drop="LIGHT" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GUARD" type="ABYSS_GUARD" ai="simple_abyssguard" srange="7" sangle="300" arange="2" )"
	R"(attack_speed="2000" hpgauge="3"><stats maxHp="16990"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" )"
	R"(/></stats><bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" )"
	R"(/></npc_template>)"
	// npc_templates.xml:14270-14284 without its <equipment> (:14274-14281)
	R"(<npc_template npc_id="204056" level="55" name="traufnir" name_id="352493" height="2" title_id="350315" group_drop="DARK" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ASMODIANS" tribe="GUARD_DARK" type="ABYSS_GUARD" ai="simple_abyssguard" srange="7" sangle="300" arange="2" )"
	R"(attack_speed="2000" hpgauge="3"><stats maxHp="18756"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" )"
	R"(/></stats><bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" )"
	R"(/></npc_template>)"
	// npc_templates.xml:476137-476150 without its <equipment> (:476141-476147)
	R"(<npc_template npc_id="799837" level="50" name="rima" name_id="355151" height="1.8" title_id="370253" group_drop="LIGHT" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" attack_speed="2000" )"
	R"(hpgauge="3"><stats maxHp="14614"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.25" side="0.35" upper="1.8" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:476151-476164 without its <equipment> (:476155-476161)
	R"(<npc_template npc_id="799838" level="50" name="socinus" name_id="355152" height="1.8" title_id="370253" group_drop="LIGHT" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" )"
	R"(attack_speed="2000" hpgauge="3"><stats maxHp="14614"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" )"
	R"(/></stats><bound_radius front="0.25" side="0.35" upper="1.8" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" )"
	R"(/></npc_template>)"
	// npc_templates.xml:442957-442961
	R"(<npc_template npc_id="700759" level="1" name="pluma bait" name_id="371494" height="1" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" )"
	R"(tribe="FIELD_OBJECT_LIGHT" type="GENERAL" ai="quest_use_item" sangle="0" attack_speed="2000" hpgauge="3"><stats maxHp="172" /><bound_radius )"
	R"(front="0.125" side="0.175" upper="1" /><talk_info distance="5" delay="3" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:105029-105034
	R"(<npc_template npc_id="216608" level="52" name="lightningbeak pabu" name_id="321942" height="1.74" group_drop="LUPYLLINI" rank="SEASONED" )"
	R"(rating="NORMAL" race="BEAST" tribe="MONSTER" ai="aggressive" srange="8" sangle="240" arange="2" attack_speed="2081" hpgauge="4" )"
	R"(floatcorpse="true" cancel_level="90"><stats maxHp="17801"><speeds walk="0.34" group_walk="0.34" run="6" run_fight="6" group_run_fight="6" )"
	R"(/></stats><bound_radius front="1.5" side="1.5" upper="4.387" /></npc_template>)"
	// npc_templates.xml:477683-477696 without its <equipment> (:477687-477693)
	R"(<npc_template npc_id="799949" level="45" name="frigga" name_id="354667" height="2" title_id="314357" group_drop="DARK" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ASMODIANS" tribe="GENERAL_DARK" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" attack_speed="2000" )"
	R"(hpgauge="3"><stats maxHp="12428"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" func_dialogs="59" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:29792-29805 without its <equipment> (:29796-29802)
	R"(<npc_template npc_id="205621" level="56" name="valith" name_id="355384" height="2.16" title_id="370408" group_drop="DARK" rank="NOVICE" )"
	R"(rating="NORMAL" race="ASMODIANS" tribe="GENERAL_DARK" type="GENERAL" ai="general" srange="20" sangle="300" attack_speed="2000" )"
	R"(hpgauge="2"><stats maxHp="12733"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.3" side="0.42" upper="2.16" /><talk_info distance="5" is_dialog="true" func_dialogs="59" can_talk_invisible="false" )"
	R"(/></npc_template>)"
	// npc_templates.xml:454877-454881
	R"(<npc_template npc_id="730556" level="1" name="oriel coin fountain" name_id="462427" height="2" group_drop="NONE" rank="DISCIPLINED" )"
	R"(rating="NORMAL" tribe="FIELD_OBJECT_LIGHT" type="GENERAL" ai="general" srange="12" attack_speed="2000" hpgauge="3"><stats maxHp="172" )"
	R"(/><bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="8" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:523751-523755
	R"(<npc_template npc_id="804788" level="1" name="fountain of luck" name_id="465927" height="2" group_drop="NONE" rank="DISCIPLINED" )"
	R"(rating="NORMAL" tribe="FIELD_OBJECT_LIGHT" type="GENERAL" ai="aggressive" srange="12" attack_speed="2000" hpgauge="3"><stats maxHp="172" )"
	R"(/><bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="8" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:524600-524615 without its <equipment> (:524604-524612)
	R"(<npc_template npc_id="804876" level="60" name="monodia" name_id="466102" height="2" title_id="370723" group_drop="LIGHT" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="10" arange="2" attack_speed="2000" hpgauge="3"><stats )"
	R"(maxHp="23691"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius front="0.25" )"
	R"(side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:531401-531414 without its <equipment> (:531405-531411)
	R"(<npc_template npc_id="830062" level="40" name="auminus" name_id="461247" height="1.3" title_id="350402" group_drop="LIGHT" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="10" sangle="300" arange="2" )"
	R"(attack_speed="2000" hpgauge="3"><stats maxHp="9426"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" )"
	R"(/></stats><bound_radius front="0.1625" side="0.2275" upper="1.3" /><talk_info distance="5" is_dialog="true" func_dialogs="46 79 58 80" )"
	R"(can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:522812-522825 without its <equipment> (:522816-522822)
	R"(<npc_template npc_id="804704" level="65" name="eukraton" name_id="465821" height="2.1" title_id="370722" group_drop="LIGHT" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" arange="2" attack_speed="2000" )"
	R"(hpgauge="3"><stats maxHp="26116"><speeds walk="1.3" group_walk="1.3" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.2625" side="0.3675" upper="2.1" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:465885-465898 without its <equipment> (:465889-465895)
	R"(<npc_template npc_id="798600" level="50" name="eremitia" name_id="351501" height="1.8" title_id="370181" group_drop="LIGHT" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" )"
	R"(attack_speed="2000" hpgauge="3" state="6"><stats maxHp="14614"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" )"
	R"(group_run_fight="4.2" /></stats><bound_radius front="0.25" side="0.35" upper="1.8" /><talk_info distance="5" is_dialog="true" )"
	R"(can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:34298-34313 without its <equipment> (:34302-34310)
	R"(<npc_template npc_id="205985" level="55" name="junos" name_id="461375" height="2" title_id="370379" group_drop="LIGHT" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="300" attack_speed="2000" hpgauge="3"><stats )"
	R"(maxHp="18756"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius front="0.25" )"
	R"(side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:2554-2569 without its <equipment> (:2558-2566)
	R"(<npc_template npc_id="203099" level="25" name="selene" name_id="351024" height="2" title_id="350428" group_drop="LIGHT" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ELYOS" tribe="GUARD" type="ABYSS_GUARD" ai="simple_abyssguard" srange="7" sangle="240" arange="2" attack_speed="2000" )"
	R"(hpgauge="3"><stats maxHp="4136"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:20575-20590 without its <equipment> (:20579-20587)
	R"(<npc_template npc_id="204582" level="50" name="ibelia" name_id="351985" height="2.4" title_id="350612" group_drop="LIGHT" rank="VETERAN" )"
	R"(rating="ELITE" race="ELYOS" tribe="GUARD" type="ABYSS_GUARD" ai="simple_abyssguard" srange="15" arange="2" attack_speed="1900" hpgauge="14" )"
	R"(cancel_level="20"><stats maxHp="115839"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="8" group_run_fight="4.2" )"
	R"(/></stats><bound_radius front="0.3" side="0.42" upper="2.4" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" )"
	R"(/></npc_template>)"
	// npc_templates.xml:19528-19542 without its <equipment> (:19532-19539)
	R"(<npc_template npc_id="204505" level="50" name="sulates" name_id="351905" height="2" title_id="350427" group_drop="LIGHT" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GUARD" ai="general" srange="20" sangle="300" arange="2" attack_speed="2000" )"
	R"(hpgauge="3"><stats maxHp="14535"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="7" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)";

inline constexpr const char* T1B_ITEMS_XML =
	// item_templates.xml:876981-876983
	R"(<item_template id="182200215" name="Ancient Cube" level="1" cName="quest_1127a" mask="20545" item_group="QUEST" quality="COMMON" price="1" )"
	R"(desc="1106447"><inventory id="2"/></item_template>)"
	// item_templates.xml:746311-746316
	R"(<item_template id="152200129" name="Design: Expert Adamantium Dagger" level="35" cName="rec_l_ws_dagger_n_l2_p_35a" mask="12414" )"
	R"(max_stack_count="20" item_group="RECIPE" quality="LEGEND" price="480000" race="ELYOS" desc="731885" activate_target="STANDALONE" )"
	R"(activate_count="1"><actions><craftlearn recipeid="155000129"/></actions><uselimits usedelayid="52"/></item_template>)"
	// item_templates.xml:696339-696347
	R"(<item_template id="125001837" name="Triumphant Helm" level="10" cName="ac_head_n_u0_q_10a" mask="4172" item_group="HEAD" quality="UNIQUE" )"
	R"(price="2350" desc="741234"><modifiers><add name="MAXHP" value="145" bonus="true"/><add name="MAXMP" value="145" bonus="true"/><rate )"
	R"(name="FLY_SPEED" value="7" bonus="true"/><add name="MAGICAL_RESIST" value="48" bonus="true"/><add name="EVASION" value="32" )"
	R"(bonus="true"/></modifiers></item_template>)"
	// item_templates.xml:877416-877418
	R"(<item_template id="182201309" name="Jewel Box" level="1" cName="quest_1323a" mask="20545" item_group="QUEST" quality="COMMON" price="1" )"
	R"(desc="1106716"><inventory id="2"/></item_template>)"
	// item_templates.xml:896236-896238
	R"(<item_template id="186000002" name="Bronze Coin" level="20" cName="coin_02" mask="12410" max_stack_count="10000" item_group="COINS" )"
	R"(quality="COMMON" price="8208" desc="703679"><inventory id="1"/></item_template>)"
	// item_templates.xml:896799-896801
	R"(<item_template id="186000242" name="Ceramium Medal" level="65" cName="medal_05" mask="12364" max_stack_count="10000" item_group="MEDALS" )"
	R"(quality="RARE" price="25000" desc="812760"><inventory id="1"/></item_template>)"
	// item_templates.xml:896778-896780
	R"(<item_template id="186000236" name="Blood Mark" level="65" cName="coin_pvp_01" mask="12360" max_stack_count="10000" item_group="COINS" )"
	R"(quality="RARE" price="100" desc="812754"><inventory id="1"/></item_template>)"
	// item_templates.xml:896442-896444
	R"(<item_template id="186000102" name="Fortuneers Token" level="50" cName="coin_hunters_01" mask="12360" max_stack_count="10000" )"
	R"(item_group="COINS" quality="COMMON" price="5" desc="762228"><inventory id="1"/></item_template>)"
	// item_templates.xml:905215-905220
	R"(<item_template id="188050737" name="Fortuneer Reward Chest" level="50" cName="wrap_basic_reward_50c" casting_delay="1500" mask="12364" )"
	R"(max_stack_count="100" quality="COMMON" price="5" restrict="50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50" desc="766313" )"
	R"(activate_target="STANDALONE" activate_count="1"><actions><decompose/></actions><uselimits usedelay="5000" usedelayid="85"/></item_template>)"
	// item_templates.xml:896362-896364
	R"(<item_template id="186000066" name="Lesser Ancient Icon" level="50" cName="treasure_04d" mask="12360" max_stack_count="100" quality="RARE" )"
	R"(price="300" desc="746115"><acquisition type="AP" ap="300"/></item_template>)"
	// item_templates.xml:896359-896361
	R"(<item_template id="186000065" name="Ancient Icon" level="50" cName="treasure_03d" mask="12360" max_stack_count="100" quality="RARE" )"
	R"(price="600" desc="746114"><acquisition type="AP" ap="600"/></item_template>)"
	// item_templates.xml:896356-896358
	R"(<item_template id="186000064" name="Greater Ancient Icon" level="50" cName="treasure_02d" mask="12360" max_stack_count="100" quality="RARE" )"
	R"(price="900" desc="746113"><acquisition type="AP" ap="900"/></item_template>)"
	// item_templates.xml:896353-896355
	R"(<item_template id="186000063" name="Major Ancient Icon" level="50" cName="treasure_01d" mask="12360" max_stack_count="100" quality="RARE" )"
	R"(price="1200" desc="746112"><acquisition type="AP" ap="1200"/></item_template>)"
	// item_templates.xml:896428-896430
	R"(<item_template id="186000096" name="Platinum Medal" level="60" cName="medal_03" mask="12364" max_stack_count="1000" item_group="MEDALS" )"
	R"(quality="RARE" price="15000" desc="752849"><inventory id="1"/></item_template>)"
	// item_templates.xml:890763-890765
	R"(<item_template id="182290000" name="Issued Steel Ingot" level="1" cName="item_part_ws_q5000_a" mask="28736" max_stack_count="100" )"
	R"(quality="COMMON" price="1" desc="1191001"><inventory id="2"/></item_template>)"
	// item_templates.xml:891714-891716
	R"(<item_template id="182290317" name="Steel Chisel" level="1" cName="item_ws_q5000" mask="28736" max_stack_count="100" quality="COMMON" )"
	R"(price="1" desc="1192001"><inventory id="2"/></item_template>)"
	// item_templates.xml:877183-877189
	R"(<item_template id="182200549" name="Ancient Stone Fragment" level="16" cName="quest_1182a" mask="20545" item_group="QUEST" quality="COMMON" )"
	R"(price="1" desc="1106537" activate_target="STANDALONE" activate_count="1000"><actions><queststart questid="1182"/></actions><uselimits )"
	R"(usedelay="15000" usedelayid="61"/><inventory id="2"/></item_template>)"
	// item_templates.xml:879133-879139
	R"(<item_template id="182203249" name="Chieftain's Baton" level="1" cName="quest_2274a" mask="20545" item_group="QUEST" quality="COMMON" )"
	R"(price="1" desc="1109199" activate_target="STANDALONE" activate_count="1000"><actions><queststart questid="2274"/></actions><uselimits )"
	R"(usedelay="2000" usedelayid="61"/><inventory id="2"/></item_template>)"
	// the items of T1B_ITEM_GROUPS_XML's rows below (ItemRaceEntry.afterUnmarshal wants each one's template in the same load)
	// item_templates.xml:874551
	R"(<item_template id="182005206" name="Rusted Medal" level="40" cName="junk_gathering_test_01" mask="12414" max_stack_count="1000" )"
	R"(quality="JUNK" price="500" desc="741640"/>)"
	// item_templates.xml:896559-896561
	R"(<item_template id="186000147" name="Mithril Medal" level="60" cName="medal_04" mask="12364" max_stack_count="1000" item_group="MEDALS" )"
	R"(quality="RARE" price="15000" desc="792098"><inventory id="1"/></item_template>)"
	// item_templates.xml:744888
	R"(<item_template id="152020001" name="Steel Ingot" level="10" cName="ws_part_steel_01a" mask="12414" max_stack_count="1000" quality="COMMON" )"
	R"(price="40" race="ELYOS" desc="707452"/>)"
	// item_templates.xml:850039
	R"(<item_template id="169400010" name="Charcoal Briquette" level="10" cName="shopmaterial_ws_01a" mask="12414" max_stack_count="1000" )"
	R"(quality="COMMON" price="100" desc="703688"/>)"
	// item_templates.xml:850049
	R"(<item_template id="169400020" name="Lesser Whetstone" level="10" cName="shopmaterial_ws_11a" mask="12414" max_stack_count="1000" )"
	R"(quality="COMMON" price="25" desc="703698"/>)"
	// item_templates.xml:759637-759642
	R"(<item_template id="152202286" name="Design: Steel Ingot Bundle" level="10" cName="rec_l_ws_ws_part_mass_steel_01a" mask="12414" )"
	R"(max_stack_count="20" item_group="RECIPE" quality="COMMON" price="1200" race="ELYOS" desc="767028" activate_target="STANDALONE" )"
	R"(activate_count="1"><actions><craftlearn recipeid="155002286"/></actions><uselimits usedelayid="52"/></item_template>)"
	// item_templates.xml:785483-785488
	R"(<item_template id="152207288" name="Design: Steel Ingot Bundle" level="10" cName="rec_d_ws_ws_part_mass_d_steel_01a" mask="12414" )"
	R"(max_stack_count="20" item_group="RECIPE" quality="COMMON" price="1200" race="ASMODIANS" desc="767648" activate_target="STANDALONE" )"
	R"(activate_count="1"><actions><craftlearn recipeid="155007288"/></actions><uselimits usedelayid="52"/></item_template>)"
	// item_templates.xml:745987-745992
	R"(<item_template id="152200075" name="Design: Steel Ingot" level="10" cName="rec_l_ws_ws_part_steel_01a" mask="12414" max_stack_count="20" )"
	R"(item_group="RECIPE" quality="COMMON" price="100" race="ELYOS" desc="731831" activate_target="STANDALONE" )"
	R"(activate_count="1"><actions><craftlearn recipeid="155000075"/></actions><uselimits usedelayid="52"/></item_template>)"
	// item_templates.xml:771821-771826
	R"(<item_template id="152205075" name="Design: Steel Ingot" level="10" cName="rec_d_ws_ws_part_d_steel_01a" mask="12414" max_stack_count="20" )"
	R"(item_group="RECIPE" quality="COMMON" price="100" race="ASMODIANS" desc="736273" activate_target="STANDALONE" )"
	R"(activate_count="1"><actions><craftlearn recipeid="155005075"/></actions><uselimits usedelayid="52"/></item_template>)";

/**
 * items/item_groups.xml, the bonus groups the finishes of 15205 (MEDAL, level 2) and 5000 (TASK, combineskill 40002, combine_skillpoint 1)
 * draw from (BonusService.getQuestBonus), cut to the rows those quests can match; each row verbatim (file:line):
 * - medals: its five level-2 rows (ItemRaceEntry/IdLevelReward.matchesLevel: only a row of the bonus level matches);
 * - the four TASK groups: the rows of skill 40002 that cover skill point 1 (CraftItem minLevel..maxLevel, CraftRecipe level..min(level + 40,
 *   level / 100 * 100 + 99)), both races, and for craft_materials, which has none, its first row of skill 40002 so that the group stays (a
 *   group is drawn by its chance before its rows are filtered);
 * - the eighteen pet food groups as empty elements: ItemGroupsData.afterUnmarshal reads each one (as tests/economy/P5-09a/BonusServiceTest.cpp).
 * The other groups (food, manastones, medicine, events) are left out: no quest here asks for their types.
 */
inline constexpr const char* T1B_ITEM_GROUPS_XML =
	R"(<item_groups>)"
	// :3, :122, :266
	R"(<craft_materials bonusType="TASK" chance="47"><item id="152020001" skill="40002" minLevel="5" maxLevel="10"/></craft_materials>)"
	// :267, :387, :392, :421
	R"(<craft_shop bonusType="TASK" chance="47"><item id="169400010" skill="40002" minLevel="1" maxLevel="1"/><item id="169400020" skill="40002" )"
	R"(minLevel="1" maxLevel="400"/></craft_shop>)"
	// :422, :541, :671, :683
	R"(<craft_bundles bonusType="TASK" chance="2"><item id="152202286" skill="40002" level="1"/><item id="152207288" skill="40002" )"
	R"(level="1"/></craft_bundles>)"
	// :684, :2237, :4072, :4345
	R"(<craft_recipes bonusType="TASK" chance="4"><item id="152200075" skill="40002" level="1"/><item id="152205075" skill="40002" )"
	R"(level="1"/></craft_recipes>)"
	// :4508, :4513-4517, :4539
	R"(<medals bonusType="MEDAL" chance="100"><item id="182005206" level="2" count="1" chance="67"/><item id="186000096" level="2" count="1" )"
	R"(chance="13.5"/><item id="186000096" level="2" count="2" chance="6.5"/><item id="186000147" level="2" count="1" chance="10"/><item )"
	R"(id="186000147" level="2" count="2" chance="3"/></medals>)"
	// :5257-6124, the pet food groups without their rows
	R"(<feed_fluid group="JUNK"></feed_fluid><feed_armor group="JUNK"></feed_armor><feed_thorn group="JUNK"></feed_thorn><feed_bone )"
	R"(group="JUNK"></feed_bone><feed_balaur_material group="JUNK"></feed_balaur_material><feed_soul group="JUNK"></feed_soul><feed_exclude )"
	R"(group="JUNK"></feed_exclude><stinking_junk group="JUNK"></stinking_junk><feed_healthy_all group="NONE"></feed_healthy_all><feed_healthy_spicy )"
	R"(group="NONE"></feed_healthy_spicy><feed_powder_biscuit group="NONE"></feed_powder_biscuit><feed_crystal_biscuit )"
	R"(group="NONE"></feed_crystal_biscuit><feed_gem_biscuit group="NONE"></feed_gem_biscuit><poppy_snack group="NONE"></poppy_snack>)"
	R"(<tasty_poppy_snack group="NONE"></tasty_poppy_snack><nutritious_poppy_snack group="NONE"></nutritious_poppy_snack><feed_shugo_event_coin )"
	R"(group="NONE"></feed_shugo_event_coin><feed_aether_cherry group="NONE"></feed_aether_cherry>)"
	R"(</item_groups>)";

inline constexpr const char* T1B_SKILLS_XML =
	// skill_templates.xml:204345-204349
	R"(<skill_template skill_id="40002" name="Weaponsmithing" nameId="280385" stack="WEAPONSMITH" lvl="1" skilltype="NONE" skillsubtype="NONE" )"
	R"(tslot="NONE" activation="NONE" cooldown="0" duration="0"><useconditions><move_casting allow="false" /></useconditions></skill_template>)";

inline constexpr const char* T1B_RECIPES_XML =
	R"(<recipe_templates>)"
	// recipe_templates.xml:33787-33791
	R"(<recipe_template id="155004001" nameid="704205" skillid="40002" race="ELYOS" skillpoint="1" productid="182290317" )"
	R"(quantity="1"><components_data><component quantity="1" itemid="182290000"/></components_data></recipe_template>)"
	R"(</recipe_templates>)";

/** player_experience_table.xml:3-68, all 66 levels (the comments left out) */
inline constexpr const char* T1B_EXPERIENCE_TABLE_XML =
	"<player_experience_table><exp>0</exp><exp>400</exp><exp>1433</exp><exp>3820</exp><exp>9054</exp><exp>17655</exp><exp>30978</exp>"
	"<exp>52010</exp><exp>82982</exp><exp>126069</exp><exp>182252</exp><exp>260622</exp><exp>360825</exp><exp>490331</exp><exp>649169</exp>"
	"<exp>844378</exp><exp>1083018</exp><exp>1401356</exp><exp>1808613</exp><exp>2314771</exp><exp>2941893</exp><exp>3769257</exp>"
	"<exp>4811154</exp><exp>6110198</exp><exp>7632340</exp><exp>9377726</exp><exp>11395643</exp><exp>13731725</exp><exp>16339413</exp>"
	"<exp>19378549</exp><exp>23162749</exp><exp>27585843</exp><exp>32841197</exp><exp>39127217</exp><exp>47350762</exp><exp>57829684</exp>"
	"<exp>70654362</exp><exp>87571065</exp><exp>107018757</exp><exp>129815732</exp><exp>157211282</exp><exp>189272188</exp>"
	"<exp>226933751</exp><exp>267247400</exp><exp>310053925</exp><exp>355815203</exp><exp>404823687</exp><exp>456685353</exp>"
	"<exp>511683757</exp><exp>570162075</exp><exp>632268545</exp><exp>701585822</exp><exp>776831823</exp><exp>857090855</exp>"
	"<exp>947120930</exp><exp>1051346275</exp><exp>1175571620</exp><exp>1318550121</exp><exp>1484090156</exp><exp>1674064804</exp>"
	"<exp>1913274732</exp><exp>2162140395</exp><exp>2419819338</exp><exp>2700930959</exp><exp>3209499233</exp><exp>3794060468</exp>"
	"</player_experience_table>";

inline constexpr int32_t BAEVRUNERK = 798008;    // Poeta: 1127's start and end npc, the npc of its var-1 script (poeta.xml:69, 89)
inline constexpr int32_t ANCIENT_CUBE = 700001;  // 1127's quest object, the ids of its <on_talk_event> (poeta.xml:70)
inline constexpr int32_t ANTEROS = 203788;       // Sanctum: 1941's start npc (sanctum.xml:547), 5000's first start npc (work_order.xml:4)
inline constexpr int32_t FASIMEDES = 203700;     // 1941's end npc
inline constexpr int32_t LODAS = 730019;         // Eltnen: 1323's talk npc (eltnen.xml:399)
inline constexpr int32_t JUSTACHYS = 203939;     // 1323's end npc
inline constexpr int32_t LETON = 804588;         // Kaldor: 13818's start (and so end) npc (kaldor.xml:122)
inline constexpr int32_t THRASYMEDES = 203707;   // Sanctum: 3910's start and end npc (sanctum.xml:392)
inline constexpr int32_t TRAUFNIR = 204056;      // Pandaemonium: 4922's start and end npc (pandaemonium.xml:359)
inline constexpr int32_t RIMA = 799837;          // Fortuneers: one of 36504's two end npcs (fortuneers.xml:104)
inline constexpr int32_t SOCINUS = 799838;       // the other
inline constexpr int32_t PLUMA_BAIT = 700759;    // 36504's spawner object (fortuneers.xml:105)
inline constexpr int32_t LIGHTNINGBEAK_PABU = 216608; // 36504's monster, end_var 1
inline constexpr int32_t FRIGGA = 799949;        // Gelkmaros: one of 21281's two start npcs (gelkmaros.xml:416)
inline constexpr int32_t VALITH = 205621;        // the other
inline constexpr int32_t ORIEL_COIN_FOUNTAIN = 730556; // Cygnea: one of 15205's two fountains (cygnea.xml:179)
inline constexpr int32_t FOUNTAIN_OF_LUCK = 804788;    // the other
inline constexpr int32_t MONODIA = 804876;       // Cygnea: 15220's start (and so end) npc (cygnea.xml:182)
inline constexpr int32_t AUMINUS = 830062;       // 5000's second start npc (work_order.xml:4)
inline constexpr int32_t ANCIENT_CUBE_ITEM = 182200215;  // 1127's collect item, given by its npc_use (quest_data.xml:1104, poeta.xml:78)
inline constexpr int32_t JEWEL_BOX = 182201309;          // 1323's work item, its start item (quest_data.xml:2132)
inline constexpr int32_t BRONZE_COIN = 186000002;        // 1323's reward item (quest_data.xml:2129)
inline constexpr int32_t TRIUMPHANT_HELM = 125001837;    // 1941's reward item (quest_data.xml:8307)
inline constexpr int32_t EXPERT_DAGGER_DESIGN = 152200129; // 1941's first selectable reward (quest_data.xml:8295)
inline constexpr int32_t CERAMIUM_MEDAL = 186000242;     // 13818's reward items (quest_data.xml:41529-41530)
inline constexpr int32_t BLOOD_MARK = 186000236;
inline constexpr int32_t FORTUNEERS_TOKEN = 186000102;   // 36504's reward items (quest_data.xml:68306-68307)
inline constexpr int32_t FORTUNEER_REWARD_CHEST = 188050737;
inline constexpr int32_t LESSER_ANCIENT_ICON = 186000066; // 21281's four relics, categories 0-3 (quest_data.xml:51922-51925)
inline constexpr int32_t ANCIENT_ICON = 186000065;
inline constexpr int32_t GREATER_ANCIENT_ICON = 186000064;
inline constexpr int32_t MAJOR_ANCIENT_ICON = 186000063;
inline constexpr int32_t PLATINUM_MEDAL = 186000096;     // 15205's coin, its collect and inventory item (quest_data.xml:43579, 43581)
inline constexpr int32_t ISSUED_STEEL_INGOT = 182290000; // 5000's component, 4 of them (work_order.xml:5)
inline constexpr int32_t STEEL_CHISEL = 182290317;       // 5000's crafted collect item, 3 of them (quest_data.xml:26741)
inline constexpr int32_t WEAPONSMITHING = 40002;         // 1941's skill_id and 5000's combineskill
inline constexpr int32_t STEEL_CHISEL_RECIPE = 155004001; // 5000's recipe_id
inline constexpr int32_t EUKRATON = 804704;              // Cygnea: 15204's start and end npc (cygnea.xml:190)
inline constexpr int32_t KALDOR = 600090000;             // 13818's only world
inline constexpr int32_t CYGNEA = 210070000;             // 15204's only world
inline constexpr const char* CRIMSON_HILLS = "CRIMSON_HILLS_210070000"; // one of 15220's two zones
inline constexpr const char* CORAL_RISE = "CORAL_RISE_210070000";       // the other
inline constexpr int32_t EREMITIA = 798600;              // Sanctum: 19009's end npc (sanctum.xml:554)
inline constexpr int32_t JUNOS = 205985;                 // Empyrean Crucible: 18214's and 18224's start and end npc (empyrean_crucible.xml:83, 85)
inline constexpr int32_t SELENE = 203099;                // Verteron: 1182's end npc (verteron.xml:198)
inline constexpr int32_t IBELIA = 204582;                // Heiron: 1514's first talk npc (heiron.xml:441)
inline constexpr int32_t SULATES = 204505;               // 1514's second talk npc
inline constexpr int32_t STONE_FRAGMENT = 182200549;     // 1182's work item, its start item with <queststart questid="1182"/>
inline constexpr int32_t CHIEFTAINS_BATON = 182203249;   // 2274's start item with <queststart questid="2274"/>
inline constexpr int32_t EMPYREAN_CRUCIBLE = 300350000;  // one of 18214's and 18224's two worlds
inline constexpr int32_t RUSTED_MEDAL_2 = 182005206;     // the level-2 medal bonus rows (item_groups.xml:4513-4517), with the Platinum Medal
inline constexpr int32_t MITHRIL_MEDAL = 186000147;
/** The rows of 5000's TASK bonus an Elyos can get (item_groups.xml:387, 392, 541, 2237); the Asmodian ones (:671, :4072) never match him */
inline constexpr int32_t CHARCOAL_BRIQUETTE = 169400010;
inline constexpr int32_t LESSER_WHETSTONE = 169400020;
inline constexpr int32_t STEEL_INGOT_BUNDLE_DESIGN = 152202286;
inline constexpr int32_t STEEL_INGOT_DESIGN = 152200075;

inline constexpr int32_t SM_EMOTION_OPCODE = 37;   // ServerPacketsOpcodes.java:55
inline constexpr int32_t SM_USE_OBJECT_OPCODE = 197; // ServerPacketsOpcodes.java:215
inline constexpr int32_t SM_STATUPDATE_EXP_OPCODE = 8; // ServerPacketsOpcodes.java:26

/** SM_USE_OBJECT (SM_USE_OBJECT.java:36-41): D player, D target, D time, C actionType */
inline std::vector<uint8_t> useObject(int32_t playerObjId, int32_t targetObjId, int32_t time, int32_t actionType) {
	return items::javaPacket(SM_USE_OBJECT_OPCODE, PacketWriter().D(playerObjId).D(targetObjId).D(time).C(actionType));
}

/** SM_QUEST_ACTION of a quest with an extra_category (21281, 15205: COIN_QUEST): writeImpl writes nothing (SM_QUEST_ACTION.java:51-53) */
inline std::vector<uint8_t> emptyQuestAction() {
	return items::javaPacket(SM_QUEST_ACTION_OPCODE, PacketWriter());
}

class QuestTemplate1bTest : public QuestTemplateTest {
protected:
	void SetUp() override {
		QuestTemplateTest::SetUp();
		savedMaxExpert = configs::main::CraftConfig::MAX_EXPERT_CRAFTING_SKILLS.load();
		savedMaxMaster = configs::main::CraftConfig::MAX_MASTER_CRAFTING_SKILLS.load();
		configs::main::CraftConfig::MAX_EXPERT_CRAFTING_SKILLS.store(2); // CraftConfig.java: gameserver.craft.max.expert.skills, default 2
		configs::main::CraftConfig::MAX_MASTER_CRAFTING_SKILLS.store(1); // gameserver.craft.max.master.skills, default 1
		republish(dataholders::DataManager::QUEST_DATA, HANDLER_QUESTS_XML, {TEMPLATE_QUESTS_XML, T1B_QUESTS_XML}, "</quests>");
		republish(dataholders::DataManager::XML_QUESTS, TEMPLATE_SCRIPTS_XML, {T1B_SCRIPTS_XML}, "</quest_scripts>");
		std::string npcsXml(HANDLER_NPCS_XML);
		npcsXml.erase(npcsXml.rfind("</npc_templates>"));
		npcsXml += TEMPLATE_NPCS_XML;
		npcsXml += T1B_NPCS_XML;
		npcsXml += "</npc_templates>";
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), npcsXml));
		std::string itemsXml(items::ITEM_TEMPLATES_XML);
		itemsXml.erase(itemsXml.rfind("</item_templates>"));
		itemsXml += HANDLER_ITEMS_XML;
		itemsXml += TEMPLATE_ITEMS_XML;
		itemsXml += T1B_ITEMS_XML;
		itemsXml += "</item_templates>";
		dataholders::DataManager::ITEM_DATA.resetForTests();
		xml::LoadContext& itemContext = contexts.emplace_back(); // the bonus groups find their items through this load's XmlIDs
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(itemContext, itemsXml));
		dataholders::DataManager::ITEM_GROUPS_DATA.resetForTests();
		dataholders::DataManager::ITEM_GROUPS_DATA.publish(xml::bindString<dataholders::ItemGroupsData>(itemContext, T1B_ITEM_GROUPS_XML));
		// the GP and quest-AP rates at Java's defaults (RatesConfig.java: "1.0, 2.0"; the unit tests load no configuration)
		savedGpRates = *configs::main::RatesConfig::GP_RATES.get();
		savedApQuestRates = *configs::main::RatesConfig::AP_QUEST_RATES.get();
		configs::main::RatesConfig::GP_RATES.set({1.0f, 2.0f});
		configs::main::RatesConfig::AP_QUEST_RATES.set({1.0f, 2.0f});
		std::string skillsXml(items::SKILL_TEMPLATES_XML);
		skillsXml.erase(skillsXml.rfind("</skill_data>"));
		skillsXml += T1B_SKILLS_XML;
		skillsXml += "</skill_data>";
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the base fixture's TearDown resets this one
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(contexts.emplace_back(), skillsXml));
		dataholders::DataManager::RECIPE_DATA.resetForTests();
		dataholders::DataManager::RECIPE_DATA.publish(xml::bindString<dataholders::RecipeData>(contexts.emplace_back(), T1B_RECIPES_XML));
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(contexts.emplace_back(), T1B_EXPERIENCE_TABLE_XML));
		// a reward's exp can level the player up, which asks the skill tree for new skills: none here
		dataholders::DataManager::SKILL_TREE_DATA.resetForTests();
		dataholders::DataManager::SKILL_TREE_DATA.publish(xml::bindString<dataholders::SkillTreeData>(contexts.emplace_back(), "<skill_tree/>"));
		// finishQuest dates a repeatable quest's next start in the server's zone (GSConfig.TIME_ZONE_ID, loaded from no configuration here)
		savedZone = configs::main::GSConfig::TIME_ZONE_ID.load();
		configs::main::GSConfig::TIME_ZONE_ID.store(std::chrono::locate_zone("UTC"));
	}

	void TearDown() override {
		QuestTemplateTest::TearDown();
		dataholders::DataManager::ITEM_GROUPS_DATA.resetForTests();
		configs::main::RatesConfig::GP_RATES.set(savedGpRates);
		configs::main::RatesConfig::AP_QUEST_RATES.set(savedApQuestRates);
		dataholders::DataManager::RECIPE_DATA.resetForTests();
		dataholders::DataManager::SKILL_TREE_DATA.resetForTests();
		configs::main::GSConfig::TIME_ZONE_ID.store(savedZone);
		configs::main::CraftConfig::MAX_EXPERT_CRAFTING_SKILLS.store(savedMaxExpert);
		configs::main::CraftConfig::MAX_MASTER_CRAFTING_SKILLS.store(savedMaxMaster);
	}

	/** Publishes `holder` again: the rows of `document` (closed by `closing`) followed by the rows of `fragments` */
	template <class H>
	void republish(xml::HolderRef<H>& holder, std::string_view document, std::initializer_list<std::string_view> fragments,
		std::string_view closing) {
		std::string text(document);
		text.erase(text.rfind(closing));
		for (std::string_view fragment : fragments)
			text += fragment;
		text += closing;
		holder.resetForTests();
		holder.publish(xml::bindString<H>(contexts.emplace_back(), text));
	}

	/**
	 * makeQuester's player with the parts every loaded player has and the 1b quests reach: npc factions (a level change reads them, and a
	 * reward's exp can level up), an empty skill list (the crafting-grade checks read it) and an empty recipe list
	 */
	Quester* makePlayer(int32_t objectId, std::string_view name, gameserver::model::Race race, int32_t level) {
		Quester* q = makeQuester(objectId, name, race, level);
		q->player().setNpcFactions(std::make_unique<gameserver::model::gameobjects::player::npcFaction::NpcFactions>(q->player()));
		q->player().setSkillList(gameserver::model::skill::PlayerSkillList::create());
		q->player().setRecipeList(gameserver::model::gameobjects::player::RecipeList::create());
		return q;
	}

	/** The quester's skill list as PlayerSkillListDAO loads it (stored, no packet): Weaponsmithing at `level` */
	static void knowsWeaponsmithing(Quester& quester, int32_t level) {
		quester.player().setSkillList(gameserver::model::skill::PlayerSkillList::create({gameserver::model::skill::PlayerSkillEntry::create(
			WEAPONSMITHING, level, 0, gameserver::model::gameobjects::Persistable_PersistentState::UPDATED)}));
	}

	/** A cast of `skillId` as Skill.useSkill hands it to QuestEngine.onUseSkill, after clearing the packets; fails on an engine error line */
	void useSkill(Quester& quester, int32_t skillId) {
		quester.clearSent();
		network::test::LogCapture log({"com.aionemu.gameserver.questEngine"});
		EXPECT_TRUE(QuestEngine::getInstance().onUseSkill(*QuestEnv::create(nullptr, quester.player(), 0), skillId));
		if (log.contains("error|"))
			ADD_FAILURE() << "skill " << skillId << ":\n" << log.dump();
	}

	/** A player kill in a world as PvpService.rewardPlayer hands it to QuestEngine.onKillInWorld (the victim as the env's object) */
	void killInWorld(Quester& quester, Quester& victim, int32_t worldId) {
		quester.clearSent();
		network::test::LogCapture log({"com.aionemu.gameserver.questEngine"});
		EXPECT_TRUE(QuestEngine::getInstance().onKillInWorld(*QuestEnv::create(at(victim.player()), quester.player(), 0), worldId));
		if (log.contains("error|"))
			ADD_FAILURE() << "kill in world " << worldId << ":\n" << log.dump();
	}

	/** A player kill in a zone as PvpService.rewardPlayer hands it to QuestEngine.onKillInZone (the victim as the env's object) */
	void killInZone(Quester& quester, Quester& victim, std::string_view zone) {
		quester.clearSent();
		network::test::LogCapture log({"com.aionemu.gameserver.questEngine"});
		EXPECT_TRUE(QuestEngine::getInstance().onKillInZone(*QuestEnv::create(at(victim.player()), quester.player(), 0), zone));
		if (log.contains("error|"))
			ADD_FAILURE() << "kill in zone " << zone << ":\n" << log.dump();
	}

	/** A dialog action without a target (the answer to an item's accept window) as CM_DIALOG_SELECT hands it to QuestEngine.onDialog */
	bool select(Quester& quester, int32_t questId, int32_t dialogActionId) {
		quester.clearSent();
		network::test::LogCapture log({"com.aionemu.gameserver.questEngine"});
		bool answer = QuestEngine::getInstance().onDialog(*envOf(quester, questId, dialogActionId));
		if (log.contains("error|"))
			ADD_FAILURE() << "quest " << questId << " action " << dialogActionId << " without a target:\n" << log.dump();
		return answer;
	}

	/** An item use as CM_USE_ITEM hands it to QuestEngine.onItemUseEvent (the env without a target and a quest id) */
	HandlerResult useItem(Quester& quester, gameserver::model::gameobjects::Item& item) {
		quester.clearSent();
		network::test::LogCapture log({"com.aionemu.gameserver.questEngine"});
		HandlerResult result = QuestEngine::getInstance().onItemUseEvent(*envOf(quester, 0, 0), item);
		if (log.contains("error|"))
			ADD_FAILURE() << "item use:\n" << log.dump();
		return result;
	}

	/** A start item used as CM_USE_ITEM uses it: its <queststart> action's act (QuestStartAction.java:40-66, E-10), after clearing the packets */
	void useStartItem(Quester& quester, gameserver::model::gameobjects::Item& item) {
		quester.clearSent();
		network::test::LogCapture log({"com.aionemu.gameserver.questEngine", "com.aionemu.gameserver.services"});
		const gameserver::model::templates::item::actions::QuestStartAction* action = nullptr;
		for (const auto& itemAction : item.getItemTemplate()->getActions()->getItemActions()) {
			if (const auto* questStart = dynamic_cast<const gameserver::model::templates::item::actions::QuestStartAction*>(itemAction.get()))
				action = questStart;
		}
		ASSERT_NE(action, nullptr) << item.getItemId() << " has no <queststart>";
		action->act(quester.player(), Ptr<gameserver::model::gameobjects::Item>(item), nullptr);
		if (log.contains("error|"))
			ADD_FAILURE() << "start item " << item.getItemId() << ":\n" << log.dump();
	}

	/** The item ids of `candidates` the quester holds, with their counts (the bonus a finish drew) */
	std::vector<std::pair<int32_t, int64_t>> heldOf(Quester& quester, std::initializer_list<int32_t> candidates) {
		std::vector<std::pair<int32_t, int64_t>> found;
		for (int32_t itemId : candidates) {
			if (int64_t count = held(quester, itemId); count > 0)
				found.emplace_back(itemId, count);
		}
		return found;
	}

	/** The last packet the quester was sent, empty if none */
	static std::vector<uint8_t> lastSent(Quester& quester) {
		const std::vector<std::vector<uint8_t>> sent = quester.sent();
		return sent.empty() ? std::vector<uint8_t>{} : sent.back();
	}

	/** "itemId xcount" of each (item, count), for a failure message */
	static std::string describe(const std::vector<std::pair<int32_t, int64_t>>& items) {
		std::string text = items.empty() ? "none" : "";
		for (const auto& [itemId, count] : items)
			text += (text.empty() ? "" : ", ") + std::to_string(itemId) + " x" + std::to_string(count);
		return text;
	}

	int32_t savedMaxExpert = 0;
	int32_t savedMaxMaster = 0;
	const std::chrono::time_zone* savedZone = nullptr;
	std::vector<float> savedGpRates;
	std::vector<float> savedApQuestRates;
};

/** Seeds the thread's random generator for a case and restores the state before it afterwards (the bonus draws are Rnd's) */
struct SeededRnd {
	explicit SeededRnd(uint64_t seed) : saved(commons::utils::Rnd::generator()) { commons::utils::Rnd::seedCurrentThreadForTests(seed); }
	~SeededRnd() { commons::utils::Rnd::generator() = saved; }
	SeededRnd(const SeededRnd&) = delete;
	SeededRnd& operator=(const SeededRnd&) = delete;
	commons::utils::Rnd::Xoshiro256PlusPlus saved;
};

} // namespace aion::gameserver::questEngine::handlers::test::templates
