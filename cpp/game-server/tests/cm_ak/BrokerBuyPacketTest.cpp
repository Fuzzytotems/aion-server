// CM_BUY_BROKER_ITEM (P5-15 with P5-09b's BrokerService.buyBrokerItem and putToSettled, the broker lane of 2026-10-18), read and run against
// the offers of the test database (BrokerPacketTestSupport.h).
//
// Java: CM_BUY_BROKER_ITEM.java and BrokerService.java:222-323. Expectations are derived by hand from those bodies.

#include "BrokerPacketTestSupport.h"

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/network/aion/clientpackets/CM_BROKER_LIST.h"
#include "aion/gameserver/network/aion/clientpackets/CM_BROKER_SEARCH.h"
#include "aion/gameserver/network/aion/clientpackets/CM_BUY_BROKER_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BROKER_SERVICE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/ExchangeService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

/** The friend CM_BUY_BROKER_ITEM.h declares: the fields readImpl decoded */
struct CM_BUY_BROKER_ITEMTestAccess {
	static int32_t brokerObjId(const CM_BUY_BROKER_ITEM& p) { return p.brokerObjId; }
	static int32_t itemUniqueId(const CM_BUY_BROKER_ITEM& p) { return p.itemUniqueId; }
	static int64_t itemCount(const CM_BUY_BROKER_ITEM& p) { return p.itemCount; }
};

namespace testing::items {
namespace {

using serverpackets::SM_BROKER_SERVICE;
using serverpackets::SM_SYSTEM_MESSAGE;

TEST(BrokerBuyReadTest, TheBodyIsReadAsJavaReadsIt) {
	EconomyDriver<CM_BUY_BROKER_ITEM> buy(CM_BUY_BROKER_ITEM_OPCODE);
	ASSERT_TRUE(buy.readOn(PacketWriter().D(5).D(6).Q(0x0102030405060708LL).data, nullptr));
	EXPECT_EQ(CM_BUY_BROKER_ITEMTestAccess::brokerObjId(buy), 5);
	EXPECT_EQ(CM_BUY_BROKER_ITEMTestAccess::itemUniqueId(buy), 6);
	EXPECT_EQ(CM_BUY_BROKER_ITEMTestAccess::itemCount(buy), 0x0102030405060708LL);
	EXPECT_EQ(buy.getRemainingBytes(), 0);
}

// ---- CM_BUY_BROKER_ITEM --------------------------------------------------------------------------------------------------------------------

class BrokerBuyTest : public BrokerPacketTest {
protected:
	void buy(int32_t offerId, int64_t count, int32_t brokerId = 0) {
		readAndRun<CM_BUY_BROKER_ITEM>(CM_BUY_BROKER_ITEM_OPCODE, PacketWriter().D(brokerId != 0 ? brokerId : broker->getObjectId()).D(offerId).Q(count).data);
	}

