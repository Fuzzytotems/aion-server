// P5-06b, M5d H-03 (m5d-plan.md §7): the item helpers of AbstractQuestHandler (AbstractQuestHandler.java:531-659, 866-977) - giving and
// taking quest items, the collect and existence checks, the use of a quest object and of a quest item (the use bar: SM_ITEM_USAGE_ANIMATION,
// then after 3 s the item taken, the reward item, the movie and the step).
//
// The items are the shipped quest items of the Poeta quests (QuestHandlerTestSupport.h): 1111 collects 3 Sylphen Wings (182200223), 1114's
// work item is the Nymph's Dress (182200217), 1103's quest object drops the Kerub Grain Sack (182200201). An item packet whose body is the
// item blob is compared against the server's own serialization with the add or update type Java passes (ItemPacketTestSupport.h); a deletion
// is SM_DELETE_ITEM with the ItemDeleteType mask the quest status selects (ItemPacketService.java:114-152: START 0x34, COMPLETE 0x31, other
// statuses 0, no status DEC_ITEM_USE's 0x17).

#include "QuestHandlerTestSupport.h"

#include <chrono>
#include <cstdint>
#include <vector>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemAddType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test {
namespace {

namespace DialogAction = gameserver::model::DialogAction;
using gameserver::model::gameobjects::Item;
using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::VisibleObject;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::item::ItemPacketService_ItemAddType;
using services::item::ItemPacketService_ItemUpdateType;

Ptr<VisibleObject> at(VisibleObject& object) {
	return Ptr<VisibleObject>(object);
}

class AbstractQuestHandlerItemTest : public QuestHandlerTest {
protected:
	/** SM_INVENTORY_ADD_ITEM of the player's first cube item of `itemId`, as ItemService.addItem adds a new row */
	std::vector<uint8_t> added(int32_t itemId, ItemPacketService_ItemAddType addType) {
		Ptr<Item> item = player().getInventory().getFirstItemByItemId(itemId);
		if (!item)
			return {};
		return me->serializedFor(network::aion::serverpackets::SM_INVENTORY_ADD_ITEM({item}, player(), addType));
	}

	/** SM_INVENTORY_UPDATE_ITEM of a stack as it is now */
	std::vector<uint8_t> updated(Item& item, ItemPacketService_ItemUpdateType updateType) {
		return me->serializedFor(network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM(player(), item, updateType));
	}

	std::vector<uint8_t> loreItem(int32_t itemId) {
		return me->serializedFor(SM_SYSTEM_MESSAGE::STR_CAN_NOT_GET_LORE_ITEM(dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId)->getL10n()));
	}

	static int32_t vars(const Ref<QuestState>& qs) { return qs->getQuestVars()->getQuestVars(); }
};

// giveQuestItem (AbstractQuestHandler.java:618-641): only the count the player lacks is added - as a QUEST_WORK_ITEM row updated with
// INC_ITEM_COLLECT unless the caller names others - and a player who holds enough gets STR_CAN_NOT_GET_LORE_ITEM; both answer true. An id or
// a count of 0 gives nothing and answers false.
TEST_F(AbstractQuestHandlerItemTest, GiveQuestItemAddsOnlyTheMissingCount) {
	PlainHandler handler(1101);
	Ref<QuestEnv> env = envOf(*me, 1101, DialogAction::NULL_);
	EXPECT_TRUE(handler.giveQuestItem(*env, NYMPHS_DRESS, 1));
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 1);
	EXPECT_EQ(items::packetsOf(me->sent(), items::SM_INVENTORY_ADD_ITEM_OPCODE),
		cp::exactly({added(NYMPHS_DRESS, ItemPacketService_ItemAddType::QUEST_WORK_ITEM)}));

	Item& wings = holdItem(*me, 820041, SYLPHEN_WINGS, 1);
	me->clearSent();
	EXPECT_TRUE(handler.giveQuestItem(*env, SYLPHEN_WINGS, 3));
	EXPECT_EQ(held(*me, SYLPHEN_WINGS), 3) << "2 added to the 1 held";
	EXPECT_EQ(items::packetsOf(me->sent(), items::SM_INVENTORY_UPDATE_ITEM_OPCODE),
		cp::exactly({updated(wings, ItemPacketService_ItemUpdateType::INC_ITEM_COLLECT)}));

	for (int64_t count : {int64_t{3}, int64_t{2}}) {
		me->clearSent();
		EXPECT_TRUE(handler.giveQuestItem(*env, SYLPHEN_WINGS, count)) << count;
		EXPECT_EQ(me->sent(), cp::exactly({loreItem(SYLPHEN_WINGS)})) << count;
		EXPECT_EQ(held(*me, SYLPHEN_WINGS), 3);
	}

	me->clearSent();
	EXPECT_FALSE(handler.giveQuestItem(*env, 0, 1));
	EXPECT_FALSE(handler.giveQuestItem(*env, NYMPHS_DRESS, 0));
	EXPECT_TRUE(me->sent().empty());

	EXPECT_TRUE(handler.giveQuestItem(*env, KERUB_GRAIN_SACK, 2, ItemPacketService_ItemAddType::ITEM_COLLECT));
	EXPECT_EQ(items::packetsOf(me->sent(), items::SM_INVENTORY_ADD_ITEM_OPCODE),
		cp::exactly({added(KERUB_GRAIN_SACK, ItemPacketService_ItemAddType::ITEM_COLLECT)}));
	me->clearSent();
	EXPECT_TRUE(handler.giveQuestItem(*env, KERUB_GRAIN_SACK, 5, ItemPacketService_ItemAddType::QUEST_WORK_ITEM,
		ItemPacketService_ItemUpdateType::INC_ITEM_MERGE));
	EXPECT_EQ(held(*me, KERUB_GRAIN_SACK), 5);
	EXPECT_EQ(items::packetsOf(me->sent(), items::SM_INVENTORY_UPDATE_ITEM_OPCODE),
		cp::exactly({updated(*player().getInventory().getFirstItemByItemId(KERUB_GRAIN_SACK), ItemPacketService_ItemUpdateType::INC_ITEM_MERGE)}));
}

