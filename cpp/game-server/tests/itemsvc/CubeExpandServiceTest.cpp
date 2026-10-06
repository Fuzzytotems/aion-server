// M5c P-05 (m5c-plan.md §5, P5-07, W-09): CubeExpandService (the cube expander's question and its RequestResponseHandler, the three expansion
// kinds, the ticket and total limits) and ExpandInventoryAction (the cube tickets; the warehouse arm calls WarehouseService, ported with the
// M5b-3 leftovers), against CubeExpandService.java:29-125 and ExpandInventoryAction.java:29-55. Tests of P-06.
//
// Expectations: tools/oracle `oracle.py m5c-economy --no-profile --set gameserver.siege.enable=false --npc 798008` (and `--map 400010000 --npc
// 279022`) with `--npc-expands`, `--quest-expands`, `--item-expands` and `--set gameserver.npcexpands.limit=4` gives each answer below: the question
// (900686, the price as its parameter), the yes (the price taken, one npc expansion, 9 slots, STR_EXTEND_INVENTORY_SIZE_EXTENDED(9), SM_CUBE_UPDATE
// with the counters), STR_WAREHOUSE_EXPAND_NOT_ENOUGH_MONEY, and the three refusals. The values the oracle does not print are Java's arithmetic
// on the data: the minimum refusal's level is minExpansionLevel - 1 (CubeExpandService.java:42), the cube limit is StorageType.CUBE's 27 plus 9
// per expansion (Player.java:428-430, StorageType.java:7). The packets whose fields the service chooses are Java's bytes (SM_CUBE_UPDATE.java:
// 75-79, SM_ITEM_USAGE_ANIMATION.java:22-30, 73-86); the question, the messages and the kinah update are compared against the server's own
// serialization of the packet Java constructs there (their bytes are pinned by the sm tests).

#include "PlayerItemsTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/templates/item/actions/ExpandInventoryAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/CubeExpandService.h"
#include "aion/gameserver/services/item/ItemPacketService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test::playeritems {
namespace {

using model::gameobjects::Npc;
using model::templates::item::actions::ExpandInventoryAction;
using network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t CUBE_LIMIT = 27; // StorageType.CUBE (StorageType.java:7)
constexpr int32_t CUBE_ROW = 9;

class CubeExpandServiceTest : public PlayerItemsTest {
protected:
	model::gameobjects::player::PlayerCommonData& commonData() { return *player().getCommonData(); }

	void setExpands(int32_t npc, int32_t quest, int32_t item) {
		commonData().setNpcExpands(npc);
		commonData().setQuestExpands(quest);
		commonData().setItemExpands(item);
	}

	std::vector<uint8_t> question(int32_t price) {
		return serialized(SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_WAREHOUSE_EXPAND_WARNING, 0, 0, std::to_string(price)));
	}

	std::vector<uint8_t> systemMessage(SM_SYSTEM_MESSAGE&& packet) { return serialized(std::move(packet)); }

	/** SM_CUBE_UPDATE.cubeSize(CUBE, player) (SM_CUBE_UPDATE.java:29-52, 71-79): action 0, CUBE's ordinal 0, the cube's item count, the counters */
	static std::vector<uint8_t> cubeSize(int32_t itemsCount, int32_t npcExpands, int32_t questExpands, int32_t itemExpands) {
		return javaPacket(SM_CUBE_UPDATE_OPCODE, PacketWriter().C(0).C(0).D(itemsCount).C(npcExpands).C(questExpands).C(itemExpands));
	}

	std::vector<uint8_t> kinahUpdate() {
		return serialized(SM_INVENTORY_UPDATE_ITEM(player(), *player().getInventory().getKinahItem(), ItemPacketService::ItemUpdateType::DEC_KINAH_CUBE));
	}

	std::string npcName(model::gameobjects::Npc& npc) { return npc.getObjectTemplate()->getL10n(); }
};

// ------------------------------------------------------------------------------------------------------------------------- expandCube

TEST_F(CubeExpandServiceTest, TheExpanderAsksAndAYesPaysAndExpandsTheCubeByOneRow) {
	// CubeExpandService.java:29-63: Baevrunerk (cube_expander.xml:4-6) expands to level 1 for 1,000: the question STR_WAREHOUSE_EXPAND_WARNING
	// with the price; CubeExpandService$1.acceptRequest (:52-57) takes the price with DEC_KINAH_CUBE and npcExpand (:73-92) sends
	// STR_EXTEND_INVENTORY_SIZE_EXTENDED(9), counts the expansion, sets the cube limit and sends SM_CUBE_UPDATE
	giveKinah(960001, 1500);
	inCube(960002, BANDAGE, 1);
	ASSERT_EQ(player().getInventory().getLimit(), CUBE_LIMIT);

	CubeExpandService::expandCube(player(), npc(BAEVRUNERK));
	EXPECT_EQ(sent(), cp::exactly({question(1000)}));
	EXPECT_EQ(player().getInventory().getKinah(), 1500) << "nothing is paid before the answer";
	clearSent();

	ASSERT_TRUE(player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_WAREHOUSE_EXPAND_WARNING, 1));

	EXPECT_EQ(player().getInventory().getKinah(), 500);
	EXPECT_EQ(player().getNpcExpands(), 1);
	EXPECT_EQ(player().getInventory().getLimit(), CUBE_LIMIT + CUBE_ROW);
	EXPECT_EQ(sent(), cp::exactly({kinahUpdate(), systemMessage(SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_SIZE_EXTENDED(9)), cubeSize(1, 1, 0, 0)}));
}

