#pragma once

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
class ItemRestrictionService {
public:
	/** Check if item can be moved from storage by player */
	static bool isItemRestrictedFrom(model::gameobjects::player::Player& player, model::gameobjects::Item& item, model::items::storage::StorageType storageType);
	/** Check if item can be moved to storage by player */
	static bool isItemRestrictedTo(model::gameobjects::player::Player& player, model::gameobjects::Item& item, model::items::storage::StorageType storageType);
	/** Check, whether the item can be removed */
	static bool canRemoveItem(model::gameobjects::player::Player& player, model::gameobjects::Item& item);
};

} // namespace aion::gameserver::services::item
