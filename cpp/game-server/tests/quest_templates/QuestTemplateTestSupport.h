#pragma once

// P5-06c, M5d T-04 (m5d-plan.md §7): the shared fixture of the XML template tests - the handlers of quest_script_data built by the kinds'
// register_ bodies (T-02) and driven through QuestEngine on in-world players, as the talk, kill, level and item hooks drive them.
//
// - It extends the handler-base fixture (tests/quest_handlers/QuestHandlerTestSupport.h, P5-06b): the same Poeta map instance, questers and
//   packet helpers, whose rows it keeps, plus the rows below. SetUp publishes QUEST_DATA, NPC_DATA and ITEM_DATA again with both sets of rows
//   and the experience table with its first 62 levels, and binds XML_QUESTS from the quest_script_data rows.
// - Rows are copied from the shipped data (file:line beside each; whitespace between tags removed, an npc's <equipment> left out as the comment
//   says: no case reads it).
// - Each case registers the XML quests it needs: `registerXml(id)` calls that quest's XMLQuest::register_ with the engine, as
//   QuestEngine.init's loop does (QuestEngine.java:104-105; in C++ the loop is still the AION_PARTIAL at QuestEngine.cpp:111, D3).
// - The hooks go through QuestEngine (onDialog, onKill, onLevelChanged, onEnterWorld, onEnterZone, onItemUseEvent), which catches and logs a
//   handler's exception as "QE: exception in ..." and answers false. The helpers (talk, kill, enterZone, levelChanged, enterWorld) capture
//   the questEngine loggers and fail the case on any error line, so a throwing template cannot pass as an answer of false or as silence.
// - Expected pages and effects come from the templates (handlers/template/*.java) and `oracle.py m5d-quest --quest ID`.

#include "../quest_handlers/QuestHandlerTestSupport.h"

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/dataholders/XMLQuests.bind.h"
#include "aion/gameserver/dataholders/XMLQuests.h"
#include "aion/gameserver/questEngine/handlers/models/XMLQuest.h"
#include "aion/gameserver/world/zone/ZoneName.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::templates {

