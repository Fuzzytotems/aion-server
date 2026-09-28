// M5c K-01 (m5c-plan.md §5, P5-15): CM_BUY_ITEM (C_BUY_SELL), the client's trade with a shop, a private store or a merchant pet - buy from an
// npc, sell to it, buy back from it, buy from a player's private store by the index of the seller's list, sell to a pet.
//
// Java: CM_BUY_ITEM.java:47-143. The read cases lay each body out field by field from the Java readImpl (D seller, H action, UH amount, then per
// item D id and Q count) with every audit bound: an amount outside [0, 36], a negative count, a count above 20,000 and an item id of 0 or
// below outside the private store (action 0). The run cases drive runImpl against the economy packet fixture (EconomyPacketTestSupport.h),
// one npc per combination of the three predicates the arms ask (Npc.java:361-384: canSell = a trade row and the BUY dialog, canBuy = the SELL
// dialog or canSell, canPurchase = a purchase row and the TRADE_SELL_LIST dialog): minalinerk 798007 (all but canPurchase: BUY and SELL,
// npc_trade_list.xml:2186-2189 and its two goods tabs), mogironerk 798088 (canBuy alone: BUY and SELL without a trade row), piarinerk 801531
// (canPurchase alone: the purchase row :9874-9876 and purchase list 13), baevrunerk 798008 (none: a trade row but neither dialog) and lalrinerk
// 833548 (canSell and canBuy, but LEVEL_HIGH 55: DialogService.isInteractionAllowed refuses the level-1 player), and a second player's private
// store. The arms reach the trade lane's TradeService (T-01), PrivateStoreService (T-03) and the player-items lane's
// RepurchaseService.repurchaseFromShop (P-03); the ledgers are TradeServiceTest's numbers (the fixture's prices).
//
// Also here: the private store window of CM_DIALOG_SELECT with a player target (PlayerController.onDialogSelect's BUY arm,
// PlayerController.java:555-557), which the trade lane handed to K-01, since a buyer opens the store window with it before he buys.
//
// NOT COVERED, and named: the pet arm (action 17 at a merchant pet: a Pet needs the pet data, a master and its commons data; the arm is one
// call, TradeService.performSellToShop with the pet function's rate, whose ledger TradeServiceTest covers with the vendor rate) and the ABYSS
// purchase-template arm of action 1 (TradeService.performSellForAPToShop reaches the unported AbyssPointsService.addAp,
// docs/deviations/P5-09b.md); the other purchase templates are covered at piarinerk.

#include "EconomyPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/PrivateStore.h"
#include "aion/gameserver/model/trade/RepurchaseList.h"
#include "aion/gameserver/model/trade/TradeItem.h"
#include "aion/gameserver/model/trade/TradeList.h"
#include "aion/gameserver/model/trade/TradePSItem.h"
#include "aion/gameserver/network/aion/clientpackets/CM_BUY_ITEM.h"
#include "aion/gameserver/network/aion/clientpackets/CM_DIALOG_SELECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PRIVATE_STORE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/PrivateStoreService.h"
#include "aion/gameserver/services/RepurchaseService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_BUY_ITEM_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friend CM_BUY_ITEM.h declares: the fields readImpl decoded, which Java keeps private */
struct CM_BUY_ITEMTestAccess {
	static int32_t sellerObjId(const CM_BUY_ITEM& p) { return p.sellerObjId; }
	static int16_t tradeActionId(const CM_BUY_ITEM& p) { return p.tradeActionId; }
	static int32_t amount(const CM_BUY_ITEM& p) { return p.amount; }
	static int32_t itemId(const CM_BUY_ITEM& p) { return p.itemId; }
	static int64_t count(const CM_BUY_ITEM& p) { return p.count; }
	static bool isAudit(const CM_BUY_ITEM& p) { return p.isAudit; }
	static runtime::Ptr<model::trade::TradeList> tradeList(const CM_BUY_ITEM& p) { return runtime::Ptr<model::trade::TradeList>(p.tradeList); }
	static runtime::Ptr<model::trade::RepurchaseList> repurchaseList(const CM_BUY_ITEM& p) {
		return runtime::Ptr<model::trade::RepurchaseList>(p.repurchaseList);
	}
};

