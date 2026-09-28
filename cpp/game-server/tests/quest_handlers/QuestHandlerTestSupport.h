#pragma once

// P5-06b, M5d H-07 (m5d-plan.md §7): the shared fixture of the handler-base tests - AbstractQuestHandler's helpers driven on in-world players.
//
// - The players stand in a Poeta map instance whose packets a TestClient records (tests/cm_ak/ItemPacketTestSupport.h, included by relative path
//   as tests/quest does). Each is online and stored in the World, as QuestLifecycleTest's quester is (PlayerCommonData.getPlayer finds an
//   online player through the World, which the exp reward of finishQuest needs). The level is set while the Player does not exist yet, so no
//   level change runs; a level above 9 needs the daeva flag first (PlayerCommonData.setExp keeps a non-daeva below level 10).
// - Rows are copied from the shipped data (file:line beside each; whitespace between tags removed, an npc's <equipment> left out as the comment
//   says: no case reads it). The experience table is its first 26 levels (player_experience_table.xml:3-28).
// - ProbeHandler is a fabricated handler registered with the engine for a real quest: it records every onDialogEvent QuestEngine.onDialog hands
//   it (the follow-up of sendQuestEndDialog lands there) and answers with a fixed value. PlainHandler keeps every hook of AbstractQuestHandler;
//   the cases call its public helpers directly.
// - Expected packets are Java's bytes where the fields are the packet's own (the writeImpl of SM_DIALOG_WINDOW.java, SM_QUEST_ACTION.java,
//   SM_PLAY_MOVIE.java, SM_ITEM_USAGE_ANIMATION.java; opcodes of ServerPacketsOpcodes.java); a packet whose body the server builds from other
//   objects (an item blob, a localized message, SM_EMOTION's position) is compared against the server's own serialization of the packet Java
//   constructs there.

#include "../cm_ak/ItemPacketTestSupport.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/utils/cron/ThreadPoolManagerRunnableRunner.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test {

namespace cp = network::aion::clientpackets::testing;
namespace items = network::aion::clientpackets::testing::items;
using model::QuestEnv;
using model::QuestState;
using model::QuestStatus;
using network::test::PacketWriter;
using runtime::Ptr;
using runtime::Ref;

