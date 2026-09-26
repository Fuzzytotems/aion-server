#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/item/fwd.h"

namespace aion::gameserver::services::item {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author Ranastic, Estrayl
 */
class ItemPurificationService {
public:
	static bool isPurificationAllowed(model::gameobjects::player::Player& player, model::gameobjects::Item& baseItem, int32_t resultItemId);
	static bool decreaseMaterials(model::gameobjects::player::Player& player, model::gameobjects::Item& baseItem, int32_t resultItemId);
	static void upgradeItem(model::gameobjects::player::Player& player, model::gameobjects::Item& sourceItem, int32_t targetItemId);
};

} // namespace aion::gameserver::services::item