namespace testing::items {
namespace {

using Access = CM_BUY_ITEMTestAccess;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using model::trade::TradePSItem;
using network::test::LogCapture;
using serverpackets::SM_SYSTEM_MESSAGE;

const char* CM_BUY_ITEM_LOGGER = "com.aionemu.gameserver.network.aion.clientpackets.CM_BUY_ITEM";

/** the decoded opcodes of ClientPacketInfo.gen.inc (Java AionClientPacketFactory.java:79, :82; State.IN_GAME) */
constexpr int32_t CM_BUY_ITEM_OPCODE = 51;
constexpr int32_t CM_DIALOG_SELECT_OPCODE = 54;

constexpr int32_t POTIONS = 800001; // 100 x Minor Life Potion (price 250, sellable)
constexpr int32_t SWORD = 800002;   // 1 x Training Sword
constexpr int32_t POWDER = 800003;  // Worn Metal Powder (price 2200), on piarinerk's purchase list only
constexpr int32_t A_KINAH = 800009;
constexpr int32_t B_POTIONS = 810001;
constexpr int32_t B_SWORD = 810002;
constexpr int32_t B_KINAH = 810009;
constexpr int32_t SELLER = 710303;

/** C_BUY_SELL: D seller, H action, UH amount, then D item id and Q count per item (CM_BUY_ITEM.java:47-66) */
std::vector<uint8_t> buyBody(int32_t seller, int32_t action, std::initializer_list<std::pair<int32_t, int64_t>> itemCounts) {
	PacketWriter writer;
	writer.D(seller).H(action).H(static_cast<int32_t>(itemCounts.size()));
	for (const auto& [itemId, count] : itemCounts)
		writer.D(itemId).Q(count);
	return writer.data;
}

/** The trade list's lines as {item id, count}, in order */
std::vector<std::pair<int32_t, int64_t>> linesOf(runtime::Ptr<model::trade::TradeList> tradeList) {
	std::vector<std::pair<int32_t, int64_t>> lines;
	for (runtime::Ptr<model::trade::TradeItem> item : tradeList->getTradeItems())
		lines.emplace_back(item->getItemId(), item->getCount());
	return lines;
}

using Lines = std::vector<std::pair<int32_t, int64_t>>;

class BuyItemPacketTest : public EconomyPacketTest {
protected:
	void SetUp() override {
		EconomyPacketTest::SetUp();
		stored(POTIONS, MINOR_LIFE_POTION, 100);
		stored(SWORD, TRAINING_SWORD, 1);
	}

	std::unique_ptr<EconomyDriver<CM_BUY_ITEM>> read(const std::vector<uint8_t>& body) { return readPacket<CM_BUY_ITEM>(CM_BUY_ITEM_OPCODE, body); }

	void buy(const std::vector<uint8_t>& body) { readAndRun<CM_BUY_ITEM>(CM_BUY_ITEM_OPCODE, body); }

	void setKinah(int64_t count) { stored(A_KINAH, KINAH, count); }

