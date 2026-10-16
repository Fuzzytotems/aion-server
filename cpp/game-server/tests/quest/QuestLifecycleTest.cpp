// P5-06a, M5d E-02, E-03, E-05 and E-06 (m5d-plan.md §7): QuestService's start, finish, reward, abandon, collect-item and timer bodies
// (QuestService.java:77-273, 400-470, 516-664, 811-885, 935-947) on an in-world player.
//
// The player is a level-9 WARRIOR in a Poeta map instance whose packets a TestClient records (tests/cm_ak/ItemPacketTestSupport.h, included by
// relative path as QuestDropTest.cpp includes InWorldPacketRunSupport.h). Level 9 with the start exp of level 9 (82,982): every exp reward
// below lands inside level 9 or at the non-daeva cap of 126,069 (PlayerCommonData.setExp), so no case runs the level-up path. The player is
// online (PlayerEnterWorldService sets it), as the cap needs.
// Rates are set to tell the rates apart: QUEST_KINAH 3.0, XP_QUEST 2.0, XP_SOLO 1.0 at membership 0 (the defaults are 1.0 each). The quest
// size limit and the permission that lifts it are Java's defaults, 40 and 10 (CustomConfig.java:95, MembershipConfig.java:43).
//
// Fixture rows are copied from the shipped data (file:line beside each row); quest 9619's row is whole, the npc 203057 "mires" without its
// <equipment> (npc_templates.xml:1963-1970: no case reads it). The rewards before rates are the data's, and quest 1101's are those
// `oracle.py m5d-quest --quest 1101` prints (120 kinah, 130 exp). Expected packets are Java's bytes where the packet's fields are its own
// (SM_QUEST_ACTION.java writeImpl, SM_NEARBY_QUESTS.java:22-30); a localized system message is compared against the server's serialization
// of the message Java constructs there.
//
// The cube arm of giveReward reaches CubeExpandService.questExpand, which M5c stage 1 ported (P-05), so it has a case here since the overlay's
// rebase onto a75d281ff. Not here: the AP and GP arms of giveReward and BonusService.getQuestBonus (E-09, the dialog-and-rewards lane: their
// bodies are still AION_UNPORTED, so no case reaches them), the warehouse arm (WarehouseService, group K) and ChallengeTaskService (P5-10).
// getEachDropMembersGroup/Alliance (E-04) has no case: GeneralTeam::getMembers and the PlayerGroup/PlayerAlliance constructors are AION_UNPORTED (M5g), so no group can be built. Two arms need the
// database, which this executable does not open: a title that is new to the player (TitleList.addTitle stores it through PlayerTitleListDAO
// before it sends STR_QUEST_GET_REWARD_TITLE, so only the unknown-title throw is a case here) and abandonQuest's work-order recipe
// (RecipeList.deleteRecipe asks PlayerRecipesDAO first; the WorkOrdersData of XML_QUESTS is P5-06c's). docs/deviations/P5-06a.md records
// both as W.

#include "../cm_ak/ItemPacketTestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/NpcFactionsData.bind.h"
#include "aion/gameserver/dataholders/NpcFactionsData.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/TitleData.bind.h"
#include "aion/gameserver/dataholders/TitleData.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/ENpcFactionQuestState.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFaction.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestItems.h"
#include "aion/gameserver/model/templates/quest/QuestRepeatCycleInfo.h"
#include "aion/gameserver/model/templates/rewards/BonusType.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATUPDATE_DP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/questEngine/handlers/HandlerResult.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/utils/cron/ThreadPoolManagerRunnableRunner.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::test {
namespace {

namespace cp = network::aion::clientpackets::testing;
namespace items = network::aion::clientpackets::testing::items;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using network::test::PacketWriter;
using questEngine::model::QuestEnv;
using questEngine::model::QuestState;
using questEngine::model::QuestStatus;
using runtime::Ptr;
using runtime::Ref;

constexpr const char* QUESTS_XML =
	R"(<quests>)"
	// quest_data.xml:6-8
	R"(<quest id="1000" name="Prologue" nameId="1102000" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1")"
	R"( cannot_share="true" cannot_giveup="true" race_permitted="ELYOS" category="QUEST"><rewards exp="1"/></quest>)"
	// quest_data.xml:9-19
	R"(<quest id="1001" name="The Kerubim Threat" nameId="1102001" quest_zone="Poeta" minlevel_permitted="2" max_repeat_count="1")"
	R"( cannot_share="true" cannot_giveup="true" race_permitted="ELYOS" category="MISSION"><collect_items><collect_item)"
	R"( item_id="182200001" count="3"/></collect_items><rewards exp="2100"><selectable_reward_item item_id="114100806" count="1"/>)"
	R"(<selectable_reward_item item_id="114300816" count="1"/><selectable_reward_item item_id="114500778" count="1"/></rewards>)"
	R"(<quest_drop npc_id="210671" item_id="182200001" drop_each_member="1" collecting_step="7"/></quest>)"
	// quest_data.xml:68-124
	R"(<quest id="1007" name="A Ceremony in Sanctum" nameId="1102007" quest_zone="Ascension Quests" minlevel_permitted="9")"
	R"( max_repeat_count="1" cannot_share="true" cannot_giveup="true" use_class_reward="1" race_permitted="ELYOS" category="MISSION">)"
	R"(<rewards exp="13125"><!-- Custom --><reward_item item_id="162001057" count="5"/><reward_item item_id="182400001" count="250000"/>)"
	R"(</rewards><rewards exp="13125"><!-- Custom --><reward_item item_id="162001057" count="5"/><reward_item item_id="182400001")"
	R"( count="250000"/></rewards><rewards exp="13125"><!-- Custom --><reward_item item_id="162001057" count="5"/><reward_item)"
	R"( item_id="182400001" count="250000"/></rewards><rewards exp="13125"><!-- Custom --><reward_item item_id="162001057" count="5"/>)"
	R"(<reward_item item_id="182400001" count="250000"/></rewards><rewards exp="13125"><!-- Custom --><reward_item item_id="162001057")"
	R"( count="5"/><reward_item item_id="182400001" count="250000"/></rewards><rewards exp="13125"><!-- Custom --><reward_item)"
	R"( item_id="162001057" count="5"/><reward_item item_id="182400001" count="250000"/></rewards><start_conditions><finished)"
	R"( quest_id="1006"/></start_conditions><fighter_selectable_reward item_id="100000652" count="1"/><fighter_selectable_reward)"
	R"( item_id="100900494" count="1"/><fighter_selectable_reward item_id="101300485" count="1"/><knight_selectable_reward)"
	R"( item_id="100000652" count="1"/><knight_selectable_reward item_id="100900494" count="1"/><ranger_selectable_reward)"
	R"( item_id="100200614" count="1"/><ranger_selectable_reward item_id="100000652" count="1"/><ranger_selectable_reward)"
	R"( item_id="101700525" count="1"/><assassin_selectable_reward item_id="100200614" count="1"/><assassin_selectable_reward)"
	R"( item_id="100000652" count="1"/><assassin_selectable_reward item_id="101700525" count="1"/><wizard_selectable_reward)"
	R"( item_id="100600545" count="1"/><wizard_selectable_reward item_id="100500508" count="1"/><elementalist_selectable_reward)"
	R"( item_id="100600545" count="1"/><elementalist_selectable_reward item_id="100500508" count="1"/><priest_selectable_reward)"
	R"( item_id="100100506" count="1"/><priest_selectable_reward item_id="101500506" count="1"/><chanter_selectable_reward)"
	R"( item_id="100100506" count="1"/><chanter_selectable_reward item_id="101500506" count="1"/><bard_selectable_reward)"
	R"( item_id="102000536" count="1"/><gunner_selectable_reward item_id="101800515" count="1"/><rider_selectable_reward)"
	R"( item_id="102100496" count="1"/></quest>)"
	// quest_data.xml:895-897
	R"(<quest id="1101" name="Sleeping on the Job" nameId="1102201" quest_zone="Poeta" minlevel_permitted="1" max_repeat_count="1")"
	R"( can_report="true" race_permitted="ELYOS" category="IMPORTANT"><rewards gold="120" exp="130"/></quest>)"
	// quest_data.xml:964-972
	R"(<quest id="1111" name="Insomnia Medicine" nameId="1102211" quest_zone="Poeta" minlevel_permitted="3" max_repeat_count="1")"
	R"( can_report="true" race_permitted="ELYOS" category="QUEST"><collect_items><collect_item item_id="182200223" count="3"/>)"
	R"(</collect_items><rewards gold="960" exp="1595"/><rewards gold="450" exp="1150" ccheck="-1"/><quest_drop npc_id="210261")"
	R"( item_id="182200223"/><quest_drop npc_id="210674" item_id="182200223"/></quest>)"
	// quest_data.xml:980-988
	R"(<quest id="1113" name="Mushroom Thieves" nameId="1102213" quest_zone="Poeta" minlevel_permitted="4" max_repeat_count="1")"
	R"( can_report="true" race_permitted="ELYOS" category="QUEST"><rewards exp="1738"><selectable_reward_item item_id="162000052")"
	R"( count="3"/><selectable_reward_item item_id="162000057" count="3"/><selectable_reward_item item_id="169000003" count="150"/>)"
	R"(<reward_item item_id="160003001" count="3"/></rewards><quest_kill step="0" var="0" count="8" npc_ids="210262 210675")"
	R"( seq="0"/></quest>)"
	// quest_data.xml:989-995
	R"(<quest id="1114" name="The Nymph's Gown" nameId="1102214" quest_zone="Poeta" minlevel_permitted="4" max_repeat_count="1")"
	R"( can_report="true" cannot_share="true" race_permitted="ELYOS" category="QUEST"><rewards gold="1920" exp="4367"/><rewards)"
	R"( gold="960" exp="3120"/><quest_work_items><quest_work_item item_id="182200217"/></quest_work_items></quest>)"
	// quest_data.xml:1075-1087
	R"(<quest id="1124" name="Avenging Tutty" nameId="1102224" quest_zone="Poeta" minlevel_permitted="7" max_repeat_count="1")"
	R"( can_report="true" cannot_share="true" race_permitted="ELYOS" category="QUEST"><collect_items><collect_item)"
	R"( item_id="182200210" count="3"/></collect_items><rewards gold="2250" exp="6655" title="7"><reward_item item_id="162000012")"
	R"( count="4"/></rewards><quest_drop npc_id="210092" item_id="182200210" chance="75"/><quest_drop npc_id="210096")"
	R"( item_id="182200210" chance="75"/><start_conditions><finished quest_id="1123"/></start_conditions></quest>)"
	// quest_data.xml:1785-1793
	R"xml(<quest id="1209" name="[Spend Coin] Iron (Warrior and Scout)" nameId="1102379" quest_zone="Verteron" minlevel_permitted="99")xml"
	R"( max_repeat_count="255" cannot_share="true" race_permitted="ELYOS" extra_category="COIN_QUEST" category="QUEST")"
	R"( restricted="true"><inventory_items><inventory_item item_id="186000001" count="2"/><inventory_item item_id="186000001")"
	R"( count="4"/></inventory_items><rewards icheck="0"/><rewards icheck="1"/><class_permitted>)"
	R"(WARRIOR SCOUT GLADIATOR TEMPLAR ASSASSIN RANGER</class_permitted></quest>)"
	// quest_data.xml:1960-1970
	R"(<quest id="1305" name="More Manduri Frillnecks" nameId="1102405" quest_zone="Eltnen" minlevel_permitted="20")"
	R"( max_repeat_count="10" reward_repeat_count="10" can_report="true" cannot_share="true" race_permitted="ELYOS")"
	R"( category="QUEST"><rewards gold="2800" exp="66900"/><bonus level="20" type="FOOD"/><extended_rewards gold="5000"><reward_item)"
	R"( item_id="188050588" count="1"/></extended_rewards><quest_kill step="0" var="0" count="10" npc_ids="211687 211688" seq="0"/>)"
	R"(<start_conditions><finished quest_id="1421"/></start_conditions></quest>)"
	// quest_data.xml:6160-6171
	R"(<quest id="1692" name="A Day Older and Deeper in Debt" nameId="1102883" quest_zone="Heiron" minlevel_permitted="38")"
	R"( max_repeat_count="1" cannot_share="true" race_permitted="ELYOS" category="QUEST" restricted="true"><collect_items>)"
	R"(<collect_item item_id="152000104" count="10"/><collect_item item_id="182400001" count="3146"/></collect_items><rewards)"
	R"( gold="94840" exp="2108696"><reward_item item_id="186000004" count="3"/></rewards><start_conditions><finished)"
	R"( quest_id="1691"/></start_conditions></quest>)"
	// quest_data.xml:10047-10049
	R"(<quest id="2101" name="On Your Feet!" nameId="1103201" quest_zone="Ishalgen" minlevel_permitted="1" max_repeat_count="1")"
	R"( race_permitted="ASMODIANS" category="IMPORTANT"><rewards gold="80" exp="130"/></quest>)"
	// quest_data.xml:32693-32698
	R"(<quest id="9554" name="[Event] Lato's Blessing" nameId="1112025" quest_zone="Verteron" minlevel_permitted="10")"
	R"( max_repeat_count="255" cannot_share="true" race_permitted="ELYOS" category="EVENT"><rewards gold="10" exp="100"/><rewards)"
	R"( gold="100" exp="100"/><rewards gold="1000" exp="100"/><gender_permitted>MALE</gender_permitted></quest>)"
	// quest_data.xml:32816-32818
	R"(<quest id="9600" name="[Test] Limited Quest Daily" nameId="1100511" quest_zone="Test zone" minlevel_permitted="10")"
	R"( max_repeat_count="255" race_permitted="ELYOS" category="QUEST" repeat_cycle="ALL"><rewards gold="9600" exp="9600"/></quest>)"
	// quest_data.xml:32819-32821
	R"(<quest id="9601" name="[Test] Limited Quest Mondays Only" nameId="1100512" quest_zone="Test zone" minlevel_permitted="10")"
	R"( max_repeat_count="255" race_permitted="ELYOS" category="QUEST" repeat_cycle="MON WED FRI"><rewards gold="9601" exp="9601"/>)"
	R"(</quest>)"
	// quest_data.xml:32831-32833
	R"(<quest id="9605" name="[Test] Limited Quest Fridays Only" nameId="1100516" quest_zone="Test zone" minlevel_permitted="10")"
	R"( max_repeat_count="255" race_permitted="ELYOS" category="QUEST" repeat_cycle="FRI"><rewards gold="9605" exp="9605"/></quest>)"
	// quest_data.xml:32958-32972
	R"(<quest id="9618" name="[Test] Expand the Range of Repeated Additional Rewards" nameId="1100539" quest_zone="Test zone")"
	R"( minlevel_permitted="1" max_repeat_count="5" reward_repeat_count="5" race_permitted="PC_ALL" category="QUEST"><rewards)"
	R"( gold="9618" exp="9618"/><extended_rewards><selectable_reward_item item_id="186000122" count="1"/><selectable_reward_item)"
	R"( item_id="186000122" count="2"/><selectable_reward_item item_id="186000122" count="3"/><selectable_reward_item)"
	R"( item_id="186000122" count="4"/><selectable_reward_item item_id="186000122" count="5"/><selectable_reward_item)"
	R"( item_id="186000122" count="6"/><selectable_reward_item item_id="186000122" count="7"/><selectable_reward_item)"
	R"( item_id="186000122" count="8"/><selectable_reward_item item_id="186000122" count="9"/><selectable_reward_item)"
	R"( item_id="186000122" count="10"/></extended_rewards></quest>)"
	// quest_data.xml:32973-33039
	R"(<quest id="9619" name="[Test] Add Repeated Additional Rewards to Each Class" nameId="1100540" quest_zone="Test zone")"
	R"( minlevel_permitted="1" max_repeat_count="5" reward_repeat_count="5" use_class_reward="2" race_permitted="PC_ALL")"
	R"( category="QUEST"><rewards gold="9619" exp="9619"/><fighter_selectable_reward item_id="100001082" count="1"/>)"
	R"(<fighter_selectable_reward item_id="100100820" count="1"/><fighter_selectable_reward item_id="100900822" count="1"/>)"
	R"(<fighter_selectable_reward item_id="101300791" count="1"/><fighter_selectable_reward item_id="101700865" count="1"/>)"
	R"(<fighter_selectable_reward item_id="186000122" count="1"/><fighter_selectable_reward item_id="186000122" count="2"/>)"
	R"(<fighter_selectable_reward item_id="186000122" count="3"/><knight_selectable_reward item_id="100001082" count="1"/>)"
	R"(<knight_selectable_reward item_id="100100820" count="1"/><knight_selectable_reward item_id="100900822" count="1"/>)"
	R"(<knight_selectable_reward item_id="186000122" count="1"/><knight_selectable_reward item_id="186000122" count="2"/>)"
	R"(<knight_selectable_reward item_id="186000122" count="3"/><knight_selectable_reward item_id="186000122" count="4"/>)"
	R"(<knight_selectable_reward item_id="186000122" count="5"/><ranger_selectable_reward item_id="100200955" count="1"/>)"
	R"(<ranger_selectable_reward item_id="100001082" count="1"/><ranger_selectable_reward item_id="101700865" count="1"/>)"
	R"(<ranger_selectable_reward item_id="186000122" count="1"/><ranger_selectable_reward item_id="186000122" count="2"/>)"
	R"(<ranger_selectable_reward item_id="186000122" count="3"/><ranger_selectable_reward item_id="186000122" count="4"/>)"
	R"(<ranger_selectable_reward item_id="186000122" count="5"/><assassin_selectable_reward item_id="100200955" count="1"/>)"
	R"(<assassin_selectable_reward item_id="100001082" count="1"/><assassin_selectable_reward item_id="101700865" count="1"/>)"
	R"(<assassin_selectable_reward item_id="186000122" count="1"/><assassin_selectable_reward item_id="186000122" count="2"/>)"
	R"(<assassin_selectable_reward item_id="186000122" count="3"/><assassin_selectable_reward item_id="186000122" count="4"/>)"
	R"(<assassin_selectable_reward item_id="186000122" count="5"/><wizard_selectable_reward item_id="100600898" count="1"/>)"
	R"(<wizard_selectable_reward item_id="100500842" count="1"/><wizard_selectable_reward item_id="186000122" count="1"/>)"
	R"(<wizard_selectable_reward item_id="186000122" count="2"/><wizard_selectable_reward item_id="186000122" count="3"/>)"
	R"(<wizard_selectable_reward item_id="186000122" count="4"/><wizard_selectable_reward item_id="186000122" count="5"/>)"
	R"(<wizard_selectable_reward item_id="186000122" count="6"/><elementalist_selectable_reward item_id="100600898" count="1"/>)"
	R"(<elementalist_selectable_reward item_id="100500842" count="1"/><elementalist_selectable_reward item_id="186000122")"
	R"( count="1"/><elementalist_selectable_reward item_id="186000122" count="2"/><elementalist_selectable_reward)"
	R"( item_id="186000122" count="3"/><elementalist_selectable_reward item_id="186000122" count="4"/>)"
	R"(<elementalist_selectable_reward item_id="186000122" count="5"/><elementalist_selectable_reward item_id="186000122")"
	R"( count="6"/><priest_selectable_reward item_id="100100820" count="1"/><priest_selectable_reward item_id="101500848")"
	R"( count="1"/><priest_selectable_reward item_id="186000122" count="1"/><priest_selectable_reward item_id="186000122")"
	R"( count="2"/><priest_selectable_reward item_id="186000122" count="3"/><priest_selectable_reward item_id="186000122")"
	R"( count="4"/><priest_selectable_reward item_id="186000122" count="5"/><priest_selectable_reward item_id="186000122")"
	R"( count="6"/><chanter_selectable_reward item_id="100100820" count="1"/><chanter_selectable_reward item_id="101500848")"
	R"( count="1"/><chanter_selectable_reward item_id="186000122" count="1"/><chanter_selectable_reward item_id="186000122")"
	R"( count="2"/><chanter_selectable_reward item_id="186000122" count="3"/><chanter_selectable_reward item_id="186000122")"
	R"( count="4"/><chanter_selectable_reward item_id="186000122" count="5"/><chanter_selectable_reward item_id="186000122")"
	R"( count="6"/></quest>)"
	// quest_data.xml:67769-67778
	R"(<quest id="35007" name="[Daily] Chasing a Memory" nameId="1126007" quest_zone="Alabaster Order" minlevel_permitted="30")"
	R"( maxlevel_permitted="39" max_repeat_count="255" cannot_share="true" race_permitted="ELYOS" category="FACTION" npcfaction_id="2")"
	R"( restricted="true"><collect_items><collect_item item_id="182210002" count="1"/></collect_items><rewards exp="203989"><reward_item)"
	R"( item_id="186000100" count="1"/><reward_item item_id="188050556" count="1"/></rewards><quest_drop npc_id="700788")"
	R"( item_id="182210002" drop_each_member="1"/></quest>)"
	// quest_data.xml:67779-67788
	R"(<quest id="35008" name="[Daily] Documenting the Scraps" nameId="1126008" quest_zone="Alabaster Order" minlevel_permitted="30")"
	R"( maxlevel_permitted="39" max_repeat_count="255" cannot_share="true" race_permitted="ELYOS" category="FACTION" npcfaction_id="2")"
	R"( restricted="true"><collect_items><collect_item item_id="182210003" count="3"/></collect_items><rewards exp="203989"><reward_item)"
	R"( item_id="186000100" count="1"/><reward_item item_id="188050556" count="1"/></rewards><quest_drop npc_id="700789")"
	R"( item_id="182210003" drop_each_member="1"/></quest>)"
	// quest_data.xml:74083-74088
	R"(<quest id="50008" name="[Event/Daily] Solorius Shugo Slackers!" nameId="1150009" quest_zone="Oriel" minlevel_permitted="9")"
	R"( max_repeat_count="255" cannot_share="true" race_permitted="ELYOS" category="EVENT" repeat_cycle="ALL"><rewards><reward_item)"
	R"( item_id="160010203" count="1"/><reward_item item_id="186000177" count="4"/></rewards></quest>)"
	// quest_data.xml:74224-74235
	R"(<quest id="50023" name="[Event] Elyos Perseverance" nameId="1150060" quest_zone="Oriel" minlevel_permitted="9")"
	R"( max_repeat_count="255" cannot_share="true" race_permitted="ELYOS" category="EVENT"><collect_items start_check="true">)"
	R"(<collect_item item_id="186000179" count="1"/><collect_item item_id="186000179" count="3"/></collect_items><rewards)"
	R"( ccheck="0"><reward_item item_id="188051780" count="1"/></rewards><rewards ccheck="1"><reward_item item_id="188051782")"
	R"( count="1"/></rewards></quest>)"
	// quest_data.xml:74305-74307
	R"(<quest id="50033" name="[Event] The Lustrous Cube" nameId="1150202" quest_zone="Event" minlevel_permitted="21" max_repeat_count="1")"
	R"( cannot_share="true" race_permitted="PC_ALL" category="EVENT"><rewards extend_inventory="1"/></quest>)"
	// quest_data.xml:74480-74485
	R"(<quest id="51008" name="[Event/Daily] Non-Helping Hands" nameId="1150012" quest_zone="Pernon" minlevel_permitted="9")"
	R"( max_repeat_count="255" cannot_share="true" race_permitted="ASMODIANS" category="EVENT" repeat_cycle="ALL"><rewards>)"
	R"(<reward_item item_id="160010203" count="1"/><reward_item item_id="186000177" count="4"/></rewards></quest>)"
	// quest_data.xml:78794-78803
	R"xml(<quest id="80316" name="Befitting the Hero of the Guardians (for damage dealers)" nameId="1199915" quest_zone="Reshanta")xml"
	R"( minlevel_permitted="25" maxlevel_permitted="35" max_repeat_count="1" cannot_share="true" race_permitted="ELYOS" category="EVENT">)"
	R"(<collect_items><collect_item item_id="122000906" count="1"/></collect_items><rewards exp="1000"><reward_item item_id="122000906")"
	R"( count="1"/><reward_item item_id="122000906" count="1"/></rewards><class_permitted>WARRIOR SCOUT MAGE PRIEST ENGINEER ARTIST)"
	R"( GLADIATOR TEMPLAR ASSASSIN RANGER CHANTER</class_permitted></quest>)"
	// quest_data.xml:81264-81274
	R"(<quest id="80588" name="Enter with Luck" nameId="1185791" quest_zone="Lucky Danuar Reliquary" minlevel_permitted="65")"
	R"( max_repeat_count="1" race_permitted="ELYOS" category="EVENT"><rewards dp="2000"><selectable_reward_item item_id="160001386")"
	R"( count="20"/><selectable_reward_item item_id="160001388" count="20"/><selectable_reward_item item_id="160001391" count="20"/>)"
	R"(<reward_item item_id="164000259" count="30"/><reward_item item_id="164000114" count="20"/><reward_item item_id="164000116")"
	R"( count="20"/><reward_item item_id="164002228" count="3"/></rewards></quest>)"
	R"(</quests>)";

