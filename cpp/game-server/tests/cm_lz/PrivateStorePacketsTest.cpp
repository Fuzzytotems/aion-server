// M5c K-01 (m5c-plan.md §5, P5-16): CM_PRIVATE_STORE (C_PERSONAL_SHOP), the player's store rows - or an empty list, which closes the store -
// and CM_PRIVATE_STORE_NAME (C_SHOP_MSG), the store's name.
//
// Java: CM_PRIVATE_STORE.java:23-42, CM_PRIVATE_STORE_NAME.java:27-35. The read cases lay each body out from the Java readImpl (an unsigned
// count, then per row D object id, D item id, UH count and Q price; the name a UTF-16 string); the run cases drive runImpl on the economy
// packet fixture (EconomyPacketTestSupport.h) into the trade lane's PrivateStoreService (T-03): the rows the store keeps, in order, the shop
// emotion (EmotionType.OPEN_PRIVATESHOP 33, CLOSE_PRIVATESHOP 34), the stored and broadcast name, and Java's NullPointerException for a name
// without a store (PrivateStoreService.java:229). Buying from the store (CM_BUY_ITEM action 0) is tests/cm_ak/BuyItemPacketTest.cpp's.

#include "../cm_ak/EconomyPacketTestSupport.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/PrivateStore.h"
#include "aion/gameserver/model/trade/TradePSItem.h"
#include "aion/gameserver/network/aion/clientpackets/CM_PRIVATE_STORE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_PRIVATE_STORE_NAME.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_PRIVATE_STORE_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_PRIVATE_STORE_NAME_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friends the two headers declare: the fields readImpl decoded, which Java keeps private */
struct CM_PRIVATE_STORETestAccess {
	/** {object id, item id, count, price} per row */
	static std::vector<std::array<int64_t, 4>> rows(const CM_PRIVATE_STORE& p) {
		std::vector<std::array<int64_t, 4>> result;
		for (const runtime::Ref<model::trade::TradePSItem>& item : p.tradePSItems)
			result.push_back({item->getItemObjId(), item->getItemId(), item->getCount(), item->getPrice()});
		return result;
	}
};

struct CM_PRIVATE_STORE_NAMETestAccess {
	static std::string name(const CM_PRIVATE_STORE_NAME& p) { return p.name; }
};