TEST_F(CubeExpandServiceTest, AYesWithoutThePriceOnlySaysSoAndANoDoesNothing) {
	// CubeExpandService.java:53-56: tryDecreaseKinah(1000) fails for 999 -> STR_WAREHOUSE_EXPAND_NOT_ENOUGH_MONEY, no expansion; a "no" reaches
	// RequestResponseHandler.denyRequest, which does nothing; exactly the price is enough (Storage.tryDecreaseKinah: kinah >= amount)
	giveKinah(960101, 999);
	Npc& baevrunerk = npc(BAEVRUNERK);
	CubeExpandService::expandCube(player(), baevrunerk);
	clearSent();
	ASSERT_TRUE(player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_WAREHOUSE_EXPAND_WARNING, 1));
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_WAREHOUSE_EXPAND_NOT_ENOUGH_MONEY())}));
	EXPECT_EQ(player().getInventory().getKinah(), 999);
	EXPECT_EQ(player().getNpcExpands(), 0);
	EXPECT_EQ(player().getInventory().getLimit(), CUBE_LIMIT);

	CubeExpandService::expandCube(player(), baevrunerk);
	clearSent();
	ASSERT_TRUE(player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_WAREHOUSE_EXPAND_WARNING, 0));
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(player().getNpcExpands(), 0);

	player().getInventory().increaseKinah(1);
	CubeExpandService::expandCube(player(), baevrunerk);
	clearSent();
	ASSERT_TRUE(player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_WAREHOUSE_EXPAND_WARNING, 1));
	EXPECT_EQ(player().getInventory().getKinah(), 0);
	EXPECT_EQ(player().getNpcExpands(), 1);
}

TEST_F(CubeExpandServiceTest, AQuestionAlreadyPendingIsNotAskedAgain) {
	// CubeExpandService.java:59-62: ResponseRequester.putRequest refuses a second handler for STR_WAREHOUSE_EXPAND_WARNING, and nothing is sent
	Npc& baevrunerk = npc(BAEVRUNERK);
	CubeExpandService::expandCube(player(), baevrunerk);
	clearSent();
	CubeExpandService::expandCube(player(), baevrunerk);
	EXPECT_TRUE(sent().empty());
}

TEST_F(CubeExpandServiceTest, EachExpanderSellsOnlyItsOwnLevels) {
	// CubeExpandService.java:39-49: the next npc expansion must be between the template's lowest level and the lower of its highest level and
	// gameserver.npcexpands.limit, and have a price. Baevrunerk sells level 1 only; Jarumonerk (cube_expander.xml:15-17) level 5 only
	Npc& baevrunerk = npc(BAEVRUNERK);
	Npc& jarumonerk = npc(JARUMONERK);

	setExpands(1, 0, 0); // the 2nd npc expansion at Baevrunerk: above his maximum 1
	CubeExpandService::expandCube(player(), baevrunerk);
	EXPECT_EQ(sent(), cp::exactly({systemMessage(
						  SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_CANT_EXTEND_MORE_DUE_TO_MAXIMUM_EXTEND_LEVEL_BY_THIS_NPC(npcName(baevrunerk), 1))}));
	clearSent();

	setExpands(0, 0, 0); // the 1st at Jarumonerk: below his minimum 5, the message names 5 - 1
	CubeExpandService::expandCube(player(), jarumonerk);
	EXPECT_EQ(sent(), cp::exactly({systemMessage(
						  SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_CANT_EXTEND_DUE_TO_MINIMUM_EXTEND_LEVEL_BY_THIS_NPC(npcName(jarumonerk), 4))}));
	clearSent();

	setExpands(4, 0, 0); // the 5th at Jarumonerk: his level, 360,000
	CubeExpandService::expandCube(player(), jarumonerk);
	EXPECT_EQ(sent(), cp::exactly({question(360000)}));
	player().getResponseRequester().denyAll();
	clearSent();

	// gameserver.npcexpands.limit = 4 caps his maximum: the 5th is refused with the capped maximum
	configs::main::CustomConfig::NPC_CUBE_EXPANDS_SIZE_LIMIT.store(4);
	CubeExpandService::expandCube(player(), jarumonerk);
	EXPECT_EQ(sent(), cp::exactly({systemMessage(
						  SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_CANT_EXTEND_MORE_DUE_TO_MAXIMUM_EXTEND_LEVEL_BY_THIS_NPC(npcName(jarumonerk), 4))}));
}

