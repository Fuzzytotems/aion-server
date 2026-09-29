// M5c T-01 / T-04 (m5c-plan.md §5, P5-09b): TradeService at minalinerk 798007 - buying (the goods-list validation, the buy price of §2.10,
// the exact-kinah purchase, the free slots, canTrade, a REWARD vendor's tokens), selling (the sell reward, the split and the whole stack, the
// buy-back list, the refusals, the shipped daily sell limit through PlayerLimitService.updateSellLimit, P-02), the buy-back ledger through
// RepurchaseService.repurchaseFromShop (P-03), an abyss vendor's AP and medals (AbyssPointsService.addAp, ported by M5d E-09 under a test-file
// lease on this file, m5d-plan.md §18.5) and the loud arms (the unhandled npc type, selling for AP, the trade-in refusals).
//
// Java: TradeService.java:51-387. The numbers are §2.10's and `oracle.py m5c-trade --npc 798007 --set gameserver.siege.enable=false`'s
// (prices 125 % and taxes 113 %): Minor Life Elixir (template price 250) 352 each, Extraction Tools (1000) 1,412, the Minor Life Potion's sell
// reward 50 each and the Training Sword's 1.

#include "TradeTestSupport.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.h"
#include "aion/gameserver/model/trade/RepurchaseList.h"
#include "aion/gameserver/model/trade/TradeList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/RepurchaseService.h"
#include "aion/gameserver/services/TradeService.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/services/item/ItemSocketService.h"
#include "aion/gameserver/services/player/PlayerLimitService.h"

namespace aion::gameserver::economy::test::trade {
namespace {

using model::trade::TradeList;
using network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::TradeService;
using services::item::ItemPacketService_ItemUpdateType;

const char* AUDIT_LOGGER = "AUDIT_LOG";
const char* TRADE_SERVICE_LOGGER = "com.aionemu.gameserver.services.TradeService";

constexpr int32_t POTIONS = 800001; // 100 x Minor Life Potion
constexpr int32_t SWORD = 800002;   // 1 x Training Sword
constexpr int32_t JUICE = 800003;   // 12 x Mercenary's Fruit Juice (not sellable)
constexpr int32_t A_KINAH = 800009;

constexpr int64_t ELIXIR_PRICE = 352;  // getBuyPrice(250): 250 -> 312 -> 312 -> 352
constexpr int64_t TOOLS_PRICE = 1412;  // getBuyPrice(1000): 1000 -> 1250 -> 1250 -> 1412
constexpr int64_t POTION_REWARD = 50;  // getSellReward(250, 20)
constexpr int64_t SWORD_REWARD = 1;    // getSellReward(5, 20)

class TradeServiceTest : public TradeTest {
protected:
	void SetUp() override {
		TradeTest::SetUp();
		give(a(), POTIONS, MINOR_LIFE_POTION, 100);
		give(a(), SWORD, TRAINING_SWORD, 1);
		give(a(), JUICE, FRUIT_JUICE, 12);
	}

	/** CM_BUY_ITEM(npc, 13, [itemId, count]...) at minalinerk (CM_BUY_ITEM.java:115-116) */
	bool buy(std::initializer_list<std::pair<int32_t, int64_t>> itemCounts) {
		runtime::Ref<TradeList> tradeList = TradeList::create(merchant().getObjectId());
		for (const auto& [itemId, count] : itemCounts)
			tradeList->addItem(itemId, count);
		return TradeService::performBuyFromShop(merchant(), a(), *tradeList);
	}

	/** CM_BUY_ITEM(npc, 1, [objectId, count]...): the "item id" of a sell list is the object id (CM_BUY_ITEM.java:120-123) */
	bool sell(std::initializer_list<std::pair<int32_t, int64_t>> objectCounts) {
		runtime::Ref<TradeList> tradeList = TradeList::create(merchant().getObjectId());
		for (const auto& [objId, count] : objectCounts)
			tradeList->addItem(objId, count);
		return TradeService::performSellToShop(a(), *tradeList, nullptr);
	}

	/** The object ids of A's buy-back list */
	std::unordered_set<int32_t> repurchaseIds() {
		std::unordered_set<int32_t> ids;
		for (const runtime::Ptr<Item>& item : services::RepurchaseService::getInstance().getRepurchaseItems(a().getObjectId()))
			ids.insert(item->getObjectId());
		return ids;
	}

