// P5-07 ItemMoveService, ItemRestrictionService and ItemSplitService (m5b3-plan.md T-03, T-07): moving within and between storages, the
// merge of `slot == -1`, the swap of two items and its packet order, splitting a stack and kinah, and the restriction table, against
// ItemMoveService.java:25-126, ItemRestrictionService.java:21-79 and ItemSplitService.java:29-147. The character has no legion, so a
// cross-storage split reaches LegionService.addWHItemHistory's no-op (T-08, LegionService.java:1082-1092). Not covered: the refusals while
// trading (player.isTrading(): ExchangeService.registerExchange is still unported and its exchange map is private, so no test can open one).

#include "ItemServicesTestSupport.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include "aion/commons/utils/ExitCode.h"
#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/ShutdownHook.h"
#include "aion/gameserver/configs/main/LegionConfig.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_WAREHOUSE_ADD_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_WAREHOUSE_UPDATE_ITEM.h"
#include "aion/gameserver/services/item/ItemMoveService.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemRestrictionService.h"
#include "aion/gameserver/services/item/ItemSplitService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test {
namespace {

using network::aion::serverpackets::SM_INVENTORY_ADD_ITEM;
using network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using network::aion::serverpackets::SM_WAREHOUSE_ADD_ITEM;
using network::aion::serverpackets::SM_WAREHOUSE_UPDATE_ITEM;
using ItemAddType = ItemPacketService::ItemAddType;
using ItemUpdateType = ItemPacketService::ItemUpdateType;
using PersistentState = model::gameobjects::Persistable::PersistentState;

// StorageType ids (StorageType.java): the bytes the client packets carry
constexpr int8_t CUBE_ID = 0;
constexpr int8_t REGULAR_WAREHOUSE_ID = 1;
constexpr int8_t ACCOUNT_WAREHOUSE_ID = 2;

/** Item.equipmentSlot's initial value, ItemStorage.FIRST_AVAILABLE_SLOT (Item.java:45, ItemStorage.java:16) */
constexpr int64_t FIRST_AVAILABLE_SLOT = 65535;

void noExit(int32_t) {
}

/**
 * GameServer.isShuttingDownSoon() for the scope: a shutdown scheduled with 30 s left (GameServer.initShutdown -> ShutdownHook.initShutdown), its
 * hook thread held in the first worldHasPlayers until the scope ends, every other operation and the exit a no-op (the pattern of
 * tests/app/GameServerTest.cpp ShutdownQueriesReadTheShutdownHook). The destructor releases the thread, waits for it and resets the hook.
 */
class ShutdownSoonScope {
public:
	ShutdownSoonScope() {
		ShutdownHook::setExitFunctionForTests(&noExit);
		ShutdownHook::Operations ops;
		ops.worldHasPlayers = [this] {
			while (!release.load())
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			return false;
		};
		ops.announceShutdown = [](int32_t) {};
		ops.shutdownNetwork = [] {};
		ops.dumpStats = [] {};
		ops.saveData = [] {};
		ops.shutdownRuntime = [] {};
		ops.sleep = [](std::chrono::milliseconds) {};
		ShutdownHook::setOperationsForTests(ops);
		GameServer::initShutdown(commons::utils::ExitCode::NORMAL, 30);
	}

	~ShutdownSoonScope() {
		release.store(true);
		static_cast<void>(ShutdownHook::getInstance().awaitCompletion(std::chrono::seconds(10)));
		ShutdownHook::resetForTests();
	}

	ShutdownSoonScope(const ShutdownSoonScope&) = delete;
	ShutdownSoonScope& operator=(const ShutdownSoonScope&) = delete;

private:
	std::atomic<bool> release{false};
};

/** The account warehouse owned by the player for the scope (PlayerEnterWorldService: the entering player owns it), unowned again afterwards */
class AccountWarehouseOwnerScope {
public:
	AccountWarehouseOwnerScope(model::items::storage::Storage& accountWarehouse, model::gameobjects::player::Player& player)
		: warehouse(accountWarehouse) {
		warehouse.setOwner(Ptr<model::gameobjects::player::Player>(player));
	}

	~AccountWarehouseOwnerScope() { warehouse.setOwner(nullptr); } // the account warehouse would otherwise keep the player (and so its account) alive

	AccountWarehouseOwnerScope(const AccountWarehouseOwnerScope&) = delete;
	AccountWarehouseOwnerScope& operator=(const AccountWarehouseOwnerScope&) = delete;

private:
	model::items::storage::Storage& warehouse;
};

class ItemMoveSplitServiceTest : public ItemServicesTest {
protected:
	/** The items of a storage with this template id that are not among `known` */
	std::vector<Ptr<Item>> newItemsOf(StorageType type, int32_t itemId, const std::vector<int32_t>& known = {}) {
		std::vector<Ptr<Item>> created;
		for (const Ptr<Item>& item : storage(type).getItemsByItemId(itemId)) {
			if (std::find(known.begin(), known.end(), item->getObjectId()) == known.end())
				created.push_back(item);
		}
		return created;
	}

