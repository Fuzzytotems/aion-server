#pragma once

#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author ATracer
 */
class ItemCooldownsDAO {
public:
	static void loadItemCooldowns(model::gameobjects::player::Player& player);
	static void storeItemCooldowns(model::gameobjects::player::Player& player);
private:
	static void deleteItemCooldowns(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::dao
