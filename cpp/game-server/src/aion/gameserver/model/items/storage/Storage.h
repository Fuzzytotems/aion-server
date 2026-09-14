#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentLinkedQueue.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/storage/IStorage.h"
#include "aion/gameserver/model/items/storage/fwd.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemAddType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemDeleteType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"

namespace aion::gameserver::model::items::storage {

/**
 * Hub header (docs/design/hub-headers.md). A part (runtime::OwnedPart, parts.json: Player.inventory, regularWarehouse, petBags, cabinets,
 * Account.accountWarehouse). The Java constructors take no owner: subclasses bind it (`bindOwner(owner)` in the PlayerStorage/LegionWarehouse
 * constructors, before the part is published).
 * The package-private methods taking the actor are the implementation of the IStorage methods of the subclasses, which pass their actor field;
 * that field is null for an account warehouse without an entering player and the Java code checks it, so `actor` is `Ptr<Player>` in every
 * overload. `item` of decreaseItemCount is nullable (decreaseKinah passes a missing kinah item), `questStatus` of the 5-argument
 * decreaseItemCount and of decreaseByItemId(int, long, QuestStatus, Player) is `std::optional` (the shorter overloads pass null).
 * The actor overloads would hide the IStorage overloads of the same name, so the IStorage names are brought in with using-declarations.
 *
 * @author KID, ATracer
 */
class Storage : public runtime::OwnedPart, public IStorage {
private:
	// Java: private static final Logger log = LoggerFactory.getLogger("ITEM_LOG") - namespace-scope logger in Storage.cpp
	const runtime::Ref<ItemStorage> itemStorage;
	runtime::Field<runtime::Ref<gameobjects::Item>> kinahItem{};
	const StorageType storageType;
	runtime::ConcurrentLinkedQueue<runtime::Ref<gameobjects::Item>> deletedItems{};
	/** Can be of 2 types: UPDATED and UPDATE_REQUIRED */
	runtime::Field<gameobjects::Persistable::PersistentState> persistentState{gameobjects::Persistable::PersistentState::UPDATED};

protected:
	/** Java abstract class: only subclasses construct (and bind the late-bound owner of) a storage */
	explicit Storage(StorageType storageType);

	/** @param withDeletedItems Java leaves the deleted items queue null when false (LegionStorageProxy); the C++ queue always exists */
	Storage(StorageType storageType, bool withDeletedItems);

public:
	~Storage() override;

	int64_t getKinah() override;

	runtime::Ptr<gameobjects::Item> getKinahItem() override { return kinahItem.get(); }

	StorageType getStorageType() override { return storageType; }

	using IStorage::increaseKinah;

	void increaseKinah(int64_t amount, runtime::Ptr<gameobjects::player::Player> actor);

	void increaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType, runtime::Ptr<gameobjects::player::Player> actor);

	using IStorage::tryDecreaseKinah;

	/**
	 * Decrease kinah by {@code amount} but check first that its enough in storage
	 *
	 * @return true if decrease was successful
	 */
	bool tryDecreaseKinah(int64_t amount, runtime::Ptr<gameobjects::player::Player> actor);