	/** SM_WAREHOUSE_ADD_ITEM: writeC(warehouseType id), writeH(addType mask), writeH(1), writeD(objectId) */
	void expectWarehouseAdd(const std::vector<uint8_t>& packet, Item& item, int32_t warehouseType, int32_t addTypeMask) {
		ASSERT_EQ(javaOpcodeOf(packet), SM_WAREHOUSE_ADD_ITEM_OPCODE);
		PacketReader reader(cp::bodyOf(packet));
		EXPECT_EQ(reader.C(), warehouseType);
		EXPECT_EQ(reader.H(), addTypeMask);
		EXPECT_EQ(reader.H(), 1);
		EXPECT_EQ(reader.D(), item.getObjectId());
	}
};

// ------------------------------------------------------------------------------------------------------------------------- moveItem

TEST_F(ItemMoveSplitServiceTest, AMoveInsideTheSameStorageOnlyChangesTheSlotAndSendsNothing) {
	// ItemMoveService.java:41-45 -> moveInSameStorage (:80-84): the storage and the item become UPDATE_REQUIRED, the item takes the slot; no
	// packet at all (m5b3-plan.md Y9); the same slot again changes nothing
	Item& potions = stored(810701, MINOR_LIFE_POTION, 10, StorageType::CUBE, 3);
	const PersistentState loaded = potions.getPersistentState(); // a DAO item: Java null, NOACTION in C++ (Item.cpp)
	ASSERT_NE(loaded, PersistentState::UPDATE_REQUIRED);
	ASSERT_EQ(player().getInventory().getPersistentState(), PersistentState::UPDATED);

	ItemMoveService::moveItem(player(), 810701, CUBE_ID, CUBE_ID, 3);
	EXPECT_EQ(potions.getPersistentState(), loaded) << "the same slot: moveInSameStorage is not called";
	EXPECT_EQ(player().getInventory().getPersistentState(), PersistentState::UPDATED);

	ItemMoveService::moveItem(player(), 810701, CUBE_ID, CUBE_ID, 7);
	EXPECT_EQ(potions.getEquipmentSlot(), 7);
	EXPECT_EQ(potions.getPersistentState(), PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(player().getInventory().getPersistentState(), PersistentState::UPDATE_REQUIRED);
	EXPECT_TRUE(player().getInventory().getItemByObjId(810701));
	EXPECT_TRUE(sent().empty());

	// an unknown item or storage: nothing (:26-39)
	ItemMoveService::moveItem(player(), 999999, CUBE_ID, REGULAR_WAREHOUSE_ID, 0);
	ItemMoveService::moveItem(player(), 810701, 99, REGULAR_WAREHOUSE_ID, 0);
	ItemMoveService::moveItem(player(), 810701, CUBE_ID, 99, 0);
	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(player().getInventory().getItemByObjId(810701));
}

TEST_F(ItemMoveSplitServiceTest, AMoveToTheWarehouseDeletesFromTheCubeThenAddsToTheWarehouse) {
	// ItemMoveService.java:74-77: remove, SM_DELETE_ITEM(MOVE 0x14) + the cube size, the slot, then Storage.add with the default ITEM_COLLECT:
	// SM_WAREHOUSE_ADD_ITEM(warehouse id 1) + the warehouse size (m5b3-plan.md Y9's junk case). The junk's mask 12414 has STORABLE_IN_WH
	// (item_templates.xml:874138, ItemMask.java: 1 << 3)
	Item& junk = stored(810702, SPARKIE_CARAPACE_FRAGMENT, 7, StorageType::CUBE, 2);
	ItemMoveService::moveItem(player(), 810702, CUBE_ID, REGULAR_WAREHOUSE_ID, 5);
	EXPECT_FALSE(player().getInventory().getItemByObjId(810702));
	EXPECT_TRUE(player().getWarehouse().getItemByObjId(810702));
	EXPECT_EQ(junk.getItemLocation(), 1);
	EXPECT_EQ(junk.getEquipmentSlot(), 5);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 4u);
	EXPECT_EQ(packets[0], deleteItem(810702, 0x14));
	EXPECT_EQ(packets[1], cubeSize(StorageType::CUBE, 0));
	expectWarehouseAdd(packets[2], junk, 1, 0x19);
	EXPECT_EQ(packets[2], serialized(SM_WAREHOUSE_ADD_ITEM(junk, 1, player(), ItemAddType::ITEM_COLLECT)));
	EXPECT_EQ(packets[3], cubeSize(StorageType::REGULAR_WAREHOUSE, 1));
}

TEST_F(ItemMoveSplitServiceTest, AnItemTheWarehouseRefusesIsUnlockedAndStays) {
	// ItemMoveService.java:46-54: isItemRestrictedTo(REGULAR_WAREHOUSE) sends STR_WAREHOUSE_CANT_DEPOSIT_ITEM (ItemRestrictionService.java:
	// 36-44) for the event potion (mask 12352 has no STORABLE_IN_WH, item_templates.xml:835126); sendItemUnlockPacket: SM_INVENTORY_ADD_ITEM
	// (ALL_SLOT 0x13) + the cube size, no delete (m5b3-plan.md Y9)
	Item& eventPotion = stored(810703, EVENT_ACCELEROX, 3);
	ItemMoveService::moveItem(player(), 810703, CUBE_ID, REGULAR_WAREHOUSE_ID, 0);
	EXPECT_TRUE(player().getInventory().getItemByObjId(810703));
	EXPECT_EQ(player().getWarehouse().size(), 0);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_WAREHOUSE_CANT_DEPOSIT_ITEM()),
						  serialized(SM_INVENTORY_ADD_ITEM({Ptr<Item>(eventPotion)}, player(), ItemAddType::ALL_SLOT)), cubeSize(StorageType::CUBE, 1)}));
}