// removeQuestItem (AbstractQuestHandler.java:644-659): the three-argument form takes a positive count with the status of the handler's quest
// (START without one); the four-argument form any count but 0 with the status given. Not enough items takes what there is and answers false.
TEST_F(AbstractQuestHandlerItemTest, RemoveQuestItemDeletesWithTheQuestsStatus) {
	PlainHandler handler(1101);
	Ref<QuestEnv> env = envOf(*me, 1101, DialogAction::NULL_);
	holdItem(*me, 820051, SYLPHEN_WINGS, 3);
	EXPECT_FALSE(handler.removeQuestItem(*env, SYLPHEN_WINGS, 0));
	EXPECT_FALSE(handler.removeQuestItem(*env, SYLPHEN_WINGS, -1));
	EXPECT_FALSE(handler.removeQuestItem(*env, 0, 3));
	EXPECT_EQ(held(*me, SYLPHEN_WINGS), 3);
	EXPECT_TRUE(me->sent().empty());
	EXPECT_TRUE(handler.removeQuestItem(*env, SYLPHEN_WINGS, 3)) << "no quest state: START";
	EXPECT_EQ(items::packetsOf(me->sent(), items::SM_DELETE_ITEM_OPCODE), cp::exactly({items::deleteItem(820051, 0x34)}));

	hold(*me, 1101, QuestStatus::COMPLETE);
	holdItem(*me, 820052, SYLPHEN_WINGS, 3);
	me->clearSent();
	EXPECT_TRUE(handler.removeQuestItem(*env, SYLPHEN_WINGS, 3));
	EXPECT_EQ(items::packetsOf(me->sent(), items::SM_DELETE_ITEM_OPCODE), cp::exactly({items::deleteItem(820052, 0x31)})) << "COMPLETE";

	holdItem(*me, 820053, SYLPHEN_WINGS, 2);
	me->clearSent();
	EXPECT_TRUE(handler.removeQuestItem(*env, SYLPHEN_WINGS, 2, QuestStatus::REWARD));
	EXPECT_EQ(items::packetsOf(me->sent(), items::SM_DELETE_ITEM_OPCODE), cp::exactly({items::deleteItem(820053, 0)})) << "the status given";
	EXPECT_FALSE(handler.removeQuestItem(*env, 0, 2, QuestStatus::START));
	EXPECT_FALSE(handler.removeQuestItem(*env, SYLPHEN_WINGS, 0, QuestStatus::START));

	holdItem(*me, 820054, SYLPHEN_WINGS, 2);
	EXPECT_FALSE(handler.removeQuestItem(*env, SYLPHEN_WINGS, 3)) << "only 2 held";
	EXPECT_EQ(held(*me, SYLPHEN_WINGS), 0);
}

