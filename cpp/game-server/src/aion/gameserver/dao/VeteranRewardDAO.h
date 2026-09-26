#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Neon
 */
class VeteranRewardDAO {
public:
	static int32_t loadReceivedMonths(model::gameobjects::player::Player& player);
	static bool storeReceivedMonths(model::gameobjects::player::Player& player, int32_t months);
};

} // namespace aion::gameserver::dao
