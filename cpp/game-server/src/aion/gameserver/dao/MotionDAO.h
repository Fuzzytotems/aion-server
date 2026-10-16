#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/player/motion/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author MrPoke
 */
class MotionDAO {
public:
	static void loadMotionList(model::gameobjects::player::Player& player);
	static bool storeMotion(int32_t objectId, model::gameobjects::player::motion::Motion& motion);
	static bool deleteMotion(int32_t objectId, int32_t motionId);
	static bool updateMotion(int32_t objectId, model::gameobjects::player::motion::Motion& motion);
};

} // namespace aion::gameserver::dao