// checkQuestItems and checkQuestItemsSimple (AbstractQuestHandler.java:531-573): at the step, the collect items of quest_data (collectItemCheck
// takes them) move the step on - with the reward flag and an item given first - and show the ok page; without them the fail page, or for the
// simple form the closed window. Another step answers false; no quest state is Java's NullPointerException.
TEST_F(AbstractQuestHandlerItemTest, CheckQuestItemsTakesTheCollectItemsAndMovesTheStep) {
	PlainHandler handler(1111);
	Npc& mires = npcOf(MIRES);
	const int32_t npc = mires.getObjectId();
	Ref<QuestEnv> env = envOf(*me, 1111, DialogAction::CHECK_USER_HAS_QUEST_ITEM, at(mires));
	EXPECT_THROW(handler.checkQuestItems(*env, 0, 1, true, 10000, 10001), runtime::NullPointerException);
	Ref<QuestState> qs = hold(*me, 1111, QuestStatus::START);
	holdItem(*me, 820061, SYLPHEN_WINGS, 2);
	EXPECT_TRUE(handler.checkQuestItems(*env, 0, 1, true, 10000, 10001));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 10001, 1111)})) << "2 of 3";
	EXPECT_EQ(held(*me, SYLPHEN_WINGS), 2);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);

	holdItem(*me, 820062, SYLPHEN_WINGS, 1);
	me->clearSent();
	EXPECT_TRUE(handler.checkQuestItems(*env, 0, 1, true, 10000, 10001));
	EXPECT_EQ(held(*me, SYLPHEN_WINGS), 0);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(vars(qs), 1);
	std::vector<std::vector<uint8_t>> sent = me->sent();
	ASSERT_GE(sent.size(), 3u);
	EXPECT_EQ(std::vector<std::vector<uint8_t>>(sent.end() - 3, sent.end()),
		cp::exactly({questUpdate(1111, REWARD, 1), noNearbyQuests(), dialogWindow(npc, 10000, 1111)}));
	me->clearSent();
	EXPECT_FALSE(handler.checkQuestItems(*env, 0, 1, true, 10000, 10001)) << "the step is 1";
	EXPECT_TRUE(me->sent().empty());

	qs->setStatus(QuestStatus::START);
	qs->setQuestVar(0);
	holdItem(*me, 820063, SYLPHEN_WINGS, 3);
	EXPECT_TRUE(handler.checkQuestItems(*env, 0, 2, false, 10000, 10001, NYMPHS_DRESS, 1));
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 1);
	EXPECT_EQ(vars(qs), 2);
	EXPECT_EQ(qs->getStatus(), QuestStatus::START);

	qs->setQuestVar(0);
	holdItem(*me, 820064, SYLPHEN_WINGS, 2);
	me->clearSent();
	EXPECT_TRUE(handler.checkQuestItemsSimple(*env, 0, 1, false, 10000, 0, 0));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 0, 0)})) << "the simple form closes the window";
	holdItem(*me, 820065, SYLPHEN_WINGS, 1);
	EXPECT_TRUE(handler.checkQuestItemsSimple(*env, 0, 1, true, 10000, KERUB_GRAIN_SACK, 2));
	EXPECT_EQ(held(*me, KERUB_GRAIN_SACK), 2);
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(me->sent().back(), dialogWindow(npc, 10000, 1111));
	EXPECT_FALSE(handler.checkQuestItemsSimple(*env, 0, 1, true, 10000, 0, 0)) << "the step is 1";
}

