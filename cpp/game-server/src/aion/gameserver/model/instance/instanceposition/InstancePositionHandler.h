#pragma once

#include <cstdint>

#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::instance::instanceposition {

/**
 * Java interface InstancePositionHandler (fieldmap K5, confined): a pure interface.
 *
 * @author xTz
 */
class InstancePositionHandler {
public:
	virtual void initialize(int32_t mapId, int32_t instanceId) = 0;

	virtual void port(gameobjects::player::Player& player, int32_t zone, int32_t position) = 0;

protected:
	virtual ~InstancePositionHandler() = default;
};

} // namespace aion::gameserver::model::instance::instanceposition
