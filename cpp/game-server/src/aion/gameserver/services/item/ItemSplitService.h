#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/storage/fwd.h"
#include "aion/gameserver/services/item/fwd.h"

namespace aion::gameserver::services::item {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class ItemSplitService {
public:
	/** Move part of stack into different slot */
	static void splitItem(model::gameobjects::player::Player& player, int32_t itemObjId, int32_t destinationObjId, int64_t splitAmount, int16_t slotNum, int8_t sourceStorageType, int8_t destinationStorageType);
	/** Merge 2 stacks with simple validation */
	static void mergeStacks(model::items::storage::IStorage& sourceStorage, model::items::storage::IStorage& destStorage, model::gameobjects::Item& sourceItem, model::gameobjects::Item& targetItem, int64_t count);
private:
	static void moveKinah(model::gameobjects::player::Player& player, model::items::storage::IStorage& source, int64_t splitAmount);
	static void updateKinahCount(model::items::storage::IStorage& source, int64_t splitAmount, model::items::storage::IStorage& destination);
};

} // namespace aion::gameserver::services::item
