#pragma once

#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

class PortalCooldownsDAO {
public:
	static void loadPortalCooldowns(model::gameobjects::player::Player& player);
	static void storePortalCooldowns(model::gameobjects::player::Player& player);
private:
	static void deletePortalCooldowns(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::dao