inline constexpr const char* HANDLER_QUESTS_XML =
	R"(<quests>)"
	// quest_data.xml:9-19
	R"(<quest id="1001" name="The Kerubim Threat" nameId="1102001" quest_zone="Poeta" minlevel_permitted="2" max_repeat_count="1")"
	R"( cannot_share="true" cannot_giveup="true" race_permitted="ELYOS" category="MISSION"><collect_items><collect_item)"
	R"( item_id="182200001" count="3"/></collect_items><rewards exp="2100"><selectable_reward_item item_id="114100806")"
	R"( count="1"/><selectable_reward_item item_id="114300816" count="1"/><selectable_reward_item item_id="114500778")"
	R"( count="1"/></rewards><quest_drop npc_id="210671" item_id="182200001" drop_each_member="1" collecting_step="7"/></quest>)"
	// quest_data.xml:884-894
	R"(<quest id="1100" name="Kalio's Call" nameId="1102200" quest_zone="Poeta" minlevel_permitted="3" max_repeat_count="1")"
	R"( cannot_share="true" cannot_giveup="true" race_permitted="ELYOS" category="MISSION"><rewards exp="510"><selectable_reward_item)"
	R"( item_id="100000095" count="1"/><selectable_reward_item item_id="100200113" count="1"/><selectable_reward_item)"
	R"( item_id="100100012" count="1"/><selectable_reward_item item_id="100600035" count="1"/><selectable_reward_item)"
	R"( item_id="115000011" count="1"/><selectable_reward_item item_id="101800182" count="1"/><selectable_reward_item)"
	R"( item_id="102000195" count="1"/></rewards></quest>)"
	// quest_data.xml:895-897
	R"(<quest id="1101" name="Sleeping on the Job" nameId="1102201" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1")"
	R"( can_report="true" race_permitted="ELYOS" category="IMPORTANT"><rewards gold="120" exp="130"/></quest>)"
	// quest_data.xml:898-904
	R"(<quest id="1102" name="Kerubar Hunt" nameId="1102202" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1")"
	R"( can_report="true" cannot_share="true" race_permitted="ELYOS" category="IMPORTANT"><rewards gold="400" exp="180"/><quest_kill)"
	R"( step="0" var="0" count="3" npc_ids="210133 210134" seq="0"/><start_conditions><finished)"
	R"( quest_id="1101"/></start_conditions></quest>)"
	// quest_data.xml:905-914
	R"(<quest id="1103" name="Grain Thieves" nameId="1102203" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1")"
	R"( can_report="true" cannot_share="true" race_permitted="ELYOS" category="IMPORTANT"><collect_items><collect_item)"
	R"( item_id="182200201" count="3"/></collect_items><rewards gold="290" exp="590"/><quest_drop npc_id="700105")"
	R"( item_id="182200201"/><start_conditions><finished quest_id="1102"/></start_conditions></quest>)"
	// quest_data.xml:915-920
	R"(<quest id="1104" name="Report to Polinia" nameId="1102204" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1")"
	R"( can_report="true" cannot_share="true" race_permitted="ELYOS" category="IMPORTANT"><rewards gold="90")"
	R"( exp="520"/><start_conditions><finished quest_id="1103"/></start_conditions></quest>)"
	// quest_data.xml:964-972
	R"(<quest id="1111" name="Insomnia Medicine" nameId="1102211" quest_zone="Poeta" minlevel_permitted="3" max_repeat_count="1")"
	R"( can_report="true" race_permitted="ELYOS" category="QUEST"><collect_items><collect_item item_id="182200223")"
	R"( count="3"/></collect_items><rewards gold="960" exp="1595"/><rewards gold="450" exp="1150" ccheck="-1"/><quest_drop)"
	R"( npc_id="210261" item_id="182200223"/><quest_drop npc_id="210674" item_id="182200223"/></quest>)"
	// quest_data.xml:989-995
	R"(<quest id="1114" name="The Nymph's Gown" nameId="1102214" quest_zone="Poeta" minlevel_permitted="4" max_repeat_count="1")"
	R"( can_report="true" cannot_share="true" race_permitted="ELYOS" category="QUEST"><rewards gold="1920" exp="4367"/><rewards)"
	R"( gold="960" exp="3120"/><quest_work_items><quest_work_item item_id="182200217"/></quest_work_items></quest>)"
	// quest_data.xml:1654-1662
	R"(<quest id="1194" name="[Group] Reducing Tursin Strength" nameId="1102364" quest_zone="Verteron" minlevel_permitted="18")"
	R"( max_repeat_count="1" cannot_share="true" race_permitted="ELYOS" category="QUEST"><rewards exp="90000"><selectable_reward_item)"
	R"( item_id="121000766" count="1"/><selectable_reward_item item_id="121000767" count="1"/></rewards><start_conditions><finished)"
	R"( quest_id="1193"/></start_conditions></quest>)"
	// quest_data.xml:1663-1667
	R"(<quest id="1195" name="[Group] Tursin Assassination" nameId="1102365" quest_zone="Verteron" minlevel_permitted="19")"
	R"( max_repeat_count="1" cannot_share="true" race_permitted="ELYOS" category="QUEST"><start_conditions><finished)"
	R"( quest_id="1194"/></start_conditions></quest>)"
	// quest_data.xml:9395-9407
	R"(<quest id="2014" name="Scout it Out" nameId="1103114" quest_zone="Altgard" minlevel_permitted="10" max_repeat_count="1")"
	R"( cannot_share="true" cannot_giveup="true" race_permitted="ASMODIANS" category="MISSION"><collect_items><collect_item)"
	R"( item_id="182203015" count="1"/></collect_items><rewards exp="34650"><selectable_reward_item item_id="114100796")"
	R"( count="1"/><selectable_reward_item item_id="114300806" count="1"/><selectable_reward_item item_id="114500768")"
	R"( count="1"/><selectable_reward_item item_id="114600727" count="1"/></rewards><quest_drop npc_id="700136" item_id="182203015")"
	R"( drop_each_member="1" collecting_step="1"/><class_permitted>WARRIOR SCOUT MAGE PRIEST ENGINEER ARTIST GLADIATOR TEMPLAR ASSASSIN)"
	R"( RANGER SORCERER SPIRIT_MASTER CHANTER CLERIC GUNNER BARD</class_permitted></quest>)"
	// quest_data.xml:9408-9424
	R"(<quest id="2015" name="Take the Initiative" nameId="1103115" quest_zone="Altgard" minlevel_permitted="12" max_repeat_count="1")"
	R"( cannot_share="true" cannot_giveup="true" race_permitted="ASMODIANS" category="MISSION"><collect_items><collect_item)"
	R"( item_id="182203016" count="1"/></collect_items><rewards exp="34181"><selectable_reward_item item_id="111100764")"
	R"( count="1"/><selectable_reward_item item_id="111300769" count="1"/><selectable_reward_item item_id="111500752")"
	R"( count="1"/><selectable_reward_item item_id="111600744" count="1"/><selectable_reward_item item_id="111301587"/><reward_item)"
	R"( item_id="186000006" count="1"/></rewards><start_conditions><finished)"
	R"( quest_id="2014"/></start_conditions><class_permitted>WARRIOR SCOUT MAGE PRIEST ENGINEER ARTIST GLADIATOR TEMPLAR ASSASSIN)"
	R"( RANGER SORCERER SPIRIT_MASTER CHANTER CLERIC GUNNER BARD</class_permitted></quest>)"
	// quest_data.xml:9442-9458
	R"(<quest id="2017" name="Trespassers at the Observatory" nameId="1103117" quest_zone="Altgard" minlevel_permitted="15")"
	R"( max_repeat_count="1" cannot_share="true" cannot_giveup="true" race_permitted="ASMODIANS")"
	R"( category="MISSION"><collect_items><collect_item item_id="182203020" count="1"/></collect_items><rewards)"
	R"( exp="39601"><selectable_reward_item item_id="112100724" count="1"/><selectable_reward_item item_id="112300721")"
	R"( count="1"/><selectable_reward_item item_id="112500709" count="1"/><selectable_reward_item item_id="112600720")"
	R"( count="1"/><reward_item item_id="186000006" count="2"/></rewards><quest_drop npc_id="210532" item_id="182203020")"
	R"( drop_each_member="1" collecting_step="7"/><start_conditions><finished)"
	R"( quest_id="2015"/></start_conditions><class_permitted>WARRIOR SCOUT MAGE PRIEST ENGINEER ARTIST GLADIATOR TEMPLAR ASSASSIN)"
	R"( RANGER SORCERER SPIRIT_MASTER CHANTER CLERIC GUNNER BARD</class_permitted></quest>)"
	// quest_data.xml:10709-10723
	R"(<quest id="2236" name="Rarified Tastes" nameId="1103336" quest_zone="Altgard" minlevel_permitted="14" max_repeat_count="1")"
	R"( race_permitted="ASMODIANS" category="IMPORTANT"><collect_items><collect_item item_id="182203225")"
	R"( count="5"/></collect_items><rewards exp="16495"><reward_item item_id="186000006" count="1"/></rewards><quest_drop)"
	R"( npc_id="210567" item_id="182203225" drop_each_member="1"/><quest_drop npc_id="210568" item_id="182203225")"
	R"( drop_each_member="1"/><quest_drop npc_id="210449" item_id="182203225" drop_each_member="1"/><quest_drop npc_id="210450")"
	R"( item_id="182203225" drop_each_member="1"/><start_conditions><finished quest_id="24112"/></start_conditions></quest>)"
	// quest_data.xml:10724-10735
	R"(<quest id="2237" name="A Fertile Field" nameId="1103337" quest_zone="Altgard" minlevel_permitted="14" max_repeat_count="1")"
	R"( race_permitted="ASMODIANS" category="IMPORTANT"><collect_items><collect_item item_id="182203226")"
	R"( count="3"/></collect_items><rewards gold="9420" exp="16495"><reward_item item_id="186000006" count="1"/></rewards><quest_drop)"
	R"( npc_id="700145" item_id="182203226" drop_each_member="1"/><start_conditions><finished)"
	R"( quest_id="24112"/></start_conditions></quest>)"
	// quest_data.xml:11192-11211
	R"(<quest id="2292" name="Making a New Start" nameId="1103392" quest_zone="Altgard" minlevel_permitted="14" max_repeat_count="1")"
	R"( race_permitted="ASMODIANS" category="IMPORTANT"><collect_items><collect_item item_id="122000039" count="1"/><collect_item)"
	R"( item_id="122000040" count="1"/><collect_item item_id="122000041" count="1"/></collect_items><rewards)"
	R"( exp="16495"><selectable_reward_item item_id="120001520"/><selectable_reward_item item_id="120001521"/></rewards><quest_drop)"
	R"( npc_id="210599" item_id="122000039" drop_each_member="1"/><quest_drop npc_id="210622" item_id="122000039")"
	R"( drop_each_member="1"/><quest_drop npc_id="210623" item_id="122000040" drop_each_member="1"/><quest_drop npc_id="210620")"
	R"( item_id="122000040" drop_each_member="1"/><quest_drop npc_id="210621" item_id="122000041" drop_each_member="1"/><quest_drop)"
	R"( npc_id="210624" item_id="122000041" drop_each_member="1"/><start_conditions><finished)"
	R"( quest_id="24112"/></start_conditions></quest>)"
	// quest_data.xml:17459-17480
	R"(<quest id="2911" name="Song of Blessing" nameId="1104111" quest_zone="Pandaemonium" minlevel_permitted="10" max_repeat_count="1")"
	R"( cannot_share="true" race_permitted="ASMODIANS" category="QUEST"><rewards exp="2820"/><rewards gold="1000" exp="2250"/><start_conditions>)"
	R"(<finished quest_id="2009" reward="0"/></start_conditions><start_conditions><finished quest_id="2009" reward="1"/></start_conditions>)"
	R"(<start_conditions><finished quest_id="2009" reward="2"/></start_conditions><start_conditions><finished quest_id="2009" reward="3"/>)"
	R"(</start_conditions><start_conditions><finished quest_id="2009" reward="4"/></start_conditions><start_conditions><finished)"
	R"( quest_id="2009" reward="5"/></start_conditions></quest>)"
	// quest_data.xml:21636-21665
	R"(<quest id="3905" name="[Group] To Catch a Dragon" nameId="1103004" quest_zone="Sanctum" minlevel_permitted="35" max_repeat_count="1")"
	R"( race_permitted="ELYOS" category="QUEST" restricted="true"><collect_items><collect_item item_id="186000067" count="1"/><collect_item)"
	R"( item_id="186000068" count="1"/><collect_item item_id="186000069" count="1"/><collect_item item_id="182400001")"
	R"( count="2400000"/></collect_items><rewards exp="806224"><reward_item item_id="186000003" count="3"/><reward_item item_id="188050808")"
	R"( count="1"/></rewards><quest_drop item_id="186000067" npc_id="214804" chance="5"/><quest_drop item_id="186000068" npc_id="214804")"
	R"( chance="5"/><quest_drop item_id="186000069" npc_id="214804" chance="5"/><quest_drop item_id="186000067" npc_id="700462")"
	R"( chance="5"/><quest_drop item_id="186000068" npc_id="700462" chance="5"/><quest_drop item_id="186000069" npc_id="700462")"
	R"( chance="5"/><quest_drop item_id="186000067" npc_id="700463" chance="5"/><quest_drop item_id="186000068" npc_id="700463")"
	R"( chance="5"/><quest_drop item_id="186000069" npc_id="700463" chance="5"/><quest_drop item_id="186000067" npc_id="700464")"
	R"( chance="5"/><quest_drop item_id="186000068" npc_id="700464" chance="5"/><quest_drop item_id="186000069" npc_id="700464")"
	R"( chance="5"/><quest_drop item_id="186000067" npc_id="700471" chance="5"/><quest_drop item_id="186000068" npc_id="700471")"
	R"( chance="5"/><quest_drop item_id="186000069" npc_id="700471" chance="5"/><quest_drop item_id="186000067" npc_id="700466")"
	R"( chance="5"/><quest_drop item_id="186000068" npc_id="700466" chance="5"/><quest_drop item_id="186000069" npc_id="700466")"
	R"( chance="5"/></quest>)"
	// quest_data.xml:57650-57655
	R"(<quest id="24112" name="No Laissez-faire for Lepharists" nameId="1129973" quest_zone="Altgard" minlevel_permitted="14")"
	R"( max_repeat_count="1" cannot_share="true" race_permitted="ASMODIANS" category="IMPORTANT"><rewards gold="15480")"
	R"( exp="17552"><reward_item item_id="188053406" count="1"/></rewards><quest_kill step="0" var="0" count="1" npc_ids="210510")"
	R"( seq="0"/></quest>)"
	// quest_data.xml:81913-81922
	R"(<quest id="80673" name="[Event] Verify Symphony of Legend - The First Movement" nameId="1800533" quest_zone="Event")"
	R"( minlevel_permitted="10" max_repeat_count="255" cannot_share="true" race_permitted="PC_ALL")"
	R"( category="EVENT"><collect_items><collect_item item_id="188100252" count="10"/></collect_items><rewards/><bonus level="1")"
	R"( type="EVENTS"/><start_conditions><finished quest_id="80677"/></start_conditions></quest>)"
	// quest_data.xml:81923-81932
	R"(<quest id="80674" name="[Event] Verify Symphony of Legend - The Second Movement" nameId="1800534" quest_zone="Event")"
	R"( minlevel_permitted="10" max_repeat_count="255" cannot_share="true" race_permitted="PC_ALL")"
	R"( category="EVENT"><collect_items><collect_item item_id="188100253" count="10"/></collect_items><rewards/><bonus level="2")"
	R"( type="EVENTS"/><start_conditions><finished quest_id="80677"/></start_conditions></quest>)"
	// quest_data.xml:81933-81942
	R"(<quest id="80675" name="[Event] Verify Symphony of Legend - The Third Movement" nameId="1800535" quest_zone="Event")"
	R"( minlevel_permitted="10" max_repeat_count="255" cannot_share="true" race_permitted="PC_ALL")"
	R"( category="EVENT"><collect_items><collect_item item_id="188100254" count="10"/></collect_items><rewards/><bonus level="3")"
	R"( type="EVENTS"/><start_conditions><finished quest_id="80677"/></start_conditions></quest>)"
	// quest_data.xml:81943-81952
	R"(<quest id="80676" name="[Event] Verify Symphony of Legend - The Fourth Movement" nameId="1800536" quest_zone="Event")"
	R"( minlevel_permitted="10" max_repeat_count="255" cannot_share="true" race_permitted="PC_ALL")"
	R"( category="EVENT"><collect_items><collect_item item_id="188100255" count="10"/></collect_items><rewards/><bonus level="4")"
	R"( type="EVENTS"/><start_conditions><finished quest_id="80677"/></start_conditions></quest>)"
	// quest_data.xml:81953-81960
	R"(<quest id="80677" name="[Event] How to Verify the Symphony of Legend" nameId="1800537" quest_zone="Event")"
	R"( minlevel_permitted="10" max_repeat_count="1" cannot_share="true" race_permitted="PC_ALL")"
	R"( category="EVENT"><collect_items><collect_item item_id="188100252" count="10"/></collect_items><rewards><reward_item)"
	R"( item_id="182007170" count="1"/></rewards></quest>)"
	R"(</quests>)";