inline constexpr const char* TEMPLATE_QUESTS_XML =
	// quest_data.xml:921-927
	R"(<quest id="1105" name="The Snuffler Headache" nameId="1102205" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1" )"
	R"(can_report="true" race_permitted="ELYOS" category="QUEST"><collect_items><collect_item item_id="182200202" )"
	R"(count="3"/></collect_items><rewards gold="450" exp="535"/><quest_drop npc_id="210079" item_id="182200202"/></quest>)"
	// quest_data.xml:928-936
	R"(<quest id="1106" name="Helping Kales" nameId="1102206" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1" can_report="true" )"
	R"(cannot_share="true" race_permitted="ELYOS" category="QUEST"><rewards exp="300"/><start_conditions><finished )"
	R"(quest_id="1105"/></start_conditions><quest_work_items><quest_work_item item_id="182200203"/></quest_work_items></quest>)"
	// quest_data.xml:996-998
	R"(<quest id="1115" name="The Elim's Message" nameId="1102215" quest_zone="Poeta" minlevel_permitted="4" max_repeat_count="1" )"
	R"(can_report="true" race_permitted="ELYOS" category="QUEST"><rewards gold="680" exp="2673"/></quest>)"
	// quest_data.xml:1185-1194
	R"(<quest id="1137" name="Ancient Lobnite Fossil" nameId="1102307" quest_zone="Verteron" minlevel_permitted="10" max_repeat_count="1" )"
	R"(cannot_share="true" race_permitted="ELYOS" category="QUEST"><collect_items><collect_item item_id="182200513" )"
	R"(count="1"/></collect_items><rewards gold="2520" exp="5250"/><quest_drop npc_id="700108" item_id="182200513" )"
	R"(drop_each_member="1"/><quest_work_items><quest_work_item item_id="182200512"/></quest_work_items></quest>)"
	// quest_data.xml:1015-1020
	R"(<quest id="1118" name="Polinia's Ointment" nameId="1102218" quest_zone="Poeta" minlevel_permitted="6" max_repeat_count="1" )"
	R"(can_report="true" cannot_share="true" race_permitted="ELYOS" category="QUEST"><rewards gold="370" )"
	R"(exp="2167"/><quest_work_items><quest_work_item item_id="182200224"/></quest_work_items></quest>)"
	// quest_data.xml:5860-5866
	R"(<quest id="1666" name="Taking it to the Indratu" nameId="1102857" quest_zone="Heiron" minlevel_permitted="44" max_repeat_count="1" )"
	R"(race_permitted="ELYOS" category="IMPORTANT" restricted="true"><rewards exp="5895626"><reward_item item_id="186000005" )"
	R"(count="20"/></rewards><quest_kill step="0" count="5" npc_ids="214055 219112 219139" seq="0"/><quest_kill step="0" count="5" npc_ids="213964 )"
	R"(219106 219132 235520" seq="1"/></quest>)"
	// quest_data.xml:7688-7691
	R"(<quest id="1839" name="A Deal with Silvius" nameId="1104639" quest_zone="Reshanta" minlevel_permitted="30" max_repeat_count="200" )"
	R"(race_permitted="ELYOS" category="QUEST" restricted="true"><rewards exp="564357" ap="400"/><quest_kill step="0" var="0" count="67" )"
	R"(npc_ids="214744 214745 214746 214747 214748 214749 214750 214751" seq="0"/></quest>)"
	// quest_data.xml:18977-18985
	R"(<quest id="3018" name="Wanted: Fork Ear Rokes" nameId="1113017" quest_zone="Theobomos" minlevel_permitted="22" max_repeat_count="1" )"
	R"(race_permitted="ELYOS" category="QUEST"><collect_items><collect_item item_id="182208009" count="1"/></collect_items><rewards gold="42270" )"
	R"(exp="128250"><reward_item item_id="186000002" count="1"/></rewards><quest_drop npc_id="213928" item_id="182208009" )"
	R"(drop_each_member="1"/></quest>)"
	// quest_data.xml:20829-20836
	R"(<quest id="3340" name="Preparing for the Destruction of the Lepharist Bastion" nameId="1141743" quest_zone="Eltnen" minlevel_permitted="33" )"
	R"(max_repeat_count="1" cannot_share="true" race_permitted="ELYOS" category="QUEST"><collect_items><collect_item item_id="182215333" )"
	R"(count="1"/></collect_items><rewards exp="3509468" title="11"/><quest_drop npc_id="211031" item_id="182215333" chance="80" )"
	R"(drop_each_member="1" collecting_step="1"/><class_permitted>RIDER</class_permitted></quest>)"
	// quest_data.xml:25810-25817
	R"(<quest id="4914" name="Lifeform Remodeling Report" nameId="1104249" quest_zone="Dark Poeta" minlevel_permitted="48" max_repeat_count="1" )"
	R"(cannot_share="true" race_permitted="ASMODIANS" category="QUEST"><rewards gold="94140" exp="5390338"><reward_item item_id="186000010" )"
	R"(count="1"/></rewards><quest_work_items><quest_work_item item_id="182207127"/></quest_work_items></quest>)"
	// quest_data.xml:36147-36172
	R"(<quest id="11250" name="[Spy/League] Price of Agency" nameId="1125160" quest_zone="Inggison" minlevel_permitted="54" max_repeat_count="30" )"
	R"(reward_repeat_count="30" cannot_share="true" race_permitted="ELYOS" category="QUEST" target="LEAGUE" restricted="true"><rewards )"
	R"(exp="5373536"/><extended_rewards gold="16800"><reward_item item_id="182206878"/></extended_rewards><start_conditions><finished )"
	R"(quest_id="11129"/><unfinished>11256</unfinished><noacquired>11256</noacquired></start_conditions><start_conditions><finished )"
	R"(quest_id="11131"/><unfinished>11258</unfinished><noacquired>11258</noacquired></start_conditions><start_conditions><finished )"
	R"(quest_id="11287"/><unfinished>11262</unfinished><noacquired>11262</noacquired></start_conditions><start_conditions>)"
	R"(<unfinished>11266</unfinished><noacquired>11266</noacquired></start_conditions><class_permitted>WARRIOR )"
	R"(SCOUT MAGE PRIEST ENGINEER ARTIST GLADIATOR TEMPLAR</class_permitted></quest>)"
	// quest_data.xml:41443-41453
	R"(<quest id="13809" name="Tree is Company" nameId="1111683" quest_zone="Kaldor" minlevel_permitted="65" max_repeat_count="1" )"
	R"(cannot_share="true" race_permitted="ELYOS" category="QUEST"><rewards gold="150660" exp="3446553"><selectable_reward_item )"
	R"(item_id="166050007" count="2"/><selectable_reward_item item_id="166050008" count="2"/></rewards><quest_work_items><quest_work_item )"
	R"(item_id="182215485" count="1"/><quest_work_item item_id="182215486" count="1"/><quest_work_item item_id="182215487" )"
	R"(count="1"/></quest_work_items></quest>)"
	// quest_data.xml:41578-41582
	R"(<quest id="13830" name="Stigma 101" nameId="1801119" quest_zone="Eltnen" minlevel_permitted="30" max_repeat_count="1" cannot_share="true" )"
	R"(race_permitted="ELYOS" category="PRIMARY" data_driven="true"><rewards exp="46544"><reward_item item_id="188053787"/></rewards></quest>)"
	// quest_data.xml:42982-42991
	R"(<quest id="15001" name="Lending Both Hands" nameId="1800713" quest_zone="Cygnea" minlevel_permitted="55" max_repeat_count="1" )"
	R"(race_permitted="ELYOS" category="IMPORTANT" data_driven="true"><rewards gold="96300" exp="8828737"><selectable_reward_item )"
	R"(item_id="167000518" count="2"/><selectable_reward_item item_id="167000555" count="2"/><selectable_reward_item item_id="166000194" )"
	R"(count="1"/><reward_item item_id="186000231" count="4"/></rewards><quest_kill step="0" var="1" count="5" npc_ids="235790 235791" )"
	R"(seq="1"/><quest_kill step="0" var="2" count="5" npc_ids="235799 235800" seq="2"/></quest>)"
	// quest_data.xml:42992-43007
	R"(<quest id="15002" name="Cold Hands, Warm Luciferin" nameId="1800714" quest_zone="Cygnea" minlevel_permitted="55" max_repeat_count="1" )"
	R"(race_permitted="ELYOS" category="IMPORTANT" data_driven="true"><collect_items><collect_item item_id="182215663" )"
	R"(count="7"/></collect_items><rewards gold="96300" exp="8828737"><selectable_reward_item item_id="160002336" )"
	R"(count="7"/><selectable_reward_item item_id="160002337" count="7"/><selectable_reward_item item_id="160002338" )"
	R"(count="7"/><selectable_reward_item item_id="160002339" count="7"/><selectable_reward_item item_id="160002340" )"
	R"(count="7"/><selectable_reward_item item_id="160002341" count="7"/><reward_item item_id="186000231" count="4"/></rewards><quest_drop )"
	R"(npc_id="235792" item_id="182215663" drop_each_member="1"/><quest_drop npc_id="235793" item_id="182215663" drop_each_member="1"/></quest>)"
	// quest_data.xml:43758-43766
	R"(<quest id="16900" name="Take the Arid Ground" nameId="1140684" quest_zone="Eltnen" minlevel_permitted="28" max_repeat_count="1" )"
	R"(race_permitted="ELYOS" category="QUEST"><rewards gold="85320" exp="186175"><selectable_reward_item item_id="120000860" )"
	R"(count="1"/><selectable_reward_item item_id="120000861" count="1"/><selectable_reward_item item_id="123000895" )"
	R"(count="1"/><selectable_reward_item item_id="123000896" count="1"/></rewards><quest_kill step="0" var="0" count="1" npc_ids="231549 231551" )"
	R"(seq="0"/></quest>)"
	// quest_data.xml:46762-46776
	R"(<quest id="18739" name="Urgent Deed: Get the Seed" nameId="1800937" quest_zone="Raksang Ruins" minlevel_permitted="60" )"
	R"(max_repeat_count="10" race_permitted="ELYOS" category="PRIMARY" data_driven="true"><collect_items><collect_item item_id="182215692" )"
	R"(count="5"/></collect_items><rewards exp="9492173"><reward_item item_id="186000231" count="5"/></rewards><quest_drop npc_id="236008" )"
	R"(item_id="182215692" chance="80"/><quest_drop npc_id="236007" item_id="182215692" chance="80"/><quest_drop npc_id="236084" )"
	R"(item_id="182215692" chance="80"/><quest_drop npc_id="236303" item_id="182215692"/><quest_drop npc_id="236304" )"
	R"(item_id="182215692"/><quest_drop npc_id="236305" item_id="182215692"/><quest_drop npc_id="236306" item_id="182215692"/></quest>)"
	// quest_data.xml:46807-46812
	R"(<quest id="18741" name="A Versed Order" nameId="1800939" quest_zone="Raksang Ruins" minlevel_permitted="60" max_repeat_count="10" )"
	R"(race_permitted="ELYOS" category="PRIMARY" data_driven="true"><rewards exp="9492173"><reward_item item_id="186000231" )"
	R"(count="5"/></rewards><quest_kill step="0" var="1" count="50" npc_ids="236019 236018 236098 236021 236096 236097 236020" seq="1"/></quest>)"
	// quest_data.xml:47466-47468
	R"(<quest id="18970" name="The Corridor Lore" nameId="1801224" quest_zone="Cygnea" minlevel_permitted="65" max_repeat_count="1" )"
	R"(race_permitted="ELYOS" category="IMPORTANT" data_driven="true"><rewards gold="150660" exp="3446553"/></quest>)"
	// quest_data.xml:57979-57982
	R"(<quest id="24230" name="A Grave Situation" nameId="1800192" quest_zone="Altgard" minlevel_permitted="14" max_repeat_count="1" )"
	R"(race_permitted="ASMODIANS" category="IMPORTANT"><rewards exp="17552"/><quest_kill step="0" var="0" count="9" npc_ids="210504 210505" )"
	R"(seq="0"/></quest>)"
	// quest_data.xml:63126-63131
	R"(<quest id="29601" name="Instruction on Instructors" nameId="1800389" quest_zone="Fatebound Abbey" minlevel_permitted="10" )"
	R"(maxlevel_permitted="65" max_repeat_count="255" repeat_cycle="ALL" cannot_share="true" race_permitted="ASMODIANS" category="IMPORTANT" )"
	R"(data_driven="true"><inventory_items><inventory_item item_id="164000336"/></inventory_items><rewards gold="48150" exp="21032"/></quest>)"
	// quest_data.xml:68096-68101
	R"(<quest id="35052" name="[Daily] Alabaster Orders" nameId="1800622" quest_zone="Alabaster Order" minlevel_permitted="56" )"
	R"(maxlevel_permitted="65" max_repeat_count="255" cannot_share="true" race_permitted="ELYOS" category="FACTION" npcfaction_id="2" )"
	R"(data_driven="true"><rewards exp="1477634"><reward_item item_id="186000100" count="6"/></rewards><quest_kill step="0" var="1" count="5" )"
	R"(npc_ids="702760" seq="0"/></quest>)"
	// quest_data.xml:69074-69080
	R"(<quest id="39005" name="Artillery Strike" nameId="1140005" quest_zone="Brusthonin" minlevel_permitted="60" max_repeat_count="255" )"
	R"(race_permitted="ELYOS" category="PUBLIC" target="AREA"><rewards exp="283476"><reward_item item_id="188051844" count="1"/><reward_item )"
	R"(item_id="164000076" count="5"/></rewards><quest_kill step="0" var="0" count="2" npc_ids="209470" seq="0"/></quest>)"
	// quest_data.xml:80928-80933
	R"(<quest id="80545" name="[Event/Daily] A Gracious Thought" nameId="1185744" quest_zone="Event" minlevel_permitted="10" )"
	R"(max_repeat_count="255" cannot_share="true" race_permitted="ELYOS" category="EVENT" repeat_cycle="ALL"><rewards><reward_item )"
	R"(item_id="188052962" count="1"/></rewards><gender_permitted>FEMALE</gender_permitted></quest>)";

