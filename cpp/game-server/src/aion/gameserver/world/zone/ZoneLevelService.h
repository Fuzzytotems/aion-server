#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::world::zone {

/**
 * Checks the water level (drowning) and the death level of the map for players.
 * <p>
 * C++: a static-only class.
 *
 * @author ATracer
 */
class ZoneLevelService {
private:
	static constexpr int64_t DROWN_PERIOD = 1000;

public:
	ZoneLevelService() = delete;

	/** Check water level (start drowning) and map death level (die) */
	static void checkZoneLevels(model::gameobjects::player::Player& player);

private:
	static void stopDrowning(model::gameobjects::player::Player& player);

	static void startDrowning(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::world::zone
