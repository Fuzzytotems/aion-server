// P5-06c, M5d T-01b, T-02, T-03 and T-04 (m5d-plan.md §7, §18.3): the xml_quest kind - XmlQuestData.register_ building XmlQuest
// (XmlQuest.java) and the xmlQuest language it runs (handlers/models/xmlQuest/**), driven through QuestEngine on in-world players
// (QuestTemplate1bTestSupport.h):
// - 1127 "Ancient Cube" (poeta.xml:69-111), the only xml_quest of the data: offered and started at baevrunerk; in START with var 0 the ancient
//   cube's USE_OBJECT runs <npc_use> (the use bar for 3000 ms, then the cube item and var 1); with var 1 baevrunerk answers QUEST_SELECT with
//   page 2375 and CHECK_USER_HAS_QUEST_ITEM with <collect_items> (REWARD and page 5 with the item, page 2716 without); in REWARD the template
//   reports it (2400 kinah, 4015 exp).
// - The language's other elements on fabricated fragments bound with the data's own binding (the data uses only quest_status, npc_use,
//   give_item, set_quest_var, npc_dialog, collect_items and set_quest_status): every condition with its operators, AND/OR, take_item,
//   npc_dialog with a quest_id and without a quest, set_quest_status COMPLETE, start_quest, collect_items with removeItems, override, and
//   <on_kill_event> with its <complite>.
// Pages, statuses and vars as `oracle.py m5d-quest --no-profile --quest 1127` prints them (its steps are the language, which the oracle does not
// model: they come from XmlQuest.java and the xmlQuest classes).