TEST_F(ItemMoveSplitServiceTest, SlotMinusOneMergesIntoTheTargetStacksFirst) {
	// ItemMoveService.java:59-68 -> ItemSplitService.mergeStacks (:103-113): the target stack takes min(count, free) with INC_ITEM_COLLECT
	// (different storage types), the source gives it with DEC_ITEM_SPLIT_MOVE; the rest moves like any item
	Item& source = stored(810704, MINOR_LIFE_POTION, 10, StorageType::CUBE, 1);
	Item& target = stored(810705, MINOR_LIFE_POTION, 995, StorageType::REGULAR_WAREHOUSE, 0);
	ItemMoveService::moveItem(player(), 810704, CUBE_ID, REGULAR_WAREHOUSE_ID, -1);
	EXPECT_EQ(target.getItemCount(), 1000) << "max_stack_count 1000 (item_templates.xml:830724): 5 were free";
	EXPECT_EQ(source.getItemCount(), 5);
	EXPECT_TRUE(player().getWarehouse().getItemByObjId(810704)) << "the rest moved";
	EXPECT_EQ(source.getEquipmentSlot(), -1);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 6u);
	EXPECT_EQ(javaOpcodeOf(packets[0]), SM_WAREHOUSE_UPDATE_ITEM_OPCODE);
	EXPECT_EQ(packets[0], serialized(SM_WAREHOUSE_UPDATE_ITEM(player(), target, 1, ItemUpdateType::INC_ITEM_COLLECT)));
	EXPECT_EQ(javaOpcodeOf(packets[1]), SM_INVENTORY_UPDATE_ITEM_OPCODE);
	EXPECT_EQ(trailingMask(packets[1]), 0x0A) << "DEC_ITEM_SPLIT_MOVE";
	EXPECT_EQ(packets[1], serialized(SM_INVENTORY_UPDATE_ITEM(player(), source, ItemUpdateType::DEC_ITEM_SPLIT_MOVE)));
	EXPECT_EQ(packets[2], deleteItem(810704, 0x14));
	EXPECT_EQ(packets[3], cubeSize(StorageType::CUBE, 0));
	expectWarehouseAdd(packets[4], source, 1, 0x19);
	EXPECT_EQ(packets[5], cubeSize(StorageType::REGULAR_WAREHOUSE, 2));
}

TEST_F(ItemMoveSplitServiceTest, AStackThatMergesCompletelyIsDeletedAndNotMovedAgain) {
	// ItemMoveService.java:63-65: `if (item.getItemCount() == 0) return;` after the merge emptied the source: Storage.decreaseItemCount deleted
	// it with ItemDeleteType.fromUpdateType(DEC_ITEM_SPLIT_MOVE) = MOVE
	Item& source = stored(810706, MINOR_LIFE_POTION, 3, StorageType::CUBE, 1);
	Item& target = stored(810707, MINOR_LIFE_POTION, 995, StorageType::REGULAR_WAREHOUSE, 0);
	ItemMoveService::moveItem(player(), 810706, CUBE_ID, REGULAR_WAREHOUSE_ID, -1);
	EXPECT_EQ(target.getItemCount(), 998);
	EXPECT_EQ(source.getItemCount(), 0);
	EXPECT_EQ(source.getPersistentState(), PersistentState::DELETED);
	EXPECT_EQ(player().getWarehouse().size(), 1);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_WAREHOUSE_UPDATE_ITEM(player(), target, 1, ItemUpdateType::INC_ITEM_COLLECT)),
						  deleteItem(810706, 0x14), cubeSize(StorageType::CUBE, 0)}));
}

TEST_F(ItemMoveSplitServiceTest, AFullTargetStorageSendsItsFullMessageAndUnlocksTheItem) {
	// ItemMoveService.java:69-73: IStorage.getStorageIsFullMessage (IStorage.java:109-112: REGULAR_WAREHOUSE -> STR_WAREHOUSE_DEPOSIT_FULL_BASKET),
	// then sendItemUnlockPacket; the regular warehouse holds 24 (StorageType.java)
	for (int32_t i = 0; i < 24; i++)
		stored(810720 + i, SPARKIE_CARAPACE_FRAGMENT, 1, StorageType::REGULAR_WAREHOUSE, i);
	Item& potions = stored(810708, MINOR_LIFE_POTION, 5);
	ItemMoveService::moveItem(player(), 810708, CUBE_ID, REGULAR_WAREHOUSE_ID, 3);
	EXPECT_TRUE(player().getInventory().getItemByObjId(810708));
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_WAREHOUSE_DEPOSIT_FULL_BASKET()),
						  serialized(SM_INVENTORY_ADD_ITEM({Ptr<Item>(potions)}, player(), ItemAddType::ALL_SLOT)), cubeSize(StorageType::CUBE, 1)}));
}

TEST_F(ItemMoveSplitServiceTest, ANonStackableItemMovedWithSlotMinusOneIsNeverMerged) {
	// ItemMoveService.java:59-60: the merge loop runs only for `item.getItemTemplate().isStackable()`. The Training Sword has no max_stack_count
	// (item_templates.xml:375: ItemTemplate's default 1, not stackable); a sword of the same id in the warehouse is not merged into (a merge would
	// send the target's SM_WAREHOUSE_UPDATE_ITEM and the source's SM_INVENTORY_UPDATE_ITEM for a count of 0), the sword moves like any item
	Item& sword = stored(810740, TRAINING_SWORD, 1, StorageType::CUBE, 2);
	stored(810741, TRAINING_SWORD, 1, StorageType::REGULAR_WAREHOUSE, 0);
	ItemMoveService::moveItem(player(), 810740, CUBE_ID, REGULAR_WAREHOUSE_ID, -1);
	EXPECT_TRUE(player().getWarehouse().getItemByObjId(810740));
	EXPECT_EQ(player().getWarehouse().size(), 2);
	EXPECT_EQ(sword.getEquipmentSlot(), -1);
	EXPECT_EQ(sent(), cp::exactly({deleteItem(810740, 0x14), cubeSize(StorageType::CUBE, 0),
						  serialized(SM_WAREHOUSE_ADD_ITEM(sword, 1, player(), ItemAddType::ITEM_COLLECT)), cubeSize(StorageType::REGULAR_WAREHOUSE, 2)}));
}

