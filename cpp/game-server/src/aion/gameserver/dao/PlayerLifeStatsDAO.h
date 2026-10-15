#pragma once

#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Mr. Poke
 */
class PlayerLifeStatsDAO {
public:
	static void loadPlayerLifeStat(model::gameobjects::player::Player& player);
	static void insertPlayerLifeStat(model::gameobjects::player::Player& player);
	static void updatePlayerLifeStat(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::dao
