// P4-13 storage operations without an acting player (an account warehouse before a character enters): ItemStorage slots and capacities,
// Storage add/put/remove/delete, kinah, merge leftovers, the full-storage messages of every StorageType, PersistentState transitions of the
// storage and its items, and the lifetimes of stored and deleted items (LeakCensus). Expectations are derived by hand from Storage.java,
// ItemStorage.java, PlayerStorage.java, IStorage.java and Item.java. The Java code sends packets to the actor wherever it has one; without an
// actor it dereferences null in the same places, which the tests assert as NullPointerException.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ItemsTestSupport.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/items/storage/ItemStorage.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemDeleteType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::model::items::storage {
namespace {

using gameobjects::Item;
using runtime::Ptr;
using runtime::Ref;
using PersistentState = gameobjects::Persistable::PersistentState;

#define TEST_SCOPE runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST))

class StorageOperationsTest : public testing::Test {
protected:
	void SetUp() override {
		potion = test::registerItem(R"(id="160000001" max_stack_count="100")");
		otherPotion = test::registerItem(R"(id="160000002" max_stack_count="100")");
		kinah = test::registerItem(R"(id="182400001" max_stack_count="1000000000")");
		cubeItem = test::registerItem(R"(id="170000001")", R"(<inventory id="1"/>)");
	}

	void TearDown() override { runtime::Reclaimer::getInstance().drain(); }

	/** Java AccountService: new PlayerStorage(null, StorageType.ACCOUNT_WAREHOUSE) of an account without an entering character */
	static Ref<account::Account> accountWithWarehouse(int32_t id) {
		Ref<account::Account> account = account::Account::create(id);
		account->setAccountWarehouse(std::make_unique<PlayerStorage>(*account, StorageType::ACCOUNT_WAREHOUSE));
		return account;
	}