// ------------------------------------------------------------------------------------------------------------------------- switchItemsInStorages

TEST_F(ItemMoveSplitServiceTest, ASwapDeletesBothItemsBeforeAddingThemWithEachOthersSlot) {
	// ItemMoveService.java:112-125: the slots are exchanged, both removed, both SM_DELETE_*(MOVE) sent, then each added to the other storage
	// ("correct UI update order is 1)delete items 2) add items", m5b3-plan.md Y16)
	Item& potions = stored(810801, MINOR_LIFE_POTION, 10, StorageType::CUBE, 4);
	Item& junk = stored(810802, SPARKIE_CARAPACE_FRAGMENT, 2, StorageType::REGULAR_WAREHOUSE, 9);
	ItemMoveService::switchItemsInStorages(player(), CUBE_ID, 810801, REGULAR_WAREHOUSE_ID, 810802);
	EXPECT_TRUE(player().getWarehouse().getItemByObjId(810801));
	EXPECT_TRUE(player().getInventory().getItemByObjId(810802));
	EXPECT_EQ(potions.getEquipmentSlot(), 9);
	EXPECT_EQ(junk.getEquipmentSlot(), 4);
	EXPECT_EQ(potions.getItemLocation(), 1);
	EXPECT_EQ(junk.getItemLocation(), 0);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 8u);
	EXPECT_EQ(packets[0], deleteItem(810801, 0x14));
	EXPECT_EQ(packets[1], cubeSize(StorageType::CUBE, 0));
	EXPECT_EQ(packets[2], deleteWarehouseItem(1, 810802, 0x14));
	EXPECT_EQ(packets[3], cubeSize(StorageType::REGULAR_WAREHOUSE, 0));
	EXPECT_EQ(packets[4], serialized(SM_INVENTORY_ADD_ITEM({Ptr<Item>(junk)}, player(), ItemAddType::ITEM_COLLECT)));
	EXPECT_EQ(packets[5], cubeSize(StorageType::CUBE, 1));
	expectWarehouseAdd(packets[6], potions, 1, 0x19);
	EXPECT_EQ(packets[7], cubeSize(StorageType::REGULAR_WAREHOUSE, 1));
}

TEST_F(ItemMoveSplitServiceTest, ARefusedSwapUnlocksBothItems) {
	// ItemMoveService.java:99-110: the event potion may not go to the warehouse -> its message, then both unlock packets; nothing moves
	Item& eventPotion = stored(810803, EVENT_ACCELEROX, 1, StorageType::CUBE, 4);
	Item& junk = stored(810804, SPARKIE_CARAPACE_FRAGMENT, 2, StorageType::REGULAR_WAREHOUSE, 9);
	ItemMoveService::switchItemsInStorages(player(), CUBE_ID, 810803, REGULAR_WAREHOUSE_ID, 810804);
	EXPECT_TRUE(player().getInventory().getItemByObjId(810803));
	EXPECT_TRUE(player().getWarehouse().getItemByObjId(810804));
	EXPECT_EQ(eventPotion.getEquipmentSlot(), 4);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_WAREHOUSE_CANT_DEPOSIT_ITEM()),
						  serialized(SM_INVENTORY_ADD_ITEM({Ptr<Item>(eventPotion)}, player(), ItemAddType::ALL_SLOT)), cubeSize(StorageType::CUBE, 1),
						  serialized(SM_WAREHOUSE_ADD_ITEM(junk, 1, player(), ItemAddType::ALL_SLOT)), cubeSize(StorageType::REGULAR_WAREHOUSE, 1)}));

	// Java dereferences both storages unchecked (:87-90): an unknown storage type is a NullPointerException
	EXPECT_THROW(ItemMoveService::switchItemsInStorages(player(), 99, 810803, REGULAR_WAREHOUSE_ID, 810804), runtime::NullPointerException);
}

TEST_F(ItemMoveSplitServiceTest, ASwapIsRefusedWhenTheReplaceItemMayNotGoWhereTheSourceWas) {
	// ItemMoveService.java:102: the fourth check, isItemRestrictedTo(replaceItem, sourceStorage). The source (junk, mask 12414, item_templates.xml:
	// 874138) comes from the warehouse, the replace item is the event potion in the cube (mask 12352 has no STORABLE_IN_WH, :835126): the first
	// three checks pass, the fourth sends STR_WAREHOUSE_CANT_DEPOSIT_ITEM; then the source's unlock (the warehouse) and the replace item's (the cube)
	Item& junk = stored(810805, SPARKIE_CARAPACE_FRAGMENT, 2, StorageType::REGULAR_WAREHOUSE, 9);
	Item& eventPotion = stored(810806, EVENT_ACCELEROX, 1, StorageType::CUBE, 4);
	ItemMoveService::switchItemsInStorages(player(), REGULAR_WAREHOUSE_ID, 810805, CUBE_ID, 810806);
	EXPECT_TRUE(player().getWarehouse().getItemByObjId(810805));
	EXPECT_TRUE(player().getInventory().getItemByObjId(810806));
	EXPECT_EQ(junk.getEquipmentSlot(), 9);
	EXPECT_EQ(eventPotion.getEquipmentSlot(), 4);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_WAREHOUSE_CANT_DEPOSIT_ITEM()),
						  serialized(SM_WAREHOUSE_ADD_ITEM(junk, 1, player(), ItemAddType::ALL_SLOT)), cubeSize(StorageType::REGULAR_WAREHOUSE, 1),
						  serialized(SM_INVENTORY_ADD_ITEM({Ptr<Item>(eventPotion)}, player(), ItemAddType::ALL_SLOT)), cubeSize(StorageType::CUBE, 1)}));
}