/** The quest_script_data rows of the quests the cases register (quest_script_data/<file>:<line>) */
inline constexpr const char* TEMPLATE_SCRIPTS_XML =
	R"(<quest_scripts>)"
	// poeta.xml:54
	R"(<report_to id="1101" start_npc_ids="203049" end_npc_ids="203057"/>)"
	// poeta.xml:130
	R"(<monster_hunt id="1102" start_npc_ids="203057"/>)"
	// poeta.xml:114
	R"(<item_collecting id="1103" start_npc_ids="203057"/>)"
	// poeta.xml:56
	R"(<report_to id="1106" start_npc_ids="203050" end_npc_ids="203061"/>)"
	// poeta.xml:58-61
	R"(<report_to_many id="1115" start_npc_ids="203075"><npc_infos npc_ids="203072"/><npc_infos npc_ids="203058"/></report_to_many>)"
	// poeta.xml:62-65
	R"(<report_to_many id="1118" start_npc_ids="203059"><npc_infos npc_ids="203070"/><npc_infos npc_ids="203079"/></report_to_many>)"
	// heiron.xml:488
	R"(<monster_hunt id="1666" start_npc_ids="800413"/>)"
	// pandaemonium.xml:348-351
	R"(<report_to_many id="4914" start_item_id="182207127"><npc_infos npc_ids="204182"/><npc_infos npc_ids="203385"/></report_to_many>)"
	// stigma.xml:19
	R"(<report_on_levelup id="13830" end_npc_ids="798380"/>)"
	// cygnea.xml:151
	R"(<item_collecting id="15002" start_npc_ids="804698"/>)"
	// cygnea.xml:147
	R"(<report_to id="18970" start_npc_ids="804709" end_npc_ids="805213 805214 805215"/>)"
	// altgard.xml:229
	R"(<monster_hunt id="24230" start_npc_ids="832821" end_reward="true"/>)"
	// fatebound_abbey.xml:40-45
	R"(<report_to_many id="29601" start_npc_ids="804662"><npc_infos npc_ids="804663"/><npc_infos npc_ids="804664"/><npc_infos )"
	R"(npc_ids="804665"/><npc_infos npc_ids="804666"/></report_to_many>)"
	// alabaster_order.xml:159
	R"(<monster_hunt id="35052" end_npc_ids="804941"/>)"
	// sanctum.xml:358
	R"(<report_to id="80545" start_npc_ids="831999"/>)"
	// verteron.xml:140
	R"(<item_collecting id="1137" start_npc_ids="203111"/>)"
	// theobomos.xml:151
	R"(<item_collecting id="3018" start_dialog_id="4762" start_npc_ids="730105" start_dialog_id2="1011" end_npc_ids="798150"/>)"
	// eltnen.xml:392
	R"(<item_collecting id="3340" start_npc_ids="203901" next_npc_id="204042" end_npc_ids="204042"/>)"
	// inggison.xml:441
	R"(<monster_hunt id="11250" start_npc_ids="799038" start_dialog_id="4762"/>)"
	// eltnen.xml:469
	R"(<monster_hunt id="16900" start_npc_ids="203901" end_npc_ids="203965" end_dialog_id="2375"/>)"
	// raksang_ruins.xml:33
	R"(<item_collecting id="18739" start_zone="IDRAKSHA_SOLO_STARTCHIOCE_NPC_206390_7_300610000" end_npc_ids="804707"/>)"
	// raksang_ruins.xml:41
	R"(<monster_hunt id="18741" start_zone="IDRAKSHA_SOLO_STARTCHIOCE_NPC_206390_7_300610000" end_npc_ids="804707"/>)"
	// reshanta.xml:618
	R"(<monster_hunt id="1839" start_npc_ids="263597"/>)"
	// cygnea.xml:193
	R"(<monster_hunt id="15001" start_npc_ids="804698"/>)"
	// kaldor.xml:59-64
	R"(<report_to_many id="13809" start_npc_ids="802427"><npc_infos npc_ids="730969"/><npc_infos npc_ids="730970"/><npc_infos )"
	R"(npc_ids="730971"/><npc_infos npc_ids="802427"/></report_to_many>)"
	// brusthonin.xml:222
	R"(<monster_hunt id="39005" end_npc_ids="800500" invasion_world="220050000"/>)"
	R"(</quest_scripts>)";

