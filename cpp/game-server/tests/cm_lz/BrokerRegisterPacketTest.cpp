// CM_REGISTER_BROKER_ITEM (P5-16 with P5-09b's BrokerService.registerItem, the broker lane of 2026-10-18), read and run against the test database
// (tests/cm_ak/BrokerPacketTestSupport.h, included by relative path as the other cm_lz economy tests include their cm_ak fixtures).
//
// Java: CM_REGISTER_BROKER_ITEM.java and BrokerService.java:325-420. The fixture's prices are the shipped defaults (prices 100, modifier 100,
// taxes 100) and there is no siege, so the influence is 0: PricesService (PricesService.java:20-53, 86-90) makes the prices 125 and the taxes
// 113, and getPriceForService(c) = c * 125 / 100 * 100 / 100 * 113 / 100, each step rounded down: 10 -> 13, 40 -> 56, 80 -> 113. A commission
// below 10 is 10 without the service price. Expectations are derived by hand from those bodies.

#include "../cm_ak/BrokerPacketTestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include "aion/gameserver/network/aion/clientpackets/CM_BROKER_REGISTERED.h"
#include "aion/gameserver/network/aion/clientpackets/CM_REGISTER_BROKER_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BROKER_SERVICE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/ExchangeService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

/** The friend CM_REGISTER_BROKER_ITEM.h declares: the fields readImpl decoded */
struct CM_REGISTER_BROKER_ITEMTestAccess {
	static int32_t brokerObjId(const CM_REGISTER_BROKER_ITEM& p) { return p.brokerObjId; }
	static int32_t itemUniqueId(const CM_REGISTER_BROKER_ITEM& p) { return p.itemUniqueId; }
	static int64_t price(const CM_REGISTER_BROKER_ITEM& p) { return p.price; }
	static int64_t itemCount(const CM_REGISTER_BROKER_ITEM& p) { return p.itemCount; }
	static bool splittingAvailable(const CM_REGISTER_BROKER_ITEM& p) { return p.splittingAvailable; }
};