	/** The potions of A's buy-back list (a split stack is a new item) */
	runtime::Ptr<Item> repurchasedPotions() {
		for (const runtime::Ptr<Item>& item : services::RepurchaseService::getInstance().getRepurchaseItems(player().getObjectId()))
			if (item->getItemId() == MINOR_LIFE_POTION)
				return item;
		return nullptr;
	}
};

// --- readImpl ---------------------------------------------------------------------------------------------------------------------------------

TEST_F(BuyItemPacketTest, ASaleListReadsTheSellerTheActionAndEachItemsIdAndCount) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER, AUDIT_LOGGER});
	auto p = read(buyBody(0x01020304, 1, {{0x0A0B0C0D, 19999}, {7, 20000}}));

	EXPECT_EQ(Access::sellerObjId(*p), 0x01020304);
	EXPECT_EQ(Access::tradeActionId(*p), 1);
	EXPECT_EQ(Access::amount(*p), 2);
	EXPECT_EQ(Access::itemId(*p), 7) << "the last item read";
	EXPECT_EQ(Access::count(*p), 20000) << "20,000 is the largest count that is not audited";
	EXPECT_FALSE(Access::isAudit(*p));
	ASSERT_TRUE(Access::tradeList(*p));
	EXPECT_EQ(Access::tradeList(*p)->getSellerObjId(), 0x01020304);
	EXPECT_EQ(linesOf(Access::tradeList(*p)), (Lines{{0x0A0B0C0D, 19999}, {7, 20000}}));
	EXPECT_FALSE(Access::repurchaseList(*p)) << "only action 2 builds a repurchase list";
	EXPECT_EQ(p->unread(), 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
	EXPECT_FALSE(capture.contains("might be abusing")) << capture.dump();
}

TEST_F(BuyItemPacketTest, TheCountIsALongAndEveryListActionAddsTheLine) {
	// the actions of the Java switch that fill the trade list (:75-83): 0, 1, 13, 14, 15, 16, 17
	for (int32_t action : {0, 1, 13, 14, 15, 16, 17}) {
		auto p = read(buyBody(SELLER, action, {{MINOR_LIFE_ELIXIR, 0}, {EXTRACTION_TOOLS, 3}}));
		EXPECT_FALSE(Access::isAudit(*p)) << action;
		EXPECT_EQ(linesOf(Access::tradeList(*p)), (Lines{{MINOR_LIFE_ELIXIR, 0}, {EXTRACTION_TOOLS, 3}})) << action;
		EXPECT_EQ(p->unread(), 0) << action;
	}
	// a count whose high word is set is read as the long it is, and audited (above 20,000) rather than cut to its low word
	LogCapture capture({AUDIT_LOGGER});
	auto p = read(buyBody(SELLER, 13, {{MINOR_LIFE_ELIXIR, 0x100000001LL}}));
	EXPECT_TRUE(Access::isAudit(*p));
	EXPECT_EQ(Access::count(*p), 0x100000001LL);
	EXPECT_TRUE(capture.contains("might be abusing CM_BUY_ITEM item: 162000052 count: 4294967297")) << capture.dump();
}

TEST_F(BuyItemPacketTest, AnActionOutsideTheSwitchReadsTheItemsButListsNone) {
	auto p = read(buyBody(SELLER, 5, {{MINOR_LIFE_ELIXIR, 1}}));

	EXPECT_FALSE(Access::isAudit(*p));
	ASSERT_TRUE(Access::tradeList(*p)) << "every action but 2 gets a trade list";
	EXPECT_TRUE(linesOf(Access::tradeList(*p)).empty());
	EXPECT_EQ(Access::itemId(*p), MINOR_LIFE_ELIXIR);
	EXPECT_EQ(p->unread(), 0);
}