inline constexpr const char* TEMPLATE_NPCS_XML =
	// npc_templates.xml:1839-1853 without its <equipment> (:1843-1850)
	R"(<npc_template npc_id="203049" level="20" name="elpas" name_id="351000" height="2" title_id="350496" group_drop="LIGHT" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" arange="2" attack_speed="2000" )"
	R"(hpgauge="3"><stats maxHp="2961"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:1854-1869 without its <equipment> (:1858-1866)
	R"(<npc_template npc_id="203050" level="10" name="kales" name_id="351072" height="2" title_id="350444" group_drop="LIGHT" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" arange="2" attack_speed="2000" )"
	R"(hpgauge="3"><stats maxHp="1392"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:2726-2742 without its <equipment> (:2730-2739)
	R"(<npc_template npc_id="203111" level="20" name="spiros" name_id="351036" height="2" title_id="350300" group_drop="LIGHT" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ELYOS" tribe="GUARD" type="ABYSS_GUARD" ai="simple_abyssguard" srange="7" sangle="240" arange="2" attack_speed="2000" )"
	R"(hpgauge="3"><stats maxHp="2961"><speeds walk="1.3" group_walk="1.3" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:6349-6363 without its <equipment> (:6353-6360)
	R"(<npc_template npc_id="203385" level="40" name="hnoss" name_id="351891" height="2" title_id="370098" group_drop="DARK" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ASMODIANS" tribe="GENERAL_DARK" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" attack_speed="2000" )"
	R"(hpgauge="3"><stats maxHp="9426"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:1974-1989 without its <equipment> (:1978-1986)
	R"(<npc_template npc_id="203058" level="10" name="asteros" name_id="351001" height="2" title_id="350427" group_drop="LIGHT" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" arange="2" attack_speed="2000" )"
	R"(hpgauge="3"><stats maxHp="1392"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:2020-2035 without its <equipment> (:2024-2032)
	R"(<npc_template npc_id="203061" level="10" name="uno" name_id="351004" height="2" title_id="350374" group_drop="LIGHT" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" arange="2" attack_speed="2000" )"
	R"(hpgauge="3"><stats maxHp="1392"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" func_dialogs="2 3" can_talk_invisible="false" )"
	R"(/></npc_template>)"
	// npc_templates.xml:2146-2152
	R"(<npc_template npc_id="203070" level="40" name="kustanon" name_id="351062" height="1.8" title_id="350420" group_drop="NONE" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" arange="2" )"
	R"(attack_speed="2000" hpgauge="3"><stats maxHp="9426"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" )"
	R"(/></stats><bound_radius front="0.25" side="0.35" upper="1.8" /><talk_info distance="3" is_dialog="true" func_dialogs="44" )"
	R"(can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:2168-2182 without its <equipment> (:2172-2179)
	R"(<npc_template npc_id="203072" level="10" name="feira" name_id="351010" height="2" title_id="350433" group_drop="LIGHT" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" arange="2" attack_speed="2000" )"
	R"(hpgauge="3" state="6"><stats maxHp="1392"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" )"
	R"(/></stats><bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" )"
	R"(/></npc_template>)"
	// npc_templates.xml:2215-2230 without its <equipment> (:2219-2227)
	R"(<npc_template npc_id="203075" level="20" name="namus" name_id="351012" height="2.4" title_id="350434" group_drop="LIGHT" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" arange="2" attack_speed="2000" )"
	R"(hpgauge="3"><stats maxHp="2961"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.3" side="0.42" upper="2.4" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:2278-2292 without its <equipment> (:2282-2289)
	R"(<npc_template npc_id="203079" level="10" name="melponeh" name_id="351013" height="2" title_id="350470" group_drop="LIGHT" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" arange="2" )"
	R"(attack_speed="2000" hpgauge="3"><stats maxHp="1392"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" )"
	R"(/></stats><bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" )"
	R"(/></npc_template>)"
	// npc_templates.xml:54511-54516
	R"(<npc_template npc_id="210134" level="2" name="striped kerub" name_id="300111" height="1.372" group_drop="CHERUBIM" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="MAGICALMONSTER" tribe="MONSTER" type="MONSTER" ai="aggressive" srange="7" sangle="240" arange="2" attack_speed="2100" )"
	R"(hpgauge="3"><stats maxHp="199"><speeds walk="0.6" group_walk="0.6" run="7" run_fight="5.5" group_run_fight="7" /></stats><bound_radius )"
	R"(front="0.525" side="0.275" upper="1.372" /></npc_template>)"
	// npc_templates.xml:13056-13070 without its <equipment> (:13060-13067)
	R"(<npc_template npc_id="203965" level="45" name="castor" name_id="351764" height="2.4" title_id="350525" group_drop="LIGHT" rank="EXPERT" )"
	R"(rating="HERO" race="ELYOS" tribe="GUARD" type="ABYSS_GUARD" ai="simple_abyssguard" srange="10" arange="4" attack_speed="2100" hpgauge="20" )"
	R"(cancel_level="0"><stats maxHp="236165"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="8" group_run_fight="4.2" )"
	R"(/></stats><bound_radius front="0.3" side="0.42" upper="2.4" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" )"
	R"(/></npc_template>)"
	// npc_templates.xml:14075-14091 without its <equipment> (:14079-14088)
	R"(<npc_template npc_id="204042" level="45" name="laigas" name_id="351842" height="2.4" title_id="350300" group_drop="LIGHT" rank="VETERAN" )"
	R"(rating="ELITE" race="ELYOS" tribe="GUARD" type="ABYSS_GUARD" ai="simple_abyssguard" srange="15" arange="2" attack_speed="2100" hpgauge="14" )"
	R"(cancel_level="20"><stats maxHp="127590"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="8" group_run_fight="4.2" )"
	R"(/></stats><bound_radius front="0.3" side="0.42" upper="2.4" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" )"
	R"(/></npc_template>)"
	// npc_templates.xml:56585-56596 without its <equipment> (:56589-56594)
	R"(<npc_template npc_id="210504" level="14" name="grave robbing sentry" name_id="300643" height="1.76" title_id="350453" group_drop="LEPHAR" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="DEMIHUMANOID" tribe="LEHPAR" type="MONSTER" ai="aggressive" srange="6" sangle="270" arange="2" )"
	R"(attack_speed="2100" hpgauge="3"><stats maxHp="1998"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="5" group_run_fight="6" )"
	R"(/></stats><bound_radius front="0.4" side="0.4" upper="1.76" /></npc_template>)"
	// npc_templates.xml:83492-83500 without its <equipment> (:83496-83498)
	R"(<npc_template npc_id="213964" level="44" name="indratu defender" name_id="316231" height="2.8860002" title_id="350637" )"
	R"(group_drop="LIZARDMANFIGHTER" rank="DISCIPLINED" rating="ELITE" race="LIZARDMAN" tribe="NLIZARDMAN" type="MONSTER" ai="aggressive" )"
	R"(srange="8" sangle="240" arange="2" attack_speed="2205" hpgauge="11" cancel_level="40"><stats maxHp="51035"><speeds walk="1.76" )"
	R"(group_walk="1.76" run="7" run_fight="6" group_run_fight="7" /></stats><bound_radius front="1.025" side="2" upper="2.886" /></npc_template>)"
	// npc_templates.xml:451959-451962
	R"(<npc_template npc_id="702760" level="1" name="cygnea aetheric field stone" name_id="466204" height="2" group_drop="NONE" rank="DISCIPLINED" )"
	R"(rating="NORMAL" tribe="MONSTER" type="MONSTER" ai="noaction" attack_speed="2000" hpgauge="3"><stats maxHp="20" /><bound_radius front="0.25" )"
	R"(side="0.35" upper="2" /></npc_template>)"
	// npc_templates.xml:452660-452664
	R"(<npc_template npc_id="730105" level="1" name="wanted: fork ear rokes" name_id="371023" height="2" group_drop="NONE" rank="DISCIPLINED" )"
	R"(rating="NORMAL" tribe="FIELD_OBJECT_LIGHT" type="GENERAL" ai="quest_start_use_item" sangle="0" attack_speed="2000" hpgauge="3"><stats )"
	R"(maxHp="172" /><bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" )"
	R"(/></npc_template>)"
	// npc_templates.xml:462674-462689 without its <equipment> (:462678-462686)
	R"(<npc_template npc_id="798150" level="30" name="crios" name_id="351879" height="2" title_id="370029" group_drop="LIGHT" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="270" arange="2" attack_speed="2100" )"
	R"(hpgauge="3" state="6"><stats maxHp="6438"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" )"
	R"(/></stats><bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" )"
	R"(/></npc_template>)"
	// npc_templates.xml:464598-464611 without its <equipment> (:464602-464608)
	R"(<npc_template npc_id="798380" level="40" name="persephone" name_id="351895" height="2" title_id="350410" group_drop="LIGHT" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="10" sangle="300" arange="2" )"
	R"(attack_speed="2000" hpgauge="3"><stats maxHp="9426"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" )"
	R"(/></stats><bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" func_dialogs="4 125" )"
	R"(can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:468202-468216 without its <equipment> (:468206-468213)
	R"(<npc_template npc_id="799038" level="56" name="laestrygos" name_id="354538" height="1.8" title_id="461443" group_drop="LIGHT" )"
	R"(rank="VETERAN" rating="ELITE" race="ELYOS" tribe="GUARD" type="ABYSS_GUARD" abyss_type="ETC" ai="aggressive" srange="20" arange="37" )"
	R"(attack_speed="2300" hpgauge="14" cancel_level="20"><stats maxHp="145120"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="7" )"
	R"(group_run_fight="6" /></stats><bound_radius front="0.25" side="0.35" upper="1.8" /><talk_info distance="5" is_dialog="true" )"
	R"(can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:522346-522359 without its <equipment> (:522350-522356)
	R"(<npc_template npc_id="804662" level="5" name="melanka" name_id="465749" height="1.8" title_id="465760" group_drop="DARK" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ASMODIANS" tribe="GENERAL_DARK" type="GENERAL" ai="general" srange="20" attack_speed="2000" hpgauge="3"><stats )"
	R"(maxHp="556"><speeds walk="1.3" group_walk="1.68" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius front="0.25" )"
	R"(side="0.35" upper="1.8" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:522360-522373 without its <equipment> (:522364-522370)
	R"(<npc_template npc_id="804663" level="60" name="alda" name_id="465750" height="1.8" title_id="465761" group_drop="DARK" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ASMODIANS" tribe="GENERAL_DARK" type="GENERAL" ai="general" srange="20" arange="2" attack_speed="2000" )"
	R"(hpgauge="3"><stats maxHp="23691"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.225" side="0.315" upper="1.8" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:522374-522387 without its <equipment> (:522378-522384)
	R"(<npc_template npc_id="804664" level="55" name="leopold" name_id="465751" height="1.9" title_id="465761" group_drop="DARK" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="ASMODIANS" tribe="GENERAL_DARK" type="GENERAL" ai="general" srange="20" arange="2" )"
	R"(attack_speed="2000" hpgauge="3"><stats maxHp="18756"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" )"
	R"(/></stats><bound_radius front="0.2375" side="0.3325" upper="1.9" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" )"
	R"(/></npc_template>)"
	// npc_templates.xml:522402-522415 without its <equipment> (:522406-522412)
	R"(<npc_template npc_id="804666" level="55" name="sigurd" name_id="465753" height="2.2" title_id="465761" group_drop="DARK" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ASMODIANS" tribe="GENERAL_DARK" type="GENERAL" ai="general" srange="20" arange="2" attack_speed="2000" )"
	R"(hpgauge="3"><stats maxHp="18756"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.275" side="0.385" upper="2.2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:522725-522739 without its <equipment> (:522729-522736)
	R"(<npc_template npc_id="804698" level="65" name="nubes" name_id="465815" height="2.1" title_id="370721" group_drop="LIGHT" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" arange="2" attack_speed="2000" hpgauge="3"><stats )"
	R"(maxHp="26116"><speeds walk="1.3" group_walk="1.3" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius front="0.2625" )"
	R"(side="0.3675" upper="2.1" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:522887-522900 without its <equipment> (:522891-522897)
	R"(<npc_template npc_id="804709" level="65" name="brunte" name_id="465826" height="2" title_id="370722" group_drop="LIGHT" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" arange="2" attack_speed="2000" hpgauge="3" )"
	R"(state="6"><stats maxHp="26116"><speeds walk="1.3" group_walk="1.3" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:525489-525501 without its <equipment> (:525493-525498)
	R"(<npc_template npc_id="804941" level="65" name="donand" name_id="466180" height="2" title_id="370251" group_drop="LIGHT" rank="DISCIPLINED" )"
	R"(rating="ELITE" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" arange="2" attack_speed="2300" hpgauge="11" )"
	R"(cancel_level="40"><stats maxHp="117675"><speeds walk="1.3" group_walk="1.3" run="6" run_fight="4.2" group_run_fight="4.2" )"
	R"(/></stats><bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" )"
	R"(/></npc_template>)"
	// npc_templates.xml:528940-528956 without its <equipment> (:528944-528953)
	R"(<npc_template npc_id="805213" level="65" name="finderyux" name_id="466414" height="2" title_id="466420" group_drop="LIGHT" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" arange="2" attack_speed="2000" )"
	R"(hpgauge="3"><stats maxHp="26116"><speeds walk="1.3" group_walk="1.3" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:552085-552091
	R"(<npc_template npc_id="831999" level="60" name="florarinerk" name_id="464203" height="2" group_drop="NONE" rank="DISCIPLINED" )"
	R"(rating="NORMAL" tribe="USEALL" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" attack_speed="2000" hpgauge="3"><stats )"
	R"(maxHp="23691"><speeds walk="1" group_walk="1" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius front="0.25" )"
	R"(side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:557867-557882 without its <equipment> (:557871-557879)
	R"(<npc_template npc_id="832821" level="15" name="brodir" name_id="465610" height="2" title_id="350307" group_drop="DARK" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="ASMODIANS" tribe="GENERAL_DARK" type="GENERAL" ai="general" srange="7" arange="2" attack_speed="2000" )"
	R"(hpgauge="3"><stats maxHp="2256"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:261546-261560 without its <equipment> (:261550-261557)
	R"(<npc_template npc_id="263597" level="40" name="silvius" name_id="312820" height="2" title_id="314351" group_drop="LIGHT" rank="EXPERT" )"
	R"(rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" abyss_type="ETC" ai="general" srange="10" sangle="240" arange="2" )"
	R"(attack_speed="2100" hpgauge="5" cancel_level="20"><stats maxHp="22986"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4" )"
	R"(group_run_fight="4.2" /></stats><bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" )"
	R"(can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:89048-89056 without its <equipment> (:89052-89054)
	R"(<npc_template npc_id="214744" level="35" name="ashikar swordlord" name_id="318350" height="3.00625" group_drop="LIZARDMANFIGHTER" )"
	R"(rank="SEASONED" rating="ELITE" race="LIZARDMAN" tribe="AGGRESSIVESUPPORTMONSTER" type="ABYSS_GUARD" ai="aggressive" srange="12" )"
	R"(sangle="240" arange="2" attack_speed="2100" hpgauge="12" cancel_level="30"><stats maxHp="36134"><speeds walk="1.76" group_walk="1.76" )"
	R"(run="7" run_fight="8" group_run_fight="7" /></stats><bound_radius front="1.28125" side="2.5" upper="3.00625" /></npc_template>)"
	// npc_templates.xml:483510-483525 without its <equipment> (:483514-483522)
	R"(<npc_template npc_id="800413" level="45" name="javlantia" name_id="372360" height="1.8" title_id="370507" group_drop="LIGHT" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="300" arange="2" )"
	R"(attack_speed="2000" hpgauge="3"><stats maxHp="11878"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="7" group_run_fight="4.2" )"
	R"(/></stats><bound_radius front="0.25" side="0.35" upper="1.8" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" )"
	R"(/></npc_template>)"
	// npc_templates.xml:191457-191462
	R"(<npc_template npc_id="235790" level="55" name="webtoe vortile" name_id="333896" height="1.54" group_drop="VARANUS" rank="DISCIPLINED" )"
	R"(rating="NORMAL" race="BEAST" tribe="MONSTER" type="MONSTER" ai="general" srange="8" arange="2" attack_speed="1938" cast_speed="800" )"
	R"(hpgauge="3"><stats maxHp="13286"><speeds walk="1.524" group_walk="1.524" run="5" run_fight="7" group_run_fight="10" /></stats><bound_radius )"
	R"(front="1.98" side="0.99" upper="1.54" /></npc_template>)"
	// npc_templates.xml:191509-191514
	R"(<npc_template npc_id="235799" level="55" name="blueshroom ksellid" name_id="333905" height="2.1449997" group_drop="SHELLIZARD" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="BEAST" tribe="MONSTER" type="MONSTER" ai="general" srange="8" arange="2" attack_speed="2142" )"
	R"(cast_speed="850" hpgauge="3"><stats maxHp="17607"><speeds walk="0.453" group_walk="0.453" run="8.85" run_fight="8.85" )"
	R"(group_run_fight="8.85" /></stats><bound_radius front="0.8625" side="1.125" upper="2.145" /></npc_template>)"
	// npc_templates.xml:456956-456960
	R"(<npc_template npc_id="730969" level="1" name="scorched tree" name_id="373266" height="2" group_drop="NONE" rank="DISCIPLINED" )"
	R"(rating="NORMAL" tribe="FIELD_OBJECT_ALL" type="GENERAL" ai="general" attack_speed="2000" hpgauge="3"><stats maxHp="172" /><bound_radius )"
	R"(front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:456961-456965
	R"(<npc_template npc_id="730970" level="1" name="cindery tree" name_id="373267" height="2" group_drop="NONE" rank="DISCIPLINED" )"
	R"(rating="NORMAL" tribe="FIELD_OBJECT_ALL" type="GENERAL" ai="general" attack_speed="2000" hpgauge="3"><stats maxHp="172" /><bound_radius )"
	R"(front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:456966-456970
	R"(<npc_template npc_id="730971" level="1" name="burnt tree" name_id="373268" height="2" group_drop="NONE" rank="DISCIPLINED" rating="NORMAL" )"
	R"(tribe="FIELD_OBJECT_ALL" type="GENERAL" ai="general" attack_speed="2000" hpgauge="3"><stats maxHp="172" /><bound_radius front="0.25" )"
	R"(side="0.35" upper="2" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:506103-506117 without its <equipment> (:506107-506114)
	R"(<npc_template npc_id="802427" level="65" name="caetess" name_id="356813" height="2.16" title_id="465687" group_drop="LIGHT" )"
	R"(rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" arange="2" attack_speed="2100" )"
	R"(hpgauge="3"><stats maxHp="31089"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius )"
	R"(front="0.3" side="0.42" upper="2.16" /><talk_info distance="5" is_dialog="true" can_talk_invisible="false" /></npc_template>)";