	runtime::Ptr<Item> repurchaseItemOf(int32_t itemId) {
		for (const runtime::Ptr<Item>& item : services::RepurchaseService::getInstance().getRepurchaseItems(a().getObjectId()))
			if (item->getItemId() == itemId)
				return item;
		return nullptr;
	}

	void TearDown() override {
		if (f.player)
			services::RepurchaseService::getInstance().removeRepurchaseItems(*f.player);
		TradeTest::TearDown();
	}
};

// TradeService.java:62-67, 80-164: a NORMAL merchant takes the list price of TradeList.calculateBuyListPrice and adds the goods
TEST_F(TradeServiceTest, BuyingChargesTheListPriceAndAddsTheGoods) {
	setKinah(a(), A_KINAH, 5000);

	ASSERT_TRUE(buy({{MINOR_LIFE_ELIXIR, 2}, {EXTRACTION_TOOLS, 1}}));

	EXPECT_EQ(kinah(a()), 5000 - 2 * ELIXIR_PRICE - TOOLS_PRICE);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_ELIXIR), 2);
	EXPECT_EQ(countOf(a(), EXTRACTION_TOOLS), 1);
	EXPECT_TRUE(ofOpcode(sent(), itemtest::SM_SYSTEM_MESSAGE_OPCODE).empty()) << "no refusal";
	EXPECT_EQ(ofOpcode(sent(), itemtest::SM_INVENTORY_ADD_ITEM_OPCODE).size(), 2u) << "one new stack per line (ItemAddType.BUY)";
}

// TradeList.java:56 (`availableKinah >= requiredKinah`) and Storage.java:83 (tryDecreaseKinah's `>=`): exactly enough buys, one less does not
TEST_F(TradeServiceTest, ExactlyEnoughKinahBuysAndOneLessIsRefused) {
	setKinah(a(), A_KINAH, 2 * ELIXIR_PRICE + TOOLS_PRICE - 1);

	EXPECT_FALSE(buy({{MINOR_LIFE_ELIXIR, 2}, {EXTRACTION_TOOLS, 1}}));
	EXPECT_EQ(sent(), cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_MONEY())}));
	EXPECT_EQ(kinah(a()), 2 * ELIXIR_PRICE + TOOLS_PRICE - 1);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_ELIXIR), 0);

	a().getInventory().increaseKinah(1);
	clearSent();
	EXPECT_TRUE(buy({{MINOR_LIFE_ELIXIR, 2}, {EXTRACTION_TOOLS, 1}}));
	EXPECT_EQ(kinah(a()), 0);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_ELIXIR), 2);
	EXPECT_EQ(countOf(a(), EXTRACTION_TOOLS), 1);
}

// :166-181 validateBuyItems: only the goods of the merchant's own tabs, each with a count of at least 1
TEST_F(TradeServiceTest, TheMerchantSellsOnlyItsGoodsListsAndPositiveCounts) {
	setKinah(a(), A_KINAH, 5000);
	const std::vector<std::vector<uint8_t>> refused = cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_BUY_SELL_USER_BUY_FAILED())});

	EXPECT_FALSE(buy({{MINOR_LIFE_POTION, 1}})) << "not in goods lists 132 or 720";
	EXPECT_EQ(sent(), refused);
	clearSent();
	EXPECT_FALSE(buy({{MINOR_LIFE_ELIXIR, 1}, {MINOR_LIFE_POTION, 1}})) << "one foreign line spoils the list";
	EXPECT_EQ(sent(), refused);
	clearSent();
	EXPECT_FALSE(buy({{MINOR_LIFE_ELIXIR, 0}})) << "count < 1";
	EXPECT_EQ(sent(), refused);
	clearSent();
	EXPECT_FALSE(buy({{MINOR_LIFE_ELIXIR, -5}}));
	EXPECT_EQ(sent(), refused);
	EXPECT_EQ(kinah(a()), 5000);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_ELIXIR), 0);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 100);

	clearSent();
	EXPECT_TRUE(buy({{MINOR_LIFE_ELIXIR, 1}})) << "the control: tab 720's elixir";
}

