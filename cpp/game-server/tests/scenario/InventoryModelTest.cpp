// InventoryModel (InventoryModel.h; m5b3-plan.md G-03, lifted for m5c-plan.md G-02 / X16): the model applied to item packets built field by
// field in the order of their Java writeImpl, as decoders/ItemDecodersTest.cpp builds them - SM_INVENTORY_INFO (the enter-world burst),
// SM_INVENTORY_ADD_ITEM, SM_INVENTORY_UPDATE_ITEM, SM_DELETE_ITEM, the warehouse packets and SM_CUBE_UPDATE - and its bookkeeping: sync()
// applies each recorded packet once, follow() starts over, a body that does not decode or an update of an unknown object is recorded as a
// failure instead of thrown. The item rows are shipped data: the Minor Life Potion (item_templates.xml:830724, 162000002, mask 12414, desc
// 702583), the Training Sword (:375, 100000094, mask 138366) and kinah (:894666, 182400001, mask 12350).

#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "InventoryModel.h"
#include "decoders/ItemDecoders.h"

#include "NetworkTestSupport.h"

namespace aion::gameserver::scenario {
namespace {

using network::test::PacketWriter;
using Packet = GameSession::Packet;

constexpr int32_t KINAH = InventoryModel::KINAH_ITEM_ID;
constexpr int32_t LIFE_POTION = 162000002;
constexpr int32_t TRAINING_SWORD = 100000094;
constexpr int32_t GODSTONE = 168000116;
/** ItemSlot.MAIN_HAND.getSlotIdMask() */
constexpr int64_t MAIN_HAND = 1;

Packet packet(std::string name, const PacketWriter& body) {
	Packet p;
	p.name = std::move(name);
	p.data = body.data;
	return p;
}

/** GeneralInfoBlobEntry (0x00), as ItemDecodersTest.cpp writes it */
void generalInfoEntry(PacketWriter& w, uint16_t itemMask, int64_t count) {
	w.C(0x00);
	w.H(itemMask).Q(count).S("").C(0).D(0).D(0).D(0).H(0).D(0).H(18);
}

/** EnchantInfoBlobEntry (0x0B) of an item without enchantment, with `godStoneId` in its god stone field (EnchantInfoBlobEntry.java:51) */
void enchantInfoEntry(PacketWriter& w, int32_t skinTemplateId, int32_t godStoneId) {
	w.C(0x0B);
	w.C(0).C(0).D(skinTemplateId).C(0).C(0);
	for (int i = 0; i < 6; i++)
		w.D(0);
	w.D(godStoneId);
	w.zeros(4);
	w.C(0).D(0).D(0);
	w.D(0).C(0).C(0);
	w.zeros(18).zeros(16).zeros(16).zeros(16);
	w.D(0).C(0).D(0).D(0).D(0);
}

void blob(PacketWriter& w, const PacketWriter& entries) {
	w.H(static_cast<int32_t>(entries.data.size())).B(entries.data);
}

/** the blob of a potion stack: GENERAL_INFO alone */
PacketWriter stackBlob(int64_t count) {
	PacketWriter entries;
	generalInfoEntry(entries, 12414, count);
	return entries;
}

/** the blob of the kinah item: GENERAL_INFO alone, with kinah's own mask */
PacketWriter kinahBlob(int64_t count) {
	PacketWriter entries;
	generalInfoEntry(entries, 12350, count);
	return entries;
}

/** the blob of the sword: EQUIPPED_SLOT, ENCHANT_INFO with a godstone, GENERAL_INFO */
PacketWriter swordBlob(int64_t equippedSlot, int32_t godStoneId) {
	PacketWriter entries;
	entries.C(0x06).Q(equippedSlot);
	enchantInfoEntry(entries, TRAINING_SWORD, godStoneId);
	generalInfoEntry(entries, static_cast<uint16_t>(138366 & 0xFFFF), 1);
	return entries;
}

/** one item of SM_INVENTORY_INFO / SM_INVENTORY_ADD_ITEM: object id, template id, l10n, blob, slot, cloth */
void cubeItem(PacketWriter& w, int32_t objectId, int32_t itemId, const PacketWriter& entries, int16_t slot) {
	w.D(objectId).D(itemId).S("");
	blob(w, entries);
	w.H(slot).C(0);
}

/** SM_INVENTORY_INFO of a fresh Warrior: kinah, a potion stack in slot 11 and the equipped sword */
Packet enterWorldInventory(bool firstPacket = true) {
	PacketWriter w;
	w.C(firstPacket ? 1 : 0).C(0).C(0).C(0).H(3);
	cubeItem(w, 0x7000001, KINAH, kinahBlob(1000), -1);
	cubeItem(w, 0x7000002, LIFE_POTION, stackBlob(100), 11);
	cubeItem(w, 0x7000003, TRAINING_SWORD, swordBlob(MAIN_HAND, 0), 1);
	return packet("SM_INVENTORY_INFO", w);
}

Packet addItem(int32_t objectId, int32_t itemId, int64_t count, int16_t slot) {
	PacketWriter w;
	w.H(decoders::ITEM_ADD_BUY).H(1);
	cubeItem(w, objectId, itemId, stackBlob(count), slot);
	return packet("SM_INVENTORY_ADD_ITEM", w);
}

Packet updateItem(int32_t objectId, const PacketWriter& entries, uint16_t updateType = decoders::ITEM_UPDATE_DEC_ITEM_USE) {
	PacketWriter w;
	w.D(objectId).S("");
	blob(w, entries);
	w.H(updateType);
	return packet("SM_INVENTORY_UPDATE_ITEM", w);
}

Packet deleteItem(int32_t objectId) {
	PacketWriter w;
	w.D(objectId).C(decoders::ITEM_DELETE_USE);
	return packet("SM_DELETE_ITEM", w);
}

/** SM_WAREHOUSE_ADD_ITEM (SM_WAREHOUSE_ADD_ITEM.java:33-51): type, add type, one item - object id, template id, writeC(0), l10n, blob, slot */
Packet warehouseAddItem(uint8_t warehouseType, int32_t objectId, int32_t itemId, const PacketWriter& entries, int16_t slot) {
	PacketWriter w;
	w.C(warehouseType).H(decoders::ITEM_ADD_ALL_SLOT).H(1);
	w.D(objectId).D(itemId).C(0).S("");
	blob(w, entries);
	w.H(slot);
	return packet("SM_WAREHOUSE_ADD_ITEM", w);
}

/** SM_WAREHOUSE_UPDATE_ITEM (SM_WAREHOUSE_UPDATE_ITEM.java:33-42): object id, type, l10n, the GENERAL_INFO blob, the update type */
Packet warehouseUpdateItem(int32_t objectId, uint8_t warehouseType, const PacketWriter& entries, uint16_t updateType) {
	PacketWriter w;
	w.D(objectId).C(warehouseType).S("");
	blob(w, entries);
	w.H(updateType);
	return packet("SM_WAREHOUSE_UPDATE_ITEM", w);
}

TEST(InventoryModelTest, EnterWorldThenAddUpdateAndDelete) {
	std::vector<Packet> recorded{enterWorldInventory()};
	InventoryModel model;
	model.followPackets(&recorded);
	model.sync();
	ASSERT_EQ(model.items.size(), 3u) << model.describe();
	EXPECT_EQ(model.kinah(), 1000);
	const std::optional<ModelItem> sword = model.equipped(TRAINING_SWORD);
	ASSERT_TRUE(sword);
	EXPECT_EQ(sword->equippedSlot, MAIN_HAND);
	EXPECT_EQ(sword->slot, 1);
	EXPECT_TRUE(model.byItemId(TRAINING_SWORD).empty()) << "byItemId lists unequipped stacks only";
	const std::vector<ModelItem> stacks = model.cubeStacks();
	ASSERT_EQ(stacks.size(), 1u) << "the kinah and the equipped sword take no cube slot: " << model.describe();
	EXPECT_EQ(stacks[0].itemId, LIFE_POTION);
	EXPECT_EQ(stacks[0].count, 100);
	EXPECT_EQ(stacks[0].slot, 11);
	EXPECT_EQ(stacks[0].location, ModelItem::CUBE);

	// a buy (SM_INVENTORY_ADD_ITEM), a potion used (SM_INVENTORY_UPDATE_ITEM, a count without template id or slot), the kinah paid, a deletion
	recorded.push_back(addItem(0x7000004, 162000052, 2, 12));
	recorded.push_back(updateItem(0x7000002, stackBlob(99)));
	recorded.push_back(updateItem(0x7000001, kinahBlob(296), decoders::ITEM_UPDATE_STATS_CHANGE));
	model.sync();
	EXPECT_EQ(model.byObjectId(0x7000004)->count, 2);
	EXPECT_EQ(model.byObjectId(0x7000004)->itemId, 162000052);
	const std::optional<ModelItem> potion = model.byObjectId(0x7000002);
	ASSERT_TRUE(potion);
	EXPECT_EQ(potion->count, 99);
	EXPECT_EQ(potion->itemId, LIFE_POTION) << "an update keeps the template id it does not carry";
	EXPECT_EQ(potion->slot, 11) << "... and the slot";
	EXPECT_EQ(model.kinah(), 296);
	EXPECT_EQ(model.cubeStacks().size(), 2u);

	recorded.push_back(deleteItem(0x7000004));
	model.sync();
	EXPECT_FALSE(model.byObjectId(0x7000004));
	EXPECT_TRUE(model.decodeFailures.empty()) << model.decodeFailures.front();
}

TEST(InventoryModelTest, EquipStateAndGodstoneFollowTheUpdates) {
	std::vector<Packet> recorded{enterWorldInventory()};
	InventoryModel model;
	model.followPackets(&recorded);
	// unequip: the EQUIPPED_SLOT entry says 0 (ItemPacketService.updateItemAfterEquip), then a godstone is socketed
	recorded.push_back(updateItem(0x7000003, swordBlob(0, 0), decoders::ITEM_UPDATE_STATS_CHANGE));
	model.sync();
	EXPECT_FALSE(model.equipped(TRAINING_SWORD));
	ASSERT_EQ(model.byItemId(TRAINING_SWORD).size(), 1u);
	EXPECT_EQ(model.cubeStacks().size(), 2u) << "an unequipped sword takes a cube slot";

	recorded.push_back(updateItem(0x7000003, swordBlob(0, GODSTONE), decoders::ITEM_UPDATE_STATS_CHANGE));
	recorded.push_back(updateItem(0x7000003, swordBlob(MAIN_HAND, GODSTONE), decoders::ITEM_UPDATE_STATS_CHANGE));
	model.sync();
	ASSERT_TRUE(model.equipped(TRAINING_SWORD));
	EXPECT_EQ(model.equipped(TRAINING_SWORD)->godStoneId, GODSTONE);
}

TEST(InventoryModelTest, SyncAppliesEachPacketOnceAndFollowStartsOver) {
	std::vector<Packet> recorded{enterWorldInventory()};
	InventoryModel model;
	model.sync(); // following nothing: no-op
	EXPECT_TRUE(model.items.empty());
	model.followPackets(&recorded);
	model.sync();
	recorded.push_back(updateItem(0x7000002, stackBlob(90)));
	model.sync();
	model.sync(); // nothing new
	EXPECT_EQ(model.byObjectId(0x7000002)->count, 90);

	// a packet applied twice would show here: an ADD of a new stack after its DELETE must not come back on the next sync
	recorded.push_back(addItem(0x7000005, LIFE_POTION, 5, 13));
	recorded.push_back(deleteItem(0x7000005));
	model.sync();
	model.sync();
	EXPECT_FALSE(model.byObjectId(0x7000005));

	// a relog: the new session's recorder starts with its own enter-world burst; the items are cleared, the failures are kept
	model.decodeFailures.push_back("an earlier failure");
	std::vector<Packet> relogged{enterWorldInventory()};
	model.followPackets(&relogged);
	EXPECT_TRUE(model.items.empty()) << "follow clears the items";
	model.sync();
	EXPECT_EQ(model.byObjectId(0x7000002)->count, 100) << "the new session's SM_INVENTORY_INFO, not the old update";
	EXPECT_EQ(model.decodeFailures.size(), 1u);
}

TEST(InventoryModelTest, AFirstInventoryPacketReplacesTheCubeAndKeepsTheWarehouse) {
	std::vector<Packet> recorded;
	PacketWriter warehouse; // SM_WAREHOUSE_INFO of the regular warehouse with one item: type, first, expand level, the 1/0 branch, count
	warehouse.C(decoders::STORAGE_REGULAR_WAREHOUSE).C(1).C(0).C(1).C(0).H(1);
	warehouse.D(0x7000010).D(LIFE_POTION).C(0).S("");
	blob(warehouse, stackBlob(7));
	warehouse.H(0);
	recorded.push_back(packet("SM_WAREHOUSE_INFO", warehouse));
	PacketWriter account; // the account warehouse is not modelled
	account.C(decoders::STORAGE_ACCOUNT_WAREHOUSE).C(1).C(0).H(0).H(1);
	account.D(0x7000011).D(LIFE_POTION).C(0).S("");
	blob(account, stackBlob(3));
	account.H(0);
	recorded.push_back(packet("SM_WAREHOUSE_INFO", account));
	recorded.push_back(enterWorldInventory());
	recorded.push_back(addItem(0x7000004, 162000052, 2, 12));
	recorded.push_back(enterWorldInventory(true)); // a second first packet (a relog's burst on the same recorder) replaces the cube

	InventoryModel model;
	model.followPackets(&recorded);
	model.sync();
	EXPECT_FALSE(model.byObjectId(0x7000004)) << "SM_INVENTORY_INFO's first packet clears the cube's items";
	EXPECT_FALSE(model.byObjectId(0x7000011)) << "only the regular warehouse is modelled";
	const std::vector<ModelItem> stored = model.byItemId(LIFE_POTION, ModelItem::REGULAR_WAREHOUSE);
	ASSERT_EQ(stored.size(), 1u) << model.describe();
	EXPECT_EQ(stored[0].count, 7);

	// a later packet that is not the first one adds to the cube instead
	PacketWriter more;
	more.C(0).C(0).C(0).C(0).H(1);
	cubeItem(more, 0x7000006, 162000052, stackBlob(4), 14);
	recorded.push_back(packet("SM_INVENTORY_INFO", more));
	PacketWriter moved; // SM_DELETE_WAREHOUSE_ITEM: type, object id, delete type
	moved.C(decoders::STORAGE_REGULAR_WAREHOUSE).D(0x7000010).C(decoders::ITEM_DELETE_MOVE);
	recorded.push_back(packet("SM_DELETE_WAREHOUSE_ITEM", moved));
	model.sync();
	EXPECT_EQ(model.byObjectId(0x7000006)->count, 4);
	EXPECT_EQ(model.byObjectId(0x7000002)->count, 100) << "the earlier cube items stay";
	EXPECT_TRUE(model.byItemId(LIFE_POTION, ModelItem::REGULAR_WAREHOUSE).empty());
}

TEST(InventoryModelTest, FailuresAreRecordedAndTheCubeCountKept) {
	std::vector<Packet> recorded{enterWorldInventory()};
	InventoryModel model;
	model.followPackets(&recorded);

	recorded.push_back(updateItem(0x7000099, stackBlob(1))); // an object the client never got
	PacketWriter broken;
	broken.D(0x7000002).C(decoders::ITEM_DELETE_USE).C(0); // SM_DELETE_ITEM with a trailing byte
	recorded.push_back(packet("SM_DELETE_ITEM", broken));
	PacketWriter cube; // SM_CUBE_UPDATE.cubeSize: action 0, StorageType CUBE, the stack count and the three expansion bytes
	cube.C(0).C(decoders::STORAGE_CUBE).D(2).C(1).C(0).C(0);
	recorded.push_back(packet("SM_CUBE_UPDATE", cube));
	recorded.push_back(packet("SM_PRICES", PacketWriter().C(125).C(100).C(113))); // not an item packet: ignored
	model.sync();
	model.sync(); // each packet is applied once: a second sync must not record the two failures again

	ASSERT_EQ(model.decodeFailures.size(), 2u);
	EXPECT_NE(model.decodeFailures[0].find("an update of object " + std::to_string(0x7000099)), std::string::npos) << model.decodeFailures[0];
	EXPECT_EQ(model.decodeFailures[1].rfind("SM_DELETE_ITEM at 2: ", 0), 0u) << "the packet name and its index: " << model.decodeFailures[1];
	EXPECT_TRUE(model.byObjectId(0x7000002)) << "a body that does not decode changes nothing";
	EXPECT_EQ(model.lastCubeUpdateCount.at(decoders::STORAGE_CUBE), 2);
	EXPECT_EQ(model.items.size(), 3u);
}

TEST(InventoryModelTest, WarehouseItemsAGodstoneOnArrivalAndOtherCubeUpdates) {
	std::vector<Packet> recorded{enterWorldInventory()};
	InventoryModel model;
	model.followPackets(&recorded);

	// a stack put into the regular warehouse arrives with SM_WAREHOUSE_ADD_ITEM and lives there, not in the cube
	recorded.push_back(warehouseAddItem(decoders::STORAGE_REGULAR_WAREHOUSE, 0x7000020, LIFE_POTION, stackBlob(30), 0));
	model.sync();
	const std::optional<ModelItem> stored = model.byObjectId(0x7000020);
	ASSERT_TRUE(stored) << model.describe();
	EXPECT_EQ(stored->location, ModelItem::REGULAR_WAREHOUSE);
	EXPECT_EQ(model.byItemId(LIFE_POTION, ModelItem::REGULAR_WAREHOUSE).size(), 1u);
	EXPECT_EQ(model.cubeStacks().size(), 1u) << "a warehouse stack takes no cube slot: " << model.describe();

	// a partial withdrawal: SM_WAREHOUSE_UPDATE_ITEM carries the new count
	recorded.push_back(warehouseUpdateItem(0x7000020, decoders::STORAGE_REGULAR_WAREHOUSE, stackBlob(25), decoders::ITEM_UPDATE_DEC_ITEM_SPLIT_MOVE));
	model.sync();
	EXPECT_EQ(model.byObjectId(0x7000020)->count, 25);
	EXPECT_EQ(model.byObjectId(0x7000020)->location, ModelItem::REGULAR_WAREHOUSE) << "an update keeps the storage";

	// an account-warehouse kinah row (SM_WAREHOUSE_ADD_ITEM of the kinah item into StorageType.ACCOUNT_WAREHOUSE) with a lower object id than
	// the cube's: kinah() is the cube's kinah only
	recorded.push_back(warehouseAddItem(decoders::STORAGE_ACCOUNT_WAREHOUSE, 0x7000000, KINAH, kinahBlob(5000), -1));
	model.sync();
	ASSERT_TRUE(model.byObjectId(0x7000000));
	EXPECT_EQ(model.kinah(), 1000);

	// a sword that arrives with a godstone already socketed (SM_INVENTORY_ADD_ITEM's ENCHANT_INFO entry) keeps it
	PacketWriter add;
	add.H(decoders::ITEM_ADD_BUY).H(1);
	cubeItem(add, 0x7000030, TRAINING_SWORD, swordBlob(0, GODSTONE), 15);
	recorded.push_back(packet("SM_INVENTORY_ADD_ITEM", add));
	model.sync();
	ASSERT_TRUE(model.byObjectId(0x7000030));
	EXPECT_EQ(model.byObjectId(0x7000030)->godStoneId, GODSTONE);

	// SM_CUBE_UPDATE's item count is recorded for its action 0 (cubeSize) only; stigmaSlots is action 6 with the slot count in the storage byte
	// (SM_CUBE_UPDATE.java:25-27, 70-78)
	PacketWriter cube;
	cube.C(0).C(decoders::STORAGE_CUBE).D(3).C(0).C(0).C(0);
	recorded.push_back(packet("SM_CUBE_UPDATE", cube));
	recorded.push_back(packet("SM_CUBE_UPDATE", PacketWriter().C(6).C(1)));
	model.sync();
	EXPECT_EQ(model.lastCubeUpdateCount, (std::map<int32_t, int32_t>{{decoders::STORAGE_CUBE, 3}}));
	EXPECT_TRUE(model.decodeFailures.empty()) << model.decodeFailures.front();
}

} // namespace
} // namespace aion::gameserver::scenario