inline constexpr const char* TEMPLATE_ITEMS_XML =
	// item_templates.xml:877060-877062
	R"(<item_template id="182200512" name="Broken Fossil" level="1" cName="quest_1137a" mask="20545" item_group="QUEST" quality="COMMON" price="1" )"
	R"(desc="1106463"><inventory id="2"/></item_template>)"
	// item_templates.xml:876937-876939
	R"(<item_template id="182200203" name="Grain Sack" level="1" cName="quest_1106a" mask="20545" item_group="QUEST" quality="COMMON" price="1" )"
	R"(desc="1106405"><inventory id="2"/></item_template>)"
	// item_templates.xml:877008-877010
	R"(<item_template id="182200224" name="Polinia's Ointment" level="1" cName="quest_1118a" mask="20545" item_group="QUEST" quality="COMMON" )"
	R"(price="1" desc="1106427"><inventory id="2"/></item_template>)"
	// item_templates.xml:882807-882813
	R"(<item_template id="182207127" name="Lifeform Remodeling Report" level="1" cName="doc_quest_4914a" mask="20545" quality="COMMON" price="1" )"
	R"(race="ASMODIANS" desc="1108251" activate_target="STANDALONE" activate_count="1000"><actions><read/></actions><uselimits usedelay="2000" )"
	R"(usedelayid="41"/><inventory id="2"/></item_template>)"
	// item_templates.xml:890264-890266
	R"(<item_template id="182215663" name="Vespine's Luciferin" level="1" cName="quest_15002a" mask="28736" max_stack_count="20" )"
	R"(item_group="QUEST" quality="COMMON" price="1" race="ELYOS" desc="1800870"><inventory id="2"/></item_template>)"
	// item_templates.xml:924099-924104
	R"(<item_template id="188053787" name="Stigma Support Bundle" level="20" cName="wrap_stigma_quest_rank_03" casting_delay="1000" mask="12376" )"
	R"(max_stack_count="100" quality="RARE" price="5" desc="845233" activate_target="STANDALONE" )"
	R"(activate_count="1"><actions><decompose/></actions><uselimits usedelay="3000" usedelayid="85"/></item_template>)"
	// item_templates.xml:889101-889103
	R"(<item_template id="182215333" name="Destruction Orders" level="1" cName="quest_3340a" mask="20545" item_group="QUEST" quality="COMMON" )"
	R"(price="1" desc="1141749"><inventory id="2"/></item_template>)"
	// item_templates.xml:889647-889649
	R"(<item_template id="182215485" name="Shell Ember" level="1" cName="quest_13809a" mask="20544" item_group="QUEST" quality="COMMON" price="1" )"
	R"(race="ELYOS" desc="1141777"><inventory id="2"/></item_template>)"
	// item_templates.xml:889650-889652
	R"(<item_template id="182215486" name="Brittle Outer Scale" level="1" cName="quest_13809b" mask="20544" item_group="QUEST" quality="COMMON" )"
	R"(price="1" race="ELYOS" desc="1141779"><inventory id="2"/></item_template>)"
	// item_templates.xml:889653-889655
	R"(<item_template id="182215487" name="Crimson Bloodstain" level="1" cName="quest_13809c" mask="20544" item_group="QUEST" quality="COMMON" )"
	R"(price="1" race="ELYOS" desc="1141781"><inventory id="2"/></item_template>)";