TEST_F(ItemMoveSplitServiceTest, MovesAndSwapsAreRefusedInTheLastThirtySecondsOfAShutdown) {
	// ItemMoveService.java:46-54 and :99-110: GameServer.isShuttingDownSoon() (a scheduled shutdown with at most 30 s left) refuses the move and
	// the swap that nothing else refuses (the junk, mask 12414, and the potion, mask 12414, may both go to the warehouse, item_templates.xml:874138,
	// :830724): the unlock packets, then STR_MSG_DISABLE("Shutdown Progress"); nothing moves
	Item& junk = stored(810810, SPARKIE_CARAPACE_FRAGMENT, 2, StorageType::CUBE, 1);
	Item& potions = stored(810811, MINOR_LIFE_POTION, 5, StorageType::REGULAR_WAREHOUSE, 0);
	{
		ShutdownSoonScope shutdown;
		ASSERT_TRUE(GameServer::isShuttingDownSoon());
		ItemMoveService::moveItem(player(), 810810, CUBE_ID, REGULAR_WAREHOUSE_ID, 3);
		EXPECT_TRUE(player().getInventory().getItemByObjId(810810));
		EXPECT_EQ(junk.getEquipmentSlot(), 1);
		EXPECT_EQ(sent(), cp::exactly({serialized(SM_INVENTORY_ADD_ITEM({Ptr<Item>(junk)}, player(), ItemAddType::ALL_SLOT)),
							  cubeSize(StorageType::CUBE, 1), serialized(SM_SYSTEM_MESSAGE::STR_MSG_DISABLE("Shutdown Progress"))}));

		clearSent();
		ItemMoveService::switchItemsInStorages(player(), CUBE_ID, 810810, REGULAR_WAREHOUSE_ID, 810811);
		EXPECT_TRUE(player().getInventory().getItemByObjId(810810));
		EXPECT_TRUE(player().getWarehouse().getItemByObjId(810811));
		EXPECT_EQ(junk.getEquipmentSlot(), 1);
		EXPECT_EQ(potions.getEquipmentSlot(), 0);
		EXPECT_EQ(sent(), cp::exactly({serialized(SM_INVENTORY_ADD_ITEM({Ptr<Item>(junk)}, player(), ItemAddType::ALL_SLOT)),
							  cubeSize(StorageType::CUBE, 1), serialized(SM_WAREHOUSE_ADD_ITEM(potions, 1, player(), ItemAddType::ALL_SLOT)),
							  cubeSize(StorageType::REGULAR_WAREHOUSE, 1), serialized(SM_SYSTEM_MESSAGE::STR_MSG_DISABLE("Shutdown Progress"))}));
	}
	EXPECT_FALSE(GameServer::isShuttingDownSoon()) << "the hook was reset";
}

// ------------------------------------------------------------------------------------------------------------------------- splitItem

TEST_F(ItemMoveSplitServiceTest, ASplitInsideTheCubeDecreasesTheStackThenAddsTheNewOneAtTheSlot) {
	// ItemSplitService.java:70-91: decreaseItemCount(DEC_ITEM_SPLIT) -> SM_INVENTORY_UPDATE_ITEM(0x06), the cube size, then the new stack
	// (ItemFactory.newItem(id, splitAmount), slotNum in the same storage) -> SM_INVENTORY_ADD_ITEM + the cube size (m5b3-plan.md Y10)
	Item& potions = stored(810901, MINOR_LIFE_POTION, 100, StorageType::CUBE, 0);
	ItemSplitService::splitItem(player(), 810901, 0, 40, 6, CUBE_ID, CUBE_ID);
	EXPECT_EQ(potions.getItemCount(), 60);
	std::vector<Ptr<Item>> created = newItemsOf(StorageType::CUBE, MINOR_LIFE_POTION, {810901});
	ASSERT_EQ(created.size(), 1u);
	EXPECT_EQ(created[0]->getItemCount(), 40);
	EXPECT_EQ(created[0]->getEquipmentSlot(), 6);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 4u);
	EXPECT_EQ(trailingMask(packets[0]), 0x06) << "DEC_ITEM_SPLIT";
	EXPECT_EQ(packets[0], serialized(SM_INVENTORY_UPDATE_ITEM(player(), potions, ItemUpdateType::DEC_ITEM_SPLIT)));
	EXPECT_EQ(packets[1], cubeSize(StorageType::CUBE, 1)) << "sent before the add: the new stack is not in the cube yet";
	EXPECT_EQ(packets[2], serialized(SM_INVENTORY_ADD_ITEM({created[0]}, player(), ItemAddType::ITEM_COLLECT)));
	EXPECT_EQ(packets[3], cubeSize(StorageType::CUBE, 2));
}