TEST_F(CubeExpandServiceTest, TheTotalOfAllExpansionsIsCheckedBeforeTheExpandersLevels) {
	// CubeExpandService.java:36-37, 116-124: npc + quest + item expansions + 1 above gameserver.cube.expansion_limit (11) -> CANT_EXTEND_MORE;
	// exactly 11 passes; a negative sum is refused without a message
	Npc& baevrunerk = npc(BAEVRUNERK);
	setExpands(0, 1, 10);
	CubeExpandService::expandCube(player(), baevrunerk);
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_CANT_EXTEND_MORE())}));
	clearSent();

	setExpands(0, 0, 10);
	CubeExpandService::expandCube(player(), baevrunerk);
	EXPECT_EQ(sent(), cp::exactly({question(1000)}));
	player().getResponseRequester().denyAll();
	clearSent();

	setExpands(0, 0, -5);
	CubeExpandService::expandCube(player(), baevrunerk);
	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(CubeExpandService::canExpand(player()));
}

TEST_F(CubeExpandServiceTest, AnNpcWithoutAnExpansionTemplateOnlyLogsAWarning) {
	// CubeExpandService.java:30-34: the template is looked up first, so not even the total limit is checked
	network::test::LogCapture cubeLog({"com.aionemu.gameserver.services.CubeExpandService"});
	setExpands(0, 1, 10);
	Npc& taraerinerk = npc(TARAERINERK);
	CubeExpandService::expandCube(player(), taraerinerk);
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(cubeLog.count("Cube expansion template could not be found for " + taraerinerk.toString()), 1) << cubeLog.dump();
}

// ------------------------------------------------------------------------------------------------------------------------- expand, tickets

TEST_F(CubeExpandServiceTest, EachExpansionKindCountsOnItsOwnUntilTheTotalLimit) {
	// CubeExpandService.java:73-104: type 1 npc, 2 item, 3 quest; each adds one row to the cube and sends the counters; at the total limit
	// (:116-124) CANT_EXTEND_MORE and nothing changes
	CubeExpandService::npcExpand(player());
	CubeExpandService::itemExpand(player());
	CubeExpandService::questExpand(player());
	CubeExpandService::itemExpand(player());
	EXPECT_EQ(player().getNpcExpands(), 1);
	EXPECT_EQ(player().getItemExpands(), 2);
	EXPECT_EQ(player().getQuestExpands(), 1);
	EXPECT_EQ(player().getInventory().getLimit(), CUBE_LIMIT + 4 * CUBE_ROW);
	const std::vector<uint8_t> extended = systemMessage(SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_SIZE_EXTENDED(9));
	EXPECT_EQ(sent(), cp::exactly({extended, cubeSize(0, 1, 0, 0), extended, cubeSize(0, 1, 0, 1), extended, cubeSize(0, 1, 1, 1), extended,
						  cubeSize(0, 1, 1, 2)}));
	clearSent();

	setExpands(5, 2, 4); // 11 expansions: the 12th is refused
	CubeExpandService::itemExpand(player());
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_CANT_EXTEND_MORE())}));
	EXPECT_EQ(player().getItemExpands(), 4);
}

TEST_F(CubeExpandServiceTest, ATicketExpandsOnlyUpToItsLevel) {
	// CubeExpandService.java:106-114: the total limit first, then item expansions >= the ticket level -> CANT_EXTEND_MORE
	EXPECT_TRUE(CubeExpandService::canExpandByTicket(player(), 1));
	EXPECT_TRUE(sent().empty());
	setExpands(0, 0, 1);
	EXPECT_FALSE(CubeExpandService::canExpandByTicket(player(), 1));
	EXPECT_TRUE(CubeExpandService::canExpandByTicket(player(), 2));
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_CANT_EXTEND_MORE())}));
	clearSent();
	setExpands(10, 0, 1);
	EXPECT_FALSE(CubeExpandService::canExpandByTicket(player(), 5));
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_CANT_EXTEND_MORE())})) << "one message, from canExpand";
}

// ------------------------------------------------------------------------------------------------------------------------- ExpandInventoryAction