/** player_experience_table.xml:3-64, the first 62 levels (a level-30 player for the stigma quest, level 60 for the Raksang zone quests) */
inline constexpr const char* TEMPLATE_EXPERIENCE_TABLE_XML =
	"<player_experience_table><exp>0</exp><exp>400</exp><exp>1433</exp><exp>3820</exp><exp>9054</exp><exp>17655</exp><exp>30978</exp>"
	"<exp>52010</exp><exp>82982</exp><exp>126069</exp><exp>182252</exp><exp>260622</exp><exp>360825</exp><exp>490331</exp><exp>649169</exp>"
	"<exp>844378</exp><exp>1083018</exp><exp>1401356</exp><exp>1808613</exp><exp>2314771</exp><exp>2941893</exp><exp>3769257</exp>"
	"<exp>4811154</exp><exp>6110198</exp><exp>7632340</exp><exp>9377726</exp><exp>11395643</exp><exp>13731725</exp><exp>16339413</exp>"
	"<exp>19378549</exp><exp>23162749</exp><exp>27585843</exp><exp>32841197</exp><exp>39127217</exp><exp>47350762</exp><exp>57829684</exp>"
	"<exp>70654362</exp><exp>87571065</exp><exp>107018757</exp><exp>129815732</exp><exp>157211282</exp><exp>189272188</exp>"
	"<exp>226933751</exp><exp>267247400</exp><exp>310053925</exp><exp>355815203</exp><exp>404823687</exp><exp>456685353</exp>"
	"<exp>511683757</exp><exp>570162075</exp><exp>632268545</exp><exp>701585822</exp><exp>776831823</exp><exp>857090855</exp>"
	"<exp>947120930</exp><exp>1051346275</exp><exp>1175571620</exp><exp>1318550121</exp><exp>1484090156</exp><exp>1674064804</exp>"
	"<exp>1913274732</exp><exp>2162140395</exp></player_experience_table>";