/** Added to the item fixture's rows (ItemPacketTestSupport.h: kinah among them) */
inline constexpr const char* HANDLER_ITEMS_XML =
	// item_templates.xml:876931-876933
	R"(<item_template id="182200201" name="Kerub Grain Sack" level="1" cName="quest_1103a" mask="28736" max_stack_count="20")"
	R"( item_group="QUEST" quality="COMMON" price="1" desc="1106401"><inventory id="2"/></item_template>)"
	// item_templates.xml:876987-876989
	R"(<item_template id="182200217" name="Nymph's Dress" level="1" cName="quest_1114b" mask="20545" item_group="QUEST")"
	R"( quality="COMMON" price="1" desc="1106421"><inventory id="2"/></item_template>)"
	// item_templates.xml:877005-877007
	R"(<item_template id="182200223" name="Sylphen Wings" level="1" cName="quest_1111a" mask="28736" max_stack_count="20")"
	R"( item_group="QUEST" quality="COMMON" price="1" desc="1106413"><inventory id="2"/></item_template>)"
	// item_templates.xml:921734-921739
	R"(<item_template id="188053406" name="Lesser Supply Manastone Box" level="65" cName="wrap_matter_option_tq_reward_10a")"
	R"( casting_delay="1500" mask="12360" max_stack_count="100" quality="COMMON" price="5" desc="840634" activate_target="STANDALONE")"
	R"( activate_count="1"><actions><decompose/></actions><uselimits usedelay="5000" usedelayid="85"/></item_template>)"
	// item_templates.xml:876608-876610
	R"(<item_template id="182007170" name="Symphony of Legend Copy" level="1" cName="world_event_movement_fake" mask="28736")"
	R"( max_stack_count="10000" quality="RARE" price="5" desc="842582"><inventory id="2"/></item_template>)"
	// item_templates.xml:896365-896367
	R"(<item_template id="186000067" name="Light Blade Fragment" level="50" cName="quest_ab1_01" mask="28748" max_stack_count="1000")"
	R"( item_group="QUEST" quality="RARE" price="100" desc="746132"><inventory id="2"/></item_template>)";