	/** the seller online with his own connection (putToSettled sends him the settled icon) */
	OtherPlayer& onlineSeller() { return otherPlayer(SELLER_ID, "Seller"); }
};

TEST_F(BrokerBuyTest, AWholeOfferIsBoughtSettledForTheSellerAndStored) {
	BROKER_PACKET_REQUIRE_DATABASE();
	offer(9601, MINOR_LIFE_POTION, 5, 10, SELLER_ID);
	offer(9602, MINOR_LIFE_POTION, 1, 99, SELLER_ID);
	owned(9699, KINAH, 1000);
	targetBroker();
	OtherPlayer& seller = onlineSeller();
	readAndRun<CM_BROKER_LIST>(CM_BROKER_LIST_OPCODE, listBody(broker->getObjectId(), SORT_PRICE_DESC, 0, MASK_POTION));
	clearSent();

	buy(9601, 5);

	// BrokerService.java:249-293: 5 * 10 kinah, the offer removed and settled as sold, the item (the same object) in the buyer's cube
	EXPECT_EQ(kinahOf(player()), 950);
	runtime::Ptr<Item> bought = storage(StorageType::CUBE).getItemByObjId(9601);
	ASSERT_TRUE(bought);
	EXPECT_EQ(bought->getItemCount(), 5);
	// putToSettled (:318-321): the online seller sees the settled icon with his earnings
	EXPECT_EQ(seller.sent(), std::vector<std::vector<uint8_t>>{seller.serializedFor(SM_BROKER_SERVICE(true, int64_t{50}))});
	// :295-296: the buyer's list again, from the cached mask, sort and page: without the bought offer
	SearchedItems page = searched();
	EXPECT_EQ(page.ids(), (std::vector<int32_t>{9602}));

	saved();
	EXPECT_EQ(maildb::queryLong("SELECT is_sold FROM broker WHERE item_pointer = 9601"), 1);
	EXPECT_EQ(maildb::queryLong("SELECT is_settled FROM broker WHERE item_pointer = 9601"), 1);
	EXPECT_EQ(maildb::queryLong("SELECT item_owner FROM inventory WHERE item_unique_id = 9601"), HOLDER_ID);
	EXPECT_EQ(maildb::queryLong("SELECT item_location FROM inventory WHERE item_unique_id = 9601"), 0) << "the cube";
	EXPECT_EQ(maildb::queryLong("SELECT item_count FROM inventory WHERE item_unique_id = 9699"), 950);
}

TEST_F(BrokerBuyTest, PartOfASplittableOfferIsANewStackAndTheRestStaysOffered) {
	BROKER_PACKET_REQUIRE_DATABASE();
	offer(9603, MINOR_LIFE_POTION, 10, 7, SELLER_ID, "ELYOS", true);
	owned(9699, KINAH, 1000);
	targetBroker();
	OtherPlayer& seller = onlineSeller();

	buy(9603, 4);

	// :259-271: the offer keeps 6, a new BrokerItem of a new item of 4 is settled as sold
	EXPECT_EQ(kinahOf(player()), 972);
	EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(9603)) << "the offer's item stays with the broker";
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 4);
	EXPECT_EQ(seller.sent(), std::vector<std::vector<uint8_t>>{seller.serializedFor(SM_BROKER_SERVICE(true, int64_t{28}))});
	clearSent();
	readAndRun<CM_BROKER_LIST>(CM_BROKER_LIST_OPCODE, listBody(broker->getObjectId(), SORT_PRICE_ASC, 0, MASK_POTION));
	SearchedItems page = searched();
	ASSERT_EQ(page.offers.size(), 1u);
	EXPECT_EQ(page.offers[0].objectId, 9603);
	EXPECT_EQ(page.offers[0].count, 6);
	EXPECT_EQ(page.offers[0].totalPrice, 42);

	saved();
	EXPECT_EQ(maildb::queryLong("SELECT item_count FROM broker WHERE item_pointer = 9603"), 6);
	EXPECT_EQ(maildb::queryLong("SELECT is_sold FROM broker WHERE item_pointer = 9603"), 0);
	EXPECT_EQ(maildb::queryLong("SELECT COUNT(*) FROM broker WHERE is_sold = 1 AND item_count = 4 AND price = 7"), 1);
}

TEST_F(BrokerBuyTest, FewerPiecesOfAnUnsplittableOfferAreRefusedAndTheWholeStackIsBoughtAtItsPrice) {
	BROKER_PACKET_REQUIRE_DATABASE();
	offer(9604, MINOR_LIFE_POTION, 10, 7, SELLER_ID, "ELYOS", false);
	owned(9699, KINAH, 1000);
	targetBroker();

	// J-P5-09b-2 (owner's decision 2026-10-19, applied): a count other than the offer's is refused for an offer that cannot be split. Java
	// (:250, :272-274) took the whole stack for the price of the requested pieces
	buy(9604, 2);
	EXPECT_EQ(kinahOf(player()), 1000);
	EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(9604));
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 0);
	buy(9604, 11);
	EXPECT_EQ(kinahOf(player()), 1000) << "more pieces than the offer holds are refused too";

	buy(9604, 10);
	EXPECT_EQ(kinahOf(player()), 930) << "the whole stack at 10 * 7";
	runtime::Ptr<Item> bought = storage(StorageType::CUBE).getItemByObjId(9604);
	ASSERT_TRUE(bought);
	EXPECT_EQ(bought->getItemCount(), 10);
}

TEST_F(BrokerBuyTest, APackedItemIsUnpacked) {
	BROKER_PACKET_REQUIRE_DATABASE();
	offer(9605, MINOR_LIFE_POTION, 1, 10, SELLER_ID);
	maildb::execute("UPDATE inventory SET pack_count = 2 WHERE item_unique_id = 9605");
	owned(9699, KINAH, 1000);
	targetBroker();
	buy(9605, 1);
	runtime::Ptr<Item> bought = storage(StorageType::CUBE).getItemByObjId(9605);
	ASSERT_TRUE(bought);
	EXPECT_EQ(bought->getPackCount(), -2) << ":282-284";
}