TEST_F(ItemMoveSplitServiceTest, ASplitThatWouldEmptyOrOverdrawTheStackDoesNothing) {
	// ItemSplitService.java:31-33 (splitAmount <= 0) and :75-78 (count < splitAmount || nothing left)
	Item& potions = stored(810902, MINOR_LIFE_POTION, 100);
	ItemSplitService::splitItem(player(), 810902, 0, 0, 1, CUBE_ID, CUBE_ID);
	ItemSplitService::splitItem(player(), 810902, 0, -5, 1, CUBE_ID, CUBE_ID);
	ItemSplitService::splitItem(player(), 810902, 0, 100, 1, CUBE_ID, CUBE_ID);
	ItemSplitService::splitItem(player(), 810902, 0, 101, 1, CUBE_ID, CUBE_ID);
	EXPECT_EQ(potions.getItemCount(), 100);
	EXPECT_EQ(player().getInventory().size(), 1);
	EXPECT_TRUE(sent().empty());
	// a missing source that is not the storage's kinah: CHECKPOINT warning, nothing (:49-55); an unknown storage: warning (:41-45)
	ItemSplitService::splitItem(player(), 999999, 0, 5, 1, CUBE_ID, CUBE_ID);
	ItemSplitService::splitItem(player(), 810902, 0, 5, 1, 99, CUBE_ID);
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(potions.getItemCount(), 100);
}

TEST_F(ItemMoveSplitServiceTest, ASplitIntoAFullStorageSendsItsFullMessage) {
	// ItemSplitService.java:71-74: CUBE -> STR_WAREHOUSE_FULL_INVENTORY (IStorage.java:111)
	Item& potions = stored(810903, MINOR_LIFE_POTION, 100);
	for (int32_t i = 0; i < 26; i++)
		stored(810910 + i, SPARKIE_CARAPACE_FRAGMENT, 1);
	ItemSplitService::splitItem(player(), 810903, 0, 10, 1, CUBE_ID, CUBE_ID);
	EXPECT_EQ(potions.getItemCount(), 100);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_WAREHOUSE_FULL_INVENTORY())}));
}

TEST_F(ItemMoveSplitServiceTest, ASplitIntoTheWarehouseWithoutALegionMovesPartOfTheStack) {
	// ItemSplitService.java:79-91 across storages: LegionService.addWHItemHistory (:80) is a no-op without a legion (LegionService.java:1082-1092,
	// m5b3-plan.md §2.8 E-9, T-08), DEC_ITEM_SPLIT_MOVE (0x0A), the new stack keeps no slot and goes to the warehouse
	Item& potions = stored(810904, MINOR_LIFE_POTION, 100);
	ASSERT_FALSE(player().getLegion());
	ItemSplitService::splitItem(player(), 810904, 0, 30, 3, CUBE_ID, REGULAR_WAREHOUSE_ID);
	EXPECT_EQ(potions.getItemCount(), 70);
	std::vector<Ptr<Item>> created = newItemsOf(StorageType::REGULAR_WAREHOUSE, MINOR_LIFE_POTION);
	ASSERT_EQ(created.size(), 1u);
	EXPECT_EQ(created[0]->getItemCount(), 30);
	EXPECT_EQ(created[0]->getEquipmentSlot(), FIRST_AVAILABLE_SLOT) << "slotNum is used only inside one storage (:83-84): the initial slot stays";
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 4u);
	EXPECT_EQ(trailingMask(packets[0]), 0x0A);
	EXPECT_EQ(packets[0], serialized(SM_INVENTORY_UPDATE_ITEM(player(), potions, ItemUpdateType::DEC_ITEM_SPLIT_MOVE)));
	EXPECT_EQ(packets[1], cubeSize(StorageType::CUBE, 1));
	expectWarehouseAdd(packets[2], *created[0], 1, 0x19);
	EXPECT_EQ(packets[3], cubeSize(StorageType::REGULAR_WAREHOUSE, 1));
}

TEST_F(ItemMoveSplitServiceTest, ASplitOntoAStackOfTheSameIdMergesUpToItsFreeCount) {
	// ItemSplitService.java:92-97 -> mergeStacks (:103-113) inside one storage: INC_ITEM_MERGE (0x01) for the target, DEC_ITEM_SPLIT (0x06) for
	// the source, the count capped at the target's free count
	Item& source = stored(810905, MINOR_LIFE_POTION, 100);
	Item& target = stored(810906, MINOR_LIFE_POTION, 998);
	ItemSplitService::splitItem(player(), 810905, 810906, 5, 0, CUBE_ID, CUBE_ID);
	EXPECT_EQ(target.getItemCount(), 1000);
	EXPECT_EQ(source.getItemCount(), 98);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 2u);
	EXPECT_EQ(trailingMask(packets[0]), 0x01);
	EXPECT_EQ(packets[0], serialized(SM_INVENTORY_UPDATE_ITEM(player(), target, ItemUpdateType::INC_ITEM_MERGE)));
	EXPECT_EQ(trailingMask(packets[1]), 0x06);
	EXPECT_EQ(packets[1], serialized(SM_INVENTORY_UPDATE_ITEM(player(), source, ItemUpdateType::DEC_ITEM_SPLIT)));

	// a target of another id: nothing (:92)
	clearSent();
	stored(810907, SPARKIE_CARAPACE_FRAGMENT, 1);
	ItemSplitService::splitItem(player(), 810905, 810907, 5, 0, CUBE_ID, CUBE_ID);
	EXPECT_EQ(source.getItemCount(), 98);
	EXPECT_TRUE(sent().empty());
}