#include "QuestTemplate1bTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestConditions.bind.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/conditions/QuestConditions.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnKillEvent.bind.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnKillEvent.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnTalkEvent.bind.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnTalkEvent.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/QuestOperations.bind.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/QuestOperations.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::templates {
namespace {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::Npc;
using models::xmlQuest::conditions::QuestConditions;
using models::xmlQuest::events::OnKillEvent;
using models::xmlQuest::events::OnTalkEvent;
using models::xmlQuest::operations::QuestOperations;

class XmlQuestTemplateTest : public QuestTemplate1bTest {
protected:
	/** A language fragment bound as T with the data's binding (fabricated: see the file comment) */
	template <class T>
	std::unique_ptr<T> bind(std::string_view xml) {
		return xml::bindString<T>(contexts.emplace_back(), xml);
	}
};

// XmlQuestData.register (XmlQuestData.java:39-41) adds an XmlQuest, whose register (XmlQuest.java:47-70) makes the start npc a start and talk
// npc (the end npcs are the start npcs, so not again) and every id of every <on_talk_event> a talk npc
TEST_F(XmlQuestTemplateTest, RegisterMakesTheStartNpcAndTheTalkEventIdsTalkNpcs) {
	registerXml(1127);
	EXPECT_TRUE(QuestEngine::getInstance().isHaveHandler(1127));
	EXPECT_EQ(startQuests(BAEVRUNERK), (std::vector<int32_t>{1127}));
	EXPECT_EQ(talkQuests(BAEVRUNERK), (std::vector<int32_t>{1127}));
	EXPECT_EQ(talkQuests(ANCIENT_CUBE), (std::vector<int32_t>{1127}));
	EXPECT_EQ(startQuests(ANCIENT_CUBE), (std::vector<int32_t>{})) << "an event id starts nothing";
	EXPECT_EQ(killQuests(ANCIENT_CUBE), (std::vector<int32_t>{})) << "1127 has no <on_kill_event>";
}

// 1127 end to end (XmlQuest.java:73-94 and the script): the event runs first and answers the object and the var-1 dialogs; the template
// offers, starts and reports the quest
TEST_F(XmlQuestTemplateTest, Quest1127FromTheCubeNpcThroughTheObjectToTheReward) {
	registerXml(1127);
	Quester* q = makePlayer(810601, "Cubist", gameserver::model::Race::ELYOS, 2);
	holdItem(*q, 820601, items::KINAH, 1000);
	Npc& baevrunerk = npcOf(BAEVRUNERK);
	Npc& cube = npcOf(ANCIENT_CUBE);
	const int32_t atNpc = baevrunerk.getObjectId();
	const int32_t self = q->player().getObjectId();

	// no quest state: the event's <quest_status value="START" op="EQUAL"/> fails (status 0), and the template offers the quest at its start npc
	EXPECT_TRUE(talk(*q, 1127, DialogAction::QUEST_SELECT, baevrunerk));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 1011, 1127)}));
	EXPECT_FALSE(talk(*q, 0, DialogAction::USE_OBJECT, cube)) << "the cube before the quest";
	EXPECT_TRUE(q->sent().empty());
	executor->advance(std::chrono::milliseconds(3000));
	EXPECT_TRUE(q->sent().empty()) << "no use bar was started";

	EXPECT_TRUE(talk(*q, 1127, DialogAction::QUEST_ACCEPT_1, baevrunerk));
	Ptr<QuestState> qs = q->player().getQuestStateList()->getQuestState(1127);
	ASSERT_TRUE(qs);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(q->sent(), cp::exactly({questAction(1, 1127, START), noNearbyQuests(), dialogWindow(atNpc, 1003, 1127)}));
	EXPECT_EQ(held(*q, ANCIENT_CUBE_ITEM), 0) << "the template starts it without its work item";

	// START, var 0: baevrunerk's dialogs belong to var 1, and the template has nothing for START
	EXPECT_FALSE(talk(*q, 1127, DialogAction::QUEST_SELECT, baevrunerk));
	EXPECT_TRUE(q->sent().empty());

	// the cube: <npc_use> starts the use bar and schedules <finish>
	EXPECT_TRUE(talk(*q, 0, DialogAction::USE_OBJECT, cube));
	EXPECT_EQ(q->sent(), cp::exactly({useObject(self, cube.getObjectId(), 3000, 1),
							 q->serializedFor(network::aion::serverpackets::SM_EMOTION(q->player(), gameserver::model::EmotionType::START_QUESTLOOT, 0,
								 cube.getObjectId()))}));
	EXPECT_EQ(qs->getQuestVarById(0), 0);
	q->clearSent();
	executor->advance(std::chrono::milliseconds(2999));
	EXPECT_TRUE(q->sent().empty());
	EXPECT_EQ(held(*q, ANCIENT_CUBE_ITEM), 0);
	executor->advance(std::chrono::milliseconds(1));
	std::vector<std::vector<uint8_t>> sent = q->sent();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.front(), useObject(self, cube.getObjectId(), 3000, 0)) << "the bar ends first";
	EXPECT_EQ(held(*q, ANCIENT_CUBE_ITEM), 1) << "<give_item>";
	EXPECT_EQ(qs->getQuestVarById(0), 1) << "<set_quest_var var_id=\"0\" value=\"1\">";
	EXPECT_EQ(sent.back(), questUpdate(1127, START, 1));
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);

	// var 1: the cube's dialog belongs to var 0 now
	EXPECT_FALSE(talk(*q, 0, DialogAction::USE_OBJECT, cube));
	EXPECT_TRUE(q->sent().empty());
	executor->advance(std::chrono::milliseconds(3000));
	EXPECT_TRUE(q->sent().empty());
	EXPECT_EQ(held(*q, ANCIENT_CUBE_ITEM), 1);

	// var 1 at baevrunerk: QUEST_SELECT (31) shows 2375; USE_OBJECT has no <dialog>
	EXPECT_TRUE(talk(*q, 1127, DialogAction::QUEST_SELECT, baevrunerk));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 2375, 1127)}));
	EXPECT_FALSE(talk(*q, 1127, DialogAction::USE_OBJECT, baevrunerk));
	EXPECT_TRUE(q->sent().empty());

	// CHECK_USER_HAS_QUEST_ITEM (39): <collect_items> without the cube item runs <false>
	q->player().getInventory().decreaseByItemId(ANCIENT_CUBE_ITEM, 1);
	EXPECT_TRUE(talk(*q, 1127, DialogAction::CHECK_USER_HAS_QUEST_ITEM, baevrunerk));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 2716, 1127)}));
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);

	// ... and with it <true>: the item taken (no removeItems attribute removes), REWARD, page 5
	holdItem(*q, 820602, ANCIENT_CUBE_ITEM, 1);
	EXPECT_TRUE(talk(*q, 1127, DialogAction::CHECK_USER_HAS_QUEST_ITEM, baevrunerk));
	EXPECT_EQ(held(*q, ANCIENT_CUBE_ITEM), 0);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(sentOf(*q, SM_QUEST_ACTION_OPCODE), cp::exactly({questUpdate(1127, REWARD, 1)}));
	sent = q->sent();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.back(), dialogWindow(atNpc, 5, 1127));

	// REWARD: the event's condition fails again (status 4), the template shows the reward page and finishes the quest
	EXPECT_FALSE(talk(*q, 0, DialogAction::USE_OBJECT, cube));
	EXPECT_TRUE(q->sent().empty());
	EXPECT_TRUE(talk(*q, 1127, DialogAction::QUEST_SELECT, baevrunerk));
	EXPECT_EQ(q->sent(), cp::exactly({dialogWindow(atNpc, 5, 1127)}));
	EXPECT_TRUE(talk(*q, 1127, DialogAction::SELECTED_QUEST_NOREWARD, baevrunerk));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(q->player().getInventory().getKinah(), 1000 + 2400);
	EXPECT_EQ(q->f.commonData->getExp(), 400 + 4015);
	sent = q->sent();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.back(), dialogWindow(atNpc, 0, 0)) << "nothing else to offer at baevrunerk: the window closes";
}

