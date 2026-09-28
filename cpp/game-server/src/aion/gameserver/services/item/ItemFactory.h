#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/services/item/fwd.h"

namespace aion::gameserver::services::item {

/**
 * C++: a static-only class (hub-headers.md §11.1). newItem returns the new item (Ref).
 *
 * @author ATracer
 */
class ItemFactory {
public:
	static runtime::Ref<model::gameobjects::Item> newItem(int32_t itemId);
	static runtime::Ref<model::gameobjects::Item> newItem(int32_t itemId, int64_t count);
private:
	static int64_t calculateCount(const model::templates::item::ItemTemplate* itemTemplate, int64_t count);
};

} // namespace aion::gameserver::services::item
