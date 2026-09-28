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
class ItemMoveService {
public:
	static void moveItem(model::gameobjects::player::Player& player, int32_t itemObjId, int8_t sourceStorageType, int8_t destinationStorageType, int16_t slot);
private:
	static void moveInSameStorage(model::items::storage::IStorage& storage, model::gameobjects::Item& item, int16_t slot);
public:
	static void switchItemsInStorages(model::gameobjects::player::Player& player, int8_t sourceStorageType, int32_t sourceItemObjId, int8_t replaceStorageType, int32_t replaceItemObjId);
};

} // namespace aion::gameserver::services::item