namespace testing::items {
namespace {

using serverpackets::SM_BROKER_SERVICE;
using serverpackets::SM_SYSTEM_MESSAGE;

// BrokerMessages.java: NO_SPACE_AVAIABLE(3), NO_ENOUGHT_KINAH(5)
constexpr int32_t NO_SPACE_AVAILABLE = 3;
constexpr int32_t NO_ENOUGH_KINAH = 5;

/** CM_REGISTER_BROKER_ITEM.java readImpl: D(broker), D(item), Q(price), Q(count), C(splitting == 1) */
std::vector<uint8_t> registerBody(int32_t broker, int32_t item, int64_t price, int64_t count, int32_t splitting) {
	return PacketWriter().D(broker).D(item).Q(price).Q(count).C(splitting).data;
}

class BrokerRegisterTest : public BrokerPacketTest {
protected:
	void registerItem(int32_t item, int64_t price, int64_t count, int32_t splitting = 1) {
		readAndRun<CM_REGISTER_BROKER_ITEM>(CM_REGISTER_BROKER_ITEM_OPCODE, registerBody(broker->getObjectId(), item, price, count, splitting));
	}
};

TEST(BrokerRegisterReadTest, TheBodyIsReadAsJavaReadsIt) {
	std::vector<uint8_t> body = registerBody(0x01020304, -7, 0x0102030405060708LL, -2, 1);
	EconomyDriver<CM_REGISTER_BROKER_ITEM> packet(CM_REGISTER_BROKER_ITEM_OPCODE);
	ASSERT_TRUE(packet.readOn(body, nullptr));
	EXPECT_EQ(CM_REGISTER_BROKER_ITEMTestAccess::brokerObjId(packet), 0x01020304);
	EXPECT_EQ(CM_REGISTER_BROKER_ITEMTestAccess::itemUniqueId(packet), -7);
	EXPECT_EQ(CM_REGISTER_BROKER_ITEMTestAccess::price(packet), 0x0102030405060708LL);
	EXPECT_EQ(CM_REGISTER_BROKER_ITEMTestAccess::itemCount(packet), -2);
	EXPECT_TRUE(CM_REGISTER_BROKER_ITEMTestAccess::splittingAvailable(packet));
	EconomyDriver<CM_REGISTER_BROKER_ITEM> two(CM_REGISTER_BROKER_ITEM_OPCODE);
	ASSERT_TRUE(two.readOn(registerBody(1, 2, 3, 4, 2), nullptr));
	EXPECT_FALSE(CM_REGISTER_BROKER_ITEMTestAccess::splittingAvailable(two)) << "readC() == 1";
	EXPECT_EQ(two.getRemainingBytes(), 0);
}

TEST_F(BrokerRegisterTest, AWholeStackLeavesTheCubeForTheBrokerAndPaysTheCommission) {
	BROKER_PACKET_REQUIRE_DATABASE();
	owned(9401, MINOR_LIFE_POTION, 5);
	owned(9499, KINAH, 1000);
	targetBroker();

	registerItem(9401, 100, 5);

	// (long) (100 * 5 * 0.02f) = 10, not below 10, so getPriceForService(10) = 13 (BrokerService.java:378-383)
	EXPECT_EQ(kinahOf(player()), 987);
	EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(9401)) << "the whole stack: removed (:398)";
	std::vector<std::vector<uint8_t>> deletes = packetsOf(sent(), SM_DELETE_ITEM_OPCODE);
	ASSERT_EQ(deletes.size(), 1u);
	EXPECT_EQ(deletes.front(), serializedFor(serverpackets::SM_DELETE_ITEM(9401)));
	std::vector<std::vector<uint8_t>> registered = brokerPackets();
	ASSERT_EQ(registered.size(), 1u);
	PacketReader reader(bodyOf(registered.front()));
	EXPECT_EQ(reader.C(), 3) << "REGISTER_ITEM";
	EXPECT_EQ(reader.C(), 0) << "message 0";
	EXPECT_EQ(reader.C(), 1) << "registeredItemsCount (0) + 1";
	EXPECT_EQ(reader.D(), 9401) << "the item itself is the offer";
	EXPECT_EQ(reader.D(), MINOR_LIFE_POTION);
	EXPECT_EQ(reader.Q(), 500) << "price * count";
	EXPECT_EQ(ownOffers(), (std::vector<int32_t>{9401}));