namespace testing::items {
namespace {

using network::test::LogCapture;
using network::test::PacketReader;
using Rows = std::vector<std::array<int64_t, 4>>;

/** the decoded opcodes of ClientPacketInfo.gen.inc (Java AionClientPacketFactory.java:147-148, State.IN_GAME) */
constexpr int32_t CM_PRIVATE_STORE_OPCODE = 119;
constexpr int32_t CM_PRIVATE_STORE_NAME_OPCODE = 120;

constexpr int32_t OPEN_PRIVATESHOP = 33;  // EmotionType.OPEN_PRIVATESHOP(33)
constexpr int32_t CLOSE_PRIVATESHOP = 34; // EmotionType.CLOSE_PRIVATESHOP(34)

constexpr int32_t POTIONS = 800001; // 100 x Minor Life Potion
constexpr int32_t SWORD = 800002;   // 1 x Training Sword

/** C_PERSONAL_SHOP: UH row count, then per row D object id, D item id, UH count, Q price (CM_PRIVATE_STORE.java:23-33) */
std::vector<uint8_t> storeBody(const Rows& rows) {
	PacketWriter writer;
	writer.H(static_cast<int32_t>(rows.size()));
	for (const std::array<int64_t, 4>& row : rows)
		writer.D(static_cast<int32_t>(row[0])).D(static_cast<int32_t>(row[1])).H(static_cast<int32_t>(row[2])).Q(row[3]);
	return writer.data;
}

TEST(PrivateStorePacketsReadTest, TheStoreReadsAnUnsignedCountAndPerRowTheIdsAnUnsignedCountAndALongPrice) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	auto p = readAlone<CM_PRIVATE_STORE>(CM_PRIVATE_STORE_OPCODE,
		PacketWriter().H(2).D(0x0A0B0C0D).D(162000002).H(0xFFFF).Q(0x100000064LL).D(5).D(-6).H(1).Q(-1).data, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_PRIVATE_STORETestAccess::rows(*p), (Rows{{0x0A0B0C0D, 162000002, 65535, 0x100000064LL}, {5, -6, 1, -1}}));
	EXPECT_EQ(unread, 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(PrivateStorePacketsReadTest, TheRowCountIsUnsigned) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	// 0x8001 rows is 32,769 (readUH), not a negative count
	PacketWriter body;
	body.H(0x8001);
	for (int32_t i = 1; i <= 0x8001; i++)
		body.D(i).D(2).H(3).Q(4);
	int32_t unread = -1;
	auto p = readAlone<CM_PRIVATE_STORE>(CM_PRIVATE_STORE_OPCODE, body.data, unread);
	ASSERT_NE(p, nullptr);
	Rows rows = CM_PRIVATE_STORETestAccess::rows(*p);
	ASSERT_EQ(rows.size(), 32769u);
	EXPECT_EQ(rows.front(), (std::array<int64_t, 4>{1, 2, 3, 4}));
	EXPECT_EQ(rows.back(), (std::array<int64_t, 4>{0x8001, 2, 3, 4}));
	EXPECT_EQ(unread, 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(PrivateStorePacketsReadTest, AnEmptyStoreReadsNoRow) {
	int32_t unread = -1;
	auto p = readAlone<CM_PRIVATE_STORE>(CM_PRIVATE_STORE_OPCODE, PacketWriter().H(0).D(7).data, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_TRUE(CM_PRIVATE_STORETestAccess::rows(*p).empty());
	EXPECT_EQ(unread, 4);
}

TEST(PrivateStorePacketsReadTest, TheNameIsAString) {
	int32_t unread = -1;
	auto p = readAlone<CM_PRIVATE_STORE_NAME>(CM_PRIVATE_STORE_NAME_OPCODE, PacketWriter().S("Cheap potions").data, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_PRIVATE_STORE_NAMETestAccess::name(*p), "Cheap potions");
	EXPECT_EQ(unread, 0);
}

TEST(PrivateStorePacketsReadTest, TheMarkersRegisterTheClassesUnderTheirJavaOpcodes) {
	const StateSet inGame{AionConnection_State::IN_GAME};
	EXPECT_NE(dynamic_cast<CM_PRIVATE_STORE*>(CM_PRIVATE_STORE_clientPacketFactory(CM_PRIVATE_STORE_OPCODE, inGame).get()), nullptr);
	EXPECT_NE(dynamic_cast<CM_PRIVATE_STORE_NAME*>(CM_PRIVATE_STORE_NAME_clientPacketFactory(CM_PRIVATE_STORE_NAME_OPCODE, inGame).get()), nullptr);
	EXPECT_EQ(economyTableEntries("CM_PRIVATE_STORE", CM_PRIVATE_STORE_OPCODE), 1);
	EXPECT_EQ(economyTableEntries("CM_PRIVATE_STORE_NAME", CM_PRIVATE_STORE_NAME_OPCODE), 1);
}

/** The emotion type of a captured SM_EMOTION (SM_EMOTION.java writeImpl: D sender, C type) */
int32_t emotionOf(const std::vector<uint8_t>& packet) {
	PacketReader reader(bodyOf(packet));
	reader.D();
	return reader.C();
}

class PrivateStorePacketsTest : public EconomyPacketTest {
protected:
	void SetUp() override {
		EconomyPacketTest::SetUp();
		stored(POTIONS, MINOR_LIFE_POTION, 100);
		stored(SWORD, TRAINING_SWORD, 1);
	}

	void store(const Rows& rows) { readAndRun<CM_PRIVATE_STORE>(CM_PRIVATE_STORE_OPCODE, storeBody(rows)); }

	void name(std::string_view text) { readAndRun<CM_PRIVATE_STORE_NAME>(CM_PRIVATE_STORE_NAME_OPCODE, PacketWriter().S(text).data); }

	/** The store's rows in insertion order: {object id, item id, count, price} */
	Rows storeRows() {
		Rows result;
		for (const runtime::Ptr<model::trade::TradePSItem>& item : player().getStore()->getSoldItems()->values())
			result.push_back({item->getItemObjId(), item->getItemId(), item->getCount(), item->getPrice()});
		return result;
	}

	std::vector<int32_t> emotions() {
		std::vector<int32_t> types;
		for (const std::vector<uint8_t>& packet : packetsOf(sent(), SM_EMOTION_OPCODE))
			types.push_back(emotionOf(packet));
		return types;
	}
};

// :36-41 -> PrivateStoreService.createStoreWithItems: every row, in the request's order
TEST_F(PrivateStorePacketsTest, TheRowsOpenTheStoreInTheirOrder) {
	store({{SWORD, TRAINING_SWORD, 1, 5000}, {POTIONS, MINOR_LIFE_POTION, 30, 100}});

	ASSERT_TRUE(player().getStore());
	EXPECT_EQ(storeRows(), (Rows{{SWORD, TRAINING_SWORD, 1, 5000}, {POTIONS, MINOR_LIFE_POTION, 30, 100}}));
	EXPECT_EQ(emotions(), (std::vector<int32_t>{OPEN_PRIVATESHOP}));
}

// :38-39 -> PrivateStoreService.closePrivateStore
TEST_F(PrivateStorePacketsTest, AnEmptyListClosesTheStore) {
	store({{POTIONS, MINOR_LIFE_POTION, 30, 100}});
	ASSERT_TRUE(player().getStore());
	clearSent();

	store({});

	EXPECT_FALSE(player().getStore());
	EXPECT_EQ(emotions(), (std::vector<int32_t>{CLOSE_PRIVATESHOP}));
}

// CM_PRIVATE_STORE_NAME.java:33-34 -> PrivateStoreService.openPrivateStore: the name is stored and broadcast (SM_PRIVATE_STORE_NAME.java
// writeImpl: D player, S name)
TEST_F(PrivateStorePacketsTest, TheNameIsStoredAndBroadcast) {
	store({{POTIONS, MINOR_LIFE_POTION, 30, 100}});
	clearSent();

	name("Cheap potions");

	EXPECT_EQ(player().getStore()->getStoreMessage(), "Cheap potions");
	EXPECT_EQ(sent(), exactly({javaPacket(SM_PRIVATE_STORE_NAME_OPCODE, PacketWriter().D(player().getObjectId()).S("Cheap potions"))}));
}

TEST_F(PrivateStorePacketsTest, ANameWithoutAStoreIsJavasNullPointerException) {
	EXPECT_THROW(name("Cheap potions"), runtime::NullPointerException);
	EXPECT_TRUE(sent().empty());
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
