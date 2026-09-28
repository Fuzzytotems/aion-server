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
 * @author Estrayl
 */
class ItemActionService {
public:
	static void identifyItem(model::gameobjects::player::Player& player, model::gameobjects::Item& item);
	static void applyTuneResult(model::gameobjects::player::Player& player, model::gameobjects::Item& item);
};

} // namespace aion::gameserver::services::item
