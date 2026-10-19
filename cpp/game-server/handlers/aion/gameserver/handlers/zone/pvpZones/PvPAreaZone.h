#pragma once

#include "aion/gameserver/handlers/zone/pvpZones/PvPZone.h"

namespace aion::gameserver::handlers::zone::pvpZones {

/**
 * The zone handler of the Sanctum and Pandaemonium coliseums ("LC1_PVP_SUB_C_110010000", "DC1_PVP_ZONE_120010000"): a player inside keeps
 * his buffs on death when CustomConfig.KEEP_BUFFS_IN_COLISEUM is set; leaving, he no longer does; a death teleports him to the coliseum's
 * entrance of his capital.
 * <p>
 * Java: data/handlers/zone/pvpZones/PvPAreaZone.java, @ZoneNameAnnotation(value = "LC1_PVP_SUB_C_110010000 DC1_PVP_ZONE_120010000") (the
 * marker is in the .cpp).
 *
 * @author MrPoke
 */
class PvPAreaZone : public PvPZone {
	AION_MAKE_REF_FRIEND

protected:
	PvPAreaZone() = default;

public:
	/** The registry's factory (HandlerRegistry.h ZoneHandlerClass): Java's newInstance() */
	static runtime::Ref<PvPAreaZone> create();

	void onEnterZone(Creature& creature, ZoneInstance& zone) override;

	void onLeaveZone(Creature& creature, ZoneInstance& zone) override;

protected:
	void doTeleport(Player& player, const ZoneName* zoneName) override;
};

} // namespace aion::gameserver::handlers::zone::pvpZones
