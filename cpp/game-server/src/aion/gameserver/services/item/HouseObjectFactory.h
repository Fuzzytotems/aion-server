#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/services/item/fwd.h"

namespace aion::gameserver::services::item {

/**
 * C++: a static-only class (hub-headers.md §11.1). HouseObject<?> is the erased HouseObject (hub-headers.md §8.1); the factories return new
 * objects (Ref).
 *
 * @author Rolandas
 */
class HouseObjectFactory final {
public:
	/** For loading data from DB */
	static runtime::Ref<model::gameobjects::HouseObject> createNew(model::house::HouseRegistry& registry, int32_t objectId, int32_t objectTemplateId);
	/** For transferring item from inventory to house registry */
	static runtime::Ref<model::gameobjects::HouseObject> createNew(model::house::House& house, const model::templates::item::ItemTemplate* itemTemplate);
};

} // namespace aion::gameserver::services::item