TEST_F(CubeExpandServiceTest, ACubeTicketIsUsedUpAndExpandsTheCube) {
	// ExpandInventoryAction.java:29-35: CUBE -> canExpandByTicket(level); :37-55: the ticket is used up (decreaseByObjectId), STR_USE_ITEM, the
	// three-argument SM_ITEM_USAGE_ANIMATION to the player (time 0, end 1, unk3 1) and itemExpand
	Item& ticket = inCube(960701, EXPAND_CUBE_TICKET_1, 1);
	const ExpandInventoryAction& action = onlyActionOf<ExpandInventoryAction>(EXPAND_CUBE_TICKET_1);
	EXPECT_TRUE(action.canAct(player(), Ptr<Item>(ticket), nullptr));
	EXPECT_TRUE(sent().empty());

	action.act(player(), Ptr<Item>(ticket), nullptr);

	EXPECT_FALSE(player().getInventory().getItemByObjId(960701)) << "the ticket is used up";
	EXPECT_EQ(player().getItemExpands(), 1);
	EXPECT_EQ(player().getInventory().getLimit(), CUBE_LIMIT + CUBE_ROW);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 6u) << ::testing::PrintToString(opcodesOf(packets));
	// Storage.decreaseByObjectId's own packets (DEC_ITEM_USE -> ItemDeleteType.USE, 0x17, ItemPacketService.java:119):
	// ItemPacketService.sendItemDeletePacket's SM_DELETE_ITEM and SM_CUBE_UPDATE (:178-185), the cube size before the expansion
	EXPECT_EQ(packets[0], deleteItem(960701, 0x17));
	EXPECT_EQ(packets[1], cubeSize(0, 0, 0, 0));
	packets.erase(packets.begin(), packets.begin() + 2);
	EXPECT_EQ(packets, cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_USE_ITEM(ticket.getL10n())),
						   itemUsageAnimation(player().getObjectId(), 960701, EXPAND_CUBE_TICKET_1, 0, 1, 1),
						   systemMessage(SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_SIZE_EXTENDED(9)), cubeSize(0, 0, 0, 1)}));

	// the next level-1 ticket is refused, a level-2 ticket is not
	clearSent();
	Item& second = inCube(960702, EXPAND_CUBE_TICKET_1, 1);
	Item& levelTwo = inCube(960703, EXPAND_CUBE_TICKET_2, 1);
	EXPECT_FALSE(action.canAct(player(), Ptr<Item>(second), nullptr));
	EXPECT_TRUE(onlyActionOf<ExpandInventoryAction>(EXPAND_CUBE_TICKET_2).canAct(player(), Ptr<Item>(levelTwo), nullptr));
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_EXTEND_INVENTORY_CANT_EXTEND_MORE())}));
}

TEST_F(CubeExpandServiceTest, ATicketThatIsNotInTheCubeDoesNothing) {
	// ExpandInventoryAction.java:39-40: decreaseByObjectId fails -> return
	Item& ticket = loose(960801, EXPAND_CUBE_TICKET_1, 1);
	onlyActionOf<ExpandInventoryAction>(EXPAND_CUBE_TICKET_1).act(player(), Ptr<Item>(ticket), nullptr);
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(player().getItemExpands(), 0);
}

TEST_F(CubeExpandServiceTest, TheWarehouseTicketExpandsTheWarehouseOnce) {
	// ExpandInventoryAction.java:33, :51-52: the WAREHOUSE arm calls WarehouseService (ported with the M5b-3 leftovers, CP3):
	// canExpandByTicket allows a level-1 ticket to a character without bonus expansions (WarehouseService.java:87-95), act uses the ticket
	// up and expand(player, false) adds a bonus expansion with STR_EXTEND_CHAR_WAREHOUSE_SIZE_EXTENDED(8) (WarehouseService.java:72-85)
	Item& ticket = inCube(960901, EXPAND_WAREHOUSE_TICKET_1, 1);
	const ExpandInventoryAction& action = onlyActionOf<ExpandInventoryAction>(EXPAND_WAREHOUSE_TICKET_1);
	runtime::resetUnportedHitsForTests();
	EXPECT_TRUE(action.canAct(player(), Ptr<Item>(ticket), nullptr));
	clearSent();
	action.act(player(), Ptr<Item>(ticket), nullptr);
	EXPECT_FALSE(player().getInventory().getItemByObjId(960901));
	EXPECT_EQ(player().getWhBonusExpands(), 1);
	const std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(std::count(packets.begin(), packets.end(), systemMessage(SM_SYSTEM_MESSAGE::STR_EXTEND_CHAR_WAREHOUSE_SIZE_EXTENDED(8))), 1);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
	Item& second = inCube(960902, EXPAND_WAREHOUSE_TICKET_1, 1);
	clearSent();
	EXPECT_FALSE(action.canAct(player(), Ptr<Item>(second), nullptr)) << "1 bonus expansion - 0 warehouse quests >= ticket level 1";
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_EXTEND_CHAR_WAREHOUSE_CANT_EXTEND_MORE())}));
}

} // namespace
} // namespace aion::gameserver::services::item::test::playeritems
