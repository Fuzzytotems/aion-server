#pragma once

#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Rolandas
 */
class HouseObjectCooldownsDAO {
public:
	static void loadHouseObjectCooldowns(model::gameobjects::player::Player& player);
	static void storeHouseObjectCooldowns(model::gameobjects::player::Player& player);
private:
	static void deleteHouseObjectCoolDowns(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::dao