/** The item fixture's rows (ItemPacketTestSupport.h: kinah among them) are kept; these are added */
constexpr const char* ITEMS_XML =
	// item_templates.xml:3966-3972
	R"(<item_template id="100000652" name="Prophecy Sword" level="10" cName="sword_n_r1_q_10a" mask="138316" item_group="SWORD" quality="RARE")"
	R"( price="2200" desc="740963" attack_type="PHYSICAL" max_enchant="10" m_slots="2"><modifiers><add name="PHYSICAL_CRITICAL" value="13")"
	R"( bonus="true"/></modifiers><weapon_stats hit_count="2" attack_range="1500" magical_accuracy="27" parry="296" physical_accuracy="208")"
	R"( critical="50" attack_speed="1400" max_damage="57" min_damage="45"/><idian burn_attack="29" burn_defend="12"/></item_template>)"
	// item_templates.xml:81493-81499
	R"(<item_template id="100900494" name="Oracle Greatsword" level="10" cName="2hsword_n_r1_q_10a" mask="138316" item_group="GREATSWORD")"
	R"( quality="RARE" price="2200" desc="741018" attack_type="PHYSICAL" max_enchant="10" m_slots="2"><modifiers><add name="PHYSICAL_CRITICAL")"
	R"( value="13" bonus="true"/></modifiers><weapon_stats hit_count="3" attack_range="2000" magical_accuracy="27" parry="376")"
	R"( physical_accuracy="258" critical="10" attack_speed="2400" max_damage="99" min_damage="93"/><idian burn_attack="50" burn_defend="12"/>)"
	R"(</item_template>)"
	// item_templates.xml:96207-96213
	R"(<item_template id="101300485" name="Prophecy Spear" level="10" cName="polearm_n_r1_q_10a" mask="138316" item_group="POLEARM")"
	R"( quality="RARE" price="2200" desc="741023" attack_type="PHYSICAL" max_enchant="10" m_slots="2"><modifiers><add name="PHYSICAL_ATTACK")"
	R"( value="7" bonus="true"/></modifiers><weapon_stats hit_count="4" attack_range="2500" magical_accuracy="27" parry="336")"
	R"( physical_accuracy="158" critical="50" attack_speed="2800" max_damage="133" min_damage="71"/><idian burn_attack="58" burn_defend="12"/>)"
	R"(</item_template>)"
	// item_templates.xml:743617
	R"(<item_template id="152000104" name="Platinum Ore" level="30" cName="noblemetal_n_c_30a" mask="4222" max_stack_count="10000")"
	R"( item_group="GATHERABLE" quality="COMMON" price="35" desc="702035"/>)"
	// item_templates.xml:827151-827156
	R"(<item_template id="160003001" name="Roast Porgus" level="10" cName="shop_food_phyattack_10a" mask="12414")"
	R"( max_stack_count="1000" quality="COMMON" price="200" desc="730178" activate_target="STANDALONE" activate_count="1"><actions>)"
	R"(<skilluse level="1" skillid="10036"/></actions><uselimits usedelay="5000" usedelayid="22"/></item_template>)"
	// item_templates.xml:830784-830789
	R"(<item_template id="162000012" name="Minor Life Serum" level="10" cName="potion_hp_10a" mask="12414" max_stack_count="1000")"
	R"( quality="RARE" price="350" desc="702602" activate_target="STANDALONE" activate_count="1"><actions><skilluse level="1")"
	R"( skillid="9899"/></actions><uselimits usedelay="30000" usedelayid="11"/></item_template>)"
	// item_templates.xml:831033-831038
	R"(<item_template id="162000052" name="Minor Life Elixir" level="10" cName="shop_remedy_hp_10a" mask="12414")"
	R"( max_stack_count="1000" quality="COMMON" price="250" desc="741694" activate_target="STANDALONE" activate_count="1"><actions>)"
	R"(<skilluse level="1" skillid="10202"/></actions><uselimits usedelay="60000" usedelayid="11"/></item_template>)"
	// item_templates.xml:831063-831068
	R"(<item_template id="162000057" name="Minor Mana Elixir" level="10" cName="shop_remedy_mp_10a" mask="12414")"
	R"( max_stack_count="1000" quality="COMMON" price="250" desc="741699" activate_target="STANDALONE" activate_count="1"><actions>)"
	R"(<skilluse level="1" skillid="10207"/></actions><uselimits usedelay="60000" usedelayid="11"/></item_template>)"
	// item_templates.xml:832137-832143
	R"(<item_template id="162001057" name="Tea of Repose - 100% Recovery" level="10" cName="world_fellow_cash_item_vp_recovery_100")"
	R"( mask="12360" max_stack_count="1000" quality="LEGEND" price="5" restrict="10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10")"
	R"( desc="799586" activate_target="STANDALONE" activate_count="1"><actions><skilluse level="1" skillid="10491"/></actions><acquisition)"
	R"( type="REWARD" item="186000409" count="4"/><uselimits usedelay="2000" usedelayid="140"/></item_template>)"
	// item_templates.xml:833003-833008
	R"(<item_template id="164000114" name="Fine Fireproof Scroll" level="50" cName="scroll_regist_fire_60a" mask="12414" max_stack_count="1000")"
	R"( quality="COMMON" price="4200" restrict="50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50" desc="759933" activate_target="STANDALONE")"
	R"( activate_count="1"><actions><skilluse level="5" skillid="9918"/></actions><uselimits usedelay="15000" usedelayid="31"/></item_template>)"
	// item_templates.xml:833015-833020
	R"(<item_template id="164000116" name="Fine Waterproof Scroll" level="50" cName="scroll_regist_water_60a" mask="12414" max_stack_count="1000")"
	R"( quality="COMMON" price="4200" restrict="50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50" desc="759935" activate_target="STANDALONE")"
	R"( activate_count="1"><actions><skilluse level="5" skillid="9919"/></actions><uselimits usedelay="15000" usedelayid="31"/></item_template>)"
	// item_templates.xml:833884-833889
	R"(<item_template id="164000259" name="Premium Anti-Shock Scroll" level="70" cName="scroll_shield_all_70a" mask="12414" max_stack_count="1000")"
	R"( quality="COMMON" price="7331" restrict="60 60 60 60 60 60 60 60 60 60 60 60 60 60 60 60 60" desc="812371" activate_target="STANDALONE")"
	R"( activate_count="1"><actions><skilluse level="7" skillid="10770"/></actions><uselimits usedelay="60000" usedelayid="32"/></item_template>)"
	// item_templates.xml:835765-835770 (the name's dash is U+2013, spelled as its UTF-8 bytes)
	R"(<item_template id="164002228" name="Administrator's Boon )"
	"\xE2\x80\x93"
	R"( 1-Time Pass" level="1" cName="world_cash_scroll_start_kit_01c_1d" mask="12410" max_stack_count="100" quality="LEGEND" price="5")"
	R"( desc="825732" activate_target="STANDALONE" activate_count="1"><actions><skilluse level="1" skillid="10350"/></actions><uselimits)"
	R"( usedelay="15000" usedelayid="31"/></item_template>)"
	// item_templates.xml:848722
	R"(<item_template id="169000003" name="Minor Power Shard" level="1" cName="battery_01" mask="12414" max_stack_count="10000")"
	R"( item_group="POWER_SHARDS" quality="COMMON" price="5" desc="701811" weapon_boost="10"/>)"
	// item_templates.xml:876987-876989
	R"(<item_template id="182200217" name="Nymph's Dress" level="1" cName="quest_1114b" mask="20545" item_group="QUEST")"
	R"( quality="COMMON" price="1" desc="1106421"><inventory id="2"/></item_template>)"
	// item_templates.xml:877005-877007
	R"(<item_template id="182200223" name="Sylphen Wings" level="1" cName="quest_1111a" mask="28736" max_stack_count="20")"
	R"( item_group="QUEST" quality="COMMON" price="1" desc="1106413"><inventory id="2"/></item_template>)"
	// item_templates.xml:896233-896235
	R"(<item_template id="186000001" name="Iron Coin" level="10" cName="coin_01" mask="12410" max_stack_count="10000")"
	R"( item_group="COINS" quality="COMMON" price="5328" desc="703678"><inventory id="1"/></item_template>)"
	// item_templates.xml:896436-896438
	R"(<item_template id="186000100" name="Progress Token" level="30" cName="coin_protection_01" mask="12360" max_stack_count="10000")"
	R"( item_group="COINS" quality="COMMON" price="5" desc="762226"><inventory id="1"/></item_template>)"
	// item_templates.xml:896482-896484
	R"(<item_template id="186000122" name="Groggie" level="50" cName="coin_shulack_01" mask="12364" max_stack_count="1000")"
	R"( item_group="COINS" quality="RARE" price="5" desc="770143"><inventory id="1"/></item_template>)"
	// item_templates.xml:896638-896640
	R"(<item_template id="186000177" name="[Event] Solorius Coin" level="1" cName="event_christmas_coin_01" mask="12360")"
	R"( max_stack_count="10000" quality="RARE" price="5" desc="797464"><inventory id="1"/></item_template>)"
	// item_templates.xml:896644
	R"(<item_template id="186000179" name="[Event] Perseverance Scroll" level="1" cName="event_chaos_wingwar_coin_01" mask="12360")"
	R"( max_stack_count="10000" quality="LEGEND" price="5" desc="798191"/>)"
	// item_templates.xml:904724-904729
	R"(<item_template id="188050556" name="Lesser Alabaster Order Supplies" level="30" cName="wrap_basic_reward_30a" casting_delay="1500")"
	R"( mask="12364" max_stack_count="100" quality="COMMON" price="5" restrict="30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30 30")"
	R"( desc="764268" activate_target="STANDALONE" activate_count="1"><actions><decompose/></actions><uselimits usedelay="5000")"
	R"( usedelayid="85"/></item_template>)"
	// item_templates.xml:904934-904939
	R"(<item_template id="188050588" name="Old Green Sack" level="30" cName="wrap_q_matter_option_30a" casting_delay="1500" mask="12364")"
	R"( max_stack_count="1000" quality="COMMON" price="100" desc="764357" activate_target="STANDALONE" activate_count="1"><actions>)"
	R"(<decompose/></actions><uselimits usedelay="5000" usedelayid="85"/></item_template>)";

