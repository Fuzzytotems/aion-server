// CM_MOVE_ITEM, CM_SPLIT_ITEM and CM_REPLACE_ITEM (P5-16, m5b3-plan.md P-05/P-06): C_MOVE_ITEM_TO_ANOTHER_SLOT (drag an item to another slot or
// storage), C_MOVE_STACKABLE_ITEM (drag part of a stack, or merge two stacks) and C_SWAP_ITEM_SLOT (drop an item on one of another storage).
//
// Java: game-server/src/com/aionemu/gameserver/network/aion/clientpackets/CM_MOVE_ITEM.java:24-36, CM_SPLIT_ITEM.java:26-40,
// CM_REPLACE_ITEM.java:24-36.
//
// The three packets only read and hand their fields to ItemMoveService / ItemSplitService, whose arms are the items lane's tests
// (tests/itemsvc). What can go wrong here is the reading (field order, widths, signedness) and the argument order of the service calls, so every
// case uses values that tell the fields apart: a move is found only in its source storage, a split's amount, slot and target stack are
// different numbers, and a swap puts each item on the other's slot.

#include "../cm_ak/ItemPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/aion/clientpackets/CM_MOVE_ITEM.h"
#include "aion/gameserver/network/aion/clientpackets/CM_REPLACE_ITEM.h"
#include "aion/gameserver/network/aion/clientpackets/CM_SPLIT_ITEM.h"
#include "aion/gameserver/runtime/base/Unported.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_MOVE_ITEM_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_SPLIT_ITEM_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_REPLACE_ITEM_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friends the three headers declare: the fields readImpl decoded, which Java keeps without getters */
struct CM_MOVE_ITEMTestAccess {
	static int32_t itemObjId(const CM_MOVE_ITEM& p) { return p.itemObjId; }
	static int32_t source(const CM_MOVE_ITEM& p) { return p.source; }
	static int32_t destination(const CM_MOVE_ITEM& p) { return p.destination; }
	static int32_t slot(const CM_MOVE_ITEM& p) { return p.slot; }
};

struct CM_SPLIT_ITEMTestAccess {
	static int32_t sourceItemObjId(const CM_SPLIT_ITEM& p) { return p.sourceItemObjId; }
	static int64_t itemAmount(const CM_SPLIT_ITEM& p) { return p.itemAmount; }
	static int32_t sourceStorageType(const CM_SPLIT_ITEM& p) { return p.sourceStorageType; }
	static int32_t destinationItemObjId(const CM_SPLIT_ITEM& p) { return p.destinationItemObjId; }
	static int32_t destinationStorageType(const CM_SPLIT_ITEM& p) { return p.destinationStorageType; }
	static int32_t slotNum(const CM_SPLIT_ITEM& p) { return p.slotNum; }
};

struct CM_REPLACE_ITEMTestAccess {
	static int32_t sourceStorageType(const CM_REPLACE_ITEM& p) { return p.sourceStorageType; }
	static int32_t sourceItemObjId(const CM_REPLACE_ITEM& p) { return p.sourceItemObjId; }
	static int32_t replaceStorageType(const CM_REPLACE_ITEM& p) { return p.replaceStorageType; }
	static int32_t replaceItemObjId(const CM_REPLACE_ITEM& p) { return p.replaceItemObjId; }
};