TEST_F(BuyItemPacketTest, TheAmountIsAnUnsignedShortAndMoreThan36IsAudited) {
	LogCapture capture({AUDIT_LOGGER});
	// 0xFFFF is 65,535 (readUH), not -1
	auto p = read(PacketWriter().D(SELLER).H(13).H(0xFFFF).data);
	EXPECT_EQ(Access::amount(*p), 65535);
	EXPECT_TRUE(Access::isAudit(*p));
	EXPECT_TRUE(capture.contains(player().toString() + " might be abusing CM_BUY_ITEM amount: 65535")) << capture.dump();
	EXPECT_FALSE(Access::tradeList(*p)) << ":53-57: the audit returns before the list is created";

	// 37: audited, and nothing more is read
	std::vector<uint8_t> body = PacketWriter().D(SELLER).H(13).H(37).data;
	for (int32_t i = 1; i <= 37; i++)
		body.insert(body.end(), {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
	p = read(body);
	EXPECT_TRUE(Access::isAudit(*p));
	EXPECT_TRUE(capture.contains("might be abusing CM_BUY_ITEM amount: 37")) << capture.dump();
	EXPECT_EQ(p->unread(), 37 * 12);

	// 36: the largest amount that is read
	PacketWriter writer;
	writer.D(SELLER).H(13).H(36);
	for (int32_t i = 1; i <= 36; i++)
		writer.D(i).Q(1);
	p = read(writer.data);
	EXPECT_FALSE(Access::isAudit(*p));
	EXPECT_EQ(Access::amount(*p), 36);
	EXPECT_EQ(linesOf(Access::tradeList(*p)).size(), 36u);
	EXPECT_EQ(p->unread(), 0);
	EXPECT_EQ(capture.count("might be abusing"), 2) << capture.dump();
}

TEST_F(BuyItemPacketTest, ANegativeCountIsAuditedAndEndsTheList) {
	LogCapture capture({AUDIT_LOGGER});

	auto p = read(buyBody(SELLER, 13, {{MINOR_LIFE_ELIXIR, 1}, {EXTRACTION_TOOLS, -1}, {MINOR_LIFE_ELIXIR, 2}}));

	EXPECT_TRUE(Access::isAudit(*p));
	EXPECT_EQ(Access::itemId(*p), EXTRACTION_TOOLS);
	EXPECT_EQ(Access::count(*p), -1);
	EXPECT_TRUE(capture.contains(player().toString() + " might be abusing CM_BUY_ITEM item: 165000001 count: -1")) << capture.dump();
	EXPECT_EQ(linesOf(Access::tradeList(*p)), (Lines{{MINOR_LIFE_ELIXIR, 1}})) << ":69-73: the audit breaks out before the line is added";
	EXPECT_EQ(p->unread(), 12) << "the third item is never read";
}

TEST_F(BuyItemPacketTest, ACountAbove20000IsAudited) {
	LogCapture capture({AUDIT_LOGGER});

	auto p = read(buyBody(SELLER, 1, {{POTIONS, 20001}}));

	EXPECT_TRUE(Access::isAudit(*p));
	EXPECT_TRUE(capture.contains("might be abusing CM_BUY_ITEM item: 800001 count: 20001")) << capture.dump();
	EXPECT_TRUE(linesOf(Access::tradeList(*p)).empty());
}

TEST_F(BuyItemPacketTest, AnItemIdOfZeroOrBelowIsAuditedExceptAsAPrivateStoreIndex) {
	LogCapture capture({AUDIT_LOGGER});

	auto store = read(buyBody(SELLER, 0, {{0, 1}}));
	EXPECT_FALSE(Access::isAudit(*store)) << "action 0: the id is the index into the seller's list";
	EXPECT_EQ(linesOf(Access::tradeList(*store)), (Lines{{0, 1}}));

	auto shop = read(buyBody(SELLER, 1, {{0, 1}}));
	EXPECT_TRUE(Access::isAudit(*shop));
	EXPECT_TRUE(capture.contains("might be abusing CM_BUY_ITEM item: 0 count: 1")) << capture.dump();

	auto negative = read(buyBody(SELLER, 13, {{-5, 1}}));
	EXPECT_TRUE(Access::isAudit(*negative));
	EXPECT_TRUE(capture.contains("might be abusing CM_BUY_ITEM item: -5 count: 1")) << capture.dump();
	EXPECT_EQ(capture.count("might be abusing"), 2) << capture.dump();
}

TEST_F(BuyItemPacketTest, ABuyBackListKeepsOnlyTheIdsOfThePlayersBuyBackItems) {
	runtime::Ref<Item> sold = loadedItem(900001, MINOR_LIFE_POTION, 10, StorageType::CUBE);
	services::RepurchaseService::getInstance().addRepurchaseItems(player(), {runtime::Ptr<Item>(sold)});

	auto p = read(buyBody(SELLER, 2, {{900001, 10}, {900002, 1}}));

	EXPECT_FALSE(Access::isAudit(*p));
	EXPECT_FALSE(Access::tradeList(*p)) << "action 2 builds no trade list";
	ASSERT_TRUE(Access::repurchaseList(*p));
	EXPECT_EQ(Access::repurchaseList(*p)->getSellerObjId(), SELLER);
	std::vector<int32_t> ids;
	for (int32_t id : Access::repurchaseList(*p)->getRepurchaseItems())
		ids.push_back(id);
	EXPECT_EQ(ids, (std::vector<int32_t>{900001})) << "RepurchaseList.addRepurchaseItem asks RepurchaseService.canRepurchase";
}

TEST_F(BuyItemPacketTest, TheMarkerRegistersTheClassUnderItsJavaOpcode) {
	EXPECT_NE(dynamic_cast<CM_BUY_ITEM*>(CM_BUY_ITEM_clientPacketFactory(CM_BUY_ITEM_OPCODE, StateSet{AionConnection_State::IN_GAME}).get()),
		nullptr);
	EXPECT_EQ(economyTableEntries("CM_BUY_ITEM", CM_BUY_ITEM_OPCODE), 1);
}

// --- runImpl ----------------------------------------------------------------------------------------------------------------------------------

// :126-131: actions 13 to 16 buy from a merchant that sells (TradeService.performBuyFromShop)
TEST_F(BuyItemPacketTest, EachBuyActionBuysFromAMerchantThatSells) {
	Npc& merchant = npcAt(MINALINERK, 103.0f);
	setKinah(5000);

	buy(buyBody(merchant.getObjectId(), 13, {{MINOR_LIFE_ELIXIR, 2}, {EXTRACTION_TOOLS, 1}}));

	EXPECT_EQ(kinahOf(player()), 5000 - 2 * ELIXIR_PRICE - TOOLS_PRICE);
	EXPECT_EQ(countOf(player(), MINOR_LIFE_ELIXIR), 2);
	EXPECT_EQ(countOf(player(), EXTRACTION_TOOLS), 1);

	int64_t kinah = kinahOf(player());
	for (int32_t action : {14, 15, 16}) {
		buy(buyBody(merchant.getObjectId(), action, {{MINOR_LIFE_ELIXIR, 1}}));
		kinah -= ELIXIR_PRICE;
		EXPECT_EQ(kinahOf(player()), kinah) << action;
	}
	EXPECT_EQ(countOf(player(), MINOR_LIFE_ELIXIR), 5);
}

// :113-125: sell to the merchant (performSellToShop without a purchase template: minalinerk has none), then buy the sold part back
// (RepurchaseService.repurchaseFromShop) - the ledger is restored
TEST_F(BuyItemPacketTest, SellingPaysTheRewardAndBuyingBackRestoresTheLedger) {
	Npc& merchant = npcAt(MINALINERK, 103.0f);
	setKinah(1000);

	buy(buyBody(merchant.getObjectId(), 1, {{POTIONS, 10}}));

	EXPECT_EQ(kinahOf(player()), 1000 + 10 * POTION_REWARD);
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 90);
	runtime::Ptr<Item> sold = repurchasedPotions();
	ASSERT_TRUE(sold);
	EXPECT_EQ(sold->getItemCount(), 10);

	buy(buyBody(merchant.getObjectId(), 2, {{sold->getObjectId(), 10}}));

	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 100);
	EXPECT_EQ(kinahOf(player()), 1000);
	EXPECT_FALSE(repurchasedPotions()) << "bought back";
}

