#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentLinkedQueue.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/storage/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemAddType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemDeleteType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"

namespace aion::gameserver::model::items::storage {

/**
 * Public interface for Storage, later will rename probably
 * <p>
 * Hub header (docs/design/hub-headers.md §9.2): an abstract class without data members, deriving the Persistable interface. getDeletedItems()
 * returns the live queue of the storage (Java returns the field). setOwner takes a nullable player (the account warehouse drops its actor).
 *
 * @author ATracer
 */
class IStorage : public gameobjects::Persistable {
public:
	virtual void setOwner(runtime::Ptr<gameobjects::player::Player> player) = 0;

	virtual int64_t getKinah() = 0;

	/** @return kinah item or null if storage never had kinah */
	virtual runtime::Ptr<gameobjects::Item> getKinahItem() = 0;

	virtual StorageType getStorageType() = 0;

	virtual void increaseKinah(int64_t amount) = 0;

	virtual void increaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) = 0;

	virtual bool tryDecreaseKinah(int64_t amount) = 0;

	virtual bool tryDecreaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) = 0;

	virtual void decreaseKinah(int64_t amount) = 0;

	virtual void decreaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) = 0;

	virtual int64_t increaseItemCount(gameobjects::Item& item, int64_t count) = 0;

	virtual int64_t increaseItemCount(gameobjects::Item& item, int64_t count, services::item::ItemPacketService_ItemUpdateType updateType) = 0;

	virtual int64_t decreaseItemCount(gameobjects::Item& item, int64_t count) = 0;

	virtual int64_t decreaseItemCount(gameobjects::Item& item, int64_t count, services::item::ItemPacketService_ItemUpdateType updateType) = 0;

	virtual int64_t decreaseItemCount(gameobjects::Item& item, int64_t count, services::item::ItemPacketService_ItemUpdateType updateType,
		questEngine::model::QuestStatus questStatus) = 0;

	/** Add operation should be used for new items incoming into storage from outside */
	virtual runtime::Ptr<gameobjects::Item> add(gameobjects::Item& item) = 0;

	virtual runtime::Ptr<gameobjects::Item> add(gameobjects::Item& item, services::item::ItemPacketService_ItemAddType addType) = 0;

	/** Put operation is used in some operations like unequip */
	virtual runtime::Ptr<gameobjects::Item> put(gameobjects::Item& item) = 0;

	virtual runtime::Ptr<gameobjects::Item> remove(gameobjects::Item& item) = 0;

	virtual runtime::Ptr<gameobjects::Item> delete_(gameobjects::Item& item) = 0;

	virtual runtime::Ptr<gameobjects::Item> delete_(gameobjects::Item& item, services::item::ItemPacketService_ItemDeleteType deleteType) = 0;

	virtual bool decreaseByItemId(int32_t itemId, int64_t count) = 0;

	virtual bool decreaseByItemId(int32_t itemId, int64_t count, questEngine::model::QuestStatus questStatus) = 0;

	virtual bool decreaseByObjectId(int32_t itemObjId, int64_t count) = 0;

	virtual bool decreaseByObjectId(int32_t itemObjId, int64_t count, services::item::ItemPacketService_ItemUpdateType updateType) = 0;

	virtual bool decreaseByObjectId(int32_t itemObjId, int64_t count, questEngine::model::QuestStatus questStatus) = 0;

	virtual runtime::Ptr<gameobjects::Item> getFirstItemByItemId(int32_t itemId) = 0;

	virtual std::vector<runtime::Ptr<gameobjects::Item>> getItemsWithKinah() = 0;

	virtual std::vector<runtime::Ptr<gameobjects::Item>> getItems() = 0;

	virtual std::vector<runtime::Ptr<gameobjects::Item>> getItemsByItemId(int32_t itemId) = 0;

	virtual runtime::Ptr<gameobjects::Item> getItemByObjId(int32_t itemObjId) = 0;

	virtual int64_t getItemCountByItemId(int32_t itemId) = 0;

	virtual bool isFull() = 0;

	virtual int32_t getFreeSlots() = 0;

	virtual int32_t getLimit() = 0;

	virtual int32_t getRowLength() = 0;

	virtual int32_t size() = 0;

	/** @return the live queue of deleted items (Java Queue<Item> field) */
	virtual runtime::ConcurrentLinkedQueue<runtime::Ref<gameobjects::Item>>& getDeletedItems() = 0;

	virtual void onLoadHandler(gameobjects::Item& item) = 0;

	/** Java default method */
	virtual network::aion::serverpackets::SM_SYSTEM_MESSAGE getStorageIsFullMessage();

	virtual ~IStorage() = default;

protected:
	IStorage() = default;
	IStorage(const IStorage&) = default;
	IStorage& operator=(const IStorage&) = default;
};

} // namespace aion::gameserver::model::items::storage