	saved();
	EXPECT_EQ(maildb::queryLong("SELECT item_location FROM inventory WHERE item_unique_id = 9401"), 126) << "StorageType.BROKER";
	EXPECT_EQ(maildb::queryLong("SELECT price FROM broker WHERE item_pointer = 9401"), 100);
	EXPECT_EQ(maildb::queryLong("SELECT item_count FROM broker WHERE item_pointer = 9401"), 5);
	EXPECT_EQ(maildb::queryLong("SELECT splitting_available FROM broker WHERE item_pointer = 9401"), 1) << "a stackable item keeps it";
	EXPECT_EQ(maildb::queryLong("SELECT seller_id FROM broker WHERE item_pointer = 9401"), HOLDER_ID);
	EXPECT_EQ(maildb::queryLong("SELECT item_count FROM inventory WHERE item_unique_id = 9499"), 987) << "the kinah item is stored";
}

TEST_F(BrokerRegisterTest, PartOfAStackIsANewItemAndTheRestStaysInTheCube) {
	BROKER_PACKET_REQUIRE_DATABASE();
	owned(9402, MINOR_LIFE_POTION, 10);
	owned(9499, KINAH, 1000);
	targetBroker();

	registerItem(9402, 30, 4);

	// (long) (30 * 4 * 0.02f) = 2, below 10: 10, without the service price
	EXPECT_EQ(kinahOf(player()), 990);
	runtime::Ptr<Item> rest = storage(StorageType::CUBE).getItemByObjId(9402);
	ASSERT_TRUE(rest);
	EXPECT_EQ(rest->getItemCount(), 6) << "decreaseItemCount(item, 4) (:395)";
	EXPECT_TRUE(packetsOf(sent(), SM_DELETE_ITEM_OPCODE).empty());
	std::vector<int32_t> offers = ownOffers();
	ASSERT_EQ(offers.size(), 1u);
	EXPECT_NE(offers.front(), 9402) << "ItemFactory.newItem(itemId, 4): a new object id";

	saved();
	EXPECT_EQ(maildb::queryLong("SELECT item_count FROM broker WHERE item_pointer = " + std::to_string(offers.front())), 4);
	EXPECT_EQ(maildb::queryLong("SELECT item_location FROM inventory WHERE item_unique_id = " + std::to_string(offers.front())), 126);
}

TEST_F(BrokerRegisterTest, AnItemThatDoesNotStackIsNeverSplit) {
	BROKER_PACKET_REQUIRE_DATABASE();
	owned(9403, TRAINING_SWORD, 1);
	owned(9499, KINAH, 1000);
	targetBroker();
	registerItem(9403, 2000, 1, 1);
	// (long) (2000 * 0.02f) = 40, so 56
	EXPECT_EQ(kinahOf(player()), 944);
	saved();
	EXPECT_EQ(maildb::queryLong("SELECT splitting_available FROM broker WHERE item_pointer = 9403"), 0) << ":389-390";
}

TEST_F(BrokerRegisterTest, TheCommissionDoublesFromTheEleventhOfferAndTheSixteenthHasNoSpace) {
	BROKER_PACKET_REQUIRE_DATABASE();
	for (int32_t i = 0; i < 10; i++)
		offer(9450 + i, MINOR_LIFE_POTION, 1, 10, HOLDER_ID);
	owned(9404, TRAINING_SWORD, 1);
	owned(9405, TRAINING_SWORD, 1);
	owned(9499, KINAH, 1000);
	targetBroker();

	registerItem(9404, 2000, 1); // ten registered: (long) (2000 * 0.04f) = 80, so 113
	EXPECT_EQ(kinahOf(player()), 887);
	std::vector<std::vector<uint8_t>> registered = brokerPackets();
	ASSERT_EQ(registered.size(), 1u);
	PacketReader reader(bodyOf(registered.front()));
	reader.C();
	reader.C();
	EXPECT_EQ(reader.C(), 11) << "SM_BROKER_SERVICE(item, 0, registeredItemsCount): the ten before it, + 1";

	clearSent();
	registerItem(9405, 2000, 1);
	EXPECT_EQ(kinahOf(player()), 774) << "eleven registered in the service: still 4 %";
	EXPECT_EQ(ownOffers().size(), 12u);
}

TEST_F(BrokerRegisterTest, FifteenOffersLeaveNoSpace) {
	BROKER_PACKET_REQUIRE_DATABASE();
	for (int32_t i = 0; i < 15; i++)
		offer(9450 + i, MINOR_LIFE_POTION, 1, 10, HOLDER_ID);
	owned(9406, TRAINING_SWORD, 1);
	owned(9499, KINAH, 1000);
	targetBroker();

	registerItem(9406, 2000, 1);
	EXPECT_EQ(sent(), exactly({serializedFor(SM_BROKER_SERVICE(NO_SPACE_AVAILABLE))})) << "BrokerService.java:372-374";
	EXPECT_EQ(kinahOf(player()), 1000);
	EXPECT_TRUE(storage(StorageType::CUBE).getItemByObjId(9406));
}

TEST_F(BrokerRegisterTest, WithoutTheCommissionNothingIsRegistered) {
	BROKER_PACKET_REQUIRE_DATABASE();
	owned(9407, TRAINING_SWORD, 1);
	owned(9499, KINAH, 39);
	targetBroker();

	registerItem(9407, 2000, 1); // a commission of 56
	EXPECT_EQ(sent(), exactly({serializedFor(SM_BROKER_SERVICE(NO_ENOUGH_KINAH))}));
	EXPECT_EQ(kinahOf(player()), 39);
	EXPECT_TRUE(storage(StorageType::CUBE).getItemByObjId(9407));

	clearSent();
	owned(9408, MINOR_LIFE_POTION, 1);
	registerItem(9408, 1, 1); // the minimum commission of 10
	EXPECT_EQ(kinahOf(player()), 29);
	EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(9408));
}