// :117-121: one free slot per line
TEST_F(TradeServiceTest, EveryLineNeedsAFreeSlot) {
	setKinah(a(), A_KINAH, 5000);
	for (int32_t objId = 830000; a().getInventory().getFreeSlots() > 1; objId++)
		give(a(), objId, TRAINING_SWORD, 1);
	ASSERT_EQ(a().getInventory().getFreeSlots(), 1);

	EXPECT_FALSE(buy({{MINOR_LIFE_ELIXIR, 1}, {EXTRACTION_TOOLS, 1}}));
	EXPECT_EQ(sent(), cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_FULL_INVENTORY())}));
	EXPECT_EQ(kinah(a()), 5000);

	clearSent();
	EXPECT_TRUE(buy({{MINOR_LIFE_ELIXIR, 1}}));
	EXPECT_EQ(kinah(a()), 5000 - ELIXIR_PRICE);
}

// :62-75: LEGION_COIN has no arm. No shipped trade list uses the type (npc_trade_list.xml), so this is a fixture row: minalinerk's own row
// with npc_type="LEGION_COIN" (marked as such, never published beside the shipped one)
TEST_F(TradeServiceTest, AnUnhandledNpcTypeIsLoggedAndBuysNothing) {
	dataholders::DataManager::TRADE_LIST_DATA.resetForTests();
	xml::LoadContext context;
	dataholders::DataManager::TRADE_LIST_DATA.publish(xml::bindString<dataholders::TradeListData>(context,
		R"(<npc_trade_list><tradelist_template npc_id="798007" npc_type="LEGION_COIN" buy_price_rate="200">)"
		R"(<tradelist id="132" /><tradelist id="720" /></tradelist_template></npc_trade_list>)"));
	setKinah(a(), A_KINAH, 5000);
	network::test::LogCapture capture({TRADE_SERVICE_LOGGER});

	EXPECT_FALSE(buy({{MINOR_LIFE_ELIXIR, 1}}));

	EXPECT_TRUE(capture.contains("Unhandled TradeNpcType:LEGION_COIN")) << capture.dump();
	EXPECT_EQ(kinah(a()), 5000);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_ELIXIR), 0);
}

// :62-70, :80-155: adetes is an ABYSS vendor, who trades without kinah (useKinah false). TradeList.calculateAbyssRewardBuyList checks the AP
// (70,350 for the shield: (int) (70350 x 1 x 100 / 100D x 100) / 100) and the acquisition items (6 Silver Medals); then the costs are taken,
// the AP first through AbyssPointsService.addAp (M5d E-09 ported it; until then this case pinned its throw), then the medals (:140-146), and
// the shield is added (:149-151). No such vendor stands on the start maps (m5c-plan.md §2.2 row 5)
TEST_F(TradeServiceTest, AnAbyssVendorTakesTheApAndThenTheMedalsForTheShield) {
	setKinah(a(), A_KINAH, 5000);
	auto buyShield = [this] {
		runtime::Ref<TradeList> tradeList = TradeList::create(npcOf(ADETES).getObjectId());
		tradeList->addItem(SQUAD_LEADERS_SHIELD, 1);
		return TradeService::performBuyFromShop(npcOf(ADETES), a(), *tradeList);
	};
	const std::vector<uint8_t> notEnoughAp = serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_ABYSSPOINT());

	EXPECT_FALSE(buyShield()) << "0 AP";
	EXPECT_EQ(sent(), cp::exactly({notEnoughAp, notEnoughAp})) << "TradeList.java's own message, then TradeService.java:105-107's";

	a().setAbyssRank(model::gameobjects::player::AbyssRank::create(0, 0, 70350, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0));
	clearSent();
	EXPECT_FALSE(buyShield()) << "the AP, but no medals";
	EXPECT_EQ(sent(), cp::exactly({notEnoughAp}));

	give(a(), 830001, SILVER_MEDAL, 6);
	clearSent();
	EXPECT_TRUE(buyShield());
	EXPECT_EQ(a().getAbyssRank()->getAp(), 0) << "AbyssPointsService.addAp(player, -70350)";
	EXPECT_EQ(kinah(a()), 5000) << "no kinah at an ABYSS vendor";
	EXPECT_EQ(countOf(a(), SILVER_MEDAL), 0);
	EXPECT_EQ(countOf(a(), SQUAD_LEADERS_SHIELD), 1);
	// the AP first (:130-131): AbyssPointsService.java:46-48 sends STR_MSG_USE_ABYSSPOINT(70350) and SM_ABYSS_RANK (the points moved; the
	// rank, GRADE9_SOLDIER before and after, did not), and only then are the medals taken and the shield given
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_GE(packets.size(), 3u) << ::testing::PrintToString(itemtest::opcodesOf(packets));
	EXPECT_EQ(packets[0], serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_USE_ABYSSPOINT(70350)));
	EXPECT_EQ(itemtest::javaOpcodeOf(packets[1]), 237) << "SM_ABYSS_RANK (ServerPacketsOpcodes.java:255)";
	EXPECT_TRUE(sentB().empty()) << "no rank change, so no SM_ABYSS_RANK_UPDATE for the partner who sees A";
}