// :114, :123, :130: a merchant that neither sells nor buys takes no list - baevrunerk has a shipped trade row (:2190-2192, goods list 132 with
// the Extraction Tools), but neither the BUY nor the SELL dialog (EXTEND_INVENTORY only), so Npc.canSell and canBuy are both false
TEST_F(BuyItemPacketTest, AnNpcThatDoesNotTradeTakesNoList) {
	Npc& expander = npcAt(BAEVRUNERK, 103.0f);
	setKinah(5000);
	runtime::Ref<Item> sold = loadedItem(900001, MINOR_LIFE_POTION, 10, StorageType::CUBE);
	sold->setRepurchasePrice(500);
	services::RepurchaseService::getInstance().addRepurchaseItems(player(), {runtime::Ptr<Item>(sold)});
	LogCapture capture({AUDIT_LOGGER, CM_BUY_ITEM_LOGGER});

	buy(buyBody(expander.getObjectId(), 13, {{EXTRACTION_TOOLS, 1}}));
	buy(buyBody(expander.getObjectId(), 1, {{POTIONS, 10}}));
	buy(buyBody(expander.getObjectId(), 2, {{900001, 10}}));

	EXPECT_EQ(kinahOf(player()), 5000);
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 100);
	EXPECT_EQ(countOf(player(), EXTRACTION_TOOLS), 0) << "on his goods list, but he does not sell";
	EXPECT_TRUE(repurchasedPotions()) << "not bought back";
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(capture.dump(), "") << "no audit, no unknown action";
}