inline constexpr const char* HANDLER_NPCS_XML =
	R"(<npc_templates>)"
	// npc_templates.xml:1959-1973 without its <equipment> (:1963-1970)
	R"(<npc_template npc_id="203057" level="20" name="mires" name_id="351103" height="2" title_id="350496" group_drop="LIGHT")"
	R"( rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240" arange="2")"
	R"( attack_speed="2000" hpgauge="3"><stats maxHp="2961"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2")"
	R"( group_run_fight="4.2" /></stats><bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true")"
	R"( can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:2539-2553 without its <equipment> (:2543-2550)
	R"(<npc_template npc_id="203098" level="27" name="spatalos" name_id="351022" height="2" title_id="350502" group_drop="LIGHT")"
	R"( rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GUARD" type="ABYSS_GUARD" ai="simple_abyssguard" srange="7" sangle="240")"
	R"( arange="2" attack_speed="2000" hpgauge="3" state="6"><stats maxHp="4622"><speeds walk="1.5" group_walk="1.5" run="6")"
	R"( run_fight="4.2" group_run_fight="4.2" /></stats><bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="5")"
	R"( is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:54505-54510
	R"(<npc_template npc_id="210133" level="1" name="striped kerub" name_id="300110" height="1.372" group_drop="CHERUBIM")"
	R"( rank="DISCIPLINED" rating="NORMAL" race="MAGICALMONSTER" tribe="MONSTER" type="MONSTER" ai="aggressive" srange="7" sangle="240")"
	R"( arange="2" attack_speed="2100" hpgauge="3"><stats maxHp="143"><speeds walk="0.6" group_walk="0.6" run="7" run_fight="5.5")"
	R"( group_run_fight="7" /></stats><bound_radius front="0.525" side="0.275" upper="1.372" /></npc_template>)"
	// npc_templates.xml:440035-440039
	R"(<npc_template npc_id="700105" level="1" name="kerub grain sack" name_id="350753" height="2" group_drop="NONE" rank="DISCIPLINED")"
	R"( rating="NORMAL" tribe="FIELD_OBJECT_LIGHT" type="GENERAL" ai="quest_use_item" sangle="0" attack_speed="2000" hpgauge="3"><stats)"
	R"( maxHp="172" /><bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="3" delay="3" can_talk_invisible="false")"
	R"( /></npc_template>)"
	// npc_templates.xml:441825-441829
	R"(<npc_template npc_id="700462" level="35" name="ancient treasure box" name_id="371198" height="4" group_drop="DRAGONBOX" rank="DISCIPLINED")"
	R"( rating="NORMAL" tribe="FIELD_OBJECT_ALL" type="GENERAL" ai="chest" sangle="0" attack_speed="2000" hpgauge="3"><stats maxHp="7445" />)"
	R"(<bound_radius front="0.5" side="0.7" upper="4" /><talk_info distance="3" delay="4" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:557883-557889
	R"(<npc_template npc_id="832822" level="15" name="anmurnerk" name_id="465611" height="1.16875" title_id="350621" group_drop="NONE")"
	R"( rank="DISCIPLINED" rating="NORMAL" race="BROWNIE" tribe="GENERAL_DARK" type="GENERAL" ai="general" srange="7" arange="2")"
	R"( attack_speed="2000" hpgauge="3"><stats maxHp="2256"><speeds walk="1.5" group_walk="1.5" run="4.23" run_fight="4.23")"
	R"( group_run_fight="4.23" /></stats><bound_radius front="0.595" side="0.3774" upper="1.16875" /><talk_info distance="5")"
	R"( is_dialog="true" can_talk_invisible="false" /></npc_template>)"
	// npc_templates.xml:559193-559207 without its <equipment> (:559197-559204)
	R"(<npc_template npc_id="832967" level="65" name="sonatine" name_id="465897" height="1.8" title_id="465900" group_drop="LIGHT")"
	R"( rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" arange="2")"
	R"( attack_speed="2000" hpgauge="3"><stats maxHp="26116"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2")"
	R"( group_run_fight="4.2" /></stats><bound_radius front="0.25" side="0.35" upper="1.8" /><talk_info distance="5" is_dialog="true")"
	R"( can_talk_invisible="false" /></npc_template>)"
	R"(</npc_templates>)";

