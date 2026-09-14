#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/player/fwd.h"

namespace aion::gameserver::services::player {

/**
 * @author Source, Neon
 */
class PlayerLimitService : public runtime::Immortal {
private:
	static inline runtime::ConcurrentHashMap<int32_t, int64_t> sellLimit{AION_LOCK_CLASS(PlayerLimitService::sellLimit#stripe)};
public:
	static int64_t updateSellLimit(model::gameobjects::player::Player& player, int64_t itemPrice, int64_t itemCount);
	void scheduleUpdate();
	static PlayerLimitService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services::player