// :107-110: an npc whose isInteractionAllowed refuses the player (lalrinerk: LEVEL_HIGH 55) is audited, and the body returns before any arm -
// lalrinerk has BUY and SELL and a trade row (canSell and canBuy are true), so each arm would run without the return: the sale would pay 500,
// the buy-back would take the potions back, the buy would be answered STR_BUY_SELL_USER_BUY_FAILED
TEST_F(BuyItemPacketTest, AnNpcThatRefusesThePlayerIsAudited) {
	Npc& lalrinerk = npcAt(LALRINERK, 103.0f);
	setKinah(5000);
	runtime::Ref<Item> sold = loadedItem(900001, MINOR_LIFE_POTION, 10, StorageType::CUBE);
	sold->setRepurchasePrice(500);
	services::RepurchaseService::getInstance().addRepurchaseItems(player(), {runtime::Ptr<Item>(sold)});
	LogCapture capture({AUDIT_LOGGER});

	buy(buyBody(lalrinerk.getObjectId(), 13, {{MINOR_LIFE_ELIXIR, 1}}));
	buy(buyBody(lalrinerk.getObjectId(), 1, {{POTIONS, 10}}));
	buy(buyBody(lalrinerk.getObjectId(), 2, {{900001, 10}}));

	EXPECT_EQ(capture.count(player().toString() + " might be abusing CM_BUY_ITEM: no right trading with " + lalrinerk.toString()), 3)
		<< capture.dump();
	EXPECT_EQ(kinahOf(player()), 5000);
	EXPECT_EQ(countOf(player(), MINOR_LIFE_ELIXIR), 0);
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 100);
	EXPECT_TRUE(repurchasedPotions()) << "not bought back";
	EXPECT_TRUE(sent().empty());
}

// :113-114, :122-123, :129-130 at an npc for which Npc.canBuy is true through the SELL dialog alone and canSell is false (mogironerk: BUY and
// SELL, no trade row): he takes a sale (performSellToShop, no purchase template) and gives the buy-back, but sells nothing
TEST_F(BuyItemPacketTest, AMerchantWithoutGoodsBuysAndGivesBackButSellsNothing) {
	Npc& mogironerk = npcAt(MOGIRONERK, 103.0f);
	setKinah(1000);

	buy(buyBody(mogironerk.getObjectId(), 1, {{POTIONS, 10}}));

	EXPECT_EQ(kinahOf(player()), 1000 + 10 * POTION_REWARD);
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 90);
	runtime::Ptr<Item> sold = repurchasedPotions();
	ASSERT_TRUE(sold);

	buy(buyBody(mogironerk.getObjectId(), 2, {{sold->getObjectId(), 10}}));

	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 100);
	EXPECT_EQ(kinahOf(player()), 1000);
	EXPECT_FALSE(repurchasedPotions()) << "bought back";

	clearSent();
	buy(buyBody(mogironerk.getObjectId(), 13, {{EXTRACTION_TOOLS, 1}}));

	EXPECT_EQ(kinahOf(player()), 1000);
	EXPECT_EQ(countOf(player(), EXTRACTION_TOOLS), 0);
	EXPECT_TRUE(sent().empty()) << "performBuyFromShop never runs";
}