// :62-70, :80-164 at a REWARD vendor: gintarunerk 801517 sells Pandarunerk's Delve Scroll for 2 Ancient Coins each (acquisition REWARD, no
// AP; npc_trade_list.xml:5024-5026, item_templates.xml:833923-833925). useKinah is false; TradeList.calculateAbyssRewardBuyList adds AP for
// AP and ABYSS acquisitions only, so the list needs no AP and 2 x count coins. The coins are taken (:140-146) and the scrolls given; kinah and
// AbyssPointsService.addAp are never reached
TEST_F(TradeServiceTest, ARewardVendorTakesTheTokensAndGivesTheGoodsWithoutAp) {
	setKinah(a(), A_KINAH, 5000);
	give(a(), 830001, ANCIENT_COIN, 7);
	auto buyScrolls = [this](int64_t count) {
		runtime::Ref<TradeList> tradeList = TradeList::create(npcOf(GINTARUNERK).getObjectId());
		tradeList->addItem(DELVE_SCROLL, count);
		return TradeService::performBuyFromShop(npcOf(GINTARUNERK), a(), *tradeList);
	};

	EXPECT_FALSE(buyScrolls(4)) << "8 coins needed, 7 held";
	EXPECT_EQ(sent(), cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_ABYSSPOINT())}))
		<< "TradeService.java:105-107's only: TradeList sends its own for missing AP, not for missing items";
	EXPECT_EQ(countOf(a(), ANCIENT_COIN), 7);
	EXPECT_EQ(countOf(a(), DELVE_SCROLL), 0);

	clearSent();
	EXPECT_TRUE(buyScrolls(3));
	EXPECT_EQ(countOf(a(), ANCIENT_COIN), 7 - 2 * 3);
	EXPECT_EQ(countOf(a(), DELVE_SCROLL), 3);
	EXPECT_EQ(kinah(a()), 5000) << "no kinah at a REWARD vendor";
	EXPECT_TRUE(ofOpcode(sent(), itemtest::SM_SYSTEM_MESSAGE_OPCODE).empty());
}

// :183-249: the sell reward of each line, a split stack (a new repurchase item) and a whole stack (the item itself, deleted with SELL), the
// buy-back list replaced, and the kinah with INC_KINAH_SELL
TEST_F(TradeServiceTest, SellingPaysTheRewardAndFillsTheBuyBackList) {
	setKinah(a(), A_KINAH, 1000);

	ASSERT_TRUE(sell({{POTIONS, 10}, {SWORD, 1}}));

	EXPECT_EQ(kinah(a()), 1000 + 10 * POTION_REWARD + SWORD_REWARD);
	// :246: the kinah is paid last, as one update of the kinah item with ItemUpdateType.INC_KINAH_SELL
	std::vector<std::vector<uint8_t>> updates = ofOpcode(sent(), itemtest::SM_INVENTORY_UPDATE_ITEM_OPCODE);
	ASSERT_FALSE(updates.empty());
	EXPECT_EQ(updates.back(),
		serializedFor(SM_INVENTORY_UPDATE_ITEM(a(), *a().getInventory().getKinahItem(), ItemPacketService_ItemUpdateType::INC_KINAH_SELL)));
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 90);
	EXPECT_FALSE(a().getInventory().getItemByObjId(SWORD));
	// the whole sword stack: deleted with ItemDeleteType.SELL (0x1F) and kept as its own repurchase item
	EXPECT_EQ(ofOpcode(sent(), itemtest::SM_DELETE_ITEM_OPCODE), cp::exactly({deleteItem(SWORD, 0x1F)}));
	runtime::Ptr<Item> potions = repurchaseItemOf(MINOR_LIFE_POTION);
	runtime::Ptr<Item> sword = repurchaseItemOf(TRAINING_SWORD);
	ASSERT_TRUE(potions);
	ASSERT_TRUE(sword);
	EXPECT_EQ(sword->getObjectId(), SWORD);
	EXPECT_EQ(sword->getRepurchasePrice(), SWORD_REWARD);
	EXPECT_NE(potions->getObjectId(), POTIONS) << "a split stack is a new item (ItemFactory.newItem)";
	EXPECT_EQ(potions->getItemCount(), 10);
	EXPECT_EQ(potions->getRepurchasePrice(), 10 * POTION_REWARD);
	EXPECT_EQ(repurchaseIds().size(), 2u);

	// RepurchaseService.addRepurchaseItems replaces the player's set on every sale
	ASSERT_TRUE(sell({{POTIONS, 1}}));
	EXPECT_EQ(repurchaseIds().size(), 1u);
	EXPECT_EQ(kinah(a()), 1000 + 11 * POTION_REWARD + SWORD_REWARD);
}