namespace testing::items {
namespace {

using network::test::LogCapture;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";

/** the decoded opcodes of ClientPacketInfo.gen.inc:139-140, :156 (Java AionClientPacketFactory.java:184-185, :206, State.IN_GAME) */
constexpr int32_t CM_MOVE_ITEM_OPCODE = 156;
constexpr int32_t CM_SPLIT_ITEM_OPCODE = 157;
constexpr int32_t CM_REPLACE_ITEM_OPCODE = 178;

constexpr int32_t CUBE = 0;              // StorageType.CUBE.getId()
constexpr int32_t REGULAR_WAREHOUSE = 1; // StorageType.REGULAR_WAREHOUSE.getId()

/** Java CM_MOVE_ITEM.readImpl: D item, C source, C destination, H slot */
std::vector<uint8_t> moveBody(int32_t itemObjId, int32_t source, int32_t destination, int32_t slot) {
	return PacketWriter().D(itemObjId).C(source).C(destination).H(slot).data;
}

/** Java CM_SPLIT_ITEM.readImpl: D source item, Q amount, C source storage, D destination item, C destination storage, H slot */
std::vector<uint8_t> splitBody(int32_t source, int64_t amount, int32_t sourceStorage, int32_t destination, int32_t destinationStorage, int32_t slot) {
	return PacketWriter().D(source).Q(amount).C(sourceStorage).D(destination).C(destinationStorage).H(slot).data;
}

/** Java CM_REPLACE_ITEM.readImpl: C source storage, D source item, C replace storage, D replace item */
std::vector<uint8_t> replaceBody(int32_t sourceStorage, int32_t sourceItem, int32_t replaceStorage, int32_t replaceItem) {
	return PacketWriter().C(sourceStorage).D(sourceItem).C(replaceStorage).D(replaceItem).data;
}

template <class P>
std::unique_ptr<P> readPacket(const std::vector<uint8_t>& data, int32_t opcode, int32_t& unread) {
	std::vector<uint8_t> copy = data;
	auto packet = std::make_unique<P>(opcode, StateSet{AionConnection_State::IN_GAME});
	packet->setBuffer(commons::utils::ByteBuffer::wrap(copy));
	if (!packet->read())
		return nullptr;
	unread = packet->getRemainingBytes();
	return packet;
}

TEST(ItemStoragePacketsReadTest, MoveReadsItsFourFieldsInJavaOrder) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	std::unique_ptr<CM_MOVE_ITEM> p = readPacket<CM_MOVE_ITEM>(moveBody(0x01020304, 1, 3, 0xFFFF), CM_MOVE_ITEM_OPCODE, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_MOVE_ITEMTestAccess::itemObjId(*p), 0x01020304);
	EXPECT_EQ(CM_MOVE_ITEMTestAccess::source(*p), 1);
	EXPECT_EQ(CM_MOVE_ITEMTestAccess::destination(*p), 3);
	EXPECT_EQ(CM_MOVE_ITEMTestAccess::slot(*p), -1) << "readH is a signed short: 0xFFFF is the merge slot -1 of ItemMoveService.java:59";
	EXPECT_EQ(unread, 0) << "4 + 1 + 1 + 2 bytes";
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(ItemStoragePacketsReadTest, SplitReadsItsSixFieldsInJavaOrder) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	std::unique_ptr<CM_SPLIT_ITEM> p =
		readPacket<CM_SPLIT_ITEM>(splitBody(0x0A0B0C0D, 0x0000000100000002LL, 2, 0x11121314, 1, 0x8001), CM_SPLIT_ITEM_OPCODE, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_SPLIT_ITEMTestAccess::sourceItemObjId(*p), 0x0A0B0C0D);
	EXPECT_EQ(CM_SPLIT_ITEMTestAccess::itemAmount(*p), 0x0000000100000002LL) << "readQ keeps the high half (Java long)";
	EXPECT_EQ(CM_SPLIT_ITEMTestAccess::sourceStorageType(*p), 2);
	EXPECT_EQ(CM_SPLIT_ITEMTestAccess::destinationItemObjId(*p), 0x11121314);
	EXPECT_EQ(CM_SPLIT_ITEMTestAccess::destinationStorageType(*p), 1);
	EXPECT_EQ(CM_SPLIT_ITEMTestAccess::slotNum(*p), -32767) << "readH is signed: 0x8001";
	EXPECT_EQ(unread, 0) << "4 + 8 + 1 + 4 + 1 + 2 bytes";
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(ItemStoragePacketsReadTest, ReplaceReadsItsFourFieldsInJavaOrder) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	std::unique_ptr<CM_REPLACE_ITEM> p = readPacket<CM_REPLACE_ITEM>(replaceBody(1, 0x21222324, 0, 0x31323334), CM_REPLACE_ITEM_OPCODE, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_REPLACE_ITEMTestAccess::sourceStorageType(*p), 1);
	EXPECT_EQ(CM_REPLACE_ITEMTestAccess::sourceItemObjId(*p), 0x21222324);
	EXPECT_EQ(CM_REPLACE_ITEMTestAccess::replaceStorageType(*p), 0);
	EXPECT_EQ(CM_REPLACE_ITEMTestAccess::replaceItemObjId(*p), 0x31323334);
	EXPECT_EQ(unread, 0) << "1 + 4 + 1 + 4 bytes";
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(ItemStoragePacketsReadTest, ShortBodiesLogTheMissingField) {
	int32_t unread = -1;
	{
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		ASSERT_NE(readPacket<CM_MOVE_ITEM>(PacketWriter().D(1).C(0).C(1).data, CM_MOVE_ITEM_OPCODE, unread), nullptr);
		EXPECT_EQ(capture.count("Missing H"), 1) << capture.dump();
	}
	{ // the amount underflows (4 bytes left for a Q); the base class answers 0 and reads on, as Java's BaseClientPacket does
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		ASSERT_NE(readPacket<CM_SPLIT_ITEM>(PacketWriter().D(1).D(5).data, CM_SPLIT_ITEM_OPCODE, unread), nullptr);
		EXPECT_EQ(capture.count("Missing Q"), 1) << capture.dump();
	}
	{
		LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
		ASSERT_NE(readPacket<CM_REPLACE_ITEM>(PacketWriter().C(0).D(1).C(1).data, CM_REPLACE_ITEM_OPCODE, unread), nullptr);
		EXPECT_EQ(capture.count("Missing D"), 1) << capture.dump();
	}
}

TEST(ItemStoragePacketsReadTest, TheMarkersRegisterTheClassesUnderTheirJavaOpcodes) {
	const StateSet inGame{AionConnection_State::IN_GAME};
	EXPECT_NE(dynamic_cast<CM_MOVE_ITEM*>(CM_MOVE_ITEM_clientPacketFactory(CM_MOVE_ITEM_OPCODE, inGame).get()), nullptr);
	EXPECT_NE(dynamic_cast<CM_SPLIT_ITEM*>(CM_SPLIT_ITEM_clientPacketFactory(CM_SPLIT_ITEM_OPCODE, inGame).get()), nullptr);
	EXPECT_NE(dynamic_cast<CM_REPLACE_ITEM*>(CM_REPLACE_ITEM_clientPacketFactory(CM_REPLACE_ITEM_OPCODE, inGame).get()), nullptr);
	int32_t found = 0;
#define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...)                                                                             \
	if (std::string_view(#Class) == "CM_MOVE_ITEM") {                                                                                                   \
		EXPECT_EQ(opcode, CM_MOVE_ITEM_OPCODE);                                                                                                         \
		++found;                                                                                                                                        \
	} else if (std::string_view(#Class) == "CM_SPLIT_ITEM") {                                                                                           \
		EXPECT_EQ(opcode, CM_SPLIT_ITEM_OPCODE);                                                                                                        \
		++found;                                                                                                                                        \
	} else if (std::string_view(#Class) == "CM_REPLACE_ITEM") {                                                                                         \
		EXPECT_EQ(opcode, CM_REPLACE_ITEM_OPCODE);                                                                                                      \
		++found;                                                                                                                                        \
	}
#include "aion/gameserver/network/aion/ClientPacketInfo.gen.inc"
#undef AION_CLIENT_PACKET_INFO
	EXPECT_EQ(found, 3);
}

class ItemStoragePacketsRunTest : public ItemPacketTest {
protected:
	void SetUp() override {
		ItemPacketTest::SetUp();
		runtime::resetUnportedHitsForTests();
	}

	void move(int32_t itemObjId, int32_t source, int32_t destination, int32_t slot) {
		Driver<CM_MOVE_ITEM> packet(CM_MOVE_ITEM_OPCODE);
		packet.readAndRun(moveBody(itemObjId, source, destination, slot), client->get());
	}

	void split(int32_t source, int64_t amount, int32_t sourceStorage, int32_t destination, int32_t destinationStorage, int32_t slot) {
		Driver<CM_SPLIT_ITEM> packet(CM_SPLIT_ITEM_OPCODE);
		packet.readAndRun(splitBody(source, amount, sourceStorage, destination, destinationStorage, slot), client->get());
	}

	void replace(int32_t sourceStorage, int32_t sourceItem, int32_t replaceStorage, int32_t replaceItem) {
		Driver<CM_REPLACE_ITEM> packet(CM_REPLACE_ITEM_OPCODE);
		packet.readAndRun(replaceBody(sourceStorage, sourceItem, replaceStorage, replaceItem), client->get());
	}
};

TEST_F(ItemStoragePacketsRunTest, AMoveWithinTheCubeOnlyChangesTheSlot) {
	Item& junk = stored(740001, SPARKIE_CARAPACE_FRAGMENT, 2, StorageType::CUBE, 4);

	move(740001, CUBE, CUBE, 9);

	// CM_MOVE_ITEM.java:35 -> ItemMoveService.moveItem, the same-storage arm (ItemMoveService.java:41-45): the slot, no packet
	EXPECT_EQ(junk.getEquipmentSlot(), 9);
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(ItemStoragePacketsRunTest, AMoveIntoTheWarehouseLeavesTheCube) {
	Item& junk = stored(740002, SPARKIE_CARAPACE_FRAGMENT, 2, StorageType::CUBE, 4);

	move(740002, CUBE, REGULAR_WAREHOUSE, 6);

	// ItemMoveService.java:75-78: removed from the source (the cube, found there only because the source is the packet's second field),
	// deleted from the client with MOVE's mask 0x14, then added to the warehouse on the slot of the packet's last field
	EXPECT_FALSE(storage(StorageType::CUBE).getItemByObjId(740002));
	EXPECT_EQ(storage(StorageType::REGULAR_WAREHOUSE).getItemByObjId(740002).get(), &junk);
	EXPECT_EQ(junk.getEquipmentSlot(), 6);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_GE(packets.size(), 2u);
	EXPECT_EQ(packets[0], deleteItem(740002, 0x14));
	EXPECT_EQ(packets[1], cubeSize(StorageType::CUBE, 0));
	EXPECT_EQ(opcodesOf(packets), (std::vector<int32_t>{SM_DELETE_ITEM_OPCODE, SM_CUBE_UPDATE_OPCODE, SM_WAREHOUSE_ADD_ITEM_OPCODE,
									  SM_CUBE_UPDATE_OPCODE}));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(ItemStoragePacketsRunTest, AMoveOfAnItemThatIsNotInTheSourceDoesNothing) {
	stored(740003, SPARKIE_CARAPACE_FRAGMENT, 2, StorageType::CUBE, 4);

	move(740003, REGULAR_WAREHOUSE, CUBE, 6); // the source and the destination the other way round

	EXPECT_TRUE(storage(StorageType::CUBE).getItemByObjId(740003));
	EXPECT_TRUE(sent().empty());
}

TEST_F(ItemStoragePacketsRunTest, ASplitWithinTheCubeMakesANewStackOnTheSlotOfThePacket) {
	Item& potions = stored(740011, MINOR_LIFE_POTION, 10, StorageType::CUBE, 2);

	split(740011, 3, CUBE, 0, CUBE, 7);

	// CM_SPLIT_ITEM.java:39 -> ItemSplitService.splitItem(player, 740011, 0, 3, 7, 0, 0): no target stack, so a new item of 3 on slot 7 and the
	// source decreased to 7 (ItemSplitService.java:71-88)
	EXPECT_EQ(potions.getItemCount(), 7);
	std::vector<runtime::Ptr<Item>> stacks = storage(StorageType::CUBE).getItemsByItemId(MINOR_LIFE_POTION);
	ASSERT_EQ(stacks.size(), 2u);
	runtime::Ptr<Item> newStack = stacks[0]->getObjectId() == 740011 ? stacks[1] : stacks[0];
	EXPECT_EQ(newStack->getItemCount(), 3);
	EXPECT_EQ(newStack->getEquipmentSlot(), 7);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(ItemStoragePacketsRunTest, ASplitOntoAStackOfTheSameItemMergesIntoIt) {
	Item& source = stored(740012, MINOR_LIFE_POTION, 10, StorageType::CUBE, 2);
	Item& target = stored(740013, MINOR_LIFE_POTION, 5, StorageType::CUBE, 3);

	split(740012, 4, CUBE, 740013, CUBE, 3);

	// the packet's fourth field names the target stack: mergeStacks (ItemSplitService.java:89-94)
	EXPECT_EQ(source.getItemCount(), 6);
	EXPECT_EQ(target.getItemCount(), 9);
}

TEST_F(ItemStoragePacketsRunTest, ASplitIntoTheWarehouseNamesBothStorages) {
	Item& potions = stored(740014, MINOR_LIFE_POTION, 10, StorageType::CUBE, 2);

	split(740014, 3, CUBE, 0, REGULAR_WAREHOUSE, 7);

	// the source storage (the third field) is the cube, the destination (the fifth) the warehouse; a player without a legion makes
	// addWHItemHistory a no-op (LegionService.java:1082-1092)
	EXPECT_EQ(potions.getItemCount(), 7);
	std::vector<runtime::Ptr<Item>> inWarehouse = storage(StorageType::REGULAR_WAREHOUSE).getItemsByItemId(MINOR_LIFE_POTION);
	ASSERT_EQ(inWarehouse.size(), 1u);
	EXPECT_EQ(inWarehouse[0]->getItemCount(), 3);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(ItemStoragePacketsRunTest, ASwapPutsEachItemOnTheOthersSlotAndStorage) {
	Item& junk = stored(740021, SPARKIE_CARAPACE_FRAGMENT, 2, StorageType::CUBE, 4);
	Item& potions = stored(740022, MINOR_LIFE_POTION, 10, StorageType::REGULAR_WAREHOUSE, 8);

	replace(CUBE, 740021, REGULAR_WAREHOUSE, 740022);

	// CM_REPLACE_ITEM.java:35 -> ItemMoveService.switchItemsInStorages (ItemMoveService.java:86-125): both deletes first, then both adds
	EXPECT_EQ(storage(StorageType::REGULAR_WAREHOUSE).getItemByObjId(740021).get(), &junk);
	EXPECT_EQ(storage(StorageType::CUBE).getItemByObjId(740022).get(), &potions);
	EXPECT_EQ(junk.getEquipmentSlot(), 8);
	EXPECT_EQ(potions.getEquipmentSlot(), 4);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_FALSE(packets.empty());
	EXPECT_EQ(packets[0], deleteItem(740021, 0x14)) << "the cube item is the source: its delete comes first";
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(ItemStoragePacketsRunTest, ASwapWithTheStoragesTheOtherWayRoundFindsNothing) {
	stored(740023, SPARKIE_CARAPACE_FRAGMENT, 2, StorageType::CUBE, 4);
	stored(740024, MINOR_LIFE_POTION, 10, StorageType::REGULAR_WAREHOUSE, 8);

	replace(REGULAR_WAREHOUSE, 740021, CUBE, 740022);
	replace(REGULAR_WAREHOUSE, 740023, CUBE, 740024);

	EXPECT_TRUE(storage(StorageType::CUBE).getItemByObjId(740023));
	EXPECT_TRUE(storage(StorageType::REGULAR_WAREHOUSE).getItemByObjId(740024));
	EXPECT_TRUE(sent().empty());
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
