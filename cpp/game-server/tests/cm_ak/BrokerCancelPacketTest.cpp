// CM_BROKER_CANCEL_REGISTERED (P5-15 with P5-09b's BrokerService.cancelRegisteredItem, the broker lane of 2026-10-18), read and run against
// the offers of the test database (BrokerPacketTestSupport.h).
//
// Java: CM_BROKER_CANCEL_REGISTERED.java and BrokerService.java:459-485. Expectations are derived by hand from those bodies.

#include "BrokerPacketTestSupport.h"

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/network/aion/clientpackets/CM_BROKER_CANCEL_REGISTERED.h"
#include "aion/gameserver/network/aion/clientpackets/CM_BROKER_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BROKER_SERVICE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/ExchangeService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

/** The friends the packet headers declare: the fields readImpl decoded */
struct CM_BROKER_CANCEL_REGISTEREDTestAccess {
	static int32_t brokerObjId(const CM_BROKER_CANCEL_REGISTERED& p) { return p.brokerObjId; }
	static int32_t brokerItemId(const CM_BROKER_CANCEL_REGISTERED& p) { return p.brokerItemId; }
};

namespace testing::items {
namespace {

using serverpackets::SM_BROKER_SERVICE;
using serverpackets::SM_SYSTEM_MESSAGE;

TEST(BrokerCancelReadTest, TheBodyIsReadAsJavaReadsIt) {
	EconomyDriver<CM_BROKER_CANCEL_REGISTERED> cancel(CM_BROKER_CANCEL_REGISTERED_OPCODE);
	ASSERT_TRUE(cancel.readOn(PacketWriter().D(0x01020304).D(-9).data, nullptr));
	EXPECT_EQ(CM_BROKER_CANCEL_REGISTEREDTestAccess::brokerObjId(cancel), 0x01020304);
	EXPECT_EQ(CM_BROKER_CANCEL_REGISTEREDTestAccess::brokerItemId(cancel), -9);
}

// ---- CM_BROKER_CANCEL_REGISTERED -----------------------------------------------------------------------------------------------------------

class BrokerCancelTest : public BrokerPacketTest {
protected:
	void cancel(int32_t offerId, int32_t brokerId = 0) {
		readAndRun<CM_BROKER_CANCEL_REGISTERED>(CM_BROKER_CANCEL_REGISTERED_OPCODE,
			PacketWriter().D(brokerId != 0 ? brokerId : broker->getObjectId()).D(offerId).data);
	}
};

TEST_F(BrokerCancelTest, AnOwnOfferComesBackToTheCubeAndLeavesTheBroker) {
	BROKER_PACKET_REQUIRE_DATABASE();
	offer(9501, MINOR_LIFE_POTION, 3, 10, HOLDER_ID);
	offer(9502, MERCENARYS_FRUIT_JUICE, 1, 10, HOLDER_ID);
	targetBroker();

	cancel(9501);

	// BrokerService.java:475-484: added back (BROKER_RETURN), the cancel packet, then the registered list without it
	runtime::Ptr<Item> back = storage(StorageType::CUBE).getItemByObjId(9501);
	ASSERT_TRUE(back);
	EXPECT_EQ(back->getItemCount(), 3);
	std::vector<std::vector<uint8_t>> packets = brokerPackets();
	ASSERT_EQ(packets.size(), 2u);
	EXPECT_EQ(packets[0], serializedFor(SM_BROKER_SERVICE(int8_t{0}, 9501)));
	EXPECT_EQ(decodeRegisteredItems(packets[1]), (std::vector<int32_t>{9502}));

	saved();
	EXPECT_EQ(maildb::queryLong("SELECT COUNT(*) FROM broker WHERE item_pointer = 9501"), 0) << "DELETED: the row goes";
	EXPECT_EQ(maildb::queryLong("SELECT COUNT(*) FROM broker WHERE item_pointer = 9502"), 1);
}

TEST_F(BrokerCancelTest, AnotherSellersOfferIsAuditedAndKept) {
	BROKER_PACKET_REQUIRE_DATABASE();
	offer(9503, MINOR_LIFE_POTION, 3, 10, SELLER_ID);
	targetBroker();
	network::test::LogCapture capture({"EXCHANGE_LOG"});

	cancel(9503);

	EXPECT_TRUE(capture.contains("[AUDIT] Player: Holder tried to get item from broker that he doesn't own")) << capture.dump();
	EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(9503));
	EXPECT_TRUE(brokerPackets().empty()) << "returns before showRegisteredItems (:467-470)";
	clearSent();
	readAndRun<CM_BROKER_LIST>(CM_BROKER_LIST_OPCODE, listBody(broker->getObjectId(), SORT_PRICE_ASC, 0, MASK_POTION));
	EXPECT_EQ(searched().ids(), (std::vector<int32_t>{9503}));
}

TEST_F(BrokerCancelTest, AnUnknownOfferOnlyShowsTheRegisteredList) {
	BROKER_PACKET_REQUIRE_DATABASE();
	offer(9504, MINOR_LIFE_POTION, 3, 10, HOLDER_ID);
	targetBroker();
	cancel(9599);
	std::vector<std::vector<uint8_t>> packets = brokerPackets();
	ASSERT_EQ(packets.size(), 1u);
	EXPECT_EQ(decodeRegisteredItems(packets[0]), (std::vector<int32_t>{9504}));
}

TEST_F(BrokerCancelTest, DuringAnExchangeOrWithoutTheBrokerNothingIsCanceled) {
	BROKER_PACKET_REQUIRE_DATABASE();
	offer(9505, MINOR_LIFE_POTION, 3, 10, HOLDER_ID);
	targetBroker();
	network::test::LogCapture capture({"AUDIT_LOG"});
	cancel(9505, broker->getObjectId() + 1);
	EXPECT_TRUE(capture.contains("tried to unregister his registered broker item without targeting a broker")) << capture.dump();

	OtherPlayer& trader = otherPlayer(710401, "Trader");
	services::ExchangeService::getInstance().registerExchange(player(), trader.player());
	clearSent();
	cancel(9505);
	// PlayerRestrictions.canTrade: the exchange's message, and BrokerService.java:463-465 returns before the registered list
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_EXCHANGE_PARTNER_IS_EXCHANGING_WITH_OTHER())}));
	EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(9505));
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
