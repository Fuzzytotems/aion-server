#pragma once

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"
#include "aion/gameserver/world/zone/handler/ZoneHandler.h"

namespace aion::gameserver::world::zone::handler {

/**
 * A zone handler that is also notified when a creature dies inside its zone (ZoneInstance.onDie).
 * <p>
 * C++: an interface (hub-headers.md §9.2) extending ZoneHandler; implementors (the PvPZone handler) provide the runtime base and forward
 * retain()/release() as every ZoneHandler implementor does. ZoneInstance tests handlers with dynamic_cast, like Java's instanceof.
 *
 * @author MrPoke
 */
class AdvancedZoneHandler : public ZoneHandler {
public:
	/**
	 * This call if creature die in zone.
	 *
	 * @return TRUE if hadle die event.
	 */
	virtual bool onDie(model::gameobjects::Creature& attacker, model::gameobjects::Creature& target, ZoneInstance& zone) = 0;

protected:
	AdvancedZoneHandler() = default;
	AdvancedZoneHandler(const AdvancedZoneHandler&) = default;
	AdvancedZoneHandler& operator=(const AdvancedZoneHandler&) = default;
};

} // namespace aion::gameserver::world::zone::handler