// QuestConditions.checkConditionOfSet (QuestConditions.java:29-47): AND stops at the first false, OR at the first true; the conditions
// (DialogIdCondition, NpcIdCondition, PcInventoryCondition, QuestStatusCondition, QuestVarCondition .doCheck) with their operators
TEST_F(XmlQuestTemplateTest, TheConditionsWithTheirOperatorsAndTheUnionOfASet) {
	Npc& elpas = npcOf(ELPAS);
	holdItem(*me, 820603, GRAIN_SACK_1106, 2);
	hold(*me, 1101, QuestStatus::START, 5 + (7 << 6));
	hold(*me, 1102, QuestStatus::REWARD);
	auto check = [&](std::string_view conditionsXml, int32_t questId, int32_t dialogActionId,
					 Ptr<gameserver::model::gameobjects::VisibleObject> target) {
		return bind<QuestConditions>(conditionsXml)->checkConditionOfSet(*envOf(*me, questId, dialogActionId, target));
	};

	// dialog_id: EQUAL and NOT_EQUAL only; any other operator is false
	EXPECT_TRUE(check(R"(<conditions operate="AND"><dialog_id value="31" op="EQUAL"/></conditions>)", 1101, 31, nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><dialog_id value="31" op="EQUAL"/></conditions>)", 1101, 30, nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><dialog_id value="31" op="NOT_EQUAL"/></conditions>)", 1101, 30, nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><dialog_id value="31" op="GREATER_EQUAL"/></conditions>)", 1101, 31, nullptr));

	// npc_id: the target's npc id, 0 without an npc
	EXPECT_TRUE(check(R"(<conditions operate="AND"><npc_id values="203049" op="EQUAL"/></conditions>)", 1101, 0, at(elpas)));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><npc_id values="203049" op="EQUAL"/></conditions>)", 1101, 0, nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><npc_id values="203048" op="GREATER"/></conditions>)", 1101, 0, at(elpas)));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><npc_id values="203049" op="GREATER"/></conditions>)", 1101, 0, at(elpas)));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><npc_id values="203049" op="GREATER_EQUAL"/></conditions>)", 1101, 0, at(elpas)));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><npc_id values="203050" op="LESSER"/></conditions>)", 1101, 0, at(elpas)));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><npc_id values="203049" op="LESSER"/></conditions>)", 1101, 0, at(elpas)));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><npc_id values="203049" op="LESSER_EQUAL"/></conditions>)", 1101, 0, at(elpas)));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><npc_id values="1" op="NOT_EQUAL"/></conditions>)", 1101, 0, at(elpas)));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><npc_id values="203049" op="IN"/></conditions>)", 1101, 0, at(elpas)));

	// pc_inventory: the count of the item in the cube
	EXPECT_TRUE(check(R"(<conditions operate="AND"><pc_inventory item_id="182200203" count="2" op="EQUAL"/></conditions>)", 1101, 0, nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><pc_inventory item_id="182200203" count="1" op="GREATER"/></conditions>)", 1101, 0, nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><pc_inventory item_id="182200203" count="2" op="GREATER"/></conditions>)", 1101, 0, nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><pc_inventory item_id="182200203" count="2" op="GREATER_EQUAL"/></conditions>)", 1101, 0,
		nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><pc_inventory item_id="182200203" count="3" op="LESSER"/></conditions>)", 1101, 0, nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><pc_inventory item_id="182200203" count="2" op="LESSER"/></conditions>)", 1101, 0, nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><pc_inventory item_id="182200203" count="1" op="LESSER_EQUAL"/></conditions>)", 1101, 0,
		nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><pc_inventory item_id="182200203" count="2" op="LESSER_EQUAL"/></conditions>)", 1101, 0,
		nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><pc_inventory item_id="182200224" count="0" op="EQUAL"/></conditions>)", 1101, 0, nullptr))
		<< "an item not held counts 0";
	EXPECT_TRUE(check(R"(<conditions operate="AND"><pc_inventory item_id="182200203" count="0" op="NOT_EQUAL"/></conditions>)", 1101, 0,
		nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><pc_inventory item_id="182200203" count="2" op="NOT_IN"/></conditions>)", 1101, 0, nullptr));

	// quest_status: the value of the env's quest's status (START 3, REWARD 4, COMPLETE 5, LOCKED 6), 0 without a state; quest_id overrides
	EXPECT_TRUE(check(R"(<conditions operate="AND"><quest_status value="START" op="EQUAL"/></conditions>)", 1101, 0, nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><quest_status value="START" op="EQUAL"/></conditions>)", 1102, 0, nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><quest_status value="START" op="GREATER"/></conditions>)", 1102, 0, nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><quest_status value="START" op="GREATER"/></conditions>)", 1101, 0, nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><quest_status value="START" op="LESSER"/></conditions>)", 1101, 0, nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><quest_status value="REWARD" op="GREATER_EQUAL"/></conditions>)", 1102, 0, nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><quest_status value="COMPLETE" op="GREATER_EQUAL"/></conditions>)", 1102, 0, nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><quest_status value="START" op="LESSER"/></conditions>)", 1103, 0, nullptr))
		<< "no state is 0";
	EXPECT_TRUE(check(R"(<conditions operate="AND"><quest_status value="START" op="LESSER_EQUAL"/></conditions>)", 1101, 0, nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><quest_status value="START" op="LESSER_EQUAL"/></conditions>)", 1102, 0, nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><quest_status value="LOCKED" op="NOT_EQUAL"/></conditions>)", 1101, 0, nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><quest_status value="REWARD" op="EQUAL" quest_id="1102"/></conditions>)", 1101, 0, nullptr))
		<< "quest_id names another quest than the env's";
	EXPECT_FALSE(check(R"(<conditions operate="AND"><quest_status value="START" op="IN"/></conditions>)", 1101, 0, nullptr));

	// quest_var: the var of the env's quest by id, false without a state
	EXPECT_TRUE(check(R"(<conditions operate="AND"><quest_var var_id="1" value="7" op="EQUAL"/></conditions>)", 1101, 0, nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><quest_var var_id="0" value="7" op="EQUAL"/></conditions>)", 1101, 0, nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><quest_var var_id="0" value="4" op="GREATER"/></conditions>)", 1101, 0, nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><quest_var var_id="0" value="5" op="GREATER"/></conditions>)", 1101, 0, nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><quest_var var_id="0" value="5" op="GREATER_EQUAL"/></conditions>)", 1101, 0, nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><quest_var var_id="0" value="6" op="LESSER"/></conditions>)", 1101, 0, nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><quest_var var_id="0" value="4" op="LESSER_EQUAL"/></conditions>)", 1101, 0, nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><quest_var var_id="0" value="5" op="LESSER_EQUAL"/></conditions>)", 1101, 0, nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><quest_var var_id="0" value="5" op="LESSER"/></conditions>)", 1101, 0, nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><quest_var var_id="0" value="6" op="NOT_EQUAL"/></conditions>)", 1101, 0, nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><quest_var var_id="0" value="5" op="NOT_IN"/></conditions>)", 1101, 0, nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><quest_var var_id="0" value="0" op="EQUAL"/></conditions>)", 1103, 0, nullptr))
		<< "no quest state";

	// the union: AND of a true and a false is false in either order, OR of them is true in either order
	EXPECT_FALSE(check(R"(<conditions operate="AND"><dialog_id value="31" op="EQUAL"/><dialog_id value="30" op="EQUAL"/></conditions>)", 1101, 31,
		nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="AND"><dialog_id value="30" op="EQUAL"/><dialog_id value="31" op="EQUAL"/></conditions>)", 1101, 31,
		nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="AND"><dialog_id value="31" op="EQUAL"/><quest_var var_id="0" value="5" op="EQUAL"/></conditions>)",
		1101, 31, nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="OR"><dialog_id value="31" op="EQUAL"/><dialog_id value="30" op="EQUAL"/></conditions>)", 1101, 31,
		nullptr));
	EXPECT_TRUE(check(R"(<conditions operate="OR"><dialog_id value="30" op="EQUAL"/><dialog_id value="31" op="EQUAL"/></conditions>)", 1101, 31,
		nullptr));
	EXPECT_FALSE(check(R"(<conditions operate="OR"><dialog_id value="30" op="EQUAL"/><dialog_id value="29" op="EQUAL"/></conditions>)", 1101, 31,
		nullptr));
}

