#pragma once

#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Neon
 */
class AdventDAO {
public:
	static bool canReceiveReward(model::gameobjects::player::Player& player, commons::database::Date date);
	static bool storeLastReceivedDay(model::gameobjects::player::Player& player, commons::database::Date date);
};

} // namespace aion::gameserver::dao