/** npc_templates.xml:1959-1973, without its <equipment> (:1963-1970) */
constexpr const char* NPCS_XML =
	R"(<npc_templates>)"
	R"(<npc_template npc_id="203057" level="20" name="mires" name_id="351103" height="2" title_id="350496" group_drop="LIGHT")"
	R"( rank="DISCIPLINED" rating="NORMAL" race="ELYOS" tribe="GENERAL" type="GENERAL" ai="general" srange="20" sangle="240")"
	R"( arange="2" attack_speed="2000" hpgauge="3"><stats maxHp="2961"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2")"
	R"( group_run_fight="4.2" /></stats><bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="5" is_dialog="true")"
	R"( can_talk_invisible="false" /></npc_template>)"
	R"(</npc_templates>)";

/** npc_factions.xml:4, the faction of quests 35007 and 35008 */
constexpr const char* NPC_FACTIONS_XML =
	R"(<npc_factions><npc_faction id="2" name="Alabaster Order" npc_ids="799803 805145" name_id="1129000" category="DAILY" min_level="30")"
	R"( race="ELYOS"/></npc_factions>)";

/** player_experience_table.xml:3-28, the first 26 levels: a level-25 player (getLevelForExp answers at most the length - 1) */
constexpr const char* EXPERIENCE_TABLE_26_LEVELS_XML =
	"<player_experience_table><exp>0</exp><exp>400</exp><exp>1433</exp><exp>3820</exp><exp>9054</exp><exp>17655</exp><exp>30978</exp>"
	"<exp>52010</exp><exp>82982</exp><exp>126069</exp><exp>182252</exp><exp>260622</exp><exp>360825</exp><exp>490331</exp><exp>649169</exp>"
	"<exp>844378</exp><exp>1083018</exp><exp>1401356</exp><exp>1808613</exp><exp>2314771</exp><exp>2941893</exp><exp>3769257</exp>"
	"<exp>4811154</exp><exp>6110198</exp><exp>7632340</exp><exp>9377726</exp></player_experience_table>";

constexpr int32_t MIRES = 203057;
constexpr int64_t START_EXP_LEVEL_9 = 82982;    // player_experience_table.xml, level 9 (InWorldPacketRunSupport.h's first 16 levels)
constexpr int64_t START_EXP_LEVEL_10 = 126069;  // the exp a non-daeva keeps at most (PlayerCommonData.setExp)
constexpr int32_t GROGGIE = 186000122;
constexpr int32_t NYMPHS_DRESS = 182200217;
constexpr int32_t SYLPHEN_WINGS = 182200223;
constexpr int32_t PERSEVERANCE_SCROLL = 186000179;
constexpr int32_t OLD_GREEN_SACK = 188050588;
constexpr int32_t IRON_COIN = 186000001;
constexpr int32_t PROPHECY_SWORD = 100000652;
constexpr int32_t ORACLE_GREATSWORD = 100900494;
constexpr int32_t PROPHECY_SPEAR = 101300485;
constexpr int32_t TEA_OF_REPOSE = 162001057;
constexpr int32_t SM_DP_INFO_OPCODE = 7;
constexpr int32_t SM_QUEST_ACTION_OPCODE = 124;
constexpr int32_t SM_NEARBY_QUESTS_OPCODE = 127;
constexpr int32_t SM_STATUPDATE_EXP_OPCODE = 8;

/** SM_QUEST_ACTION(ADD or UPDATE, qs) (SM_QUEST_ACTION.java:67-81): C type, D quest, C status value, C 0, D vars | flags << 24, H 0 [, C 0] */
std::vector<uint8_t> questAction(int32_t type, int32_t questId, int32_t statusValue, int32_t vars = 0) {
	PacketWriter body;
	body.C(type).D(questId).C(statusValue).C(0).D(vars).H(0);
	if (type == 1) // ADD
		body.C(0);
	return items::javaPacket(SM_QUEST_ACTION_OPCODE, body);
}

/** SM_QUEST_ACTION(ABANDON, qs) (:82-84) */
std::vector<uint8_t> questAbandoned(int32_t questId) {
	return items::javaPacket(SM_QUEST_ACTION_OPCODE, PacketWriter().C(3).D(questId).D(0));
}

/** SM_QUEST_ACTION(questId), NpcFactions.sendDailyQuest's (SM_QUEST_ACTION.java:40-43, the UNK arm :99-102): C 6, D quest, H 1, H 0 */
std::vector<uint8_t> dailyQuest(int32_t questId) {
	return items::javaPacket(SM_QUEST_ACTION_OPCODE, PacketWriter().C(6).D(questId).H(1).H(0));
}

/** SM_QUEST_ACTION(questId, timer) (:85-88) */
std::vector<uint8_t> questTimer(int32_t questId, int32_t timer) {
	return items::javaPacket(SM_QUEST_ACTION_OPCODE, PacketWriter().C(4).D(questId).D(timer).C(timer > 0 ? 1 : 0));
}

/** SM_NEARBY_QUESTS with no quest (SM_NEARBY_QUESTS.java:22-30): the map instance has no start npc spawned */
std::vector<uint8_t> noNearbyQuests() {
	return items::javaPacket(SM_NEARBY_QUESTS_OPCODE, PacketWriter().C(0).H(0));
}

constexpr int32_t START = 3; // QuestStatus.value() (QuestStatus.java:11-14)
constexpr int32_t COMPLETE = 5;

class QuestLifecycleSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	explicit QuestLifecycleSpawnTemplate(model::templates::spawns::SpawnGroup& group)
		: SpawnTemplate(group, 100.0f, 102.0f, 50.0f, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/**
 * A fabricated handler: records the timer events of its quest and every quest-completed event (QuestEngine.onQuestCompleted hands each handler
 * registered for it the completed quest's id), and answers a bonus event by adding an item and returning FAILED
 */
class RecordingHandler final : public questEngine::handlers::AbstractQuestHandler {
public:
	explicit RecordingHandler(int32_t questId) : AbstractQuestHandler(questId) {}

	void register_() override {
		qe.registerOnQuestTimerEnd(questId);
		qe.registerOnInvisibleTimerEnd(questId);
		qe.registerOnBonusApply(questId, model::templates::rewards::BonusType::FOOD);
		qe.registerOnQuestCompleted(questId);
	}

	void onQuestCompletedEvent(questEngine::model::QuestEnv& env) override {
		completedQuests.push_back(env.getQuestId());
		if (onCompleted)
			onCompleted(env);
	}

	bool onQuestTimerEndEvent(questEngine::model::QuestEnv& env) override {
		timerEnds.push_back(env.getQuestId());
		return true;
	}

	bool onInvisibleTimerEndEvent(questEngine::model::QuestEnv& env) override {
		invisibleTimerEnds.push_back(env.getQuestId());
		return true;
	}

	questEngine::handlers::HandlerResult onBonusApplyEvent(questEngine::model::QuestEnv& env, model::templates::rewards::BonusType bonusType,
		std::vector<model::templates::quest::QuestItems>& rewardItems) override {
		static_cast<void>(env);
		static_cast<void>(bonusType);
		rewardItems.push_back(model::templates::quest::QuestItems(GROGGIE, 7));
		return questEngine::handlers::HandlerResult::FAILED;
	}

	static inline std::vector<int32_t> timerEnds;
	static inline std::vector<int32_t> invisibleTimerEnds;
	static inline std::vector<int32_t> completedQuests;
	/** What a case looks at when the event arrives (the state and the packets sent so far) */
	static inline std::function<void(questEngine::model::QuestEnv&)> onCompleted;
};

class QuestLifecycleTest : public items::ItemPacketTest {
protected:
	void SetUp() override {
		// the npc template names the "general" AI and this executable links no AI handler: the warn mode puts AIEngine's substitute in place
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		// Player::postConstruct loads the toy pets from the database; the tests have none
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(
			[](model::gameobjects::player::Player&) { return std::vector<Ref<model::gameobjects::player::PetCommonData>>(); });
		ItemPacketTest::SetUp();
		using configs::main::RatesConfig;
		savedQuestKinahRates = *RatesConfig::QUEST_KINAH_RATES.get();
		savedXpQuestRates = *RatesConfig::XP_QUEST_RATES.get();
		savedXpSoloRates = *RatesConfig::XP_SOLO_RATES.get();
		RatesConfig::QUEST_KINAH_RATES.set({3.0f, 6.0f});
		RatesConfig::XP_QUEST_RATES.set({2.0f, 4.0f});
		RatesConfig::XP_SOLO_RATES.set({1.0f, 2.0f});
		savedQuestSizeLimit = configs::main::CustomConfig::BASIC_QUEST_SIZE_LIMIT.load();
		savedQuestLimitDisabled = configs::main::MembershipConfig::QUEST_LIMIT_DISABLED.load();
		configs::main::CustomConfig::BASIC_QUEST_SIZE_LIMIT.store(40);
		configs::main::MembershipConfig::QUEST_LIMIT_DISABLED.store(10);
		savedTimeZone = configs::main::GSConfig::TIME_ZONE_ID.load();
		configs::main::GSConfig::TIME_ZONE_ID.store(std::chrono::locate_zone("UTC"));
		// QuestEngine::clear cancels the daily message in the cron service (QuestEngine.java:119; QuestEngineTest.cpp's fixture)
		services::cron::CronService::resetForTests();
		services::cron::CronService::initSingleton(std::make_unique<utils::cron::ThreadPoolManagerRunnableRunner>(), std::chrono::locate_zone("UTC"),
			services::cron::CronService::Driver::EXECUTOR);

		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(contexts.emplace_back(), QUESTS_XML));
		std::string itemsXml(items::ITEM_TEMPLATES_XML);
		itemsXml.erase(itemsXml.rfind("</item_templates>"));
		itemsXml += ITEMS_XML;
		itemsXml += "</item_templates>";
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(contexts.emplace_back(), itemsXml));
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), NPCS_XML));
		dataholders::DataManager::TITLE_DATA.publish(xml::bindString<dataholders::TitleData>(contexts.emplace_back(), "<player_titles/>"));

		quester = makeQuester();
		quester.player->setPosition(
			world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(100.0f, 100.0f, 50.0f)));
		quester.player->getPosition()->setIsSpawned(true);
		quester.player->setQuestStateList(model::gameobjects::player::QuestStateList::create());
		quester.commonData->setOnline(true); // PlayerEnterWorldService
		world::World::getInstance().storeObject(*quester.player); // PlayerCommonData.getPlayer finds an online player through the World
		questClient = std::make_unique<cp::TestClient>();
		questClient->enterWorld(quester);
		holdItem(820001, items::KINAH, 1000);
		RecordingHandler::timerEnds.clear();
		RecordingHandler::invisibleTimerEnds.clear();
		RecordingHandler::completedQuests.clear();
		RecordingHandler::onCompleted = nullptr;
		savedLogAudit = configs::main::LoggingConfig::LOG_AUDIT.load();
	}

	void TearDown() override {
		RecordingHandler::onCompleted = nullptr;
		configs::main::LoggingConfig::LOG_AUDIT.store(savedLogAudit);
		if (quester.player) {
			quester.player->getController().cancelTask(model::TaskId::QUEST_TIMER);
			quester.player->setClientConnection(nullptr);
			world::World::getInstance().removeObject(*quester.player);
		}
		questClient.reset();
		heldItems.clear();
		quester = {};
		npcs.clear();
		spawnGroups.clear();
		questEngine::QuestEngine::getInstance().clear(); // the handlers themselves are Immortal (RT-11)
		services::cron::CronService::resetForTests();
		ItemPacketTest::TearDown();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		dataholders::DataManager::TITLE_DATA.resetForTests();
		dataholders::DataManager::NPC_FACTIONS_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.resetForTests();
		using configs::main::RatesConfig;
		RatesConfig::QUEST_KINAH_RATES.set(savedQuestKinahRates);
		RatesConfig::XP_QUEST_RATES.set(savedXpQuestRates);
		RatesConfig::XP_SOLO_RATES.set(savedXpSoloRates);
		configs::main::CustomConfig::BASIC_QUEST_SIZE_LIMIT.store(savedQuestSizeLimit);
		configs::main::MembershipConfig::QUEST_LIMIT_DISABLED.store(savedQuestLimitDisabled);
		configs::main::GSConfig::TIME_ZONE_ID.store(savedTimeZone);
	}

	/** cp::makePlayer with the level set to 9 before the Player exists, so no level change runs (see the file comment) */
	static cp::PlayerFixture makeQuester() {
		cp::PlayerFixture f;
		f.account = model::account::Account::create(9821);
		f.commonData = model::gameobjects::player::PlayerCommonData::create(810001);
		f.commonData->setName("Quester");
		f.commonData->setRace(model::Race::ELYOS);
		f.commonData->setPlayerClass(model::PlayerClass::WARRIOR);
		f.commonData->setLevel(9);
		f.appearance = model::gameobjects::player::PlayerAppearance::create();
		f.account->addPlayerAccountData(std::make_unique<model::account::PlayerAccountData>(*f.account, *f.commonData, *f.appearance));
		f.account->setAccountWarehouse(
			std::make_unique<model::items::storage::PlayerStorage>(*f.account, model::items::storage::StorageType::ACCOUNT_WAREHOUSE));
		f.player = model::gameobjects::VisibleObject::create<cp::TestPlayer>(*f.account->getPlayerAccountData(810001), *f.account);
		f.player->setKnownlist(std::make_unique<cp::TestKnownList>(*f.player));
		f.player->setFriendList(std::make_unique<model::gameobjects::player::FriendList>(*f.player,
			std::vector<runtime::Ptr<model::gameobjects::player::Friend>>{}));
		f.player->setBlockList(model::gameobjects::player::BlockList::create());
		f.player->setPlayerSettings(model::gameobjects::player::PlayerSettings::create());
		f.player->setAbyssRank(model::gameobjects::player::AbyssRank::create(0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0));
		f.player->setEffectController(std::make_unique<controllers::effect::PlayerEffectController>(*f.player));
		f.player->setFlyController(std::make_unique<controllers::FlyController>(*f.player));
		f.player->setEmotions(std::make_unique<model::gameobjects::player::emotion::EmotionList>(*f.player));
		return f;
	}

	/**
	 * A player outside the world at `level`, for startEventQuest's filters (it reads the quest list, the level, the race, the class and the
	 * gender). The level is set while the player is offline, so no level change runs; setDaeva(true) stands for the completed ascension that
	 * lifts the non-daeva cap of level 9 (PlayerCommonData.setExp would otherwise ask updateDaeva, which loads an offline player's quests
	 * from the database).
	 */
	static cp::PlayerFixture offlinePlayer(int32_t objectId, model::PlayerClass playerClass, model::Gender gender, int32_t level) {
		cp::PlayerFixture f = cp::makePlayer(objectId, objectId, "Eventer");
		f.commonData->setPlayerClass(playerClass);
		f.commonData->setGender(gender);
		f.commonData->setDaeva(true);
		f.commonData->setLevel(level);
		f.player->setQuestStateList(model::gameobjects::player::QuestStateList::create());
		return f;
	}

	model::gameobjects::player::Player& me() { return *quester.player; }

	/** An item row of the cube as the inventory DAO loads it (onLoadHandler: no packet) */
	void holdItem(int32_t objId, int32_t itemId, int64_t count) {
		Ref<model::gameobjects::Item> item = items::loadedItem(objId, itemId, count, model::items::storage::StorageType::CUBE);
		me().getInventory().onLoadHandler(*item);
		heldItems.push_back(item);
	}

	int64_t kinah() { return me().getInventory().getKinah(); }
	int64_t held(int32_t itemId) { return me().getInventory().getItemCountByItemId(itemId); }
	int64_t exp() { return quester.commonData->getExp(); }

	/** A quest state as the DAO loads it (stored), in the player's list */
	Ref<QuestState> hold(int32_t questId, QuestStatus status, int32_t completeCount = 0) {
		Ref<QuestState> qs = QuestState::create(questId, status, 0, 0, completeCount, std::nullopt, std::nullopt, std::nullopt);
		qs->setPersistentState(model::gameobjects::Persistable::PersistentState::UPDATED);
		me().getQuestStateList()->addQuest(questId, *qs);
		return qs;
	}

	/** A fresh quest list, for the next row of a table */
	void forgetQuests() { me().setQuestStateList(model::gameobjects::player::QuestStateList::create()); }

	Ref<QuestEnv> envOf(int32_t questId, int32_t dialogActionId, Ptr<model::gameobjects::VisibleObject> target = nullptr) {
		return QuestEnv::create(target, me(), questId, dialogActionId);
	}

	model::gameobjects::Npc& spawnMires() {
		const model::templates::npc::NpcTemplate* template_ = dataholders::DataManager::NPC_DATA->getNpcTemplate(MIRES);
		Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, MIRES, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<QuestLifecycleSpawnTemplate>(*group));
		Ref<model::gameobjects::Npc> npc =
			model::gameobjects::VisibleObject::create<model::gameobjects::Npc>(std::make_unique<controllers::NpcController>(), spawn, template_);
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		return *npc;
	}

	std::vector<std::vector<uint8_t>> questerSent() { return (*questClient)->sentBytes(); }
	void clearQuesterSent() { (*questClient)->clearSent(); }
	std::vector<uint8_t> serializedForQuester(network::aion::AionServerPacket&& packet) { return cp::serialized(std::move(packet), questClient->con()); }

	/**
	 * SM_INVENTORY_UPDATE_ITEM(player, kinah, INC_KINAH_QUEST) for the quester's kinah (row 820001) at `count`, as giveReward's increaseKinah sends
	 * it (QuestService.java:222-223): serialized from a copy of the row at that count, since the live item holds only the last one
	 */
	std::vector<uint8_t> kinahUpdate(int64_t count) {
		Ref<model::gameobjects::Item> atCount = items::loadedItem(820001, items::KINAH, count, model::items::storage::StorageType::CUBE);
		return serializedForQuester(network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM(me(), *atCount,
			services::item::ItemPacketService_ItemUpdateType::INC_KINAH_QUEST));
	}

	/** SM_INVENTORY_ADD_ITEM(ITEM_COLLECT) of the first cube item of `itemId`, as ItemService.addItem adds a new row (Storage.add) */
	std::vector<uint8_t> itemAdded(int32_t itemId) {
		Ptr<model::gameobjects::Item> item = me().getInventory().getFirstItemByItemId(itemId);
		if (!item)
			return {};
		return serializedForQuester(network::aion::serverpackets::SM_INVENTORY_ADD_ITEM({item}, me(),
			services::item::ItemPacketService_ItemAddType::ITEM_COLLECT));
	}

	std::deque<xml::LoadContext> contexts;
	cp::PlayerFixture quester;
	std::unique_ptr<cp::TestClient> questClient;
	std::vector<Ref<model::gameobjects::Item>> heldItems;
	std::vector<Ref<model::gameobjects::Npc>> npcs;
	std::vector<Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<float> savedQuestKinahRates;
	std::vector<float> savedXpQuestRates;
	std::vector<float> savedXpSoloRates;
	int32_t savedQuestSizeLimit = 0;
	int8_t savedQuestLimitDisabled = 0;
	const std::chrono::time_zone* savedTimeZone = nullptr;
	bool savedLogAudit = false;
};

// finishQuest (QuestService.java:77-117) at the end npc: reward group 0 (validateAndFixRewardGroup, :134-139: one group, no warning), the
// kinah at the quest kinah rate (:222-223: 120 x 3.0), the exp at XP_QUEST (:224-227: 130 x 2.0 - XP_HUNTING would have the solo rate 1.0 and
// its cap) with the npc's name in the message, then COMPLETE with one completion, the variables reset, SM_QUEST_ACTION(UPDATE) and the
// nearby quests. A second report finds the quest COMPLETE and pays nothing (:84-85).
TEST_F(QuestLifecycleTest, FinishingAQuestInRewardPaysTheRatedKinahAndExpAndCompletesIt) {
	model::gameobjects::Npc& mires = spawnMires();
	Ref<QuestState> qs = hold(1101, QuestStatus::REWARD);
	qs->setQuestVarById(0, 2);
	clearQuesterSent();

	network::test::LogCapture log({"com.aionemu.gameserver.services.QuestService"});
	EXPECT_TRUE(QuestService::finishQuest(*envOf(1101, model::DialogAction::SELECTED_QUEST_NOREWARD, Ptr<model::gameobjects::VisibleObject>(mires))));
	EXPECT_FALSE(log.contains("warning")) << "one reward group: 0 without the warning (:134-137)\n" << log.dump();

	EXPECT_EQ(kinah(), 1000 + 360);
	EXPECT_EQ(exp(), START_EXP_LEVEL_9 + 260);
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(qs->getCompleteCount(), 1);
	EXPECT_EQ(qs->getQuestVars()->getQuestVars(), 0);
	EXPECT_EQ(qs->getRewardGroup(), std::optional<int32_t>(0));
	EXPECT_EQ(qs->getNextRepeatTime(), std::nullopt) << "not time-based";
	std::vector<std::vector<uint8_t>> sent = questerSent();
	EXPECT_EQ(items::opcodesOf(sent), (std::vector<int32_t>{items::SM_INVENTORY_UPDATE_ITEM_OPCODE, SM_STATUPDATE_EXP_OPCODE,
										   items::SM_SYSTEM_MESSAGE_OPCODE, SM_QUEST_ACTION_OPCODE, SM_NEARBY_QUESTS_OPCODE}));
	ASSERT_EQ(sent.size(), 5u);
	EXPECT_EQ(sent[2], serializedForQuester(SM_SYSTEM_MESSAGE::STR_GET_EXP(dataholders::DataManager::NPC_DATA->getNpcTemplate(MIRES)->getL10n(), 260)))
		<< "STR_GET_EXP with the end npc's name (QuestService.java:225-226)";
	EXPECT_EQ(sent[3], questAction(2, 1101, COMPLETE));
	EXPECT_EQ(sent[4], noNearbyQuests());

	clearQuesterSent();
	EXPECT_FALSE(QuestService::finishQuest(*envOf(1101, model::DialogAction::SELECTED_QUEST_NOREWARD)));
	EXPECT_EQ(kinah(), 1000 + 360);
	EXPECT_TRUE(questerSent().empty());
}

