#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentLinkedQueue.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/fwd.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::model::items::storage {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part of the acting Player (C++-only `Player.legionStorageProxy`
 * `PartSlot<LegionStorageProxy, RetireTo::RECLAIMER>`; Java creates one per `getStorage(LEGION_WAREHOUSE)` call and in
 * LegionWarehouse.increaseKinah), bound to the player in the constructor, so the actor is the part owner (`OwnerRef<Player>`; a Ref would be a
 * self cycle through the player's slot). The proxied legion warehouse is a part of its Legion, held by Ref (it retains the legion).
 *
 * @author ATracer
 */
class LegionStorageProxy : public Storage {
private:
	// fieldmap.toml: the acting player is the part owner (C++-only Player.legionStorageProxy slot, [cpp_members])
	runtime::OwnerRef<gameobjects::player::Player> actor;
	const runtime::Ref<Storage> storage;

public:
	/** Java: new LegionStorageProxy(storage, actor), bound to the acting player */
	LegionStorageProxy(team::legion::LegionWarehouse& storage, gameobjects::player::Player& actor);

	~LegionStorageProxy() override;

	using Storage::increaseKinah;

	void increaseKinah(int64_t amount) override;

	void increaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) override;

	using Storage::tryDecreaseKinah;

	bool tryDecreaseKinah(int64_t amount) override;

	bool tryDecreaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) override;

	using Storage::decreaseKinah;

	void decreaseKinah(int64_t amount) override;

	void decreaseKinah(int64_t amount, services::item::ItemPacketService_ItemUpdateType updateType) override;

	using Storage::increaseItemCount;

	int64_t increaseItemCount(gameobjects::Item& item, int64_t count) override;

	int64_t increaseItemCount(gameobjects::Item& item, int64_t count, services::item::ItemPacketService_ItemUpdateType updateType) override;

	using Storage::decreaseItemCount;

	int64_t decreaseItemCount(gameobjects::Item& item, int64_t count) override;

	int64_t decreaseItemCount(gameobjects::Item& item, int64_t count, services::item::ItemPacketService_ItemUpdateType updateType) override;

	/** @throws UnsupportedOperationException Quests should not update LWH! */
	int64_t decreaseItemCount(gameobjects::Item& item, int64_t count, services::item::ItemPacketService_ItemUpdateType updateType,
		questEngine::model::QuestStatus questStatus) override;

	using Storage::add;

	runtime::Ptr<gameobjects::Item> add(gameobjects::Item& item) override;

	runtime::Ptr<gameobjects::Item> add(gameobjects::Item& item, services::item::ItemPacketService_ItemAddType addType) override;

	using Storage::put;

	runtime::Ptr<gameobjects::Item> put(gameobjects::Item& item) override;

	using Storage::delete_;

	runtime::Ptr<gameobjects::Item> delete_(gameobjects::Item& item) override;

	runtime::Ptr<gameobjects::Item> delete_(gameobjects::Item& item, services::item::ItemPacketService_ItemDeleteType deleteType) override;

	using Storage::decreaseByItemId;

	bool decreaseByItemId(int32_t itemId, int64_t count) override;

	/** @throws UnsupportedOperationException Quests should not update LWH! */
	bool decreaseByItemId(int32_t itemId, int64_t count, questEngine::model::QuestStatus questStatus) override;

	using Storage::decreaseByObjectId;

	bool decreaseByObjectId(int32_t itemObjId, int64_t count) override;

	bool decreaseByObjectId(int32_t itemObjId, int64_t count, services::item::ItemPacketService_ItemUpdateType updateType) override;

	/** @throws UnsupportedOperationException Quests should not update LWH! */
	bool decreaseByObjectId(int32_t itemObjId, int64_t count, questEngine::model::QuestStatus questStatus) override;

	int64_t getKinah() override;

	runtime::Ptr<gameobjects::Item> getKinahItem() override;

	StorageType getStorageType() override;

	void onLoadHandler(gameobjects::Item& item) override;

	runtime::Ptr<gameobjects::Item> remove(gameobjects::Item& item) override;

	runtime::Ptr<gameobjects::Item> getFirstItemByItemId(int32_t itemId) override;

	std::vector<runtime::Ptr<gameobjects::Item>> getItemsWithKinah() override;

	std::vector<runtime::Ptr<gameobjects::Item>> getItems() override;

	std::vector<runtime::Ptr<gameobjects::Item>> getItemsByItemId(int32_t itemId) override;

	runtime::ConcurrentLinkedQueue<runtime::Ref<gameobjects::Item>>& getDeletedItems() override;

	runtime::Ptr<gameobjects::Item> getItemByObjId(int32_t itemObjId) override;

	using Storage::isFull;

	bool isFull() override;

	using Storage::getFreeSlots;

	int32_t getFreeSlots() override;

	void setLimit(int32_t limit) override;

	int32_t getLimit() override;

	int32_t size() override;

	/** @throws UnsupportedOperationException LWH doesnt have owner */
	void setOwner(runtime::Ptr<gameobjects::player::Player> player) override;
};

} // namespace aion::gameserver::model::items::storage
