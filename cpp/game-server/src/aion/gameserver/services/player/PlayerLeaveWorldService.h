#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/player/fwd.h"

namespace aion::gameserver::services::player {

/**
 * @author ATracer, Neon
 */
class PlayerLeaveWorldService {
public:
	/**
	 * This method is called when a player loses client connection, e.g. when killing the process, or due to bad network connectivity.<br>
	 * <br>
	 * <b><font color='red'>NOTICE:</font> This method must only be called from {@link AionConnection#onDisconnect()} and not from anywhere else</b>
	 */
	static void leaveWorldDelayed(model::gameobjects::player::Player& player, int64_t delayInMillis);
	/**
	 * This method saves a player and removes him from the world. It is called when a player leaves the game, which includes just two cases: either
	 * he goes back to char selection screen or is leaving the game (closing client).<br>
	 * <br>
	 * <b><font color='red'>NOTICE:</font> This method is called only from {@link CM_QUIT} and must not be called from anywhere else</b>
	 */
	static void leaveWorld(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::services::player