TEST_F(ItemMoveSplitServiceTest, AMergeOfMoreThanTheSourceStackHoldsIsRefused) {
	// ItemSplitService.java:104: mergeStacks does nothing unless `sourceItem.getItemCount() >= count`. Item.decreaseItemCount caps the decrease at
	// the stack (Item.java:334-346), so without the check a split of 50 from a stack of 10 would give the target 50 and take only 10. Onto a stack
	// of the same id in the cube (:92-97) and in the warehouse (LegionService.addWHItemHistory is a no-op without a legion): nothing changes
	Item& source = stored(810930, MINOR_LIFE_POTION, 10, StorageType::CUBE, 1);
	Item& cubeTarget = stored(810931, MINOR_LIFE_POTION, 100, StorageType::CUBE, 2);
	ItemSplitService::splitItem(player(), 810930, 810931, 50, 0, CUBE_ID, CUBE_ID);
	EXPECT_EQ(source.getItemCount(), 10);
	EXPECT_NE(source.getPersistentState(), PersistentState::DELETED);
	EXPECT_EQ(cubeTarget.getItemCount(), 100);
	EXPECT_TRUE(player().getInventory().getItemByObjId(810930));
	EXPECT_TRUE(sent().empty());

	// a second source of its own, so the warehouse arm does not depend on the first
	Item& secondSource = stored(810933, MINOR_LIFE_POTION, 10, StorageType::CUBE, 3);
	Item& warehouseTarget = stored(810932, MINOR_LIFE_POTION, 100, StorageType::REGULAR_WAREHOUSE, 0);
	ItemSplitService::splitItem(player(), 810933, 810932, 50, 0, CUBE_ID, REGULAR_WAREHOUSE_ID);
	EXPECT_EQ(secondSource.getItemCount(), 10);
	EXPECT_EQ(warehouseTarget.getItemCount(), 100);
	EXPECT_TRUE(player().getInventory().getItemByObjId(810933));
	EXPECT_TRUE(sent().empty());
}

TEST_F(ItemMoveSplitServiceTest, ASplitIntoAStorageThatRefusesTheItemResendsTheSource) {
	// ItemSplitService.java:57-62: across storages, isItemRestrictedTo(REGULAR_WAREHOUSE) sends STR_WAREHOUSE_CANT_DEPOSIT_ITEM for the event
	// potion (mask 12352 has no STORABLE_IN_WH, item_templates.xml:835126), then sendStorageUpdatePacket(CUBE, item) with its default ITEM_COLLECT
	// (ItemPacketService.java:206-208): SM_INVENTORY_ADD_ITEM(0x19) + the cube size. No stack is made, the count stays
	Item& eventPotion = stored(810940, EVENT_ACCELEROX, 3);
	ItemSplitService::splitItem(player(), 810940, 0, 1, 0, CUBE_ID, REGULAR_WAREHOUSE_ID);
	EXPECT_EQ(eventPotion.getItemCount(), 3);
	EXPECT_EQ(player().getWarehouse().size(), 0);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_WAREHOUSE_CANT_DEPOSIT_ITEM()),
						  serialized(SM_INVENTORY_ADD_ITEM({Ptr<Item>(eventPotion)}, player(), ItemAddType::ITEM_COLLECT)), cubeSize(StorageType::CUBE, 1)}));
}

TEST_F(ItemMoveSplitServiceTest, AnUnknownItemIsNotTakenForTheCubesKinah) {
	// ItemSplitService.java:49-55: a missing source falls back to the storage's kinah item only when its object id is the one asked for; any
	// other id logs the CHECKPOINT warning and moves nothing, although the cube holds kinah
	Item& kinah = stored(810950, KINAH, 1000);
	model::items::storage::Storage& accountWarehouse = storage(StorageType::ACCOUNT_WAREHOUSE);
	AccountWarehouseOwnerScope owner(accountWarehouse, player());
	ItemSplitService::splitItem(player(), 999999, 0, 5, 0, CUBE_ID, ACCOUNT_WAREHOUSE_ID);
	EXPECT_EQ(kinah.getItemCount(), 1000);
	EXPECT_FALSE(accountWarehouse.getKinahItem());
	EXPECT_TRUE(sent().empty());
}

TEST_F(ItemMoveSplitServiceTest, KinahIsSplitBetweenTheCubeAndTheAccountWarehouse) {
	// ItemSplitService.java:64-68 -> moveKinah (:115-141) -> updateKinahCount (:143-146): the source loses the kinah with DEC_ITEM_SPLIT, the
	// other storage gains it with INC_KINAH_MERGE. The account warehouse has no kinah item yet: Storage.increaseKinah adds one first
	// (ItemFactory.newItem(KINAH, 0): SM_WAREHOUSE_ADD_ITEM(2) + its size, which is 0 for the account warehouse, SM_CUBE_UPDATE.java:29-52).
	Item& kinah = stored(810920, KINAH, 1000);
	model::items::storage::Storage& accountWarehouse = storage(StorageType::ACCOUNT_WAREHOUSE);
	AccountWarehouseOwnerScope owner(accountWarehouse, player());
	ItemSplitService::splitItem(player(), 810920, 0, 300, 0, CUBE_ID, ACCOUNT_WAREHOUSE_ID);
	EXPECT_EQ(kinah.getItemCount(), 700);
	ASSERT_TRUE(accountWarehouse.getKinahItem());
	Item& accountKinah = *accountWarehouse.getKinahItem();
	EXPECT_EQ(accountKinah.getItemCount(), 300);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 4u);
	EXPECT_EQ(packets[0], serialized(SM_INVENTORY_UPDATE_ITEM(player(), kinah, ItemUpdateType::DEC_ITEM_SPLIT)));
	expectWarehouseAdd(packets[1], accountKinah, 2, 0x19);
	EXPECT_EQ(packets[2], cubeSize(StorageType::ACCOUNT_WAREHOUSE, 0));
	EXPECT_EQ(trailingMask(packets[3]), 0x05) << "INC_KINAH_MERGE";
	EXPECT_EQ(packets[3], serialized(SM_WAREHOUSE_UPDATE_ITEM(player(), accountKinah, 2, ItemUpdateType::INC_KINAH_MERGE)));

	// and back: the ACCOUNT_WAREHOUSE arm moves to the cube
	clearSent();
	ItemSplitService::splitItem(player(), accountKinah.getObjectId(), 0, 100, 0, ACCOUNT_WAREHOUSE_ID, CUBE_ID);
	EXPECT_EQ(accountKinah.getItemCount(), 200);
	EXPECT_EQ(kinah.getItemCount(), 800);
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_WAREHOUSE_UPDATE_ITEM(player(), accountKinah, 2, ItemUpdateType::DEC_ITEM_SPLIT)),
						  serialized(SM_INVENTORY_UPDATE_ITEM(player(), kinah, ItemUpdateType::INC_KINAH_MERGE))}));

	// more than the source holds: nothing (:116-117)
	clearSent();
	ItemSplitService::splitItem(player(), 810920, 0, 801, 0, CUBE_ID, ACCOUNT_WAREHOUSE_ID);
	EXPECT_EQ(kinah.getItemCount(), 800);
	EXPECT_TRUE(sent().empty());
}