/** player_experience_table.xml:3-28, the first 26 levels (getLevelForExp answers at most the length - 1) */
inline constexpr const char* HANDLER_EXPERIENCE_TABLE_XML =
	"<player_experience_table><exp>0</exp><exp>400</exp><exp>1433</exp><exp>3820</exp><exp>9054</exp><exp>17655</exp><exp>30978</exp>"
	"<exp>52010</exp><exp>82982</exp><exp>126069</exp><exp>182252</exp><exp>260622</exp><exp>360825</exp><exp>490331</exp><exp>649169</exp>"
	"<exp>844378</exp><exp>1083018</exp><exp>1401356</exp><exp>1808613</exp><exp>2314771</exp><exp>2941893</exp><exp>3769257</exp>"
	"<exp>4811154</exp><exp>6110198</exp><exp>7632340</exp><exp>9377726</exp></player_experience_table>";

inline constexpr int32_t MIRES = 203057;         // Poeta: 1101's end npc, where 1102, 1103, 1104 start (quest_script_data/poeta.xml:54-55, 114, 130)
inline constexpr int32_t SPATALOS = 203098;      // Verteron: 1194's end npc (_1194ReducingTursinStrength.java:24-25), 1195's start (verteron.xml:188)
inline constexpr int32_t GRAIN_SACK = 700105;    // the quest object 1103 collects from (its <quest_drop>)
inline constexpr int32_t STRIPED_KERUB = 210133; // one of 1102's two kill targets (quest_data.xml:900: npc_ids="210133 210134")
inline constexpr int32_t KERUB_2 = 210134;
inline constexpr int32_t ANMURNERK = 832822;     // Altgard: where 2236, 2237 and 2292 start (altgard.xml:182-183, 201)
inline constexpr int32_t SONATINE = 832967;      // the event npc of 80673-80677, which all start there (event.xml:403-407)
inline constexpr int32_t TREASURE_BOX = 700462;  // one of the six quest objects of 3905, each dropping 186000067, 186000068, 186000069
inline constexpr int32_t KERUB_GRAIN_SACK = 182200201;
inline constexpr int32_t LIGHT_BLADE_FRAGMENT = 186000067;
inline constexpr int32_t NYMPHS_DRESS = 182200217;
inline constexpr int32_t SYLPHEN_WINGS = 182200223;