// :113-118 through Npc.canPurchase alone (piarinerk: TRADE_SELL_LIST only; purchase row :9874-9876, buy_price_rate 50, purchase list 13):
// performSellToShop gets the purchase template, so an item on its list pays (long) (price * rate / 100D) and no vendor sell modifier, and an
// item off the list refuses the whole sale without a message (TradeService.java:202-214); piarinerk neither sells nor gives back
TEST_F(BuyItemPacketTest, APurchaseListMerchantBuysOnlyItsItemsAtItsRate) {
	Npc& piarinerk = npcAt(PIARINERK, 103.0f);
	stored(POWDER, WORN_METAL_POWDER, 5);
	setKinah(1000);

	buy(buyBody(piarinerk.getObjectId(), 1, {{POTIONS, 10}}));

	EXPECT_EQ(kinahOf(player()), 1000) << "Minor Life Potion is not on purchase list 13";
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 100);
	EXPECT_FALSE(repurchasedPotions());
	EXPECT_TRUE(sent().empty());

	buy(buyBody(piarinerk.getObjectId(), 1, {{POWDER, 3}}));

	EXPECT_EQ(kinahOf(player()), 1000 + 3 * POWDER_PURCHASE_REWARD);
	EXPECT_EQ(countOf(player(), WORN_METAL_POWDER), 2);
	std::unordered_set<runtime::Ptr<Item>> buyBackSet = services::RepurchaseService::getInstance().getRepurchaseItems(player().getObjectId());
	std::vector<runtime::Ptr<Item>> buyBack(buyBackSet.begin(), buyBackSet.end());
	ASSERT_EQ(buyBack.size(), 1u);
	EXPECT_EQ(buyBack[0]->getItemId(), WORN_METAL_POWDER);
	EXPECT_EQ(buyBack[0]->getRepurchasePrice(), 3 * POWDER_PURCHASE_REWARD);

	clearSent();
	buy(buyBody(piarinerk.getObjectId(), 2, {{buyBack[0]->getObjectId(), 3}}));
	buy(buyBody(piarinerk.getObjectId(), 13, {{EXTRACTION_TOOLS, 1}}));

	EXPECT_EQ(kinahOf(player()), 1000 + 3 * POWDER_PURCHASE_REWARD);
	EXPECT_EQ(countOf(player(), WORN_METAL_POWDER), 2) << "not bought back: Npc.canBuy is false";
	EXPECT_EQ(countOf(player(), EXTRACTION_TOOLS), 0);
	EXPECT_TRUE(sent().empty());
}

// :133-135: every other action at an npc is logged (0 and 17 fill a list in readImpl but have no npc arm)
TEST_F(BuyItemPacketTest, AnyOtherActionAtAnNpcIsLoggedAsUnknown) {
	Npc& merchant = npcAt(MINALINERK, 103.0f);
	setKinah(5000);
	LogCapture capture({CM_BUY_ITEM_LOGGER});

	buy(buyBody(merchant.getObjectId(), 3, {{MINOR_LIFE_ELIXIR, 1}}));
	buy(buyBody(merchant.getObjectId(), 17, {{MINOR_LIFE_ELIXIR, 1}}));
	buy(buyBody(merchant.getObjectId(), 0, {{0, 1}}));

	EXPECT_TRUE(capture.contains("Unknown shop action: 3")) << capture.dump();
	EXPECT_TRUE(capture.contains("Unknown shop action: 17")) << capture.dump();
	EXPECT_TRUE(capture.contains("Unknown shop action: 0")) << capture.dump();
	EXPECT_EQ(kinahOf(player()), 5000);
}