TEST_F(BrokerBuyTest, NothingIsBoughtOfOnesOwnOfferOrWithoutTheKinah) {
	BROKER_PACKET_REQUIRE_DATABASE();
	offer(9606, MINOR_LIFE_POTION, 1, 10, HOLDER_ID);
	offer(9607, MINOR_LIFE_POTION, 2, 600, SELLER_ID);
	owned(9699, KINAH, 1000);
	targetBroker();

	buy(9606, 1);
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_VENDOR_CAN_NOT_BUY_MY_REGISTER_ITEM())})) << ":234-237, and no list";
	EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(9606));

	clearSent();
	buy(9607, 2); // 1,200
	EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(9607));
	EXPECT_EQ(kinahOf(player()), 1000);
	EXPECT_TRUE(sent().empty()) << ":256-257: the return inside the synchronized block also skips the list (:295-296)";

	clearSent();
	buy(9698, 1); // no such offer (:231-232)
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(kinahOf(player()), 1000);
}

TEST_F(BrokerBuyTest, TheBoughtOfferLeavesTheCachedListShownAfterThePurchase) {
	BROKER_PACKET_REQUIRE_DATABASE();
	offer(9611, MINOR_LIFE_POTION, 1, 10, SELLER_ID);
	offer(9612, MINOR_LIFE_POTION, 1, 20, SELLER_ID);
	owned(9699, KINAH, 1000);
	targetBroker();
	// a search without a mask caches the potions; a list with mask 0 then shows that cache and forgets the search list (BrokerService.java:111-130)
	readAndRun<CM_BROKER_SEARCH>(CM_BROKER_SEARCH_OPCODE, searchBody(broker->getObjectId(), SORT_PRICE_ASC, 0, 0, {MINOR_LIFE_POTION}));
	readAndRun<CM_BROKER_LIST>(CM_BROKER_LIST_OPCODE, listBody(broker->getObjectId(), SORT_PRICE_ASC, 0, 0));
	clearSent();

	buy(9611, 1);

	// the purchase shows mask 0 without a search list again: the cached list, which removeFromCache (:278) has taken the offer out of
	EXPECT_EQ(searched().ids(), (std::vector<int32_t>{9612}));
}

TEST_F(BrokerBuyTest, AFullCubeBuysNothing) {
	BROKER_PACKET_REQUIRE_DATABASE();
	offer(9610, TRAINING_SWORD, 1, 10, SELLER_ID);
	owned(9699, KINAH, 1000);
	targetBroker();
	for (int32_t objId = 9900; !storage(StorageType::CUBE).isFull(); objId++)
		stored(objId, TRAINING_HAUBERK, 1);
	clearSent();

	buy(9610, 1);
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_FULL_INVENTORY())})) << "BrokerService.java:251-254, and no list";
	EXPECT_EQ(kinahOf(player()), 1000);
}

TEST_F(BrokerBuyTest, AnOfferBoughtByAnotherIsGoneAndASecondBuyFindsNothing) {
	BROKER_PACKET_REQUIRE_DATABASE();
	offer(9608, MINOR_LIFE_POTION, 1, 10, SELLER_ID);
	owned(9699, KINAH, 1000);
	targetBroker();
	buy(9608, 1);
	EXPECT_EQ(kinahOf(player()), 990);
	clearSent();
	buy(9608, 1);
	EXPECT_EQ(kinahOf(player()), 990) << "the offer left the race's map";
	EXPECT_TRUE(sent().empty());
}

TEST_F(BrokerBuyTest, DuringAnExchangeWithoutTheBrokerOrForNoPieceNothingIsBought) {
	BROKER_PACKET_REQUIRE_DATABASE();
	offer(9609, MINOR_LIFE_POTION, 1, 10, SELLER_ID);
	owned(9699, KINAH, 1000);
	targetBroker();
	network::test::LogCapture capture({"AUDIT_LOG"});

	buy(9609, 1, broker->getObjectId() + 1);
	EXPECT_TRUE(capture.contains("tried to buy an item from broker without targeting a broker")) << capture.dump();
	buy(9609, 0); // CM_BUY_BROKER_ITEM.java:35-36: itemCount < 1
	EXPECT_TRUE(sent().empty());

	OtherPlayer& trader = otherPlayer(710401, "Trader");
	services::ExchangeService::getInstance().registerExchange(player(), trader.player());
	clearSent();
	buy(9609, 1);
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_EXCHANGE_PARTNER_IS_EXCHANGING_WITH_OTHER())})) << ":226-227 returns at once";
	EXPECT_EQ(kinahOf(player()), 1000);
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