inline constexpr int32_t SM_DIALOG_WINDOW_OPCODE = 60;
inline constexpr int32_t SM_PLAY_MOVIE_OPCODE = 105;
inline constexpr int32_t SM_QUEST_ACTION_OPCODE = 124;
inline constexpr int32_t SM_NEARBY_QUESTS_OPCODE = 127;

inline constexpr int32_t START = 3; // QuestStatus.value() (QuestStatus.java:11-14)
inline constexpr int32_t REWARD = 4;
inline constexpr int32_t COMPLETE = 5;
inline constexpr int32_t LOCKED = 6;

/** SM_DIALOG_WINDOW (SM_DIALOG_WINDOW.java:29-40), a page other than MAIL and TOWN_CHALLENGE_TASK: D target, H page, D quest, H 0, H 0 */
inline std::vector<uint8_t> dialogWindow(int32_t targetObjectId, int32_t dialogPageId, int32_t questId) {
	return items::javaPacket(SM_DIALOG_WINDOW_OPCODE, PacketWriter().D(targetObjectId).H(dialogPageId).D(questId).H(0).H(0));
}

/** SM_QUEST_ACTION(ADD or UPDATE, qs) (SM_QUEST_ACTION.java:67-81): C type, D quest, C status value, C 0, D vars | flags << 24, H 0 [, C 0] */
inline std::vector<uint8_t> questAction(int32_t type, int32_t questId, int32_t statusValue, int32_t vars = 0) {
	PacketWriter body;
	body.C(type).D(questId).C(statusValue).C(0).D(vars).H(0);
	if (type == 1) // ADD
		body.C(0);
	return items::javaPacket(SM_QUEST_ACTION_OPCODE, body);
}

/** SM_QUEST_ACTION(UPDATE, qs) */
inline std::vector<uint8_t> questUpdate(int32_t questId, int32_t statusValue, int32_t vars = 0) {
	return questAction(2, questId, statusValue, vars);
}

/** SM_NEARBY_QUESTS with no quest (SM_NEARBY_QUESTS.java:22-30): the map instance has no start npc spawned */
inline std::vector<uint8_t> noNearbyQuests() {
	return items::javaPacket(SM_NEARBY_QUESTS_OPCODE, PacketWriter().C(0).H(0));
}

/** SM_PLAY_MOVIE (SM_PLAY_MOVIE.java:27-35): C cutscene movie, D object, D quest, D cutscene, C 0, C canSkip ? 0 : 1 */
inline std::vector<uint8_t> playMovie(bool isCutsceneMovie, int32_t objectId, int32_t questId, int32_t cutsceneId) {
	return items::javaPacket(SM_PLAY_MOVIE_OPCODE, PacketWriter().C(isCutsceneMovie ? 1 : 0).D(objectId).D(questId).D(cutsceneId).C(0).C(0));
}

/**
 * SM_ITEM_USAGE_ANIMATION(playerObjId, itemObjId, itemId, time, end, unk) (SM_ITEM_USAGE_ANIMATION.java:41-49, writeImpl :75-87): D player,
 * D target (the player), D item object, D item, D time, C end, C unk 0, C unk1 0, C unk2 (1 by its initializer, :19), D unk3 (the constructor's
 * unk)
 */
inline std::vector<uint8_t> itemUsageAnimation(int32_t playerObjId, int32_t itemObjId, int32_t itemId, int32_t time, int32_t end, int32_t unk3) {
	return items::javaPacket(items::SM_ITEM_USAGE_ANIMATION_OPCODE,
		PacketWriter().D(playerObjId).D(playerObjId).D(itemObjId).D(itemId).D(time).C(end).C(0).C(0).C(1).D(unk3));
}

/** One onDialogEvent a ProbeHandler received: the env as QuestEngine.onDialog handed it over */
struct DialogCall {
	int32_t handlerQuestId;
	int32_t envQuestId;
	int32_t dialogActionId;
	bool continuation;
	const QuestEnv* env;
	int32_t targetObjectId;
};

/**
 * A fabricated handler for a real quest: registers its start and talk npcs, records each onDialogEvent into the fixture's list and answers
 * `answer`. The engine keeps it after the case (Immortal, RT-11), but QuestEngine::clear in the TearDown makes it unreachable before the list
 * goes.
 */
class ProbeHandler final : public AbstractQuestHandler {
public:
	ProbeHandler(int32_t questId, std::vector<int32_t> startNpcs, std::vector<int32_t> talkNpcs, std::vector<DialogCall>& calls, bool answer)
		: AbstractQuestHandler(questId), startNpcs(std::move(startNpcs)), talkNpcs(std::move(talkNpcs)), calls(calls), answer(answer) {}