// The operations (QuestOperations.java:40-47 and the doOperate of each): run in order, answering `override` (true when absent); take_item,
// give_item, npc_dialog with and without a quest, set_quest_var and set_quest_status on the env's quest (nothing without a state), COMPLETE
// updating the nearby quests, start_quest doing nothing, collect_items keeping the items when removeItems is given (any value)
TEST_F(XmlQuestTemplateTest, TheOperationsInOrderWithTheOverrideAnswer) {
	Npc& elpas = npcOf(ELPAS);
	const int32_t atElpas = elpas.getObjectId();
	holdItem(*me, 820604, GRAIN_SACK_1106, 3);
	Ref<QuestState> qs = hold(*me, 1101, QuestStatus::START);

	auto run = [&](std::string_view operationsXml, int32_t questId) {
		me->clearSent();
		return bind<QuestOperations>(operationsXml)->operate(*envOf(*me, questId, DialogAction::QUEST_SELECT, at(elpas)));
	};

	EXPECT_TRUE(run(R"(<operations><take_item item_id="182200203" count="2"/></operations>)", 1101));
	EXPECT_EQ(held(*me, GRAIN_SACK_1106), 1);
	EXPECT_TRUE(run(R"(<operations><give_item item_id="182200203" count="4"/></operations>)", 1101));
	EXPECT_EQ(held(*me, GRAIN_SACK_1106), 5);

	EXPECT_FALSE(run(R"(<operations override="false"><npc_dialog id="1011"/><npc_dialog id="1352" quest_id="1102"/></operations>)", 1101))
		<< "override=\"false\"";
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atElpas, 1011, 1101), dialogWindow(atElpas, 1352, 1102)})) << "in order; quest_id wins";
	EXPECT_TRUE(run(R"(<operations><npc_dialog id="10"/></operations>)", 0));
	EXPECT_EQ(me->sent(), cp::exactly({me->serializedFor(network::aion::serverpackets::SM_DIALOG_WINDOW(atElpas, 10))}))
		<< "no quest: the two-argument window";

	EXPECT_TRUE(run(R"(<operations><set_quest_var var_id="1" value="9"/></operations>)", 1101));
	EXPECT_EQ(qs->getQuestVarById(1), 9);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1101, START, 9 << 6)}));
	EXPECT_TRUE(run(R"(<operations><set_quest_var var_id="0" value="3"/><set_quest_status status="REWARD"/></operations>)", 1102));
	EXPECT_TRUE(me->sent().empty()) << "1102 has no state";

	EXPECT_TRUE(run(R"(<operations><set_quest_status status="REWARD"/></operations>)", 1101));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1101, REWARD, 9 << 6)})) << "no nearby update before COMPLETE";
	EXPECT_TRUE(run(R"(<operations><set_quest_status status="COMPLETE"/></operations>)", 1101));
	EXPECT_EQ(qs->getStatus(), QuestStatus::COMPLETE);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1101, COMPLETE, 9 << 6), noNearbyQuests()}));

	EXPECT_TRUE(run(R"(<operations><start_quest id="1102"/></operations>)", 1101));
	EXPECT_TRUE(me->sent().empty());
	EXPECT_FALSE(me->player().getQuestStateList()->hasQuest(1102)) << "<start_quest> does nothing (StartQuestOperation.java:22-24)";

	// collect_items: 1127 wants its ancient cube item, which the player lacks at first
	hold(*me, 1127, QuestStatus::START);
	EXPECT_TRUE(run(R"(<operations><collect_items><true><npc_dialog id="5"/></true><false><npc_dialog id="6"/></false></collect_items></operations>)",
		1127));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atElpas, 6, 1127)}));
	holdItem(*me, 820605, ANCIENT_CUBE_ITEM, 1);
	EXPECT_TRUE(run(R"(<operations><collect_items removeItems="true"><true><npc_dialog id="7"/></true><false/></collect_items></operations>)",
		1127));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atElpas, 7, 1127)}));
	EXPECT_EQ(held(*me, ANCIENT_CUBE_ITEM), 1) << "removeItems given: the item stays (removeItems == null ? true : false)";
	EXPECT_TRUE(run(R"(<operations><collect_items><true><npc_dialog id="8"/></true><false/></collect_items></operations>)", 1127));
	EXPECT_EQ(held(*me, ANCIENT_CUBE_ITEM), 0) << "removeItems absent: the item is taken";
	std::vector<std::vector<uint8_t>> sent = me->sent();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.back(), dialogWindow(atElpas, 8, 1127));
}