// :96-97: an audited list is never run - here the first line was read and listed before the second was audited
TEST_F(BuyItemPacketTest, AnAuditedListIsNotRun) {
	Npc& merchant = npcAt(MINALINERK, 103.0f);
	setKinah(5000);

	buy(buyBody(merchant.getObjectId(), 13, {{MINOR_LIFE_ELIXIR, 1}, {EXTRACTION_TOOLS, 20001}}));

	EXPECT_EQ(kinahOf(player()), 5000);
	EXPECT_EQ(countOf(player(), MINOR_LIFE_ELIXIR), 0);
	EXPECT_TRUE(sent().empty());
}

// :96-97: a connection without an active player runs nothing (readImpl audited no line: the list is valid)
TEST_F(BuyItemPacketTest, AConnectionWithoutAPlayerRunsNothing) {
	Npc& merchant = npcAt(MINALINERK, 103.0f);
	setKinah(5000);
	TestClient loggedOut; // a connection that never entered the world: getActivePlayer() is null

	EconomyDriver<CM_BUY_ITEM> packet(CM_BUY_ITEM_OPCODE);
	ASSERT_TRUE(packet.readOn(buyBody(merchant.getObjectId(), 13, {{MINOR_LIFE_ELIXIR, 1}}), loggedOut.get()));
	EXPECT_NO_THROW(packet.runNow());

	EXPECT_EQ(kinahOf(player()), 5000);
}

// :104-105: a player target sells from his private store for action 0 only; the "item id" is the index into his list (PrivateStoreService,
// T-03). The buyer first opens the store window with CM_DIALOG_SELECT(BUY) on the seller (PlayerController.onDialogSelect: SM_PRIVATE_STORE)
TEST_F(BuyItemPacketTest, APlayersPrivateStoreSellsForAction0Only) {
	EconomyConfigScope<int8_t> dialogInfo{configs::administration::AdminConfig::DIALOG_INFO, 9}; // the Java default: no staff message
	OtherPlayer& seller = otherPlayer(SELLER, "Seller", model::Race::ELYOS, 102.0f, true);
	giveTo(seller.player(), B_SWORD, TRAINING_SWORD, 1);
	giveTo(seller.player(), B_POTIONS, MINOR_LIFE_POTION, 30);
	giveTo(seller.player(), B_KINAH, KINAH, 1000);
	std::vector<runtime::Ref<TradePSItem>> rows{
		TradePSItem::create(B_SWORD, TRAINING_SWORD, 1, 5000), TradePSItem::create(B_POTIONS, MINOR_LIFE_POTION, 30, 100)};
	std::vector<runtime::Ptr<TradePSItem>> rowPtrs(rows.begin(), rows.end());
	services::PrivateStoreService::createStoreWithItems(seller.player(), rowPtrs);
	ASSERT_TRUE(seller.player().getStore());
	setKinah(10000);
	clearSent();

	Driver<CM_DIALOG_SELECT> dialog(CM_DIALOG_SELECT_OPCODE);
	dialog.readAndRun(PacketWriter().D(SELLER).H(model::DialogAction::BUY).H(0).H(0).D(0).H(0).data, client->get());
	EXPECT_EQ(sent(), exactly({serializedFor(serverpackets::SM_PRIVATE_STORE(seller.player().getStore(), player()))}));

	// action 1 with a player target: not a private store purchase, and no npc or pet arm
	buy(buyBody(SELLER, 1, {{1, 10}}));
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 100);
	EXPECT_EQ(kinahOf(player()), 10000);

	// action 0: index 1 is the second row, the potions
	buy(buyBody(SELLER, 0, {{1, 10}}));
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 110);
	EXPECT_EQ(countOf(seller.player(), MINOR_LIFE_POTION), 20);
	EXPECT_EQ(kinahOf(player()), 10000 - 10 * 100);
	EXPECT_EQ(kinahOf(seller.player()), 1000 + 10 * 100);
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
