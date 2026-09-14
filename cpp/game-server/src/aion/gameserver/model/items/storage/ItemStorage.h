#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/items/storage/fwd.h"

namespace aion::gameserver::model::items::storage {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Storage.itemStorage`), created with create(); the
 * constructor is ported (the limit comes from the StorageType companion, StorageTypeInfo.h), so every Storage can be created.
 *
 * @author KID
 */
class ItemStorage : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	static constexpr int64_t FIRST_AVAILABLE_SLOT = 65535;

private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<gameobjects::Item>> items{AION_LOCK_CLASS(ItemStorage::items#stripe)};
	const StorageType storageType;
	runtime::Field<int32_t> limit;

protected:
	explicit ItemStorage(StorageType storageType);
	~ItemStorage() override;

public:
	/** Java: new ItemStorage(storageType) */
	static runtime::Ref<ItemStorage> create(StorageType storageType);

	/** @return A copy of the list */
	std::vector<runtime::Ptr<gameobjects::Item>> getItems();

	int32_t getLimit() const { return limit.get(); }

	void setLimit(int32_t value) { limit.set(value); }

	int32_t getRowLength();

	/** @return the first item with the item id, null if there is none */
	runtime::Ptr<gameobjects::Item> getFirstItemById(int32_t itemId);

	std::vector<runtime::Ptr<gameobjects::Item>> getItemsById(int32_t itemId);

	runtime::Ptr<gameobjects::Item> getItemByObjId(int32_t itemObjId);

	int64_t getSlotIdByItemId(int32_t itemId);

	runtime::Ptr<gameobjects::Item> getItemBySlotId(int16_t slotId);

	runtime::Ptr<gameobjects::Item> getSpecialItemBySlotId(int16_t slotId);

	int64_t getSlotIdByObjId(int32_t objId);

	/** @return True if the item was added, false if the list already contains the item */
	bool putItem(gameobjects::Item& item);

	/** @return the removed item, null if there was none */
	runtime::Ptr<gameobjects::Item> removeItem(int32_t objId);

	bool isFull();

	bool isFullSpecialCube();

	std::vector<runtime::Ptr<gameobjects::Item>> getSpecialCubeItems();

	std::vector<runtime::Ptr<gameobjects::Item>> getCubeItems();

	int32_t getFreeSlots();

	int32_t getSpecialCubeFreeSlots();

	int32_t size();
};

} // namespace aion::gameserver::model::items::storage
