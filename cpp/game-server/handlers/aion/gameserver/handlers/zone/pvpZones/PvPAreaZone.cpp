#include "aion/gameserver/handlers/zone/pvpZones/PvPAreaZone.h"

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::handlers::zone::pvpZones {

AION_ZONE_HANDLER(PvPAreaZone, "LC1_PVP_SUB_C_110010000 DC1_PVP_ZONE_120010000");

runtime::Ref<PvPAreaZone> PvPAreaZone::create() {
	return runtime::makeRef<PvPAreaZone>();
}

// Java PvPAreaZone.java:18-22
void PvPAreaZone::onEnterZone(Creature& creature, ZoneInstance& /*zone*/) {
	if (runtime::Ptr<Player> player = runtime::as<Player>(creature); player != nullptr && CustomConfig::KEEP_BUFFS_IN_COLISEUM)
		player->getEffectController()->setKeepBuffsOnDie(true);
}

// Java PvPAreaZone.java:24-28
void PvPAreaZone::onLeaveZone(Creature& creature, ZoneInstance& /*zone*/) {
	if (runtime::Ptr<Player> player = runtime::as<Player>(creature))
		player->getEffectController()->setKeepBuffsOnDie(false);
}

// Java PvPAreaZone.java:30-36
void PvPAreaZone::doTeleport(Player& player, const ZoneName* zoneName) {
	if (zoneName == ZoneName::get("LC1_PVP_SUB_C_110010000"))
		TeleportService::teleportTo(player, 110010000, 1, 1470.3f, 1343.5f, 563.7f);
	else if (zoneName == ZoneName::get("DC1_PVP_ZONE_120010000"))
		TeleportService::teleportTo(player, 120010000, 1, 1005.1f, 1528.9f, 222.1f);
}

} // namespace aion::gameserver::handlers::zone::pvpZones