inline constexpr int32_t ELPAS = 203049;       // Poeta: 1101's start npc (poeta.xml:54)
inline constexpr int32_t KALES = 203050;       // 1106's start npc (poeta.xml:56)
inline constexpr int32_t ASTEROS = 203058;     // 1115's second and last step (poeta.xml:60)
inline constexpr int32_t POLINIA = 203059;     // 1104's end npc, 1118's start npc (poeta.xml:55, 62)
inline constexpr int32_t UNO = 203061;         // 1106's end npc
inline constexpr int32_t KUSTANON = 203070;    // 1118's first step (poeta.xml:63)
inline constexpr int32_t FEIRA = 203072;       // 1115's first step (poeta.xml:59)
inline constexpr int32_t NAMUS = 203075;       // 1115's start npc (poeta.xml:58)
inline constexpr int32_t MELPONEH = 203079;    // 1118's second and last step (poeta.xml:64)
inline constexpr int32_t HNOSS = 203385;        // 4914's second and last step (pandaemonium.xml:350)
inline constexpr int32_t GRAVE_SENTRY = 210504; // one of 24230's two kill targets (quest_data.xml:57981)
inline constexpr int32_t INDRATU_DEFENDER = 213964; // 1666's second kill row, seq 1 (quest_data.xml:5864-5865)
inline constexpr int32_t FLORARINERK = 831999;  // 80545's start and end npc (sanctum.xml:358)
inline constexpr int32_t FIELD_STONE = 702760;  // 35052's kill target (quest_data.xml:68100)
inline constexpr int32_t PERSEPHONE = 798380;   // 13830's end npc (stigma.xml:19)
inline constexpr int32_t MELANKA = 804662;      // 29601's start npc (fatebound_abbey.xml:40)
inline constexpr int32_t ALDA = 804663;         // 29601's first step
inline constexpr int32_t LEOPOLD = 804664;      // 29601's second step
inline constexpr int32_t SIGURD = 804666;       // 29601's fourth and last step
inline constexpr int32_t NUBES = 804698;        // 15002's start and end npc (cygnea.xml:151)
inline constexpr int32_t BRUNTE = 804709;       // 18970's start npc (cygnea.xml:147)
inline constexpr int32_t DONAND = 804941;       // 35052's end npc (alabaster_order.xml:159)
inline constexpr int32_t FINDERYUX = 805213;    // one of 18970's three end npcs
inline constexpr int32_t BRODIR = 832821;       // 24230's start and end npc (altgard.xml:229)
inline constexpr int32_t GRAIN_SACK_1106 = 182200203;      // 1106's work item (quest_data.xml:934)
inline constexpr int32_t POLINIAS_OINTMENT = 182200224;    // 1118's work item (quest_data.xml:1018)
inline constexpr int32_t REMODELING_REPORT = 182207127;    // 4914's start item and work item (pandaemonium.xml:348, quest_data.xml:25815)
inline constexpr int32_t VESPINE_LUCIFERIN = 182215663;    // 15002's collect item, 7 of them (quest_data.xml:42994)
inline constexpr int32_t BROKEN_FOSSIL = 182200512;        // 1137's work item (quest_data.xml:1192)
inline constexpr int32_t SPIROS = 203111;       // Verteron: 1137's start and end npc (verteron.xml:140)
inline constexpr int32_t CASTOR = 203965;       // 16900's end npc, end_dialog_id 2375 (eltnen.xml:469)
inline constexpr int32_t LAIGAS = 204042;       // 3340's next npc and end npc (eltnen.xml:392)
inline constexpr int32_t WANTED_POSTER = 730105; // 3018's start npc, start_dialog_id 4762 (theobomos.xml:151)
inline constexpr int32_t CRIOS = 798150;        // 3018's end npc, start_dialog_id2 1011
inline constexpr int32_t LAESTRYGOS = 799038;   // 11250's start npc, start_dialog_id 4762 (inggison.xml:441)
/** The start zone of 18739 and 18741 (raksang_ruins.xml:33, 41), the only start_zone of the data */
inline constexpr const char* IDRAKSHA_ZONE = "IDRAKSHA_SOLO_STARTCHIOCE_NPC_206390_7_300610000";
inline constexpr int32_t STIGMA_SUPPORT_BUNDLE = 188053787; // 13830's reward item
inline constexpr int32_t SILVIUS = 263597;       // Reshanta: 1839's start and end npc (reshanta.xml:618)
inline constexpr int32_t ASHIKAR_SWORDLORD = 214744; // one of 1839's kill targets, 67 of them (quest_data.xml:7690)
inline constexpr int32_t JAVLANTIA = 800413;     // Heiron: 1666's start and end npc (heiron.xml:488)
inline constexpr int32_t WEBTOE_VORTILE = 235790; // 15001's first kill row, var 1 (quest_data.xml:42989)
inline constexpr int32_t BLUESHROOM_KSELLID = 235799; // 15001's second kill row, var 2 (quest_data.xml:42990)
inline constexpr int32_t SCORCHED_TREE = 730969; // Kaldor: 13809's step 0 (kaldor.xml:60)
inline constexpr int32_t CINDERY_TREE = 730970;  // 13809's step 1
inline constexpr int32_t BURNT_TREE = 730971;    // 13809's step 2
inline constexpr int32_t CAETESS = 802427;       // 13809's start npc and last step (kaldor.xml:59, 63)
inline constexpr int32_t DESTRUCTION_ORDERS = 182215333; // 3340's collect item, dropped at its step 1 (quest_data.xml:20831, 20834)
inline constexpr int32_t SHELL_EMBER = 182215485;        // 13809's three work items (quest_data.xml:41449-41451)
inline constexpr int32_t BRITTLE_OUTER_SCALE = 182215486;
inline constexpr int32_t CRIMSON_BLOODSTAIN = 182215487;

