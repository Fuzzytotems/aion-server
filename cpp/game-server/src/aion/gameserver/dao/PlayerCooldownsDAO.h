#pragma once

#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author nrg
 */
class PlayerCooldownsDAO {
public:
	static void loadPlayerCooldowns(model::gameobjects::player::Player& player);
	static void storePlayerCooldowns(model::gameobjects::player::Player& player);
private:
	static void deletePlayerCooldowns(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::dao