// the sell and buy-back ledger (m5c-plan.md T-04): selling 10 potions and buying them back restores the stack and the kinah
TEST_F(TradeServiceTest, BuyingBackTheSoldItemsRestoresTheLedger) {
	setKinah(a(), A_KINAH, 1000);
	ASSERT_TRUE(sell({{POTIONS, 10}}));
	runtime::Ptr<Item> sold = repurchaseItemOf(MINOR_LIFE_POTION);
	ASSERT_TRUE(sold);

	runtime::Ref<model::trade::RepurchaseList> list = model::trade::RepurchaseList::create(merchant().getObjectId());
	list->addRepurchaseItem(a(), sold->getObjectId(), 10);
	services::RepurchaseService::getInstance().repurchaseFromShop(a(), *list);

	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 100);
	EXPECT_EQ(kinah(a()), 1000);
	EXPECT_TRUE(repurchaseIds().empty());
}

// W-25 (m5c-plan.md T-04): a socketed sword sold and bought back keeps its mana stone (ItemService.copyItemInfo -> addManaStone)
TEST_F(TradeServiceTest, ASocketedItemComesBackFromTheBuyBackListWithItsManaStone) {
	constexpr int32_t SOCKETED = 800020;
	setKinah(a(), A_KINAH, 1000);
	Item& sword = give(a(), SOCKETED, SLOT_TEST_SWORD, 1);
	ASSERT_TRUE(services::item::ItemSocketService::addManaStone(runtime::Ptr<Item>(sword), MANASTONE_HP_20, false));
	ASSERT_TRUE(sell({{SOCKETED, 1}}));
	ASSERT_FALSE(a().getInventory().getItemByObjId(SOCKETED));

	runtime::Ref<model::trade::RepurchaseList> list = model::trade::RepurchaseList::create(merchant().getObjectId());
	list->addRepurchaseItem(a(), SOCKETED, 1);
	services::RepurchaseService::getInstance().repurchaseFromShop(a(), *list);

	std::vector<runtime::Ptr<Item>> back = a().getInventory().getItemsByItemId(SLOT_TEST_SWORD);
	ASSERT_EQ(back.size(), 1u);
	ASSERT_TRUE(back[0]->hasManaStones());
	auto stones = back[0]->getItemStones()->snapshot();
	ASSERT_EQ(stones.size(), 1u);
	EXPECT_EQ(stones[0]->getItemId(), MANASTONE_HP_20);
	EXPECT_EQ(kinah(a()), 1000);
}

// :194-221, :229-231: the refusals of a sale
TEST_F(TradeServiceTest, TheRefusalsOfASale) {
	setKinah(a(), A_KINAH, 1000);

	EXPECT_FALSE(sell({{999999, 1}})) << ":197-198 a fake object id";
	EXPECT_TRUE(sent().empty());

	runtime::Ptr<Item> juice = a().getInventory().getItemByObjId(JUICE);
	EXPECT_FALSE(sell({{JUICE, 1}})) << ":216-218 not sellable";
	EXPECT_EQ(sent(), cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_BUY_SELL_ITEM_CAN_NOT_BE_SELLED_TO_NPC(juice->getL10n()))}));
	EXPECT_EQ(countOf(a(), FRUIT_JUICE), 12);

	clearSent();
	network::test::LogCapture capture({AUDIT_LOGGER});
	EXPECT_FALSE(sell({{POTIONS, 101}})) << ":229-231 more than the stack";
	EXPECT_TRUE(capture.contains("tried to sell more items to npc than he has")) << capture.dump();
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 100);
	EXPECT_EQ(kinah(a()), 1000);
	EXPECT_TRUE(repurchaseIds().empty());
}

