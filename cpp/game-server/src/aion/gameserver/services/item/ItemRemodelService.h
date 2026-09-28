#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/item/fwd.h"

namespace aion::gameserver::services::item {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author Sarynth, Wakizashi
 */
class ItemRemodelService {
public:
	static void remodelItem(model::gameobjects::player::Player& player, int32_t keepItemObjId, int32_t extractItemObjId);
};

} // namespace aion::gameserver::services::item