inline Ptr<gameserver::model::gameobjects::VisibleObject> at(gameserver::model::gameobjects::VisibleObject& object) {
	return Ptr<gameserver::model::gameobjects::VisibleObject>(object);
}

class QuestTemplateTest : public QuestHandlerTest {
protected:
	void SetUp() override {
		QuestHandlerTest::SetUp();
		// MonsterHunt's debug message goes to players of this access level; the unit tests load no configuration, so Java's default (9)
		savedDialogInfo = configs::administration::AdminConfig::DIALOG_INFO.load();
		configs::administration::AdminConfig::DIALOG_INFO.store(9);
		// the handler-base rows and this file's, in one holder each
		dataholders::DataManager::QUEST_DATA.resetForTests();
		std::string questsXml(HANDLER_QUESTS_XML);
		questsXml.erase(questsXml.rfind("</quests>"));
		questsXml += TEMPLATE_QUESTS_XML;
		questsXml += "</quests>";
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(contexts.emplace_back(), questsXml));
		dataholders::DataManager::NPC_DATA.resetForTests();
		std::string npcsXml(HANDLER_NPCS_XML);
		npcsXml.erase(npcsXml.rfind("</npc_templates>"));
		npcsXml += TEMPLATE_NPCS_XML;
		npcsXml += "</npc_templates>";
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), npcsXml));
		std::string itemsXml(items::ITEM_TEMPLATES_XML);
		itemsXml.erase(itemsXml.rfind("</item_templates>"));
		itemsXml += HANDLER_ITEMS_XML;
		itemsXml += TEMPLATE_ITEMS_XML;
		itemsXml += "</item_templates>";
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(contexts.emplace_back(), itemsXml));
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(contexts.emplace_back(), TEMPLATE_EXPERIENCE_TABLE_XML));
		dataholders::DataManager::XML_QUESTS.resetForTests();
		dataholders::DataManager::XML_QUESTS.publish(xml::bindString<dataholders::XMLQuests>(contexts.emplace_back(), TEMPLATE_SCRIPTS_XML));
	}

	void TearDown() override {
		QuestHandlerTest::TearDown();
		dataholders::DataManager::XML_QUESTS.resetForTests();
		configs::administration::AdminConfig::DIALOG_INFO.store(savedDialogInfo);
	}

	int8_t savedDialogInfo = 0;

	/** QuestEngine.init's loop body for one quest (QuestEngine.java:104-105): the kind's register_ builds the template and adds it */
	static void registerXml(int32_t questId) {
		const models::XMLQuest* quest = dataholders::DataManager::XML_QUESTS->getQuest(questId);
		ASSERT_NE(quest, nullptr) << questId;
		quest->register_(QuestEngine::getInstance());
	}

	/** A copy of a QuestNpc list of the engine (getQuestNpc answers a new, empty QuestNpc for an npc nothing registered) */
	static std::vector<int32_t> talkQuests(int32_t npcId) { return QuestEngine::getInstance().getQuestNpc(npcId)->getOnTalkEvent().snapshot(); }
	static std::vector<int32_t> startQuests(int32_t npcId) { return QuestEngine::getInstance().getQuestNpc(npcId)->getOnQuestStart().snapshot(); }
	static std::vector<int32_t> killQuests(int32_t npcId) { return QuestEngine::getInstance().getQuestNpc(npcId)->getOnKillEvent().snapshot(); }

	/**
	 * A dialog action as CM_DIALOG_SELECT hands it to QuestEngine.onDialog (the quest id set; 0 for a talk, which goes through the npc's
	 * onTalkEvent list), after clearing the quester's packets. Fails the case on an error of the quest engine's loggers.
	 */
	bool talk(Quester& quester, int32_t questId, int32_t dialogActionId, gameserver::model::gameobjects::Npc& npc) {
		quester.clearSent();
		network::test::LogCapture log({"com.aionemu.gameserver.questEngine"});
		bool answer = QuestEngine::getInstance().onDialog(*envOf(quester, questId, dialogActionId, at(npc)));
		if (log.contains("error|"))
			ADD_FAILURE() << "quest " << questId << " action " << dialogActionId << ":\n" << log.dump();
		return answer;
	}

	/** A kill of `npc` as NpcController.doReward hands it to QuestEngine.onKill (the env without a quest id), after clearing the packets */
	void kill(Quester& quester, gameserver::model::gameobjects::Npc& npc) {
		quester.clearSent();
		network::test::LogCapture log({"com.aionemu.gameserver.questEngine"});
		EXPECT_TRUE(QuestEngine::getInstance().onKill(*QuestEnv::create(at(npc), quester.player(), 0)));
		if (log.contains("error|"))
			ADD_FAILURE() << "kill of " << npc.getNpcId() << ":\n" << log.dump();
	}

	/** An entry into `zone` as the zone's handler hands it to QuestEngine.onEnterZone (the env without a quest id), after clearing the packets */
	bool enterZone(Quester& quester, const world::zone::ZoneName* zone) {
		quester.clearSent();
		network::test::LogCapture log({"com.aionemu.gameserver.questEngine"});
		bool answer = QuestEngine::getInstance().onEnterZone(*QuestEnv::create(nullptr, quester.player(), 0), zone);
		if (log.contains("error|"))
			ADD_FAILURE() << "enter zone " << zone->name() << ":\n" << log.dump();
		return answer;
	}

	/** QuestEngine.onLevelChanged for the quester, after clearing the packets */
	void levelChanged(Quester& quester) {
		quester.clearSent();
		network::test::LogCapture log({"com.aionemu.gameserver.questEngine"});
		QuestEngine::getInstance().onLevelChanged(quester.player());
		if (log.contains("error|"))
			ADD_FAILURE() << "level change:\n" << log.dump();
	}

	/** QuestEngine.onEnterWorld for the quester, after clearing the packets */
	void enterWorld(Quester& quester) {
		quester.clearSent();
		network::test::LogCapture log({"com.aionemu.gameserver.questEngine"});
		QuestEngine::getInstance().onEnterWorld(quester.player());
		if (log.contains("error|"))
			ADD_FAILURE() << "enter world:\n" << log.dump();
	}

	/** The packets of one opcode the quester was sent */
	static std::vector<std::vector<uint8_t>> sentOf(Quester& quester, int32_t opcode) { return items::packetsOf(quester.sent(), opcode); }
};

} // namespace aion::gameserver::questEngine::handlers::test::templates