// :194-244: a refusal in the middle of a list returns after the earlier lines were taken and before anything is paid or listed for buy-back
// (pinned as Java has it; docs/deviations/P5-09b.md "M5c stage 1")
TEST_F(TradeServiceTest, ARefusedLineLeavesTheEarlierLinesSoldUnpaidAsInJava) {
	setKinah(a(), A_KINAH, 1000);

	EXPECT_FALSE(sell({{POTIONS, 10}, {JUICE, 1}}));

	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 90) << "the first line was taken";
	EXPECT_EQ(kinah(a()), 1000) << "and not paid";
	EXPECT_TRUE(repurchaseIds().empty()) << "nor listed for buy-back";
}

// :223-226 with the shipped sell limits (config/main/custom.properties: gameserver.limits.enable = true, enable_dynamic_cap = false;
// rates.properties:170: sell_limit 1.0, 2.0): PlayerLimitService.updateSellLimit (P-02) cuts a line to what the account's daily limit still
// pays, and a line it cannot pay at all stops the list (`break`): the earlier lines are paid and listed for buy-back, that line and every line
// after it stay unsold. The account's fresh limit is LIMIT_1_30's 5,300,047 (the fixture account's one level-1 character); the day's earlier
// sales are one updateSellLimit call that leaves 530. The limit map is static and only the LIMITS_UPDATE cron clears it: no other case of this
// executable sells with the limits on.
TEST_F(TradeServiceTest, TheDailySellLimitCutsALineAndStopsTheListAtTheFirstLineItCannotPay) {
	constexpr int32_t MORE_POTIONS = 800004;
	ConfigScope<bool> limitsOn(configs::main::CustomConfig::LIMITS_ENABLED, true);
	ConfigScope<bool> noDynamicCap(configs::main::CustomConfig::LIMITS_ENABLE_DYNAMIC_CAP, false);
	ConfigValueScope<std::vector<float>> sellLimitRates(configs::main::RatesConfig::SELL_LIMIT_RATES, {1.0f, 2.0f});
	setKinah(a(), A_KINAH, 1000);
	give(a(), MORE_POTIONS, MINOR_LIFE_POTION, 20);
	ASSERT_EQ(services::player::PlayerLimitService::updateSellLimit(a(), 5'300'047 - 530, 1), 1) << "the day's earlier sales";
	ASSERT_TRUE(sent().empty());

	// potions: 530 / 50 = 10 of the 15 are paid, 30 left; more potions: 30 / 50 = 0, the message and the end of the list; the sword (reward 1)
	// would still fit into 30
	EXPECT_TRUE(sell({{POTIONS, 15}, {MORE_POTIONS, 5}, {SWORD, 1}}));

	EXPECT_EQ(kinah(a()), 1000 + 10 * POTION_REWARD);
	EXPECT_EQ(a().getInventory().getItemByObjId(POTIONS)->getItemCount(), 90) << "the line cut to 10";
	EXPECT_EQ(a().getInventory().getItemByObjId(MORE_POTIONS)->getItemCount(), 20) << "the line the limit cannot pay";
	EXPECT_TRUE(a().getInventory().getItemByObjId(SWORD)) << "a line after it";
	EXPECT_EQ(ofOpcode(sent(), itemtest::SM_SYSTEM_MESSAGE_OPCODE), cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_DAY_CANNOT_SELL_NPC(30))}));
	EXPECT_EQ(repurchaseIds().size(), 1u);
	runtime::Ptr<Item> sold = repurchaseItemOf(MINOR_LIFE_POTION);
	ASSERT_TRUE(sold);
	EXPECT_EQ(sold->getItemCount(), 10);
	EXPECT_EQ(sold->getRepurchasePrice(), 10 * POTION_REWARD);
}