// OnTalkEvent.operate (OnTalkEvent.java:25-35) through QuestVar, QuestNpc and QuestDialog (QuestVar.java:27-38, QuestNpc.java:28-39,
// QuestDialog.java:30-39): the event's conditions, then the var of the quest's vars (-1 without a state), the npc of the target (-1 without an
// npc), the dialog of the action; a dialog without operations or failing its own conditions answers false and the search goes on. The var -1
// npc has a USE_OBJECT (-1) dialog, which only the event's condition keeps out
TEST_F(XmlQuestTemplateTest, TheTalkEventMatchesVarNpcAndDialogInTurn) {
	Npc& elpas = npcOf(ELPAS);
	Npc& mires = npcOf(MIRES);
	const int32_t atElpas = elpas.getObjectId();
	std::unique_ptr<OnTalkEvent> event = bind<OnTalkEvent>(
		R"(<on_talk_event ids="203049"><conditions operate="AND"><dialog_id value="-1" op="NOT_EQUAL"/></conditions>)"
		R"(<var value="-1"><npc id="203049"><dialog id="31"><operations><npc_dialog id="1"/></operations></dialog>)"
		R"(<dialog id="-1"><operations><npc_dialog id="9"/></operations></dialog></npc></var>)"
		R"(<var value="2"><npc id="203057"><dialog id="31"><operations><npc_dialog id="2"/></operations></dialog></npc>)"
		R"(<npc id="203049"><dialog id="31"/><dialog id="31"><conditions operate="AND"><quest_var var_id="0" value="1" op="EQUAL"/></conditions>)"
		R"(<operations><npc_dialog id="3"/></operations></dialog><dialog id="31"><operations><npc_dialog id="4"/></operations></dialog>)"
		R"(<dialog id="1002"><operations><npc_dialog id="5"/></operations></dialog></npc></var></on_talk_event>)");
	auto talkTo = [&](Npc* npc, int32_t dialogActionId) {
		me->clearSent();
		return event->operate(*envOf(*me, 1101, dialogActionId, npc ? at(*npc) : nullptr));
	};

	EXPECT_TRUE(talkTo(&elpas, DialogAction::QUEST_SELECT)) << "no state: var -1";
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atElpas, 1, 1101)}));
	EXPECT_FALSE(talkTo(&elpas, DialogAction::USE_OBJECT)) << "the event's condition";
	EXPECT_TRUE(me->sent().empty());

	Ref<QuestState> qs = hold(*me, 1101, QuestStatus::START, 2);
	EXPECT_TRUE(talkTo(&elpas, DialogAction::QUEST_SELECT)) << "var 2: the empty dialog and the failing one are passed over";
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atElpas, 4, 1101)}));
	EXPECT_TRUE(talkTo(&mires, DialogAction::QUEST_SELECT));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(mires.getObjectId(), 2, 1101)}));
	EXPECT_TRUE(talkTo(&elpas, DialogAction::QUEST_ACCEPT_1));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(atElpas, 5, 1101)}));
	EXPECT_FALSE(talkTo(&elpas, DialogAction::QUEST_ACCEPT)) << "no dialog of that id";
	EXPECT_FALSE(talkTo(nullptr, DialogAction::QUEST_SELECT)) << "no npc: npc id -1";
	EXPECT_TRUE(me->sent().empty());

	qs->setQuestVar(3);
	EXPECT_FALSE(talkTo(&elpas, DialogAction::QUEST_SELECT)) << "no var 3";
	EXPECT_TRUE(me->sent().empty());
	qs->setQuestVar(2 + (1 << 6));
	EXPECT_FALSE(talkTo(&elpas, DialogAction::QUEST_SELECT)) << "<var value> is all the vars (66), not var 0 (QuestVars.getQuestVars)";
	EXPECT_TRUE(me->sent().empty());
}