// finishQuest pays only a quest in REWARD (:84-85) and never a MISSION completed before (:87-88); a quest without a template throws Java's
// NullPointerException at template.getCategory() (:86-87)
TEST_F(QuestLifecycleTest, FinishQuestPaysNothingOutsideRewardNorForAMissionCompletedBefore) {
	clearQuesterSent();
	EXPECT_FALSE(QuestService::finishQuest(*envOf(1101, model::DialogAction::SELECTED_QUEST_NOREWARD))) << "no quest state";
	for (QuestStatus status : {QuestStatus::START, QuestStatus::COMPLETE, QuestStatus::LOCKED}) {
		forgetQuests();
		Ref<QuestState> qs = hold(1101, status);
		EXPECT_FALSE(QuestService::finishQuest(*envOf(1101, model::DialogAction::SELECTED_QUEST_NOREWARD))) << static_cast<int>(status);
		EXPECT_EQ(qs->getStatus(), status);
	}
	forgetQuests();
	Ref<QuestState> mission = hold(1001, QuestStatus::REWARD, 1);
	EXPECT_FALSE(QuestService::finishQuest(*envOf(1001, model::DialogAction::SELECTED_QUEST_NOREWARD))) << "MISSION completed once";
	EXPECT_EQ(mission->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(kinah(), 1000);
	EXPECT_EQ(exp(), START_EXP_LEVEL_9);
	EXPECT_TRUE(questerSent().empty());

	mission->setCompleteCount(0);
	EXPECT_TRUE(QuestService::finishQuest(*envOf(1001, model::DialogAction::SELECTED_QUEST_NOREWARD))) << "MISSION never completed";
	EXPECT_EQ(exp(), START_EXP_LEVEL_9 + 4200);

	hold(4242, QuestStatus::REWARD);
	EXPECT_THROW(static_cast<void>(QuestService::finishQuest(*envOf(4242, model::DialogAction::SELECTED_QUEST_NOREWARD))), runtime::NullPointerException);
}

// validateAndFixRewardGroup (QuestService.java:123-142), through finishQuest on quest 9554's three groups (10, 100 and 1000 kinah, x 3.0): an
// unset group becomes 0 with a warning when there are several; a group outside the list becomes the last with a warning; a valid one is paid.
// A state not in REWARD is left alone.
TEST_F(QuestLifecycleTest, TheRewardGroupIsValidatedBeforeItsKinahIsPaid) {
	struct Row {
		std::optional<int32_t> group;
		int64_t kinahPaid;
		int32_t groupAfter;
		const char* warning;
	};
	const std::vector<Row> rows = {
		{std::nullopt, 30, 0, "Handler for quest 9554 possibly rewarded the wrong reward group."},
		{0, 30, 0, nullptr},
		{1, 300, 1, nullptr},
		{2, 3000, 2, nullptr},
		{3, 3000, 2, "Handler for quest 9554 tried to reward a nonexistent reward group (index 3)."},
		{-1, 3000, 2, "Handler for quest 9554 tried to reward a nonexistent reward group (index -1)."},
	};
	for (const Row& row : rows) {
		network::test::LogCapture log({"com.aionemu.gameserver.services.QuestService"});
		forgetQuests();
		Ref<QuestState> qs = hold(9554, QuestStatus::REWARD);
		qs->setRewardGroup(row.group);
		int64_t before = kinah();
		EXPECT_TRUE(QuestService::finishQuest(*envOf(9554, model::DialogAction::SELECTED_QUEST_NOREWARD)));
		EXPECT_EQ(kinah() - before, row.kinahPaid) << row.group.value_or(-99);
		EXPECT_EQ(qs->getRewardGroup(), std::optional<int32_t>(row.groupAfter));
		if (row.warning)
			EXPECT_EQ(log.count(std::string("warning|com.aionemu.gameserver.services.QuestService|") + row.warning), 1) << log.dump();
		else
			EXPECT_FALSE(log.contains("warning")) << log.dump();
	}

	forgetQuests();
	Ref<QuestState> started = hold(9554, QuestStatus::START);
	started->setRewardGroup(7);
	QuestService::validateAndFixRewardGroup(started, 9554);
	EXPECT_EQ(started->getRewardGroup(), std::optional<int32_t>(7)) << "only a quest in REWARD is validated";
	QuestService::validateAndFixRewardGroup(nullptr, 9554); // Java: qs != null
}

// getRewardIndex (:216-218): SELECTED_QUEST_REWARD1..15 (8..22) are the selectable items 0..14, anything else is -1. getRewardItems (:163-183):
// the group's reward items always, plus the selectable item of that index - quest 1113's three choices; an index past them is a warning.
TEST_F(QuestLifecycleTest, SelectedQuestRewardNPaysTheGroupsSelectableItemNMinusOne) {
	EXPECT_EQ(QuestService::getRewardIndex(model::DialogAction::SELECTED_QUEST_REWARD1), 0);
	EXPECT_EQ(QuestService::getRewardIndex(model::DialogAction::SELECTED_QUEST_REWARD15), 14);
	EXPECT_EQ(QuestService::getRewardIndex(model::DialogAction::SELECTED_QUEST_REWARD1 - 1), -1);
	EXPECT_EQ(QuestService::getRewardIndex(model::DialogAction::SELECTED_QUEST_NOREWARD), -1);

	network::test::LogCapture log({"com.aionemu.gameserver.services.QuestService"});
	hold(1113, QuestStatus::REWARD);
	EXPECT_TRUE(QuestService::finishQuest(*envOf(1113, model::DialogAction::SELECTED_QUEST_REWARD2)));
	EXPECT_EQ(held(162000052), 0);
	EXPECT_EQ(held(162000057), 3) << "SELECTED_QUEST_REWARD2: the second choice";
	EXPECT_EQ(held(169000003), 0);
	EXPECT_EQ(held(160003001), 3) << "the reward item";
	EXPECT_EQ(exp(), START_EXP_LEVEL_9 + 3476);

	forgetQuests();
	hold(1113, QuestStatus::REWARD);
	EXPECT_TRUE(QuestService::finishQuest(*envOf(1113, model::DialogAction::SELECTED_QUEST_REWARD4)));
	EXPECT_EQ(held(162000057), 3);
	EXPECT_EQ(held(160003001), 6);
	EXPECT_EQ(log.count("The SelectableRewardItem list has no element on index 3. See quest id 1113"), 1) << log.dump();

	forgetQuests();
	hold(1113, QuestStatus::REWARD);
	EXPECT_TRUE(QuestService::finishQuest(*envOf(1113, model::DialogAction::SELECTED_QUEST_NOREWARD)));
	EXPECT_EQ(held(162000052) + held(162000057) + held(169000003), 3) << "no choice for SELECTED_QUEST_NOREWARD";
	EXPECT_EQ(held(160003001), 9);
}

// Class rewards (:171-194) of quest 9619 (use_class_reward="2": only on the last repeat, completeCount == reward_repeat_count - 1 = 4): the
// class's list, by SELECTED_QUEST_REWARDn or by SELECTED_QUEST_NOREWARD with the extended index - 8. A GLADIATOR takes the fighter list; a
// WARRIOR (a starting class) has none (QuestTemplate.getSelectableRewardByClass: no case).
TEST_F(QuestLifecycleTest, AClassRewardIsPaidOnTheLastRepeatFromTheClassesList) {
	network::test::LogCapture log({"com.aionemu.gameserver.services.QuestService"});
	quester.commonData->setPlayerClass(model::PlayerClass::GLADIATOR);
	hold(9619, QuestStatus::REWARD, 4);
	EXPECT_TRUE(QuestService::finishQuest(*envOf(9619, model::DialogAction::SELECTED_QUEST_REWARD6)));
	EXPECT_EQ(held(GROGGIE), 1) << "fighter_selectable_reward 6: 186000122 x 1";

	forgetQuests();
	hold(9619, QuestStatus::REWARD, 3);
	EXPECT_TRUE(QuestService::finishQuest(*envOf(9619, model::DialogAction::SELECTED_QUEST_REWARD6)));
	EXPECT_EQ(held(GROGGIE), 1) << "not the last repeat: the group's own selectable items, of which there are none";
	EXPECT_EQ(log.count("The SelectableRewardItem list has no element on index 5. See quest id 9619"), 1) << log.dump();

	forgetQuests();
	hold(9619, QuestStatus::REWARD, 4);
	Ref<QuestEnv> extended = envOf(9619, model::DialogAction::SELECTED_QUEST_NOREWARD);
	extended->setExtendedRewardIndex(14);
	EXPECT_TRUE(QuestService::finishQuest(*extended));
	EXPECT_EQ(held(GROGGIE), 1 + 2) << "extended index 14 - 8 = 6: 186000122 x 2";

	forgetQuests();
	hold(9619, QuestStatus::REWARD, 4);
	Ref<QuestEnv> below = envOf(9619, model::DialogAction::SELECTED_QUEST_NOREWARD);
	below->setExtendedRewardIndex(7);
	EXPECT_TRUE(QuestService::finishQuest(*below));
	EXPECT_EQ(held(GROGGIE), 3);
	EXPECT_EQ(log.count("The SelectableRewardByClass list has no element on index -1. See quest id 9619"), 1) << log.dump();

	quester.commonData->setPlayerClass(model::PlayerClass::WARRIOR);
	forgetQuests();
	hold(9619, QuestStatus::REWARD, 4);
	EXPECT_TRUE(QuestService::finishQuest(*envOf(9619, model::DialogAction::SELECTED_QUEST_REWARD1)));
	EXPECT_EQ(held(GROGGIE), 3);
	EXPECT_EQ(log.count("The SelectableRewardByClass list has no element on index 0. See quest id 9619. The size for WARRIOR is: 0"), 1)
		<< log.dump();
	EXPECT_EQ(exp(), START_EXP_LEVEL_10) << "5 x 9619 x 2.0 exp: a non-daeva stops at the start of level 10";
}

// use_class_reward="1" (QuestTemplate.isClassRewardOnEveryRepeat) pays the class's list on any completion (:172, 187), not only on the last
// repeat: quest 1007, a MISSION never completed and without reward_repeat_count, so isLastRepeat never holds. A GLADIATOR takes the fighter
// list - SELECTED_QUEST_REWARD2 its item 1, SELECTED_QUEST_NOREWARD with the extended index 8 its item 0 (8 - 8 = 0, the lower bound of :188)
// and with 10 its item 2 - and each time the group's own items: 5 teas and 250,000 kinah, which ItemService.addItem adds as kinah at no rate.
TEST_F(QuestLifecycleTest, AnEveryRepeatClassRewardIsPaidOnAnyCompletion) {
	network::test::LogCapture log({"com.aionemu.gameserver.services.QuestService"});
	quester.commonData->setPlayerClass(model::PlayerClass::GLADIATOR);
	hold(1007, QuestStatus::REWARD)->setRewardGroup(0);
	EXPECT_TRUE(QuestService::finishQuest(*envOf(1007, model::DialogAction::SELECTED_QUEST_REWARD2)));
	EXPECT_EQ(held(ORACLE_GREATSWORD), 1) << "fighter_selectable_reward 2";
	EXPECT_EQ(held(PROPHECY_SWORD) + held(PROPHECY_SPEAR), 0);
	EXPECT_EQ(held(TEA_OF_REPOSE), 5);
	EXPECT_EQ(kinah(), 1000 + 250000);

	struct Row {
		int32_t extendedIndex;
		int32_t itemId;
	};
	for (const Row& row : {Row{8, PROPHECY_SWORD}, Row{10, PROPHECY_SPEAR}}) {
		forgetQuests();
		hold(1007, QuestStatus::REWARD)->setRewardGroup(0);
		Ref<QuestEnv> env = envOf(1007, model::DialogAction::SELECTED_QUEST_NOREWARD);
		env->setExtendedRewardIndex(row.extendedIndex);
		EXPECT_TRUE(QuestService::finishQuest(*env));
		EXPECT_EQ(held(row.itemId), 1) << "extended index " << row.extendedIndex;
	}
	EXPECT_EQ(held(ORACLE_GREATSWORD), 1);
	EXPECT_EQ(held(TEA_OF_REPOSE), 15);
	EXPECT_EQ(kinah(), 1000 + 3 * 250000);
	EXPECT_FALSE(log.contains("warning")) << log.dump();
}

// Extended rewards (:92-95, 149-162) of quest 9618 on the Xth completion (reward_repeat_count 5: completeCount 4): with SELECTED_QUEST_NOREWARD
// the extended selectable item of the extended index - 8, else of the index - 1, else a warning; not before the Xth time
TEST_F(QuestLifecycleTest, TheExtendedRewardIsPaidOnTheXthCompletionByTheExtendedIndex) {
	network::test::LogCapture log({"com.aionemu.gameserver.services.QuestService"});
	struct Row {
		int32_t completeCount;
		int32_t dialogActionId;
		int32_t extendedIndex;
		int64_t groggies;
	};
	const std::vector<Row> rows = {
		{4, model::DialogAction::SELECTED_QUEST_NOREWARD, 10, 3}, // 10 - 8 = 2: 186000122 x 3
		{4, model::DialogAction::SELECTED_QUEST_NOREWARD, 8, 1},  // 8 - 8 = 0, the lower bound of :154: x 1 (index - 1 would be x 8)
		{4, model::DialogAction::SELECTED_QUEST_NOREWARD, 3, 3},  // 3 - 8 < 0, 3 - 1 = 2: x 3
		{4, model::DialogAction::SELECTED_QUEST_NOREWARD, 1, 1},  // 1 - 8 < 0, 1 - 1 = 0, the lower bound of :156: x 1
		{4, model::DialogAction::SELECTED_QUEST_NOREWARD, 17, 10}, // 17 - 8 = 9: x 10
		{4, model::DialogAction::SELECTED_QUEST_NOREWARD, 20, 0}, // 12 and 19 are past the ten: the warning
		{3, model::DialogAction::SELECTED_QUEST_NOREWARD, 10, 0}, // not the 5th completion
		{4, model::DialogAction::SELECTED_QUEST_REWARD1, 10, 0},  // no extended choice without SELECTED_QUEST_NOREWARD
	};
	for (const Row& row : rows) {
		forgetQuests();
		hold(9618, QuestStatus::REWARD, row.completeCount);
		Ref<QuestEnv> env = envOf(9618, row.dialogActionId);
		env->setExtendedRewardIndex(row.extendedIndex);
		int64_t before = held(GROGGIE);
		int64_t kinahBefore = kinah();
		EXPECT_TRUE(QuestService::finishQuest(*env));
		EXPECT_EQ(held(GROGGIE) - before, row.groggies) << row.completeCount << "/" << row.dialogActionId << "/" << row.extendedIndex;
		EXPECT_EQ(kinah() - kinahBefore, 9618 * 3) << "the group's kinah every time";
	}
	EXPECT_EQ(log.count("The extended SelectableRewardItem list has no element on index 12. See quest id 9618. The size is: 10"), 1) << log.dump();
}

// finishQuest's order (:101-104): the reward items first, then the kinah, the exp and the title of giveReward (:220-229). Quest 1124's title 7
// is not in the title data here, so TitleList.addTitle throws its IllegalArgumentException ("Invalid title id") after the items, the kinah and
// the exp were given - and before setStatus(COMPLETE) (:108): the quest stays in REWARD, as in Java
TEST_F(QuestLifecycleTest, FinishQuestPaysItemsThenKinahThenExpBeforeTheTitle) {
	hold(1124, QuestStatus::REWARD);
	clearQuesterSent();
	EXPECT_THROW(static_cast<void>(QuestService::finishQuest(*envOf(1124, model::DialogAction::SELECTED_QUEST_NOREWARD))),
		runtime::IllegalArgumentException);
	EXPECT_EQ(held(162000012), 4) << "the reward item";
	EXPECT_EQ(kinah(), 1000 + 2250 * 3);
	EXPECT_EQ(exp(), START_EXP_LEVEL_9 + 6655 * 2);
	EXPECT_EQ(me().getQuestStateList()->getQuestState(1124)->getStatus(), QuestStatus::REWARD);
	std::vector<int32_t> opcodes = items::opcodesOf(questerSent());
	auto first = [&](int32_t opcode) { return std::find(opcodes.begin(), opcodes.end(), opcode) - opcodes.begin(); };
	auto last = [&](int32_t opcode) { return opcodes.rend() - std::find(opcodes.rbegin(), opcodes.rend(), opcode) - 1; };
	ASSERT_LT(first(items::SM_INVENTORY_UPDATE_ITEM_OPCODE), static_cast<std::ptrdiff_t>(opcodes.size()));
	EXPECT_LT(first(items::SM_INVENTORY_ADD_ITEM_OPCODE), last(items::SM_INVENTORY_UPDATE_ITEM_OPCODE)) << "the item before the kinah";
	EXPECT_LT(last(items::SM_INVENTORY_UPDATE_ITEM_OPCODE), first(SM_STATUPDATE_EXP_OPCODE)) << "the kinah before the exp";
	EXPECT_EQ(std::count(opcodes.begin(), opcodes.end(), SM_QUEST_ACTION_OPCODE), 0);
}

// A <bonus> quest asks the engine's bonus handler (:197-205) with the list being built: the item the handler adds is paid, and its FAILED keeps
// BonusService.getQuestBonus out (:200) - quest 1305 (bonus FOOD) with a fabricated handler registered for FOOD. On its 10th completion
// (reward_repeat_count 10: completeCount 9, :92-95) the extended rewards are paid too (their order:
// TheXthCompletionAddsTheExtendedItemFirstAndPaysTheExtendedKinahLast)
TEST_F(QuestLifecycleTest, ABonusHandlerThatAnswersFailedAddsItsItemAndKeepsTheBonusOut) {
	questEngine::QuestEngine::getInstance().addQuestHandler(std::make_unique<RecordingHandler>(1305));
	hold(1305, QuestStatus::REWARD);
	EXPECT_TRUE(QuestService::finishQuest(*envOf(1305, model::DialogAction::SELECTED_QUEST_NOREWARD)));
	EXPECT_EQ(held(GROGGIE), 7) << "the handler's item";
	EXPECT_EQ(kinah(), 1000 + 2800 * 3);
	EXPECT_EQ(me().getInventory().getItemsWithKinah().size(), 2u) << "kinah and the handler's item, no bonus item";
	EXPECT_EQ(me().getQuestStateList()->getQuestState(1305)->getStatus(), QuestStatus::COMPLETE);

	forgetQuests();
	hold(1305, QuestStatus::REWARD, 9);
	EXPECT_TRUE(QuestService::finishQuest(*envOf(1305, model::DialogAction::SELECTED_QUEST_NOREWARD)));
	EXPECT_EQ(held(OLD_GREEN_SACK), 1) << "the extended reward item";
	EXPECT_EQ(held(GROGGIE), 14);
	EXPECT_EQ(kinah(), 1000 + 2800 * 3 + 2800 * 3 + 5000 * 3) << "the group's kinah and the extended kinah";
}

// The order of a 10th completion of quest 1305: finishQuest lists the extended item (:93) before the group's items (:97; here the bonus
// handler's 7 Groggies), adds them in that order (:101-102: SM_INVENTORY_ADD_ITEM and the cube size each), then pays the group's rewards and
// after them the extended ones (giveReward(rewards), giveReward(extendedRewards), :103-104): the group's kinah, its exp (capped at the start of
// level 10: STR_GET_EXP2 of the whole reward, then STR_LEVEL_LIMIT_QUEST_NOT_FINISHED1, PlayerCommonData.java:216-220), the extended kinah.
// No warning: one reward group (:134-137), and an extended reward
// without selectable items asks for no index (:152). QuestEngine.onQuestCompleted (:113) then hands every handler registered for it the
// completed quest - after COMPLETE and SM_QUEST_ACTION(UPDATE), before the nearby quests; a refused finishQuest completes nothing.
TEST_F(QuestLifecycleTest, TheXthCompletionAddsTheExtendedItemFirstAndPaysTheExtendedKinahLast) {
	questEngine::QuestEngine::getInstance().addQuestHandler(std::make_unique<RecordingHandler>(1305));
	std::optional<QuestStatus> statusAtEvent;
	size_t sentAtEvent = 0;
	RecordingHandler::onCompleted = [&](QuestEnv& env) {
		statusAtEvent = env.getPlayer()->getQuestStateList()->getQuestState(env.getQuestId())->getStatus();
		sentAtEvent = questerSent().size();
	};
	hold(1305, QuestStatus::REWARD, 9);
	clearQuesterSent();
	network::test::LogCapture log({"com.aionemu.gameserver.services.QuestService"});
	EXPECT_TRUE(QuestService::finishQuest(*envOf(1305, model::DialogAction::SELECTED_QUEST_NOREWARD)));
	EXPECT_FALSE(log.contains("warning")) << log.dump();

	std::vector<std::vector<uint8_t>> sent = questerSent();
	EXPECT_EQ(items::opcodesOf(sent),
		(std::vector<int32_t>{items::SM_INVENTORY_ADD_ITEM_OPCODE, items::SM_CUBE_UPDATE_OPCODE, items::SM_INVENTORY_ADD_ITEM_OPCODE,
			items::SM_CUBE_UPDATE_OPCODE, items::SM_INVENTORY_UPDATE_ITEM_OPCODE, SM_STATUPDATE_EXP_OPCODE, items::SM_SYSTEM_MESSAGE_OPCODE,
			items::SM_SYSTEM_MESSAGE_OPCODE, items::SM_INVENTORY_UPDATE_ITEM_OPCODE, SM_QUEST_ACTION_OPCODE, SM_NEARBY_QUESTS_OPCODE}));
	ASSERT_EQ(sent.size(), 11u);
	EXPECT_EQ(sent[0], itemAdded(OLD_GREEN_SACK)) << "the extended item first";
	EXPECT_EQ(sent[1], items::cubeSize(model::items::storage::StorageType::CUBE, 1));
	EXPECT_EQ(sent[2], itemAdded(GROGGIE)) << "then the group's (the bonus handler's)";
	EXPECT_EQ(sent[3], items::cubeSize(model::items::storage::StorageType::CUBE, 2));
	EXPECT_EQ(sent[4], kinahUpdate(1000 + 2800 * 3)) << "the group's kinah";
	EXPECT_EQ(sent[6], serializedForQuester(SM_SYSTEM_MESSAGE::STR_GET_EXP2(66900 * 2))) << "the group's exp";
	EXPECT_EQ(sent[7], serializedForQuester(SM_SYSTEM_MESSAGE::STR_LEVEL_LIMIT_QUEST_NOT_FINISHED1()));
	EXPECT_EQ(sent[8], kinahUpdate(1000 + 2800 * 3 + 5000 * 3)) << "the extended kinah after the group's exp";
	EXPECT_EQ(sent[9], questAction(2, 1305, COMPLETE));

	EXPECT_EQ(RecordingHandler::completedQuests, std::vector<int32_t>{1305});
	EXPECT_EQ(statusAtEvent, std::optional<QuestStatus>(QuestStatus::COMPLETE));
	EXPECT_EQ(sentAtEvent, 10u) << "after SM_QUEST_ACTION(UPDATE), before SM_NEARBY_QUESTS";

	EXPECT_FALSE(QuestService::finishQuest(*envOf(1305, model::DialogAction::SELECTED_QUEST_NOREWARD)));
	EXPECT_EQ(RecordingHandler::completedQuests.size(), 1u) << "a refused finishQuest completes nothing";
}

// giveReward's DP (:236-237), after finishQuest's items (:101-102): quest 80588 (dp 2000 and four reward items). A WARRIOR, a starting class,
// gets none (PlayerCommonData.setDp returns first). A GLADIATOR gets the 2000 (below the MAXDP stat's 4000): SM_DP_INFO to himself and those
// who see him (broadcastPacket(..., true)), then SM_STATUPDATE_DP - after the items and before SM_QUEST_ACTION.
TEST_F(QuestLifecycleTest, TheDpRewardGoesToADaevaAfterTheItems) {
	hold(80588, QuestStatus::REWARD);
	clearQuesterSent();
	EXPECT_TRUE(QuestService::finishQuest(*envOf(80588, model::DialogAction::SELECTED_QUEST_NOREWARD)));
	EXPECT_EQ(quester.commonData->getDp(), 0) << "a WARRIOR";
	EXPECT_TRUE(items::packetsOf(questerSent(), SM_DP_INFO_OPCODE).empty());
	EXPECT_EQ(held(164000259), 30);
	EXPECT_EQ(held(164002228), 3);

	quester.commonData->setPlayerClass(model::PlayerClass::GLADIATOR);
	forgetQuests();
	hold(80588, QuestStatus::REWARD);
	clearQuesterSent();
	EXPECT_TRUE(QuestService::finishQuest(*envOf(80588, model::DialogAction::SELECTED_QUEST_NOREWARD)));
	EXPECT_EQ(quester.commonData->getDp(), 2000);
	EXPECT_EQ(held(164002228), 6);
	std::vector<std::vector<uint8_t>> sent = questerSent();
	const std::vector<uint8_t> dpInfo = items::javaPacket(SM_DP_INFO_OPCODE, PacketWriter().D(810001).H(2000)); // SM_DP_INFO.java writeImpl
	const std::vector<uint8_t> dpUpdate = serializedForQuester(network::aion::serverpackets::SM_STATUPDATE_DP(2000));
	ASSERT_EQ(std::count(sent.begin(), sent.end(), dpInfo), 1);
	ASSERT_EQ(std::count(sent.begin(), sent.end(), dpUpdate), 1);
	std::vector<int32_t> opcodes = items::opcodesOf(sent);
	auto at = [&](const std::vector<uint8_t>& packet) { return std::find(sent.begin(), sent.end(), packet) - sent.begin(); };
	auto last = [&](int32_t opcode) { return opcodes.rend() - std::find(opcodes.rbegin(), opcodes.rend(), opcode) - 1; };
	EXPECT_EQ(std::count(opcodes.begin(), opcodes.end(), items::SM_INVENTORY_UPDATE_ITEM_OPCODE), 4) << "the four items onto their stacks";
	EXPECT_LT(last(items::SM_INVENTORY_UPDATE_ITEM_OPCODE), at(dpInfo)) << "the items before the DP";
	EXPECT_LT(at(dpInfo), at(dpUpdate));
	EXPECT_LT(at(dpUpdate), at(questAction(2, 80588, COMPLETE)));
}

// giveReward's cube arm (QuestService.java:240-241), which reaches CubeExpandService.questExpand since M5c stage 1 ported it (P-05;
// CubeExpandService.java:73-94): quest 50033, whose only reward is extend_inventory="1", adds one quest expansion -
// STR_EXTEND_INVENTORY_SIZE_EXTENDED(9), the cube one row of 9 larger than StorageType.CUBE's 27 (Player.setCubeLimit, Player.java:428-430),
// SM_CUBE_UPDATE.cubeSize(CUBE) with the counters (SM_CUBE_UPDATE.java:29-52, 71-79: the kinah is not one of the cube's items) - before COMPLETE
// and SM_QUEST_ACTION. At the expansion limit (CubeExpandService.canExpand, :116-125, with the Java default gameserver.cube.expansion_limit 11)
// the reward is STR_EXTEND_INVENTORY_CANT_EXTEND_MORE alone, and the quest completes all the same.
TEST_F(QuestLifecycleTest, TheCubeRewardAddsAQuestExpansionBeforeTheQuestCompletes) {
	struct ExpansionLimit {
		int32_t saved = configs::main::CustomConfig::CUBE_EXPANSION_LIMIT.exchange(11);
		~ExpansionLimit() { configs::main::CustomConfig::CUBE_EXPANSION_LIMIT.store(saved); }
	} expansionLimit;
	Ref<QuestState> qs = hold(50033, QuestStatus::REWARD);
	clearQuesterSent();

	EXPECT_TRUE(QuestService::finishQuest(*envOf(50033, model::DialogAction::SELECTED_QUEST_NOREWARD)));

	EXPECT_EQ(me().getQuestExpands(), 1);
	EXPECT_EQ(me().getNpcExpands(), 0);
	EXPECT_EQ(me().getItemExpands(), 0);
	EXPECT_EQ(me().getInventory().getLimit(), 27 + 9);
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(questerSent(), cp::exactly({serializedForQuester(SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_SIZE_EXTENDED(9)),
								  items::javaPacket(items::SM_CUBE_UPDATE_OPCODE, PacketWriter().C(0).C(0).D(0).C(0).C(1).C(0)),
								  questAction(2, 50033, COMPLETE), noNearbyQuests()}));

	// 10 npc expansions and this quest's 1 are 11: the next is refused
	quester.commonData->setNpcExpands(10);
	forgetQuests();
	qs = hold(50033, QuestStatus::REWARD);
	clearQuesterSent();

	EXPECT_TRUE(QuestService::finishQuest(*envOf(50033, model::DialogAction::SELECTED_QUEST_NOREWARD)));

	EXPECT_EQ(me().getQuestExpands(), 1);
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(questerSent(), cp::exactly({serializedForQuester(SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_CANT_EXTEND_MORE()),
								  questAction(2, 50033, COMPLETE), noNearbyQuests()}));
}

// calculateRepeatDate (:246-263), through finishQuest of a time-based quest: the next 09:00 of the server zone after now, for a weekly quest
// moved on to the first repeat day at or after that date (findNextRepeatDay, :265-273), with the daily or weekly message; canRepeat then
// waits for it. 2024-01-03 is a Wednesday.
TEST_F(QuestLifecycleTest, ATimeBasedQuestRepeatsAtTheNext0900OfItsRepeatDayInTheServerZone) {
	using commons::database::Timestamp;
	using model::templates::quest::QuestRepeatCycle;
	auto at = [](int64_t millis) { return std::optional<Timestamp>(Timestamp(std::chrono::milliseconds(millis))); };
	struct Row {
		const char* zone;
		int32_t questId;
		int64_t now;
		int64_t next;
		QuestRepeatCycle day; // the repeat day the weekly message names; ALL for the daily message
	};
	const std::vector<Row> rows = {
		{"UTC", 9600, 1704268800000, 1704272400000, QuestRepeatCycle::ALL}, // Wed 08:00 -> Wed 09:00 (daily)
		{"UTC", 9600, 1704272400000, 1704272400000, QuestRepeatCycle::ALL}, // Wed 09:00:00.000 is not after 09:00 -> the same 09:00
		{"UTC", 9600, 1704276000000, 1704358800000, QuestRepeatCycle::ALL}, // Wed 10:00 -> Thu 09:00
		{"UTC", 9601, 1704268800000, 1704272400000, QuestRepeatCycle::WED}, // MON WED FRI, Wed 08:00 -> Wed 09:00 is day 3 -> WED -> +0
		{"UTC", 9601, 1704276000000, 1704445200000, QuestRepeatCycle::FRI}, // Wed 10:00 -> Thu 09:00 is day 4 -> FRI -> Fri 09:00
		{"UTC", 9601, 1704535200000, 1704704400000, QuestRepeatCycle::MON}, // Sat 10:00 -> Sun 09:00 is day 7 -> none at or after -> MON
		{"UTC", 9605, 1704441600000, 1704445200000, QuestRepeatCycle::FRI}, // FRI, Fri 08:00 -> Fri 09:00 is day 5 -> FRI -> +0
		{"Europe/Berlin", 9600, 1704267000000, 1704268800000, QuestRepeatCycle::ALL}, // 07:30Z is 08:30 in Berlin -> 09:00 Berlin = 08:00Z
	};
	for (const Row& row : rows) {
		configs::main::GSConfig::TIME_ZONE_ID.store(std::chrono::locate_zone(row.zone));
		clock.setCurrentTimeMillis(row.now);
		forgetQuests();
		Ref<QuestState> qs = hold(row.questId, QuestStatus::REWARD);
		clearQuesterSent();
		EXPECT_TRUE(QuestService::finishQuest(*envOf(row.questId, model::DialogAction::SELECTED_QUEST_NOREWARD)));
		EXPECT_EQ(qs->getNextRepeatTime(), at(row.next)) << row.zone << " " << row.questId << " at " << row.now;
		EXPECT_EQ(qs->canRepeat(), row.next <= row.now) << "canRepeat waits for the next repeat time";
		std::vector<std::vector<uint8_t>> sent = questerSent();
		const std::vector<uint8_t> message = row.day == QuestRepeatCycle::ALL
			? serializedForQuester(SM_SYSTEM_MESSAGE::STR_MSG_QUEST_LIMIT_START_DAILY(9))
			: serializedForQuester(SM_SYSTEM_MESSAGE::STR_MSG_QUEST_LIMIT_START_WEEK(getL10n(row.day), 9));
		EXPECT_EQ(std::count(sent.begin(), sent.end(), message), 1) << "the repeat message, " << row.questId;
		ASSERT_GE(sent.size(), 2u);
		EXPECT_EQ(sent[sent.size() - 2], questAction(2, row.questId, COMPLETE)) << "the message comes before SM_QUEST_ACTION (:110-112)";
	}
}

// startQuest (QuestService.java:400-444): a quest the player never had is added (SM_QUEST_ACTION ADD), a COMPLETE one that can be repeated is
// added again with its count kept, any other status is an UPDATE; then the nearby quests. startQuest(env) warns only for a dialog action other
// than NULL (:401): a race-restricted quest refuses with STR_QUEST_ACQUIRE_ERROR_RACE then, silently else.
TEST_F(QuestLifecycleTest, StartQuestAddsANewOrRepeatedQuestAndUpdatesAnotherState) {
	clearQuesterSent();
	EXPECT_TRUE(QuestService::startQuest(*envOf(1101, model::DialogAction::QUEST_ACCEPT_1)));
	Ptr<QuestState> started = me().getQuestStateList()->getQuestState(1101);
	ASSERT_TRUE(started);
	EXPECT_EQ(started->getStatus(), QuestStatus::START);
	EXPECT_EQ(started->getPersistentState(), model::gameobjects::Persistable::PersistentState::NEW);
	EXPECT_EQ(questerSent(), cp::exactly({questAction(1, 1101, START), noNearbyQuests()}));

	clock.setCurrentTimeMillis(1704276000000);
	Ref<QuestState> daily = hold(50008, QuestStatus::COMPLETE, 3);
	daily->setNextRepeatTime(commons::database::Timestamp(std::chrono::milliseconds(1704272400000)));
	clearQuesterSent();
	EXPECT_TRUE(QuestService::startQuest(*envOf(50008, model::DialogAction::QUEST_ACCEPT_1)));
	EXPECT_EQ(daily->getStatus(), QuestStatus::START);
	EXPECT_EQ(daily->getCompleteCount(), 3);
	EXPECT_EQ(questerSent(), cp::exactly({questAction(1, 50008, START), noNearbyQuests()})) << "a repeat is an ADD";

	Ref<QuestState> locked = hold(1111, QuestStatus::LOCKED);
	clearQuesterSent();
	EXPECT_TRUE(QuestService::startQuest(*envOf(1111, model::DialogAction::QUEST_ACCEPT_1)));
	EXPECT_EQ(locked->getStatus(), QuestStatus::START);
	EXPECT_EQ(questerSent(), cp::exactly({questAction(2, 1111, START), noNearbyQuests()})) << "LOCKED to START is an UPDATE";

	clearQuesterSent();
	EXPECT_FALSE(QuestService::startQuest(*envOf(2101, model::DialogAction::NULL_)));
	EXPECT_TRUE(questerSent().empty()) << "NULL: no warning";
	EXPECT_FALSE(QuestService::startQuest(*envOf(2101, model::DialogAction::QUEST_ACCEPT_1)));
	EXPECT_EQ(questerSent(), cp::exactly({serializedForQuester(SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_RACE())}));
	EXPECT_FALSE(me().getQuestStateList()->hasQuest(2101));

	clearQuesterSent();
	EXPECT_TRUE(QuestService::startQuest(*envOf(1113, model::DialogAction::QUEST_ACCEPT_1), QuestStatus::REWARD, true));
	EXPECT_EQ(me().getQuestStateList()->getQuestState(1113)->getStatus(), QuestStatus::REWARD) << "the status the caller asks for";
	EXPECT_EQ(questerSent(), cp::exactly({questAction(1, 1113, 4), noNearbyQuests()}));
}

// checkQuestListSize (:552-555) in startQuest (:420-423): the QUEST-category quests in progress plus the new one against BASIC_QUEST_SIZE_LIMIT;
// a no-count quest (EVENT, NON_COUNT) is not limited, nor is a player whose membership reaches QUEST_LIMIT_DISABLED
TEST_F(QuestLifecycleTest, StartQuestRefusesAQuestAboveTheListLimitButNotANoCountQuest) {
	configs::main::CustomConfig::BASIC_QUEST_SIZE_LIMIT.store(1);
	hold(1113, QuestStatus::START);
	clearQuesterSent();
	EXPECT_FALSE(QuestService::startQuest(*envOf(1111, model::DialogAction::QUEST_ACCEPT_1)));
	EXPECT_EQ(questerSent(), cp::exactly({serializedForQuester(SM_SYSTEM_MESSAGE::STR_QUEST_ACQUIRE_ERROR_MAX_NORMAL())}));
	EXPECT_FALSE(QuestService::startQuest(*envOf(1101, model::DialogAction::QUEST_ACCEPT_1)))
		<< "an IMPORTANT quest is not a no-count quest: its start is limited";

	clearQuesterSent();
	EXPECT_TRUE(QuestService::startQuest(*envOf(50008, model::DialogAction::QUEST_ACCEPT_1))) << "EVENT: no count";

	configs::main::CustomConfig::BASIC_QUEST_SIZE_LIMIT.store(2);
	EXPECT_TRUE(QuestService::startQuest(*envOf(1111, model::DialogAction::QUEST_ACCEPT_1))) << "1 in progress + 1 <= 2";

	EXPECT_FALSE(QuestService::startQuest(*envOf(1101, model::DialogAction::QUEST_ACCEPT_1))) << "2 in progress + 1 > 2";
	configs::main::MembershipConfig::QUEST_LIMIT_DISABLED.store(0); // membership 0 reaches it
	EXPECT_TRUE(QuestService::startQuest(*envOf(1101, model::DialogAction::QUEST_ACCEPT_1)));
}

// addOrUpdateQuest (:449-465): ADD for a new state or one that was COMPLETE, UPDATE otherwise, nothing for the same status; COMPLETE resets
// the variables (and counts a completion, setStatus). A new state is made in the status asked for (:454), a COMPLETE one with its completion
// counted (new QuestState(id, COMPLETE), QuestState.java:37-40).
TEST_F(QuestLifecycleTest, AddOrUpdateQuestSendsAddUpdateOrNothing) {
	clearQuesterSent();
	QuestService::addOrUpdateQuest(me(), 1101, QuestStatus::START);
	Ptr<QuestState> qs = me().getQuestStateList()->getQuestState(1101);
	ASSERT_TRUE(qs);
	EXPECT_EQ(questerSent(), cp::exactly({questAction(1, 1101, START)}));

	clearQuesterSent();
	QuestService::addOrUpdateQuest(me(), 1101, QuestStatus::START);
	EXPECT_TRUE(questerSent().empty()) << "the same status";

	qs->setQuestVarById(0, 7);
	QuestService::addOrUpdateQuest(me(), 1101, QuestStatus::COMPLETE);
	EXPECT_EQ(qs->getQuestVars()->getQuestVars(), 0);
	EXPECT_EQ(qs->getCompleteCount(), 1);
	EXPECT_EQ(questerSent(), cp::exactly({questAction(2, 1101, COMPLETE)}));

	clearQuesterSent();
	QuestService::addOrUpdateQuest(me(), 1101, QuestStatus::START);
	EXPECT_EQ(questerSent(), cp::exactly({questAction(1, 1101, START)})) << "from COMPLETE: ADD";

	clearQuesterSent();
	QuestService::addOrUpdateQuest(me(), 1113, QuestStatus::COMPLETE);
	Ptr<QuestState> completed = me().getQuestStateList()->getQuestState(1113);
	ASSERT_TRUE(completed);
	EXPECT_EQ(completed->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(completed->getCompleteCount(), 1);
	EXPECT_EQ(questerSent(), cp::exactly({questAction(1, 1113, COMPLETE)})) << "a new state, in the status asked for";
}

// startEventQuest (:516-546): an EVENT quest for the player's race and level is created in the given status (no packet, :538); an existing
// state is reset (status, variables, reward group); another category, the other race or a level outside the range refuse
TEST_F(QuestLifecycleTest, StartEventQuestStartsOrResetsAnEventQuestOfThePlayersRaceAndLevel) {
	clearQuesterSent();
	EXPECT_TRUE(QuestService::startEventQuest(*envOf(50008, model::DialogAction::NULL_), QuestStatus::START));
	Ptr<QuestState> qs = me().getQuestStateList()->getQuestState(50008);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_TRUE(questerSent().empty());

	qs->setStatus(QuestStatus::COMPLETE);
	qs->setQuestVarById(1, 5);
	qs->setRewardGroup(1);
	EXPECT_TRUE(QuestService::startEventQuest(*envOf(50008, model::DialogAction::NULL_), QuestStatus::REWARD));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(qs->getQuestVars()->getQuestVars(), 0);
	EXPECT_EQ(qs->getRewardGroup(), std::nullopt);

	EXPECT_FALSE(QuestService::startEventQuest(*envOf(51008, model::DialogAction::NULL_), QuestStatus::START)) << "ASMODIANS";
	EXPECT_FALSE(QuestService::startEventQuest(*envOf(9554, model::DialogAction::NULL_), QuestStatus::START)) << "level 10 and up";
	EXPECT_FALSE(QuestService::startEventQuest(*envOf(1101, model::DialogAction::NULL_), QuestStatus::START)) << "not an EVENT";
	EXPECT_FALSE(me().getQuestStateList()->hasQuest(51008) || me().getQuestStateList()->hasQuest(9554) || me().getQuestStateList()->hasQuest(1101));

	forgetQuests();
	EXPECT_TRUE(QuestService::startEventQuest(*envOf(50008, model::DialogAction::NULL_), QuestStatus::REWARD));
	Ptr<QuestState> created = me().getQuestStateList()->getQuestState(50008);
	ASSERT_TRUE(created);
	EXPECT_EQ(created->getStatus(), QuestStatus::REWARD) << "a new state, in the status asked for";
	EXPECT_EQ(created->getCompleteCount(), 0);
}

// startEventQuest's class and gender filters (:529-534), asked after the level (:523-524): quest 9554 (level 10 and up, MALE) and quest 80316
// (levels 25 to 35, class_permitted without SORCERER). Both need a daeva above the quester's level 9, so the players are made outside the world
// (offlinePlayer); the level-25 ones with the first 26 levels of the experience table in place of the fixture's 16.
TEST_F(QuestLifecycleTest, StartEventQuestAsksTheClassAndGenderAfterTheLevel) {
	auto start = [](cp::PlayerFixture& p, int32_t questId) {
		return QuestService::startEventQuest(*QuestEnv::create(nullptr, *p.player, questId), QuestStatus::START);
	};
	cp::PlayerFixture eventer = offlinePlayer(810002, model::PlayerClass::GLADIATOR, model::Gender::FEMALE, 10);
	ASSERT_EQ(eventer.player->getLevel(), 10);
	EXPECT_FALSE(start(eventer, 9554)) << "FEMALE";
	EXPECT_FALSE(eventer.player->getQuestStateList()->hasQuest(9554));
	eventer.commonData->setGender(model::Gender::MALE);
	EXPECT_TRUE(start(eventer, 9554)) << "MALE";
	EXPECT_TRUE(eventer.player->getQuestStateList()->hasQuest(9554));

	dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
	dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
		xml::bindString<dataholders::PlayerExperienceTable>(contexts.emplace_back(), EXPERIENCE_TABLE_26_LEVELS_XML));
	cp::PlayerFixture sorcerer = offlinePlayer(810003, model::PlayerClass::SORCERER, model::Gender::MALE, 25);
	cp::PlayerFixture gladiator = offlinePlayer(810004, model::PlayerClass::GLADIATOR, model::Gender::MALE, 25);
	ASSERT_EQ(sorcerer.player->getLevel(), 25);
	ASSERT_EQ(gladiator.player->getLevel(), 25);
	EXPECT_FALSE(start(sorcerer, 80316)) << "SORCERER";
	EXPECT_FALSE(sorcerer.player->getQuestStateList()->hasQuest(80316));
	EXPECT_TRUE(start(gladiator, 80316)) << "GLADIATOR";
}

// abandonQuest (:849-885): a quest never completed is deleted from the list, one completed before goes back to COMPLETE (count and time
// kept, variables and flags cleared); the quest's work items are taken (only as many as the quest gives), then SM_QUEST_ACTION(ABANDON) and
// the nearby quests. A quest that cannot be given up, one COMPLETE or LOCKED, and one the player does not have are refused.
TEST_F(QuestLifecycleTest, AbandonDeletesANewQuestAndResetsACompletedOneToComplete) {
	Ref<QuestState> fresh = hold(1114, QuestStatus::START);
	holdItem(820002, NYMPHS_DRESS, 2);
	clearQuesterSent();
	EXPECT_TRUE(QuestService::abandonQuest(me(), 1114));
	EXPECT_FALSE(me().getQuestStateList()->hasQuest(1114));
	EXPECT_TRUE(me().getQuestStateList()->getDeletedQuestIds().contains(1114));
	EXPECT_EQ(fresh->getPersistentState(), model::gameobjects::Persistable::PersistentState::DELETED);
	EXPECT_EQ(held(NYMPHS_DRESS), 1) << "the one Nymph's Dress the quest gives (min(1, 2))";
	std::vector<std::vector<uint8_t>> sent = questerSent();
	ASSERT_GE(sent.size(), 2u);
	EXPECT_EQ(sent[sent.size() - 2], questAbandoned(1114));
	EXPECT_EQ(sent.back(), noNearbyQuests());

	clock.setCurrentTimeMillis(5000);
	Ref<QuestState> repeated = hold(9600, QuestStatus::REWARD, 1);
	repeated->setStatus(QuestStatus::COMPLETE); // the second completion, at 5000
	repeated->setStatus(QuestStatus::START);
	repeated->setQuestVarById(0, 5);
	repeated->setFlags(3);
	std::optional<commons::database::Timestamp> completed = repeated->getLastCompleteTime();
	ASSERT_EQ(completed, std::optional<commons::database::Timestamp>(commons::database::Timestamp(std::chrono::milliseconds(5000))));
	clock.setCurrentTimeMillis(9000);
	clearQuesterSent();
	EXPECT_TRUE(QuestService::abandonQuest(me(), 9600));
	EXPECT_EQ(me().getQuestStateList()->getQuestState(9600).get(), repeated.get());
	EXPECT_EQ(repeated->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(repeated->getCompleteCount(), 2);
	EXPECT_EQ(repeated->getLastCompleteTime(), completed) << "setStatus(COMPLETE, false)";
	EXPECT_EQ(repeated->getQuestVars()->getQuestVars(), 0);
	EXPECT_EQ(repeated->getFlags(), 0);
	EXPECT_EQ(questerSent(), cp::exactly({questAbandoned(9600), noNearbyQuests()}));

	clearQuesterSent();
	hold(1000, QuestStatus::START);
	EXPECT_FALSE(QuestService::abandonQuest(me(), 1000)) << "cannot_giveup";
	hold(1101, QuestStatus::COMPLETE, 1);
	EXPECT_FALSE(QuestService::abandonQuest(me(), 1101)) << "COMPLETE";
	hold(1111, QuestStatus::LOCKED);
	EXPECT_FALSE(QuestService::abandonQuest(me(), 1111)) << "LOCKED";
	EXPECT_FALSE(QuestService::abandonQuest(me(), 1113)) << "no state";
	EXPECT_FALSE(QuestService::abandonQuest(me(), 4242)) << "no template";
	EXPECT_TRUE(questerSent().empty());
	EXPECT_TRUE(me().getQuestStateList()->hasQuest(1000));
}

// finishQuest takes the quest's work items (removeQuestWorkItems, :107, 935-947) before COMPLETE: quest 1114's one Nymph's Dress, deleted with
// the delete type of the state's status then (Storage.decreaseByItemId(itemId, count, qs.getStatus()): ItemDeleteType.fromQuestStatus(REWARD)
// is DEFAULT, 0; START would be QUEST_START 0x34 and COMPLETE QUEST_COMPLETE 0x31)
TEST_F(QuestLifecycleTest, FinishQuestTakesTheQuestsWorkItems) {
	hold(1114, QuestStatus::REWARD)->setRewardGroup(1);
	holdItem(820003, NYMPHS_DRESS, 1);
	clearQuesterSent();
	EXPECT_TRUE(QuestService::finishQuest(*envOf(1114, model::DialogAction::SELECTED_QUEST_NOREWARD)));
	EXPECT_EQ(held(NYMPHS_DRESS), 0);
	EXPECT_EQ(kinah(), 1000 + 960 * 3) << "reward group 1";
	EXPECT_EQ(items::packetsOf(questerSent(), items::SM_DELETE_ITEM_OPCODE), cp::exactly({items::deleteItem(820003, 0)}));
}

// The npc faction arms, on quest 35007 of the Alabaster Order (npcfaction_id 2) with the faction active and 35007 its daily quest:
// - startQuest refuses another quest of the faction (35008) before any start condition, with the audit log's packet-hack line (:410-416);
// - abandonQuest aborts the quest in the faction (:869-870): NpcFactions.abortQuest sets NOTING, and sendDailyQuest offers the faction's quest
//   again (SM_QUEST_ACTION(questId): the quest's time is still ahead) before the ABANDON;
// - finishQuest completes it in the faction (:114-115): COMPLETE, with the faction's time moved to the next 09:00 (getNextTime).
TEST_F(QuestLifecycleTest, AnNpcFactionQuestIsGuardedAbortedAndCompletedInItsFaction) {
	using model::gameobjects::player::npcFaction::ENpcFactionQuestState;
	dataholders::DataManager::NPC_FACTIONS_DATA.publish(xml::bindString<dataholders::NpcFactionsData>(contexts.emplace_back(), NPC_FACTIONS_XML));
	me().setNpcFactions(std::make_unique<model::gameobjects::player::npcFaction::NpcFactions>(me()));
	Ptr<model::gameobjects::player::npcFaction::NpcFaction> faction = me().getNpcFactions().setActive(2);
	ASSERT_TRUE(faction);
	faction->setQuestId(35007);
	faction->setTime(std::numeric_limits<int32_t>::max());

	configs::main::LoggingConfig::LOG_AUDIT.store(true);
	{
		network::test::LogCapture audit({"AUDIT_LOG"});
		clearQuesterSent();
		EXPECT_FALSE(QuestService::startQuest(*envOf(35008, model::DialogAction::QUEST_ACCEPT_1)));
		EXPECT_EQ(audit.count("possibly used packet hack to start npc faction quest"), 1) << audit.dump();
		EXPECT_TRUE(questerSent().empty()) << "refused before checkStartConditions, whose level check would warn";
		EXPECT_FALSE(me().getQuestStateList()->hasQuest(35008));
	}

	hold(35007, QuestStatus::START);
	faction->setState(ENpcFactionQuestState::START);
	clearQuesterSent();
	EXPECT_TRUE(QuestService::abandonQuest(me(), 35007));
	EXPECT_EQ(faction->getState(), ENpcFactionQuestState::NOTING);
	EXPECT_EQ(questerSent(), cp::exactly({dailyQuest(35007), questAbandoned(35007), noNearbyQuests()}));

	hold(35007, QuestStatus::REWARD);
	faction->setState(ENpcFactionQuestState::START);
	EXPECT_TRUE(QuestService::finishQuest(*envOf(35007, model::DialogAction::SELECTED_QUEST_NOREWARD)));
	EXPECT_EQ(faction->getState(), ENpcFactionQuestState::COMPLETE);
	EXPECT_LT(faction->getTime(), std::numeric_limits<int32_t>::max()) << "the next 09:00";
	EXPECT_EQ(held(186000100), 1);
}

// The quest timer (:811-825, 841-847): SM_QUEST_ACTION(TIMER) with the seconds, a QUEST_TIMER task on the controller that ends in
// QuestEngine.onQuestTimerEnd after that many seconds; questTimerEnd cancels it and sends the timer 0. The invisible timer (:827-839) sends
// nothing and ends in onInvisibleTimerEnd. abandonQuest ends a running quest timer (:879-880) before its ABANDON.
TEST_F(QuestLifecycleTest, TheQuestTimerEndsInTheEngineUnlessQuestTimerEndCancelsIt) {
	questEngine::QuestEngine::getInstance().addQuestHandler(std::make_unique<RecordingHandler>(1101));
	clearQuesterSent();
	EXPECT_TRUE(QuestService::questTimerStart(*envOf(1101, model::DialogAction::NULL_), 30));
	EXPECT_EQ(questerSent(), cp::exactly({questTimer(1101, 30)}));
	EXPECT_TRUE(me().getController().hasTask(model::TaskId::QUEST_TIMER));
	executor->advance(std::chrono::milliseconds(29'999));
	EXPECT_TRUE(RecordingHandler::timerEnds.empty());
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_EQ(RecordingHandler::timerEnds, std::vector<int32_t>{1101}) << "the engine hands the handler its own quest id";

	clearQuesterSent();
	EXPECT_TRUE(QuestService::questTimerStart(*envOf(1101, model::DialogAction::NULL_), 30));
	EXPECT_TRUE(QuestService::questTimerEnd(*envOf(1101, model::DialogAction::NULL_)));
	EXPECT_FALSE(me().getController().hasTask(model::TaskId::QUEST_TIMER));
	EXPECT_EQ(questerSent(), cp::exactly({questTimer(1101, 30), questTimer(1101, 0)}));
	executor->advance(std::chrono::milliseconds(60'000));
	EXPECT_EQ(RecordingHandler::timerEnds.size(), 1u) << "cancelled";

	clearQuesterSent();
	EXPECT_TRUE(QuestService::invisibleTimerStart(*envOf(1101, model::DialogAction::NULL_), 5));
	EXPECT_TRUE(questerSent().empty());
	executor->advance(std::chrono::milliseconds(5'000));
	EXPECT_EQ(RecordingHandler::invisibleTimerEnds, std::vector<int32_t>{1101});

	hold(1111, QuestStatus::START);
	EXPECT_TRUE(QuestService::questTimerStart(*envOf(1111, model::DialogAction::NULL_), 60));
	clearQuesterSent();
	EXPECT_TRUE(QuestService::abandonQuest(me(), 1111));
	EXPECT_FALSE(me().getController().hasTask(model::TaskId::QUEST_TIMER));
	EXPECT_EQ(questerSent(), cp::exactly({questTimer(1111, 0), questAbandoned(1111), noNearbyQuests()}));
}

// collectItemCheck (:557-600): every <collect_item> count held (kinah from the purse), and with removeItem taken; without <collect_items> the
// <inventory_items> are checked and taken the same way (a row is met by exactly its count); a quest with neither passes; a quest the player
// does not have is refused only when the items are to be taken
TEST_F(QuestLifecycleTest, CollectItemCheckCountsAndTakesTheCollectOrInventoryItems) {
	holdItem(820004, SYLPHEN_WINGS, 2);
	EXPECT_FALSE(QuestService::collectItemCheck(*envOf(1111, model::DialogAction::NULL_), false)) << "2 of 3";
	holdItem(820005, SYLPHEN_WINGS, 1);
	EXPECT_TRUE(QuestService::collectItemCheck(*envOf(1111, model::DialogAction::NULL_), false)) << "3 of 3, no state needed";
	holdItem(820011, SYLPHEN_WINGS, 2);
	EXPECT_FALSE(QuestService::collectItemCheck(*envOf(1111, model::DialogAction::NULL_), true)) << "no state: nothing is taken";
	EXPECT_EQ(held(SYLPHEN_WINGS), 5);
	hold(1111, QuestStatus::START);
	EXPECT_TRUE(QuestService::collectItemCheck(*envOf(1111, model::DialogAction::NULL_), true));
	EXPECT_EQ(held(SYLPHEN_WINGS), 2);

	holdItem(820006, 152000104, 10);
	hold(1692, QuestStatus::START);
	EXPECT_FALSE(QuestService::collectItemCheck(*envOf(1692, model::DialogAction::NULL_), true)) << "1000 of 3146 kinah";
	me().getInventory().increaseKinah(2146);
	EXPECT_TRUE(QuestService::collectItemCheck(*envOf(1692, model::DialogAction::NULL_), true));
	EXPECT_EQ(kinah(), 0) << "the kinah from the purse";
	EXPECT_EQ(held(152000104), 0);

	holdItem(820007, 186000001, 3);
	hold(1209, QuestStatus::START);
	EXPECT_FALSE(QuestService::collectItemCheck(*envOf(1209, model::DialogAction::NULL_), true)) << "3 held: the second row asks 4";
	holdItem(820008, 186000001, 2);
	EXPECT_TRUE(QuestService::collectItemCheck(*envOf(1209, model::DialogAction::NULL_), true))
		<< "5 held: each row alone is met; the answer ignores what decreaseByItemId returns";
	EXPECT_EQ(held(186000001), 0) << "2 taken, then the 3 left of the second row's 4 (decreaseByItemId takes what there is)";

	holdItem(820012, IRON_COIN, 4);
	EXPECT_TRUE(QuestService::collectItemCheck(*envOf(1209, model::DialogAction::NULL_), false))
		<< "exactly the second row's 4: only a count below a row's refuses (:572)";
	EXPECT_EQ(held(IRON_COIN), 4);

	EXPECT_TRUE(QuestService::collectItemCheck(*envOf(1101, model::DialogAction::NULL_), false)) << "neither collect nor inventory items (:567-568)";
	hold(1101, QuestStatus::START);
	EXPECT_TRUE(QuestService::collectItemCheck(*envOf(1101, model::DialogAction::NULL_), true));
}

// checkAndGetCollectItemQuestRewardCategory (:625-664), relic style on quest 50023 (start_check, 1 and 3 Perseverance Scrolls, two groups):
// without an index, the first collect item held starts the quest (0); with an index, that collect item is taken and the index returned,
// else the retry message and -1; an index past the groups is -1 at once; a COMPLETE quest that may repeat starts again; an index past the
// collect items throws (List.get)
TEST_F(QuestLifecycleTest, TheCollectItemRewardCategoryStartsTheQuestOrTakesTheChosenItems) {
	EXPECT_EQ(QuestService::checkAndGetCollectItemQuestRewardCategory(*envOf(50023, model::DialogAction::NULL_)), -1) << "nothing held";
	EXPECT_FALSE(me().getQuestStateList()->hasQuest(50023));

	holdItem(820009, PERSEVERANCE_SCROLL, 1);
	EXPECT_EQ(QuestService::checkAndGetCollectItemQuestRewardCategory(*envOf(50023, model::DialogAction::NULL_)), 0);
	Ptr<QuestState> qs = me().getQuestStateList()->getQuestState(50023);
	ASSERT_TRUE(qs) << "start_check started it";
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(QuestService::checkAndGetCollectItemQuestRewardCategory(*envOf(50023, model::DialogAction::NULL_)), -1) << "already started";
	qs->setStatus(QuestStatus::REWARD);
	EXPECT_EQ(QuestService::checkAndGetCollectItemQuestRewardCategory(*envOf(50023, model::DialogAction::NULL_)), -1) << "in REWARD";

	clearQuesterSent();
	EXPECT_EQ(QuestService::checkAndGetCollectItemQuestRewardCategory(*envOf(50023, model::DialogAction::NULL_), 1), -1) << "1 of 3";
	EXPECT_EQ(questerSent(), cp::exactly({serializedForQuester(SM_SYSTEM_MESSAGE::STR_MSG_QUEST_COMPLETE_ERROR_QUEST_ITEM_RETRY(
								  dataholders::DataManager::ITEM_DATA->getItemTemplate(PERSEVERANCE_SCROLL)->getL10n()))}));
	EXPECT_EQ(held(PERSEVERANCE_SCROLL), 1);
	holdItem(820010, PERSEVERANCE_SCROLL, 3);
	EXPECT_EQ(QuestService::checkAndGetCollectItemQuestRewardCategory(*envOf(50023, model::DialogAction::NULL_), 1), 1);
	EXPECT_EQ(held(PERSEVERANCE_SCROLL), 1) << "the 3 of the second choice taken";
	EXPECT_EQ(QuestService::checkAndGetCollectItemQuestRewardCategory(*envOf(50023, model::DialogAction::NULL_), 0), 0);
	EXPECT_EQ(held(PERSEVERANCE_SCROLL), 0);
	EXPECT_EQ(QuestService::checkAndGetCollectItemQuestRewardCategory(*envOf(50023, model::DialogAction::NULL_), 2), -1) << "2 groups";

	// a COMPLETE state that may repeat (max_repeat_count 255) is startable (:641): the held scroll starts it again, an ADD from COMPLETE
	qs->setStatus(QuestStatus::COMPLETE);
	ASSERT_EQ(qs->getCompleteCount(), 1);
	holdItem(820013, PERSEVERANCE_SCROLL, 1);
	clearQuesterSent();
	EXPECT_EQ(QuestService::checkAndGetCollectItemQuestRewardCategory(*envOf(50023, model::DialogAction::NULL_)), 0);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(qs->getCompleteCount(), 1);
	EXPECT_EQ(questerSent(), cp::exactly({questAction(1, 50023, START), noNearbyQuests()}));

	EXPECT_THROW(static_cast<void>(QuestService::checkAndGetCollectItemQuestRewardCategory(*envOf(1111, model::DialogAction::NULL_), 1)),
		runtime::IndexOutOfBoundsException)
		<< "quest 1111 has two groups but one collect item";
}

} // namespace
} // namespace aion::gameserver::services::test
