// The broker's browsing packets (P5-15 with P5-09b's BrokerService, the broker lane of 2026-10-18): CM_BROKER_LIST, CM_BROKER_SEARCH,
// CM_BROKER_SELL_WINDOW and CM_BROKER_REGISTERED, read and run against the offers of the test database (BrokerPacketTestSupport.h).
//
// Java: CM_BROKER_LIST.java, CM_BROKER_SEARCH.java, CM_BROKER_SELL_WINDOW.java, CM_BROKER_REGISTERED.java and BrokerService.java:97-198, 422-447.
// Expectations are derived by hand from those bodies.

#include "BrokerPacketTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "aion/gameserver/network/aion/clientpackets/CM_BROKER_LIST.h"
#include "aion/gameserver/network/aion/clientpackets/CM_BROKER_REGISTERED.h"
#include "aion/gameserver/network/aion/clientpackets/CM_BROKER_SEARCH.h"
#include "aion/gameserver/network/aion/clientpackets/CM_BROKER_SELL_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BROKER_SERVICE.h"
#include "aion/gameserver/services/ExchangeService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

/** The friends the packet headers declare: the fields readImpl decoded */
struct CM_BROKER_LISTTestAccess {
	static int32_t brokerObjId(const CM_BROKER_LIST& p) { return p.brokerObjId; }
	static int32_t sortType(const CM_BROKER_LIST& p) { return p.sortType; }
	static int32_t page(const CM_BROKER_LIST& p) { return p.page; }
	static int32_t listMask(const CM_BROKER_LIST& p) { return p.listMask; }
};

struct CM_BROKER_SEARCHTestAccess {
	static int32_t brokerObjId(const CM_BROKER_SEARCH& p) { return p.brokerObjId; }
	static int32_t sortType(const CM_BROKER_SEARCH& p) { return p.sortType; }
	static int32_t page(const CM_BROKER_SEARCH& p) { return p.page; }
	static int32_t mask(const CM_BROKER_SEARCH& p) { return p.mask; }
	static std::vector<int32_t> itemList(const CM_BROKER_SEARCH& p) { return p.itemList; }
};

struct CM_BROKER_SELL_WINDOWTestAccess {
	static int32_t itemUniqueId(const CM_BROKER_SELL_WINDOW& p) { return p.itemUniqueId; }
};

struct CM_BROKER_REGISTEREDTestAccess {
	static int32_t brokerObjId(const CM_BROKER_REGISTERED& p) { return p.brokerObjId; }
};