// OnKillEvent.operate (OnKillEvent.java:46-68): each monster of the killed npc counts its var from start_var up to end_var and sends the update;
// once every monster's var is at its end_var the <complite> operations run; it answers false in every case, and does nothing without a
// state or for a target that is no npc
TEST_F(XmlQuestTemplateTest, TheKillEventCountsItsMonstersAndRunsCompliteWhenAllAreDone) {
	Npc& kerub = npcOf(STRIPED_KERUB);
	Npc& elpas = npcOf(ELPAS);
	std::unique_ptr<OnKillEvent> event = bind<OnKillEvent>(
		R"(<on_kill_event><monster var="0" start_var="1" end_var="3" npc_ids="210133 210134"/><monster var="1" end_var="1" npc_ids="210133"/>)"
		R"(<complite><set_quest_status status="REWARD"/></complite></on_kill_event>)");
	auto killOf = [&](Npc& npc) {
		me->clearSent();
		return event->operate(*QuestEnv::create(at(npc), me->player(), 1102));
	};

	EXPECT_FALSE(killOf(kerub)) << "no quest state";
	Ref<QuestState> qs = hold(*me, 1102, QuestStatus::START);
	EXPECT_FALSE(killOf(kerub));
	EXPECT_EQ(qs->getQuestVarById(0), 0) << "var 0 is below its start_var";
	EXPECT_EQ(qs->getQuestVarById(1), 1);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1102, START, 1 << 6)}));

	qs->setQuestVarById(0, 1);
	EXPECT_FALSE(killOf(elpas)) << "not a monster of the event";
	EXPECT_TRUE(me->sent().empty());
	EXPECT_FALSE(killOf(kerub));
	EXPECT_EQ(qs->getQuestVarById(0), 2);
	EXPECT_EQ(qs->getQuestVarById(1), 1) << "var 1 is at its end_var";
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1102, START, 2 + (1 << 6))}));

	EXPECT_FALSE(killOf(kerub));
	EXPECT_EQ(qs->getQuestVarById(0), 3);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD) << "every monster done: <complite>";
	EXPECT_EQ(me->sent(), cp::exactly({questUpdate(1102, START, 3 + (1 << 6)), questUpdate(1102, REWARD, 3 + (1 << 6))}));

	me->clearSent();
	EXPECT_FALSE(event->operate(*QuestEnv::create(nullptr, me->player(), 1102))) << "no npc";
	EXPECT_TRUE(me->sent().empty());
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test::templates
