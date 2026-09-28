#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::model::team::legion {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part of Legion (`const std::unique_ptr<LegionWarehouse>`, parts.json), bound to the
 * legion in the constructor (Java keeps no reference to it). The constructor sets the slot limit through Storage::setLimit (updateLimit), which
 * is unported, so it stays `AION_UNPORTED` after binding the owner.
 *
 * @author Simple
 */
class LegionWarehouse : public items::storage::Storage {
private:
	static constexpr int32_t DEFAULT_ROWS = 3; // hardcoded, as the client doesn't allow to change it
	static constexpr int32_t SLOTS_PER_ROW = 8;
	runtime::AtomicInteger currentUser{AION_LOCK_CLASS(LegionWarehouse::currentUser)};

public:
	explicit LegionWarehouse(Legion& legion);

	~LegionWarehouse() override;

	using Storage::increaseKinah;

	void increaseKinah(int64_t amount) override;

	/** @throws UnsupportedOperationException LWH should be used behind proxy */
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

	bool decreaseByItemId(int32_t itemId, int64_t count, questEngine::model::QuestStatus questStatus) override;

	using Storage::decreaseByObjectId;

	bool decreaseByObjectId(int32_t itemObjId, int64_t count) override;

	bool decreaseByObjectId(int32_t itemObjId, int64_t count, services::item::ItemPacketService_ItemUpdateType updateType) override;

	bool decreaseByObjectId(int32_t itemObjId, int64_t count, questEngine::model::QuestStatus questStatus) override;

	/** @throws UnsupportedOperationException LWH doesnt have owner */
	void setOwner(runtime::Ptr<gameobjects::player::Player> player) override;

	bool unsetInUse(int32_t playerObjId);

	bool setInUse(int32_t playerObjId);

	int32_t getCurrentUser();

	/** @throws UnsupportedOperationException Slot limit is controlled by the expansion level, use updateLimit() instead */
	void setLimit(int32_t limit) override;

	void updateLimit(int32_t warehouseExpansions);
};

} // namespace aion::gameserver::model::team::legion