// checkItemExistence (AbstractQuestHandler.java:575-609): an item outside collect_items, held in the count asked (and taken if asked), moves the
// step on and shows the ok page, else the fail page; the four-argument form only answers whether the player holds (and gave up) the items
TEST_F(AbstractQuestHandlerItemTest, CheckItemExistenceChecksAnyItem) {
	PlainHandler handler(1111);
	Npc& mires = npcOf(MIRES);
	const int32_t npc = mires.getObjectId();
	Ref<QuestEnv> env = envOf(*me, 1111, DialogAction::SETPRO1, at(mires));
	EXPECT_FALSE(handler.checkItemExistence(*env, NYMPHS_DRESS, 1, false)) << "none held";
	holdItem(*me, 820071, NYMPHS_DRESS, 1);
	EXPECT_FALSE(handler.checkItemExistence(*env, NYMPHS_DRESS, 2, false)) << "1 of 2";
	EXPECT_TRUE(handler.checkItemExistence(*env, NYMPHS_DRESS, 1, false));
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 1) << "kept";
	EXPECT_TRUE(handler.checkItemExistence(*env, NYMPHS_DRESS, 1, true));
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 0) << "taken";

	EXPECT_THROW(handler.checkItemExistence(*env, 0, 1, false, NYMPHS_DRESS, 1, true, 10002, 10003, 0, 0), runtime::NullPointerException);
	Ref<QuestState> qs = hold(*me, 1111, QuestStatus::START);
	me->clearSent();
	EXPECT_TRUE(handler.checkItemExistence(*env, 0, 1, false, NYMPHS_DRESS, 1, true, 10002, 10003, KERUB_GRAIN_SACK, 2));
	EXPECT_EQ(me->sent(), cp::exactly({dialogWindow(npc, 10003, 1111)})) << "not held: the fail page";
	EXPECT_EQ(vars(qs), 0);
	holdItem(*me, 820072, NYMPHS_DRESS, 1);
	me->clearSent();
	EXPECT_TRUE(handler.checkItemExistence(*env, 0, 1, false, NYMPHS_DRESS, 1, true, 10002, 10003, KERUB_GRAIN_SACK, 2));
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 0);
	EXPECT_EQ(held(*me, KERUB_GRAIN_SACK), 2);
	EXPECT_EQ(vars(qs), 1);
	EXPECT_EQ(me->sent().back(), dialogWindow(npc, 10002, 1111));
	EXPECT_FALSE(handler.checkItemExistence(*env, 0, 1, false, NYMPHS_DRESS, 1, true, 10002, 10003, 0, 0)) << "the step is 1";
}

// useQuestObject (AbstractQuestHandler.java:866-921): at the step of the variable, the item is given, the other taken, the movie played and the
// step changed, in that order; for a dying object the player's target must be the env's object (a target that is not an Npc is Java's
// ClassCastException); another step or no quest state answers false
TEST_F(AbstractQuestHandlerItemTest, UseQuestObjectGivesTakesPlaysAndMovesTheStep) {
	PlainHandler handler(1111);
	Npc& sack = npcOf(GRAIN_SACK);
	Npc& mires = npcOf(MIRES);
	const int32_t object = sack.getObjectId();
	Ref<QuestEnv> env = envOf(*me, 1111, DialogAction::USE_OBJECT, at(sack));
	EXPECT_FALSE(handler.useQuestObject(*env, 0, 1, false, false)) << "no quest state";
	Ref<QuestState> qs = hold(*me, 1111, QuestStatus::START);
	Item& wings = holdItem(*me, 820081, SYLPHEN_WINGS, 2);

	EXPECT_TRUE(handler.useQuestObject(*env, 0, 1, false, 0, NYMPHS_DRESS, 1, SYLPHEN_WINGS, 1, 44, false));
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 1);
	EXPECT_EQ(held(*me, SYLPHEN_WINGS), 1);
	std::vector<std::vector<uint8_t>> sent = me->sent();
	std::vector<std::vector<uint8_t>> steps;
	for (const std::vector<uint8_t>& packet : sent) {
		int32_t opcode = items::javaOpcodeOf(packet);
		if (opcode == items::SM_INVENTORY_ADD_ITEM_OPCODE || opcode == items::SM_INVENTORY_UPDATE_ITEM_OPCODE || opcode == SM_PLAY_MOVIE_OPCODE ||
			opcode == SM_QUEST_ACTION_OPCODE)
			steps.push_back(packet);
	}
	EXPECT_EQ(steps, cp::exactly({added(NYMPHS_DRESS, ItemPacketService_ItemAddType::QUEST_WORK_ITEM),
						 updated(wings, ItemPacketService_ItemUpdateType::DEC_ITEM_USE), playMovie(false, object, 1111, 44),
						 questUpdate(1111, START, 1)}));
	me->clearSent();
	EXPECT_FALSE(handler.useQuestObject(*env, 0, 1, false, false)) << "the step is 1";
	EXPECT_TRUE(me->sent().empty());

	EXPECT_TRUE(handler.useQuestObject(*env, 0, 2, false, 1)) << "var1";
	EXPECT_EQ(vars(qs), 1 + (2 << 6));
	me->clearSent();
	EXPECT_TRUE(handler.useQuestObject(*env, 1, 1, true, 0, 45));
	EXPECT_EQ(qs->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(me->sent(), cp::exactly({playMovie(false, object, 1111, 45), questUpdate(1111, REWARD, 1 + (2 << 6)), noNearbyQuests()}));

	qs->setStatus(QuestStatus::START);
	player().setTarget(nullptr);
	EXPECT_FALSE(handler.useQuestObject(*env, 1, 3, false, true)) << "no target";
	player().setTarget(Ptr<VisibleObject>(mires));
	EXPECT_FALSE(handler.useQuestObject(*env, 1, 3, false, true)) << "the target is not the env's object";
	EXPECT_EQ(vars(qs), 1 + (2 << 6));
	player().setTarget(Ptr<VisibleObject>(player()));
	EXPECT_THROW(handler.useQuestObject(*env, 1, 3, false, true), runtime::ClassCastException);
	EXPECT_EQ(vars(qs), 1 + (2 << 6));
	player().setTarget(nullptr);
}