namespace testing::items {
namespace {

using serverpackets::SM_BROKER_SERVICE;

// The offers most cases load (ELYOS unless named): three potions and a juice of the seller, the holder's own sword, an Asmodian potion
constexpr int32_t POTIONS_30 = 9101; // 10 potions at 30
constexpr int32_t POTIONS_50 = 9102; // 5 potions at 50
constexpr int32_t POTIONS_20 = 9103; // 2 potions at 20
constexpr int32_t JUICE = 9104;      // 3 juices at 25
constexpr int32_t OWN_SWORD = 9105;  // the holder's sword at 1000
constexpr int32_t DARK_POTIONS = 9106;

class BrokerBrowseTest : public BrokerPacketTest {
protected:
	void loadOffers() {
		offer(POTIONS_30, MINOR_LIFE_POTION, 10, 30, SELLER_ID);
		offer(POTIONS_50, MINOR_LIFE_POTION, 5, 50, SELLER_ID);
		offer(POTIONS_20, MINOR_LIFE_POTION, 2, 20, SELLER_ID);
		offer(JUICE, MERCENARYS_FRUIT_JUICE, 3, 25, SELLER_ID);
		offer(OWN_SWORD, TRAINING_SWORD, 1, 1000, HOLDER_ID);
		offer(DARK_POTIONS, MINOR_LIFE_POTION, 7, 1, ASMODIAN_SELLER_ID, "ASMODIAN");
	}
};

// ---- reading ---------------------------------------------------------------------------------------------------------------------------------

TEST(BrokerReadTest, TheBodiesAreReadAsJavaReadsThem) {
	std::vector<uint8_t> list = PacketWriter().D(0x01020304).C(0xF6).H(0xFFFE).H(0x2384).data;
	EconomyDriver<CM_BROKER_LIST> listPacket(CM_BROKER_LIST_OPCODE);
	ASSERT_TRUE(listPacket.readOn(list, nullptr));
	EXPECT_EQ(CM_BROKER_LISTTestAccess::brokerObjId(listPacket), 0x01020304);
	EXPECT_EQ(CM_BROKER_LISTTestAccess::sortType(listPacket), -10) << "readC: a signed byte";
	EXPECT_EQ(CM_BROKER_LISTTestAccess::page(listPacket), 0xFFFE) << "readUH";
	EXPECT_EQ(CM_BROKER_LISTTestAccess::listMask(listPacket), 0x2384);

	std::vector<uint8_t> search = PacketWriter().D(7).C(5).H(0xFFFF).H(9060).H(2).D(162000002).D(-3).data;
	EconomyDriver<CM_BROKER_SEARCH> searchPacket(CM_BROKER_SEARCH_OPCODE);
	ASSERT_TRUE(searchPacket.readOn(search, nullptr));
	EXPECT_EQ(CM_BROKER_SEARCHTestAccess::brokerObjId(searchPacket), 7);
	EXPECT_EQ(CM_BROKER_SEARCHTestAccess::sortType(searchPacket), 5);
	EXPECT_EQ(CM_BROKER_SEARCHTestAccess::page(searchPacket), 0xFFFF);
	EXPECT_EQ(CM_BROKER_SEARCHTestAccess::mask(searchPacket), 9060);
	EXPECT_EQ(CM_BROKER_SEARCHTestAccess::itemList(searchPacket), (std::vector<int32_t>{162000002, -3}));
	EXPECT_EQ(searchPacket.getRemainingBytes(), 0);

	std::vector<uint8_t> sell = PacketWriter().D(-5).data;
	EconomyDriver<CM_BROKER_SELL_WINDOW> sellPacket(CM_BROKER_SELL_WINDOW_OPCODE);
	ASSERT_TRUE(sellPacket.readOn(sell, nullptr));
	EXPECT_EQ(CM_BROKER_SELL_WINDOWTestAccess::itemUniqueId(sellPacket), -5);

	std::vector<uint8_t> registered = PacketWriter().D(0x11223344).data;
	EconomyDriver<CM_BROKER_REGISTERED> registeredPacket(CM_BROKER_REGISTERED_OPCODE);
	ASSERT_TRUE(registeredPacket.readOn(registered, nullptr));
	EXPECT_EQ(CM_BROKER_REGISTEREDTestAccess::brokerObjId(registeredPacket), 0x11223344);
}

// ---- CM_BROKER_LIST ------------------------------------------------------------------------------------------------------------------------

TEST_F(BrokerBrowseTest, AListShowsTheRacesOffersOfTheMaskWithTheirAveragePrice) {
	BROKER_PACKET_REQUIRE_DATABASE();
	loadOffers();
	targetBroker();

	readAndRun<CM_BROKER_LIST>(CM_BROKER_LIST_OPCODE, listBody(broker->getObjectId(), SORT_PRICE_ASC, 0, MASK_POTION));

	// BrokerService.java:107-108: a mask and an empty cache - getItemsByMask over the Elyos offers; the Asmodian potion is not among them.
	// getAveragePrice (:144-160): (30 + 50 + 20) / 3 = 33 for every potion of the race, the Asmodian one not counted
	SearchedItems page = searched();
	EXPECT_EQ(page.total, 3);
	EXPECT_EQ(page.startPage, 0);
	ASSERT_EQ(page.ids(), (std::vector<int32_t>{POTIONS_20, POTIONS_30, POTIONS_50})) << "price ascending";
	EXPECT_EQ(page.offers[0], (SentOffer{POTIONS_20, MINOR_LIFE_POTION, 40, 33, 2, "Seller"}));
	EXPECT_EQ(page.offers[1], (SentOffer{POTIONS_30, MINOR_LIFE_POTION, 300, 33, 10, "Seller"}));
	EXPECT_EQ(page.offers[2], (SentOffer{POTIONS_50, MINOR_LIFE_POTION, 250, 33, 5, "Seller"}));
}

TEST_F(BrokerBrowseTest, ThePriceSortsBothWaysAndAParentMaskTakesItsChildren) {
	BROKER_PACKET_REQUIRE_DATABASE();
	loadOffers();
	targetBroker();

	readAndRun<CM_BROKER_LIST>(CM_BROKER_LIST_OPCODE, listBody(broker->getObjectId(), SORT_PRICE_DESC, 0, MASK_CONSUMABLES));
	SearchedItems page = searched();
	EXPECT_EQ(page.total, 4) << "CONSUMABLES: the potions (1620) and the juice (1600), not the sword";
	EXPECT_EQ(page.ids(), (std::vector<int32_t>{POTIONS_50, POTIONS_30, JUICE, POTIONS_20})) << "price descending: 50, 30, 25, 20";
	EXPECT_EQ(page.offers[2].averagePrice, 25) << "the juice is the only one of its item";

	clearSent();
	readAndRun<CM_BROKER_LIST>(CM_BROKER_LIST_OPCODE, listBody(broker->getObjectId(), SORT_PRICE_ASC, 0, MASK_WEAPON));
	EXPECT_EQ(searched().ids(), (std::vector<int32_t>{OWN_SWORD})) << "WEAPON (1000..1021): the holder's own offer is listed too";
}

TEST_F(BrokerBrowseTest, AChildMaskFiltersTheCachedListAndAnotherMaskReadsTheOffersAgain) {
	BROKER_PACKET_REQUIRE_DATABASE();
	loadOffers();
	targetBroker();

	// a search under CONSUMABLES for the potion's id: getItemsByMask(CONSUMABLES) caches the mask (:190-191), then the item list narrows the
	// cached list to the three potions (:120-128)
	readAndRun<CM_BROKER_SEARCH>(CM_BROKER_SEARCH_OPCODE, searchBody(broker->getObjectId(), SORT_PRICE_ASC, 0, MASK_CONSUMABLES, {MINOR_LIFE_POTION}));
	EXPECT_EQ(searched().ids(), (std::vector<int32_t>{POTIONS_20, POTIONS_30, POTIONS_50}));

	// FOOD is a child of the cached CONSUMABLES (:101, :109-110): the cached list - three potions - is filtered, and no juice is in it
	clearSent();
	readAndRun<CM_BROKER_LIST>(CM_BROKER_LIST_OPCODE, listBody(broker->getObjectId(), SORT_PRICE_ASC, 0, MASK_FOOD));
	SearchedItems food = searched();
	EXPECT_EQ(food.total, 0);
	EXPECT_TRUE(food.offers.empty()) << "the cached list, not the offers";

	// the cache is now empty (getItemsByMask cached the empty result), so the next mask reads the offers again (:107-108)
	clearSent();
	readAndRun<CM_BROKER_LIST>(CM_BROKER_LIST_OPCODE, listBody(broker->getObjectId(), SORT_PRICE_ASC, 0, MASK_FOOD));
	EXPECT_EQ(searched().ids(), (std::vector<int32_t>{JUICE}));

	// WEAPON is no child of FOOD: the offers again, whatever the cache holds
	clearSent();
	readAndRun<CM_BROKER_LIST>(CM_BROKER_LIST_OPCODE, listBody(broker->getObjectId(), SORT_PRICE_ASC, 0, MASK_WEAPON));
	EXPECT_EQ(searched().ids(), (std::vector<int32_t>{OWN_SWORD}));
}

TEST_F(BrokerBrowseTest, MaskZeroWithoutASearchListsTheCachedList) {
	BROKER_PACKET_REQUIRE_DATABASE();
	loadOffers();
	targetBroker();
	readAndRun<CM_BROKER_LIST>(CM_BROKER_LIST_OPCODE, listBody(broker->getObjectId(), SORT_PRICE_ASC, 0, MASK_FOOD));
	ASSERT_EQ(searched().ids(), (std::vector<int32_t>{JUICE}));

	// mask 0 and no item list (:111-112): getFilteredItems, the last list - the juice - shown again under the new sort and page
	clearSent();
	readAndRun<CM_BROKER_LIST>(CM_BROKER_LIST_OPCODE, listBody(broker->getObjectId(), SORT_PRICE_DESC, 0, 0));
	EXPECT_EQ(searched().ids(), (std::vector<int32_t>{JUICE}));
}

TEST_F(BrokerBrowseTest, APageSkipsNineOffersPerPageAndShowsAtMostThirtySix) {
	BROKER_PACKET_REQUIRE_DATABASE();
	for (int32_t i = 0; i < 50; i++)
		offer(9200 + i, MINOR_LIFE_POTION, 1, 100 + i, SELLER_ID); // prices 100..149, so price ascending is id order
	targetBroker();
	const auto ids = [](int32_t from, int32_t n) {
		std::vector<int32_t> result;
		for (int32_t i = 0; i < n; i++)
			result.push_back(from + i);
		return result;
	};

	readAndRun<CM_BROKER_LIST>(CM_BROKER_LIST_OPCODE, listBody(broker->getObjectId(), SORT_PRICE_ASC, 0, MASK_POTION));
	SearchedItems first = searched();
	EXPECT_EQ(first.total, 50) << "the count of the whole list";
	EXPECT_EQ(first.ids(), ids(9200, 36)) << "limit(36)";

	clearSent();
	readAndRun<CM_BROKER_LIST>(CM_BROKER_LIST_OPCODE, listBody(broker->getObjectId(), SORT_PRICE_ASC, 2, MASK_POTION));
	SearchedItems third = searched();
	EXPECT_EQ(third.startPage, 2);
	EXPECT_EQ(third.ids(), ids(9218, 32)) << "skip(2 * 9)";

	clearSent();
	readAndRun<CM_BROKER_LIST>(CM_BROKER_LIST_OPCODE, listBody(broker->getObjectId(), SORT_PRICE_ASC, 6, MASK_POTION));
	EXPECT_TRUE(searched().offers.empty()) << "skip(54) of 50";
}

TEST_F(BrokerBrowseTest, AnUnknownSortTypeThrowsJavasIllegalArgumentException) {
	BROKER_PACKET_REQUIRE_DATABASE();
	loadOffers();
	targetBroker();
	EXPECT_THROW(readAndRun<CM_BROKER_LIST>(CM_BROKER_LIST_OPCODE, listBody(broker->getObjectId(), 8, 0, MASK_POTION)),
		runtime::IllegalArgumentException)
		<< "BrokerItem.getComparatoryByType (BrokerItem.java:360-361)";
	EXPECT_TRUE(brokerPackets().empty());
}

TEST_F(BrokerBrowseTest, WithoutTheBrokerTargetedTheListIsAuditedAndNotSent) {
	BROKER_PACKET_REQUIRE_DATABASE();
	loadOffers();
	targetBroker();
	network::test::LogCapture capture({"AUDIT_LOG"});
	readAndRun<CM_BROKER_LIST>(CM_BROKER_LIST_OPCODE, listBody(broker->getObjectId() + 1, SORT_PRICE_ASC, 0, MASK_POTION));
	readAndRun<CM_BROKER_SEARCH>(CM_BROKER_SEARCH_OPCODE, searchBody(broker->getObjectId() + 1, SORT_PRICE_ASC, 0, 0, {MINOR_LIFE_POTION}));
	readAndRun<CM_BROKER_REGISTERED>(CM_BROKER_REGISTERED_OPCODE, PacketWriter().D(broker->getObjectId() + 1).data);
	EXPECT_TRUE(brokerPackets().empty());
	EXPECT_TRUE(capture.contains("tried to browse for broker items without targeting a broker")) << capture.dump();
	EXPECT_TRUE(capture.contains("tried to search for items in broker without targeting a broker")) << capture.dump();
	EXPECT_TRUE(capture.contains("tried to view his registered broker items without targeting a broker")) << capture.dump();
}

// ---- CM_BROKER_SEARCH ----------------------------------------------------------------------------------------------------------------------

TEST_F(BrokerBrowseTest, ASearchWithoutAMaskTakesTheRacesOffersOfTheListedItems) {
	BROKER_PACKET_REQUIRE_DATABASE();
	loadOffers();
	targetBroker();

	// mask 0 and an item list (:102-106): every Elyos offer, then those of the listed item ids
	readAndRun<CM_BROKER_SEARCH>(CM_BROKER_SEARCH_OPCODE,
		searchBody(broker->getObjectId(), SORT_PRICE_DESC, 0, 0, {MERCENARYS_FRUIT_JUICE, TRAINING_SWORD}));
	SearchedItems page = searched();
	EXPECT_EQ(page.total, 2);
	EXPECT_EQ(page.ids(), (std::vector<int32_t>{OWN_SWORD, JUICE}));

	clearSent();
	readAndRun<CM_BROKER_SEARCH>(CM_BROKER_SEARCH_OPCODE, searchBody(broker->getObjectId(), SORT_PRICE_ASC, 0, 0, {LESSER_ANCIENT_KINAH}));
	EXPECT_TRUE(searched().offers.empty()) << "nothing of that item";
}

// ---- CM_BROKER_SELL_WINDOW -----------------------------------------------------------------------------------------------------------------

TEST_F(BrokerBrowseTest, TheSellWindowShowsTheLowestAndHighestPriceOfTheItemsOffers) {
	BROKER_PACKET_REQUIRE_DATABASE();
	loadOffers();
	stored(9301, MINOR_LIFE_POTION, 4);
	stored(9302, LESSER_ANCIENT_KINAH, 1);

	// BrokerService.java:422-433: the Elyos potions cost 20, 30 and 50 (the Asmodian one at 1 is not counted)
	readAndRun<CM_BROKER_SELL_WINDOW>(CM_BROKER_SELL_WINDOW_OPCODE, PacketWriter().D(9301).data);
	readAndRun<CM_BROKER_SELL_WINDOW>(CM_BROKER_SELL_WINDOW_OPCODE, PacketWriter().D(9302).data);
	readAndRun<CM_BROKER_SELL_WINDOW>(CM_BROKER_SELL_WINDOW_OPCODE, PacketWriter().D(9399).data);
	EXPECT_EQ(sent(), exactly({serializedFor(SM_BROKER_SERVICE(int8_t{0}, 9301, 20, 50)), serializedFor(SM_BROKER_SERVICE(int8_t{0}, 9302, 0, 0))}))
		<< "no offer of the item: 0 and 0 (LongSummaryStatistics' MAX_VALUE and MIN_VALUE); an item the player lacks: nothing";
}

TEST_F(BrokerBrowseTest, NoSellWindowDuringAnExchange) {
	BROKER_PACKET_REQUIRE_DATABASE();
	loadOffers();
	stored(9301, MINOR_LIFE_POTION, 4);
	OtherPlayer& trader = otherPlayer(710401, "Trader");
	services::ExchangeService::getInstance().registerExchange(player(), trader.player());
	ASSERT_TRUE(player().isTrading());
	clearSent();
	readAndRun<CM_BROKER_SELL_WINDOW>(CM_BROKER_SELL_WINDOW_OPCODE, PacketWriter().D(9301).data);
	EXPECT_TRUE(brokerPackets().empty()) << "CM_BROKER_SELL_WINDOW.java:29-30: player.isTrading()";
}

// ---- CM_BROKER_REGISTERED ------------------------------------------------------------------------------------------------------------------

TEST_F(BrokerBrowseTest, TheRegisteredListHoldsThePlayersOwnOffers) {
	BROKER_PACKET_REQUIRE_DATABASE();
	loadOffers();
	offer(9110, MINOR_LIFE_POTION, 1, 5, HOLDER_ID);
	targetBroker();

	readAndRun<CM_BROKER_REGISTERED>(CM_BROKER_REGISTERED_OPCODE, PacketWriter().D(broker->getObjectId()).data);
	std::vector<std::vector<uint8_t>> packets = brokerPackets();
	ASSERT_EQ(packets.size(), 1u);
	std::vector<int32_t> ids = decodeRegisteredItems(packets.front());
	std::sort(ids.begin(), ids.end());
	EXPECT_EQ(ids, (std::vector<int32_t>{OWN_SWORD, 9110})) << "BrokerService.java:435-447: the seller's offers of his race";
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