	void register_() override {
		for (int32_t npcId : startNpcs)
			qe.registerQuestNpc(npcId)->addOnQuestStart(questId);
		for (int32_t npcId : talkNpcs)
			qe.registerQuestNpc(npcId)->addOnTalkEvent(questId);
	}

	bool onDialogEvent(QuestEnv& env) override {
		Ptr<gameserver::model::gameobjects::VisibleObject> target = env.getVisibleObject();
		calls.push_back({questId, env.getQuestId(), env.getDialogActionId(), env.isDialogContinuationFromPreQuest(), &env,
			target ? target->getObjectId() : 0});
		return answer;
	}

private:
	const std::vector<int32_t> startNpcs;
	const std::vector<int32_t> talkNpcs;
	std::vector<DialogCall>& calls;
	const bool answer;
};

/** Every hook and helper of AbstractQuestHandler as it is; the cases call the public helpers directly */
class PlainHandler final : public AbstractQuestHandler {
public:
	explicit PlainHandler(int32_t questId) : AbstractQuestHandler(questId) {}

	void register_() override {}
};

class QuestHandlerSpawnTemplate final : public gameserver::model::templates::spawns::SpawnTemplate {
public:
	QuestHandlerSpawnTemplate(gameserver::model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** A player of the fixture with the client that records his packets */
struct Quester {
	cp::PlayerFixture f;
	std::unique_ptr<cp::TestClient> client;

	gameserver::model::gameobjects::player::Player& player() const { return *f.player; }
	std::vector<std::vector<uint8_t>> sent() const { return (*client)->sentBytes(); }
	void clearSent() const { (*client)->clearSent(); }
	std::vector<uint8_t> serializedFor(network::aion::AionServerPacket&& packet) const { return cp::serialized(std::move(packet), client->con()); }
};

class QuestHandlerTest : public items::ItemPacketTest {
protected:
	void SetUp() override {
		// the npc templates name AIs this executable links no handler for: the warn mode puts AIEngine's substitute in place
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		// Player::postConstruct loads the toy pets from the database; the tests have none
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(
			[](gameserver::model::gameobjects::player::Player&) { return std::vector<Ref<gameserver::model::gameobjects::player::PetCommonData>>(); });
		ItemPacketTest::SetUp();
		// the rates of the quest rewards at Java's defaults (RatesConfig.java; the unit tests load no configuration, and an empty list logs
		// "Missing rates")
		savedQuestKinahRates = *configs::main::RatesConfig::QUEST_KINAH_RATES.get();
		savedXpQuestRates = *configs::main::RatesConfig::XP_QUEST_RATES.get();
		configs::main::RatesConfig::QUEST_KINAH_RATES.set({1.0f});
		configs::main::RatesConfig::XP_QUEST_RATES.set({1.0f});
		// QuestEngine::clear cancels the daily message in the cron service (QuestEngine.java:119; QuestEngineTest.cpp's fixture)
		services::cron::CronService::resetForTests();
		services::cron::CronService::initSingleton(std::make_unique<utils::cron::ThreadPoolManagerRunnableRunner>(), std::chrono::locate_zone("UTC"),
			services::cron::CronService::Driver::EXECUTOR);

		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(contexts.emplace_back(), HANDLER_QUESTS_XML));
		std::string itemsXml(items::ITEM_TEMPLATES_XML);
		itemsXml.erase(itemsXml.rfind("</item_templates>"));
		itemsXml += HANDLER_ITEMS_XML;
		itemsXml += "</item_templates>";
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(contexts.emplace_back(), itemsXml));
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), HANDLER_NPCS_XML));
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(contexts.emplace_back(), HANDLER_EXPERIENCE_TABLE_XML));

		me = makeQuester(810101, "Quester", gameserver::model::Race::ELYOS, 1);
		holdItem(*me, 820001, items::KINAH, 1000);
	}

	void TearDown() override {
		for (Quester* quester : questers) {
			quester->player().setTarget(nullptr);
			quester->player().setClientConnection(nullptr);
			world::World::getInstance().removeObject(quester->player());
			quester->client.reset();
			quester->f = {};
		}
		questers.clear();
		extraQuesters.clear();
		me = nullptr;
		heldItems.clear();
		npcs.clear();
		spawnGroups.clear();
		QuestEngine::getInstance().clear(); // the handlers themselves are Immortal (RT-11)
		calls.clear();
		services::cron::CronService::resetForTests();
		ItemPacketTest::TearDown();
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.resetForTests();
		configs::main::RatesConfig::QUEST_KINAH_RATES.set(savedQuestKinahRates);
		configs::main::RatesConfig::XP_QUEST_RATES.set(savedXpQuestRates);
	}

	/**
	 * QuestLifecycleTest's quester for any race and level: the level is set before the Player exists (no level change runs), with the daeva flag
	 * first above level 9. Online, in the World and standing spawned in the fixture's Poeta instance, with a client of its own.
	 */
	Quester* makeQuester(int32_t objectId, std::string_view name, gameserver::model::Race race, int32_t level) {
		auto quester = std::make_unique<Quester>();
		cp::PlayerFixture& pf = quester->f;
		pf.account = gameserver::model::account::Account::create(objectId + 10000);
		pf.commonData = gameserver::model::gameobjects::player::PlayerCommonData::create(objectId);
		pf.commonData->setName(name);
		pf.commonData->setRace(race);
		pf.commonData->setPlayerClass(gameserver::model::PlayerClass::WARRIOR);
		if (level > 9)
			pf.commonData->setDaeva(true);
		pf.commonData->setLevel(level);
		pf.appearance = gameserver::model::gameobjects::player::PlayerAppearance::create();
		pf.account->addPlayerAccountData(std::make_unique<gameserver::model::account::PlayerAccountData>(*pf.account, *pf.commonData, *pf.appearance));
		pf.account->setAccountWarehouse(std::make_unique<gameserver::model::items::storage::PlayerStorage>(*pf.account,
			gameserver::model::items::storage::StorageType::ACCOUNT_WAREHOUSE));
		pf.player = gameserver::model::gameobjects::VisibleObject::create<cp::TestPlayer>(*pf.account->getPlayerAccountData(objectId), *pf.account);
		pf.player->setKnownlist(std::make_unique<cp::TestKnownList>(*pf.player));
		pf.player->setFriendList(std::make_unique<gameserver::model::gameobjects::player::FriendList>(*pf.player,
			std::vector<Ptr<gameserver::model::gameobjects::player::Friend>>{}));
		pf.player->setBlockList(gameserver::model::gameobjects::player::BlockList::create());
		pf.player->setPlayerSettings(gameserver::model::gameobjects::player::PlayerSettings::create());
		pf.player->setAbyssRank(gameserver::model::gameobjects::player::AbyssRank::create(0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0));
		pf.player->setEffectController(std::make_unique<controllers::effect::PlayerEffectController>(*pf.player));
		pf.player->setFlyController(std::make_unique<controllers::FlyController>(*pf.player));
		pf.player->setEmotions(std::make_unique<gameserver::model::gameobjects::player::emotion::EmotionList>(*pf.player));
		pf.player->setPosition(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(100.0f, 100.0f, 50.0f)));
		pf.player->getPosition()->setIsSpawned(true);
		pf.player->setQuestStateList(gameserver::model::gameobjects::player::QuestStateList::create());
		pf.commonData->setOnline(true); // PlayerEnterWorldService
		world::World::getInstance().storeObject(*pf.player);
		quester->client = std::make_unique<cp::TestClient>();
		quester->client->enterWorld(pf);
		Quester* raw = quester.get();
		questers.push_back(raw);
		extraQuesters.push_back(std::move(quester));
		return raw;
	}

	gameserver::model::gameobjects::player::Player& player() { return me->player(); }

	/** An item row of the cube as the inventory DAO loads it (onLoadHandler: no packet) */
	gameserver::model::gameobjects::Item& holdItem(Quester& quester, int32_t objId, int32_t itemId, int64_t count) {
		Ref<gameserver::model::gameobjects::Item> item = items::loadedItem(objId, itemId, count, gameserver::model::items::storage::StorageType::CUBE);
		quester.player().getInventory().onLoadHandler(*item);
		heldItems.push_back(item);
		return *item;
	}

	int64_t held(Quester& quester, int32_t itemId) { return quester.player().getInventory().getItemCountByItemId(itemId); }

	/** A quest state as the DAO loads it (stored), in the player's list; a COMPLETE one was completed once */
	static Ref<QuestState> hold(Quester& quester, int32_t questId, QuestStatus status, int32_t vars = 0) {
		int32_t completeCount = status == QuestStatus::COMPLETE ? 1 : 0;
		Ref<QuestState> qs = QuestState::create(questId, status, vars, 0, completeCount, std::nullopt, std::nullopt, std::nullopt);
		qs->setPersistentState(gameserver::model::gameobjects::Persistable::PersistentState::UPDATED);
		quester.player().getQuestStateList()->addQuest(questId, *qs);
		return qs;
	}

	static Ref<QuestEnv> envOf(Quester& quester, int32_t questId, int32_t dialogActionId,
		Ptr<gameserver::model::gameobjects::VisibleObject> target = nullptr) {
		return QuestEnv::create(target, quester.player(), questId, dialogActionId);
	}

	/** An npc of the template standing in the fixture's map (not spawned into the World): the target a talk or a kill hands the engine */
	gameserver::model::gameobjects::Npc& npcOf(int32_t npcId) {
		const gameserver::model::templates::npc::NpcTemplate* template_ = dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId);
		Ref<gameserver::model::templates::spawns::SpawnGroup> group =
			gameserver::model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		gameserver::model::templates::spawns::SpawnTemplate& spawn =
			group->addSpawnTemplate(std::make_unique<QuestHandlerSpawnTemplate>(*group, 100.0f, 102.0f, 50.0f));
		Ref<gameserver::model::gameobjects::Npc> npc = gameserver::model::gameobjects::VisibleObject::create<gameserver::model::gameobjects::Npc>(
			std::make_unique<controllers::NpcController>(), spawn, template_);
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		return *npc;
	}

	/** Registers a ProbeHandler with the engine (addQuestHandler calls its register_), recording into `calls` */
	void probe(int32_t questId, std::vector<int32_t> startNpcs, std::vector<int32_t> talkNpcs, bool answer = true) {
		QuestEngine::getInstance().addQuestHandler(std::make_unique<ProbeHandler>(questId, std::move(startNpcs), std::move(talkNpcs), calls, answer));
	}

	std::deque<xml::LoadContext> contexts;
	/** What the ProbeHandlers received, in order */
	std::vector<DialogCall> calls;
	Quester* me = nullptr;
	std::vector<Quester*> questers;
	std::vector<std::unique_ptr<Quester>> extraQuesters;
	std::vector<Ref<gameserver::model::gameobjects::Item>> heldItems;
	std::vector<Ref<gameserver::model::gameobjects::Npc>> npcs;
	std::vector<Ref<gameserver::model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<float> savedQuestKinahRates;
	std::vector<float> savedXpQuestRates;
};

} // namespace aion::gameserver::questEngine::handlers::test