// ------------------------------------------------------------------------------------------------------------------------- ItemRestrictionService

TEST_F(ItemMoveSplitServiceTest, TheRestrictionTableOfEachStorage) {
	// ItemRestrictionService.java:21-67, the masks of item_templates.xml:874138 (12414), :835126 (12352) against ItemMask.java
	Item& junk = loose(811001, SPARKIE_CARAPACE_FRAGMENT, 1);
	Item& eventPotion = loose(811002, EVENT_ACCELEROX, 1);
	Item& boundJunk = loose(811003, SPARKIE_CARAPACE_FRAGMENT, 1);
	boundJunk.setSoulBound(true);

	EXPECT_FALSE(ItemRestrictionService::isItemRestrictedTo(player(), junk, StorageType::REGULAR_WAREHOUSE));
	EXPECT_FALSE(ItemRestrictionService::isItemRestrictedTo(player(), junk, StorageType::ACCOUNT_WAREHOUSE));
	EXPECT_FALSE(ItemRestrictionService::isItemRestrictedTo(player(), eventPotion, StorageType::CUBE)) << "no arm for the cube";
	EXPECT_FALSE(ItemRestrictionService::isItemRestrictedFrom(player(), junk, StorageType::REGULAR_WAREHOUSE));
	EXPECT_FALSE(ItemRestrictionService::isItemRestrictedFrom(player(), junk, StorageType::CUBE));
	EXPECT_TRUE(sent().empty());

	EXPECT_TRUE(ItemRestrictionService::isItemRestrictedTo(player(), eventPotion, StorageType::REGULAR_WAREHOUSE));
	EXPECT_TRUE(ItemRestrictionService::isItemRestrictedTo(player(), eventPotion, StorageType::ACCOUNT_WAREHOUSE));
	EXPECT_TRUE(ItemRestrictionService::isItemRestrictedTo(player(), boundJunk, StorageType::ACCOUNT_WAREHOUSE))
		<< "Item.isStorableInAccWarehouse is false for a soul-bound item";
	EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_WAREHOUSE_CANT_DEPOSIT_ITEM()),
						  serialized(SM_SYSTEM_MESSAGE::STR_MSG_WAREHOUSE_CANT_ACCOUNT_DEPOSIT()),
						  serialized(SM_SYSTEM_MESSAGE::STR_MSG_WAREHOUSE_CANT_ACCOUNT_DEPOSIT())}));

	// the legion warehouse: disabled (LegionConfig.LEGION_WAREHOUSE false) -> CANT_LEGION_DEPOSIT / NO_RIGHT; enabled but no legion -> NO_RIGHT
	{
		AtomicConfigScope<bool> disabled(configs::main::LegionConfig::LEGION_WAREHOUSE, false);
		clearSent();
		EXPECT_TRUE(ItemRestrictionService::isItemRestrictedTo(player(), junk, StorageType::LEGION_WAREHOUSE));
		EXPECT_TRUE(ItemRestrictionService::isItemRestrictedFrom(player(), junk, StorageType::LEGION_WAREHOUSE));
		EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_WAREHOUSE_CANT_LEGION_DEPOSIT()),
							  serialized(SM_SYSTEM_MESSAGE::STR_GUILD_WAREHOUSE_NO_RIGHT())}));
	}
	{
		AtomicConfigScope<bool> enabled(configs::main::LegionConfig::LEGION_WAREHOUSE, true);
		clearSent();
		EXPECT_TRUE(ItemRestrictionService::isItemRestrictedTo(player(), eventPotion, StorageType::LEGION_WAREHOUSE)) << "no STORABLE_IN_LWH";
		EXPECT_TRUE(ItemRestrictionService::isItemRestrictedTo(player(), junk, StorageType::LEGION_WAREHOUSE)) << "not a legion member";
		EXPECT_TRUE(ItemRestrictionService::isItemRestrictedFrom(player(), junk, StorageType::LEGION_WAREHOUSE));
		EXPECT_EQ(sent(), cp::exactly({serialized(SM_SYSTEM_MESSAGE::STR_MSG_WAREHOUSE_CANT_LEGION_DEPOSIT()),
							  serialized(SM_SYSTEM_MESSAGE::STR_GUILD_WAREHOUSE_NO_RIGHT()), serialized(SM_SYSTEM_MESSAGE::STR_GUILD_WAREHOUSE_NO_RIGHT())}));
	}

	// :70-79: every item can be removed
	EXPECT_TRUE(ItemRestrictionService::canRemoveItem(player(), junk));
	EXPECT_TRUE(ItemRestrictionService::canRemoveItem(player(), eventPotion));
}

} // namespace
} // namespace aion::gameserver::services::item::test