// :81-82, :188-189: PlayerRestrictions.canTrade - a trading player neither buys nor sells (W-28: STR_EXCHANGE_PARTNER_IS_EXCHANGING_WITH_OTHER)
TEST_F(TradeServiceTest, ATradingPlayerNeitherBuysNorSells) {
	setKinah(a(), A_KINAH, 5000);
	services::ExchangeService::getInstance().registerExchange(a(), partner());
	ASSERT_TRUE(a().isTrading());
	clearSent();
	const std::vector<std::vector<uint8_t>> refused =
		cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_EXCHANGE_PARTNER_IS_EXCHANGING_WITH_OTHER())});

	EXPECT_FALSE(buy({{MINOR_LIFE_ELIXIR, 1}}));
	EXPECT_EQ(sent(), refused);
	clearSent();
	EXPECT_FALSE(sell({{POTIONS, 1}}));
	EXPECT_EQ(sent(), refused);
	EXPECT_EQ(kinah(a()), 5000);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 100);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_ELIXIR), 0);
}

// :251-255: selling for AP is a custom feature, off unless gameserver.selling.apitems.enabled
TEST_F(TradeServiceTest, SellingForApIsRefusedWhileTheFeatureIsDisabled) {
	ConfigScope<bool> disabled(configs::main::CustomConfig::SELLING_APITEMS_ENABLED, false);
	runtime::Ref<TradeList> tradeList = TradeList::create(merchant().getObjectId());
	tradeList->addItem(POTIONS, 1);

	EXPECT_FALSE(TradeService::performSellForAPToShop(a(), *tradeList, nullptr));

	// PacketSendUtility.sendMessage: SM_MESSAGE(0, null, msg, GOLDEN_YELLOW)
	EXPECT_EQ(sent(), cp::exactly({serializedFor(network::aion::serverpackets::SM_MESSAGE(0, "", "This feature is disabled", model::ChatType::GOLDEN_YELLOW))}));
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 100);

	// enabled, the null purchase template of a merchant without one is Java's NullPointerException (:270)
	ConfigScope<bool> enabled(configs::main::CustomConfig::SELLING_APITEMS_ENABLED, true);
	clearSent();
	EXPECT_THROW(TradeService::performSellForAPToShop(a(), *tradeList, nullptr), runtime::NullPointerException);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 100);
}

// :288-302: the trade-in's first refusals (the trade-in itself is the capital economy's, m5c-plan.md D2): no npc target, an npc without a
// trade-in list
TEST_F(TradeServiceTest, TradeInNeedsATargetedTradeInNpc) {
	std::vector<int32_t> offered{POTIONS};
	EXPECT_FALSE(TradeService::performBuyFromTradeInTrade(a(), merchant().getObjectId(), EXTRACTION_TOOLS, 1, offered)) << "no target";
	a().setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(partner()));
	EXPECT_FALSE(TradeService::performBuyFromTradeInTrade(a(), merchant().getObjectId(), EXTRACTION_TOOLS, 1, offered)) << "a player target";
	a().setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(merchant()));
	EXPECT_FALSE(TradeService::performBuyFromTradeInTrade(a(), merchant().getObjectId(), EXTRACTION_TOOLS, 1, offered))
		<< "minalinerk has no trade_in_list_template: canTradeIn() is false";
	a().setTarget(nullptr);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 100);
	EXPECT_EQ(countOf(a(), EXTRACTION_TOOLS), 0);
}

// :292-295: a full cube is refused with STR_MSG_FULL_INVENTORY before the target is looked at
TEST_F(TradeServiceTest, TradeInIsRefusedWithAFullCube) {
	for (int32_t objId = 830000; a().getInventory().getFreeSlots() > 0; objId++)
		give(a(), objId, TRAINING_SWORD, 1);
	ASSERT_TRUE(a().getInventory().isFull());
	a().setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(merchant()));
	std::vector<int32_t> offered{POTIONS};
	clearSent();

	EXPECT_FALSE(TradeService::performBuyFromTradeInTrade(a(), merchant().getObjectId(), EXTRACTION_TOOLS, 1, offered));

	EXPECT_EQ(sent(), cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_FULL_INVENTORY())}));
	a().setTarget(nullptr);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 100);
}

} // namespace
} // namespace aion::gameserver::economy::test::trade