	test::StaticDataScope staticData;
	const templates::item::ItemTemplate* potion = nullptr;
	const templates::item::ItemTemplate* otherPotion = nullptr;
	const templates::item::ItemTemplate* kinah = nullptr;
	const templates::item::ItemTemplate* cubeItem = nullptr;
};

TEST_F(StorageOperationsTest, ItemStorageCountsCubeAndSpecialCubeItemsSeparately) {
	TEST_SCOPE;
	Ref<ItemStorage> cube = ItemStorage::create(StorageType::CUBE);
	EXPECT_EQ(cube->getRowLength(), 9);
	std::vector<Ref<Item>> items;
	for (int32_t i = 0; i < 27; i++) {
		items.push_back(Item::create(1000 + i, potion, 1, false, i));
		EXPECT_TRUE(cube->putItem(*items.back()));
	}
	EXPECT_TRUE(cube->isFull());
	EXPECT_EQ(cube->getFreeSlots(), 0);
	Ref<Item> overflow = Item::create(1100, otherPotion, 1, false, 30);
	EXPECT_TRUE(cube->putItem(*overflow)) << "putItem does not check the limit";
	EXPECT_EQ(cube->getFreeSlots(), -1);
	EXPECT_FALSE(cube->putItem(*overflow)) << "putIfAbsent: an object id is stored once";

	Ref<Item> special = Item::create(1200, cubeItem, 1, false, 5);
	EXPECT_TRUE(cube->putItem(*special));
	EXPECT_EQ(cube->getCubeItems().size(), 28u) << "extra inventory items are no cube items";
	EXPECT_EQ(cube->getSpecialCubeItems().size(), 1u);
	EXPECT_EQ(cube->getSpecialCubeFreeSlots(), 101) << "CUBE special limit 102";
	EXPECT_FALSE(cube->isFullSpecialCube());
	EXPECT_EQ(cube->size(), 29);

	// slot lookups: getItemBySlotId searches the cube items, getSpecialItemBySlotId the special ones
	EXPECT_EQ(cube->getItemBySlotId(3).get(), items[3].get());
	EXPECT_EQ(cube->getItemBySlotId(5).get(), items[5].get()) << "the special item in slot 5 is not a cube item";
	EXPECT_EQ(cube->getSpecialItemBySlotId(5).get(), special.get());
	EXPECT_FALSE(cube->getSpecialItemBySlotId(3));
	EXPECT_EQ(cube->getSlotIdByObjId(1100), 30);
	EXPECT_EQ(cube->getSlotIdByObjId(4242), -1);
	EXPECT_EQ(cube->getSlotIdByItemId(160000002), 30);
	EXPECT_EQ(cube->getSlotIdByItemId(1), -1);
	EXPECT_EQ(cube->getItemsById(160000001).size(), 27u);
	EXPECT_EQ(cube->getFirstItemById(160000002).get(), overflow.get());
	EXPECT_FALSE(cube->getFirstItemById(1));

	cube->setLimit(40);
	EXPECT_FALSE(cube->isFull());
	EXPECT_EQ(cube->getFreeSlots(), 12);
	EXPECT_EQ(cube->removeItem(1100).get(), overflow.get());
	EXPECT_FALSE(cube->removeItem(1100)) << "removed once";
	EXPECT_EQ(cube->getItems().size(), 28u);

	Ref<ItemStorage> broker = ItemStorage::create(StorageType::BROKER);
	EXPECT_TRUE(broker->isFull()) << "limit 0";
	EXPECT_TRUE(broker->isFullSpecialCube()) << "special limit 0";
}

TEST_F(StorageOperationsTest, AddPutsItemsAndKinahWithoutAnActor) {
	TEST_SCOPE;
	Ref<account::Account> account = accountWithWarehouse(1);
	Storage& warehouse = account->getAccountWarehouse();
	EXPECT_EQ(warehouse.getStorageType(), StorageType::ACCOUNT_WAREHOUSE);
	EXPECT_EQ(warehouse.getPersistentState(), PersistentState::UPDATED) << "Java field initializer";
	EXPECT_EQ(warehouse.getLimit(), 16);
	EXPECT_EQ(warehouse.getRowLength(), 8);
	EXPECT_EQ(warehouse.getKinah(), 0) << "no kinah item yet";
	EXPECT_FALSE(warehouse.getKinahItem());

	Ref<Item> stack = Item::create(2001, potion, 40, false, 0);
	Ptr<Item> added = warehouse.add(*stack);
	EXPECT_EQ(added.get(), stack.get());
	EXPECT_EQ(stack->getItemLocation(), getId(StorageType::ACCOUNT_WAREHOUSE));
	EXPECT_EQ(stack->getPersistentState(), PersistentState::NEW) << "UPDATE_REQUIRED keeps a NEW item NEW";
	EXPECT_EQ(warehouse.getPersistentState(), PersistentState::UPDATE_REQUIRED);
	EXPECT_FALSE(warehouse.add(*stack)) << "the object id is already stored";
	EXPECT_EQ(warehouse.size(), 1);
	EXPECT_EQ(warehouse.getFreeSlots(), 15);

	Ref<Item> secondStack = Item::create(2002, potion, 30, false, 1);
	EXPECT_TRUE(warehouse.add_CharacterTransfer(*secondStack));
	EXPECT_EQ(warehouse.getItemCountByItemId(160000001), 70) << "all stacks of the item id";
	EXPECT_EQ(warehouse.getItemsByItemId(160000001).size(), 2u);
	EXPECT_EQ(warehouse.getItemCountByItemId(160000002), 0);
	EXPECT_EQ(warehouse.getItemByObjId(2002).get(), secondStack.get());

	Ref<Item> money = Item::create(2003, kinah, 500, false, 0);
	EXPECT_EQ(warehouse.add(*money).get(), money.get());
	EXPECT_EQ(warehouse.getKinahItem().get(), money.get());
	EXPECT_EQ(warehouse.getKinah(), 500);
	EXPECT_EQ(warehouse.size(), 2) << "kinah is kept outside the item storage";
	EXPECT_EQ(warehouse.getItems().size(), 2u);
	EXPECT_EQ(warehouse.getItemsWithKinah().size(), 3u);
	EXPECT_FALSE(warehouse.getItemByObjId(2003));

	// increaseKinah with a kinah item: Item.increaseItemCount, no packet without an actor
	warehouse.increaseKinah(250);
	EXPECT_EQ(warehouse.getKinah(), 750);
	EXPECT_FALSE(warehouse.tryDecreaseKinah(751)) << "not enough kinah: no change";
	EXPECT_EQ(warehouse.getKinah(), 750);
	warehouse.decreaseKinah(0);
	EXPECT_EQ(warehouse.getKinah(), 750) << "amount 0 does nothing";

	// Java onLoadHandler (loading from the database): no location or state change
	Ref<Item> loaded = Item::create(2004, otherPotion, 1, false, 2);
	warehouse.setPersistentState(PersistentState::UPDATED);
	warehouse.onLoadHandler(*loaded);
	EXPECT_EQ(warehouse.getItemByObjId(2004).get(), loaded.get());
	EXPECT_EQ(loaded->getItemLocation(), 0);
	EXPECT_EQ(warehouse.getPersistentState(), PersistentState::UPDATED);

	// remove: out of the storage without any state change
	EXPECT_EQ(warehouse.remove(*loaded).get(), loaded.get());
	EXPECT_FALSE(warehouse.remove(*loaded));
	EXPECT_EQ(loaded->getPersistentState(), PersistentState::NEW);
	EXPECT_EQ(warehouse.size(), 2);
}

TEST_F(StorageOperationsTest, MergeReturnsTheCountAboveTheStackLimit) {
	TEST_SCOPE;
	Ref<account::Account> account = accountWithWarehouse(2);
	Storage& warehouse = account->getAccountWarehouse();
	Ref<Item> stack = Item::create(3001, potion, 90, false, 0);
	warehouse.onLoadHandler(*stack);
	stack->setPersistentState(PersistentState::UPDATED);
	EXPECT_EQ(warehouse.increaseItemCount(*stack, 25), 15) << "max_stack_count 100: 10 merged, 15 left";
	EXPECT_EQ(stack->getItemCount(), 100);
	EXPECT_EQ(stack->getPersistentState(), PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(warehouse.increaseItemCount(*stack, 5, services::item::ItemPacketService_ItemUpdateType::INC_ITEM_MERGE), 5) << "full stack";
	EXPECT_EQ(warehouse.increaseItemCount(*stack, 0), 0) << "Item.increaseItemCount ignores counts <= 0";
	EXPECT_EQ(warehouse.getPersistentState(), PersistentState::UPDATE_REQUIRED);
}

TEST_F(StorageOperationsTest, DecreasingWithoutAnActorFollowsJavasNullDereference) {
	TEST_SCOPE;
	Ref<account::Account> account = accountWithWarehouse(3);
	Storage& warehouse = account->getAccountWarehouse();
	Ref<Item> stack = Item::create(4001, potion, 10, false, 0);
	warehouse.onLoadHandler(*stack);
	stack->setPersistentState(PersistentState::UPDATED);

	// checks before any change
	EXPECT_FALSE(warehouse.decreaseByObjectId(4242, 1)) << "unknown object id";
	EXPECT_FALSE(warehouse.decreaseByObjectId(4001, 11)) << "more than the stack holds";
	EXPECT_FALSE(warehouse.decreaseByItemId(160000002, 1)) << "no stack of the item id";
	EXPECT_EQ(stack->getItemCount(), 10);
	EXPECT_EQ(warehouse.getPersistentState(), PersistentState::UPDATED);
	EXPECT_EQ(warehouse.decreaseItemCount(nullptr, 5, nullptr), 0) << "a missing item (decreaseKinah without kinah) decreases nothing";
	warehouse.decreaseKinah(100);

	// split: the count changes, then ItemPacketService.sendItemPacket(null, ...) dereferences the missing actor before the storage state
	EXPECT_THROW(static_cast<void>(warehouse.decreaseByObjectId(4001, 4)), runtime::NullPointerException);
	EXPECT_EQ(stack->getItemCount(), 6);
	EXPECT_EQ(stack->getPersistentState(), PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(warehouse.getPersistentState(), PersistentState::UPDATED);

	// the last pieces: delete(item, USE) removes the item, marks it DELETED and queues it, then the delete packet needs the actor
	EXPECT_THROW(static_cast<void>(warehouse.decreaseByItemId(160000001, 6, questEngine::model::QuestStatus::START)), runtime::NullPointerException);
	EXPECT_EQ(stack->getItemCount(), 0);
	EXPECT_EQ(stack->getPersistentState(), PersistentState::DELETED);
	EXPECT_FALSE(warehouse.getItemByObjId(4001));
	EXPECT_EQ(warehouse.getDeletedItems().size(), 1);
	EXPECT_EQ(warehouse.getPersistentState(), PersistentState::UPDATE_REQUIRED);

	// delete of a NEW item: NOACTION instead of DELETED (nothing to delete in the database)
	Ref<Item> fresh = Item::create(4002, otherPotion, 1, false, 1);
	warehouse.onLoadHandler(*fresh);
	EXPECT_THROW(static_cast<void>(warehouse.delete_(*fresh, services::item::ItemPacketService_ItemDeleteType::DISCARD)), runtime::NullPointerException);
	EXPECT_EQ(fresh->getPersistentState(), PersistentState::NOACTION);
	EXPECT_EQ(warehouse.getDeletedItems().size(), 2);
	EXPECT_FALSE(warehouse.delete_(*fresh)) << "an item that is not stored is not deleted (no actor needed)";

	// kinah is never deleted by a decrease: the count reaches 0 and the kinah item stays
	Ref<Item> money = Item::create(4003, kinah, 100, false, 0);
	warehouse.onLoadHandler(*money);
	EXPECT_THROW(static_cast<void>(warehouse.tryDecreaseKinah(100)), runtime::NullPointerException) << "the kinah update packet needs the actor";
	EXPECT_EQ(warehouse.getKinah(), 0);
	EXPECT_EQ(warehouse.getKinahItem().get(), money.get());
	EXPECT_EQ(warehouse.getDeletedItems().size(), 2);
}

TEST_F(StorageOperationsTest, FullStorageMessagesOfEveryStorageType) {
	TEST_SCOPE;
	Ref<account::Account> account = account::Account::create(4);
	// IStorage.getStorageIsFullMessage: message ids of the SM_SYSTEM_MESSAGE factories in SM_SYSTEM_MESSAGE.java
	const auto expectedId = [](StorageType type) {
		int32_t id = getId(type);
		if (type == StorageType::CUBE)
			return 1390149; // STR_WAREHOUSE_FULL_INVENTORY
		if (id >= 1 && id <= 3)
			return 1300421; // STR_WAREHOUSE_DEPOSIT_FULL_BASKET
		if (id >= PET_BAG_MIN && id <= PET_BAG_MAX)
			return 1400638; // STR_WAREHOUSE_TOO_MANY_ITEMS_TOYPET_WAREHOUSE
		if (id >= HOUSE_WH_MIN && id <= HOUSE_WH_MAX)
			return 1401239; // STR_HOUSING_WAREHOUSE_TOO_MANY_ITEMS_WAREHOUSE
		if (type == StorageType::BROKER)
			return 1300649; // STR_VENDOR_FULL_ITEM
		return 1300499;     // MAILBOX: STR_MAIL_SEND_FULL_BASKET
	};
	std::set<int32_t> ids;
	for (size_t ordinal = 0; ordinal <= static_cast<size_t>(StorageType::MAILBOX); ordinal++) {
		StorageType type = static_cast<StorageType>(ordinal);
		auto storage = std::make_unique<PlayerStorage>(*account, type);
		network::aion::serverpackets::SM_SYSTEM_MESSAGE message = storage->getStorageIsFullMessage();
		EXPECT_EQ(message.getId(), expectedId(type)) << "storage type ordinal " << ordinal;
		ids.insert(message.getId());
	}
	EXPECT_EQ(ids.size(), 6u);
}

TEST_F(StorageOperationsTest, StoredAndDeletedItemsLiveAsLongAsTheirStorage) {
	utils::ThreadPoolManager::installBackend(nullptr);
	runtime::ManualClock clock{0};
	utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 11));
	runtime::LeakCensus::getInstance().install();
	Ref<account::Account> account;
	{
		TEST_SCOPE;
		account = accountWithWarehouse(5);
		Storage& warehouse = account->getAccountWarehouse();
		for (int32_t objectId : {5001, 5002, 5003}) {
			Ref<Item> item = Item::create(objectId, objectId == 5003 ? kinah : potion, 1, false, 0);
			warehouse.onLoadHandler(*item);
			runtime::LeakCensus::getInstance().onRemovedFromWorld(*item, "Item", objectId);
		}
		Ref<Item> deleted = Item::create(5004, potion, 1, false, 0);
		warehouse.onLoadHandler(*deleted);
		runtime::LeakCensus::getInstance().onRemovedFromWorld(*deleted, "Item", 5004);
		EXPECT_THROW(static_cast<void>(warehouse.delete_(*deleted)), runtime::NullPointerException);
		Ref<Item> removed = Item::create(5005, potion, 1, false, 0);
		warehouse.onLoadHandler(*removed);
		EXPECT_TRUE(warehouse.remove(*removed));
		runtime::LeakCensus::getInstance().onRemovedFromWorld(*removed, "Item", 5005);
	}
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(runtime::LeakCensus::getInstance().trackedCount(), 4u)
		<< "the warehouse holds two items and the kinah item and queues the deleted one; the removed item is freed";
	account.reset(); // the last reference: the account destroys its warehouse part, which releases its items and the deleted queue
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(runtime::LeakCensus::getInstance().trackedCount(), 0u);
	EXPECT_TRUE(runtime::LeakCensus::getInstance().getLeaks().empty());
	runtime::LeakCensus::getInstance().uninstall();
	utils::ThreadPoolManager::installBackend(nullptr);
	runtime::Reclaimer::getInstance().drain();
}

} // namespace
} // namespace aion::gameserver::model::items::storage