TEST_F(BrokerRegisterTest, RetailPriceLimitsRefuseTooHighPrices) {
	BROKER_PACKET_REQUIRE_DATABASE();
	owned(9409, MINOR_LIFE_POTION, 10);
	owned(9499, KINAH, 100'000'000'000LL);
	targetBroker();
	const std::vector<uint8_t> refused = serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_LIMITED_VENDOR_CANT_OVER_GOLD());

	registerItem(9409, 100'000'000'000LL, 1);
	registerItem(9409, 2'000'000'000LL, 2); // 1,000,000,000 a piece
	EXPECT_EQ(sent(), exactly({refused, refused})) << "BrokerService.java:349-352";

	// the per-piece limit holds only for more than one piece; 99,999,999,999 is still allowed
	clearSent();
	registerItem(9409, 1'999'999'998LL, 2);
	EXPECT_EQ(brokerPackets().size(), 1u);
	clearSent();
	registerItem(9409, 99'999'999'999LL, 1);
	EXPECT_EQ(brokerPackets().size(), 1u);
	EXPECT_EQ(ownOffers().size(), 2u);
}

TEST_F(BrokerRegisterTest, NothingIsRegisteredForAMissingItemTooManyPiecesOrNoPrice) {
	BROKER_PACKET_REQUIRE_DATABASE();
	owned(9410, MINOR_LIFE_POTION, 3);
	owned(9499, KINAH, 1000);
	targetBroker();

	registerItem(9499 + 100, 10, 1); // not in the cube
	registerItem(9410, 10, 4);       // more than the stack
	registerItem(9410, 0, 1);        // no price
	registerItem(9410, -5, 1);
	registerItem(9410, 10, 0); // CM_REGISTER_BROKER_ITEM.java:39-40: itemCount <= 0
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(kinahOf(player()), 1000);
	EXPECT_EQ(storage(StorageType::CUBE).getItemByObjId(9410)->getItemCount(), 3);
}

TEST_F(BrokerRegisterTest, DuringAnExchangeOrWithoutTheBrokerNothingIsRegistered) {
	BROKER_PACKET_REQUIRE_DATABASE();
	owned(9411, MINOR_LIFE_POTION, 3);
	owned(9499, KINAH, 1000);
	targetBroker();
	network::test::LogCapture capture({"AUDIT_LOG"});

	readAndRun<CM_REGISTER_BROKER_ITEM>(CM_REGISTER_BROKER_ITEM_OPCODE, registerBody(broker->getObjectId() + 1, 9411, 10, 1, 1));
	EXPECT_TRUE(capture.contains("tried to register a broker item without targeting a broker")) << capture.dump();

	readAndRun<CM_REGISTER_BROKER_ITEM>(CM_REGISTER_BROKER_ITEM_OPCODE, registerBody(broker->getObjectId() + 1, 9411, 10, 0, 1));
	EXPECT_EQ(capture.count("tried to register a broker item without targeting a broker"), 1)
		<< "CM_REGISTER_BROKER_ITEM.java:39-40: a count of 0 returns before the broker is checked";

	OtherPlayer& trader = otherPlayer(710401, "Trader");
	services::ExchangeService::getInstance().registerExchange(player(), trader.player());
	clearSent();
	registerItem(9411, 10, 1);
	EXPECT_TRUE(sent().empty()) << "CM_REGISTER_BROKER_ITEM.java:39: player.isTrading()";
	EXPECT_EQ(kinahOf(player()), 1000);
	EXPECT_EQ(storage(StorageType::CUBE).getItemByObjId(9411)->getItemCount(), 3);
}

TEST_F(BrokerRegisterTest, ADeadPlayerRegistersNothing) {
	BROKER_PACKET_REQUIRE_DATABASE();
	owned(9412, MINOR_LIFE_POTION, 3);
	owned(9499, KINAH, 1000);
	targetBroker();
	player().setLifeStats(std::make_unique<DeadPlayerLifeStats>(player()));
	clearSent();
	registerItem(9412, 10, 1);
	EXPECT_TRUE(sent().empty()) << "PlayerRestrictions.canTrade: a dead player (BrokerService.java:342-344)";
	EXPECT_EQ(kinahOf(player()), 1000);
	EXPECT_EQ(storage(StorageType::CUBE).getItemByObjId(9412)->getItemCount(), 3);
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