	bool tryDecreaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType,
		runtime::Ptr<gameobjects::player::Player> actor);

	using IStorage::decreaseKinah;

	/** just decrease kinah without any checks */
	void decreaseKinah(int64_t amount, runtime::Ptr<gameobjects::player::Player> actor);

	void decreaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType, runtime::Ptr<gameobjects::player::Player> actor);

	using IStorage::increaseItemCount;

	int64_t increaseItemCount(gameobjects::Item& item, int64_t count, runtime::Ptr<gameobjects::player::Player> actor);

	/** increase item count and return left count */
	int64_t increaseItemCount(gameobjects::Item& item, int64_t count, services::item::ItemPacketService_ItemUpdateType updateType,
		runtime::Ptr<gameobjects::player::Player> actor);

	using IStorage::decreaseItemCount;

	int64_t decreaseItemCount(runtime::Ptr<gameobjects::Item> item, int64_t count, runtime::Ptr<gameobjects::player::Player> actor);

	/** decrease item count and return left count */
	int64_t decreaseItemCount(runtime::Ptr<gameobjects::Item> item, int64_t count, services::item::ItemPacketService_ItemUpdateType updateType,
		runtime::Ptr<gameobjects::player::Player> actor);

	int64_t decreaseItemCount(runtime::Ptr<gameobjects::Item> item, int64_t count, services::item::ItemPacketService_ItemUpdateType updateType,
		std::optional<questEngine::model::QuestStatus> questStatus, runtime::Ptr<gameobjects::player::Player> actor);

	/**
	 * This method should be called only for new items added to inventory (loading from DB) If item is equiped - will be put to equipment if item is
	 * unequiped - will be put to default bag for now Kinah is stored separately as it will be used frequently
	 */
	void onLoadHandler(gameobjects::Item& item) override;

	using IStorage::add;

	runtime::Ptr<gameobjects::Item> add(gameobjects::Item& item, runtime::Ptr<gameobjects::player::Player> actor);

	runtime::Ptr<gameobjects::Item> add(gameobjects::Item& item, services::item::ItemPacketService_ItemAddType addType,
		runtime::Ptr<gameobjects::player::Player> actor);

	/** used only for character transfers */
	runtime::Ptr<gameobjects::Item> add_CharacterTransfer(gameobjects::Item& item);

	using IStorage::put;

	// a bit misleading name - but looks like its used only for equipment
	runtime::Ptr<gameobjects::Item> put(gameobjects::Item& item, runtime::Ptr<gameobjects::player::Player> actor);

	/** Remove item from storage without changing its state */
	runtime::Ptr<gameobjects::Item> remove(gameobjects::Item& item) override;

	using IStorage::delete_;

	/** Delete item from storage and mark for DB update */
	runtime::Ptr<gameobjects::Item> delete_(gameobjects::Item& item, runtime::Ptr<gameobjects::player::Player> actor);

	/** Delete item from storage and mark for DB update */
	runtime::Ptr<gameobjects::Item> delete_(gameobjects::Item& item, services::item::ItemPacketService_ItemDeleteType deleteType,
		runtime::Ptr<gameobjects::player::Player> actor);

	using IStorage::decreaseByItemId;

	bool decreaseByItemId(int32_t itemId, int64_t count, runtime::Ptr<gameobjects::player::Player> actor);

	bool decreaseByItemId(int32_t itemId, int64_t count, std::optional<questEngine::model::QuestStatus> questStatus,
		runtime::Ptr<gameobjects::player::Player> actor);

	using IStorage::decreaseByObjectId;

	bool decreaseByObjectId(int32_t itemObjId, int64_t count, runtime::Ptr<gameobjects::player::Player> actor);

	bool decreaseByObjectId(int32_t itemObjId, int64_t count, questEngine::model::QuestStatus questStatus,
		runtime::Ptr<gameobjects::player::Player> actor);

	bool decreaseByObjectId(int32_t itemObjId, int64_t count, services::item::ItemPacketService_ItemUpdateType updateType,
		runtime::Ptr<gameobjects::player::Player> actor);

	runtime::Ptr<gameobjects::Item> getFirstItemByItemId(int32_t itemId) override;

	std::vector<runtime::Ptr<gameobjects::Item>> getItemsWithKinah() override;

	std::vector<runtime::Ptr<gameobjects::Item>> getItems() override;

	std::vector<runtime::Ptr<gameobjects::Item>> getItemsByItemId(int32_t itemId) override;

	runtime::ConcurrentLinkedQueue<runtime::Ref<gameobjects::Item>>& getDeletedItems() override { return deletedItems; }

	runtime::Ptr<gameobjects::Item> getItemByObjId(int32_t itemObjId) override;

	int64_t getItemCountByItemId(int32_t itemId) override;

	bool isFull() override;

	bool isFullSpecialCube();

	bool isFull(int32_t inventory);

	int32_t getFreeSlots(int32_t inventory);

	int32_t getSpecialCubeFreeSlots();

	int32_t getFreeSlots() override;

	virtual void setLimit(int32_t limit);

	int32_t getLimit() override;

	int32_t getRowLength() override;

	/** Java final */
	PersistentState getPersistentState() override final { return persistentState.get(); }

	/** Java final */
	void setPersistentState(PersistentState value) override final { persistentState.set(value); }

	int32_t size() override;
};

} // namespace aion::gameserver::model::items::storage
