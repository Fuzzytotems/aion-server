#pragma once

#include "aion/gameserver/handlers/zone/ZonePrelude.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/world/zone/handler/AdvancedZoneHandler.h"

namespace aion::gameserver::handlers::zone::pvpZones {

/**
 * The base of the PvP zone handlers: a player who dies in a PvP zone instance tells everyone in it how, and 5 s later is revived, teleported
 * out (doTeleport) and announced in the zone.
 * <p>
 * Java: data/handlers/zone/pvpZones/PvPZone.java (abstract, implements AdvancedZoneHandler).
 *
 * @author MrPoke
 */
class PvPZone : public runtime::RefCounted, public AdvancedZoneHandler {
	AION_MAKE_REF_FRIEND

protected:
	PvPZone() = default;
	~PvPZone() override;

public:
	void onEnterZone(Creature& player, ZoneInstance& zone) override;

	void onLeaveZone(Creature& player, ZoneInstance& zone) override;

	bool onDie(Creature& lastAttacker, Creature& target, ZoneInstance& zone) override;

protected:
	virtual void doTeleport(Player& player, const ZoneName* zoneName) = 0;

public:
	/** C++ only: ZoneHandler held by Ref retains this object (hub-headers.md §9.2). */
	void retain() const noexcept override { runtime::RefCounted::retain(); }
	/** C++ only: ZoneHandler held by Ref releases this object (hub-headers.md §9.2). */
	void release() const noexcept override { runtime::RefCounted::release(); }
};

} // namespace aion::gameserver::handlers::zone::pvpZones