// useQuestItem (AbstractQuestHandler.java:923-977): at the step, the use bar starts at once (SM_ITEM_USAGE_ANIMATION 3000 ms to the player and
// his surroundings) and after 3000 ms the task ends it, takes one of the item, gives the reward item, plays the movie and changes the step;
// another step, no quest state or no player answer false and schedule nothing
TEST_F(AbstractQuestHandlerItemTest, UseQuestItemRunsTheUseBarAndMovesTheStepAfterThreeSeconds) {
	PlainHandler handler(1111);
	Ref<QuestEnv> env = envOf(*me, 1111, DialogAction::NULL_);
	Item& wings = holdItem(*me, 820091, SYLPHEN_WINGS, 3);
	const int32_t self = player().getObjectId();
	EXPECT_FALSE(handler.useQuestItem(*env, wings, 0, 1, false)) << "no quest state";
	Ref<QuestState> qs = hold(*me, 1111, QuestStatus::START);
	EXPECT_FALSE(handler.useQuestItem(*env, wings, 2, 3, false)) << "the step is 0";
	executor->advance(std::chrono::milliseconds(3000));
	EXPECT_TRUE(me->sent().empty());

	EXPECT_TRUE(handler.useQuestItem(*env, wings, 0, 1, false, NYMPHS_DRESS, 1, 55));
	EXPECT_EQ(me->sent(), cp::exactly({itemUsageAnimation(self, 820091, SYLPHEN_WINGS, 3000, 0, 0)}));
	me->clearSent();
	executor->advance(std::chrono::milliseconds(2999));
	EXPECT_TRUE(me->sent().empty());
	EXPECT_EQ(vars(qs), 0);
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_EQ(held(*me, SYLPHEN_WINGS), 2);
	EXPECT_EQ(held(*me, NYMPHS_DRESS), 1);
	EXPECT_EQ(vars(qs), 1);
	std::vector<std::vector<uint8_t>> sent = me->sent();
	ASSERT_FALSE(sent.empty());
	EXPECT_EQ(sent.front(), itemUsageAnimation(self, 820091, SYLPHEN_WINGS, 0, 1, 0));
	std::vector<int32_t> opcodes = items::opcodesOf(sent);
	std::vector<int32_t> order;
	for (int32_t opcode : opcodes) {
		if (opcode == items::SM_INVENTORY_UPDATE_ITEM_OPCODE || opcode == items::SM_INVENTORY_ADD_ITEM_OPCODE || opcode == SM_PLAY_MOVIE_OPCODE ||
			opcode == SM_QUEST_ACTION_OPCODE)
			order.push_back(opcode);
	}
	EXPECT_EQ(order, (std::vector<int32_t>{items::SM_INVENTORY_UPDATE_ITEM_OPCODE, items::SM_INVENTORY_ADD_ITEM_OPCODE, SM_PLAY_MOVIE_OPCODE,
						 SM_QUEST_ACTION_OPCODE}))
		<< "the item taken, the reward item, the movie, the step";
	EXPECT_EQ(sent.back(), questUpdate(1111, START, 1));
	EXPECT_EQ(items::packetsOf(sent, SM_PLAY_MOVIE_OPCODE), cp::exactly({playMovie(false, 0, 1111, 55)}));

	me->clearSent();
	EXPECT_TRUE(handler.useQuestItem(*env, wings, 0, 3, false, 0, 0, 0, 1)) << "var1";
	executor->advance(std::chrono::milliseconds(3000));
	EXPECT_EQ(vars(qs), 1 + (3 << 6));
	EXPECT_EQ(held(*me, SYLPHEN_WINGS), 1);
	EXPECT_TRUE(items::packetsOf(me->sent(), SM_PLAY_MOVIE_OPCODE).empty()) << "no movie";

	env->setPlayer(nullptr);
	EXPECT_FALSE(handler.useQuestItem(*env, wings, 1, 2, false)) << "no player";
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test
