#include "aion/gameserver/handlers/zone/pvpZones/PvPZone.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/zone/PvPZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::handlers::zone::pvpZones {

PvPZone::~PvPZone() = default;

// Java PvPZone.java:23-25
void PvPZone::onEnterZone(Creature& /*player*/, ZoneInstance& /*zone*/) {}

// Java PvPZone.java:27-29
void PvPZone::onLeaveZone(Creature& /*player*/, ZoneInstance& /*zone*/) {}

// Java PvPZone.java:31-54 (the lambda: a task pinning this zone handler, the dying player and the zone instance)
bool PvPZone::onDie(Creature& lastAttacker, Creature& target, ZoneInstance& zone) {
	runtime::Ptr<Player> player = runtime::as<Player>(target);
	if (player == nullptr) // Java: !(target instanceof Player player)
		return false;
	if (runtime::as<PvPZoneInstance>(zone) != nullptr) { // Java: zone instanceof PvPZoneInstance
		zone.forEach([&player, &lastAttacker](Creature& creature) {
			if (runtime::Ptr<Player> p = runtime::as<Player>(creature)) {
				if (p.get() == player.get()) // Java: p.equals(player), identity
					PacketSendUtility::sendPacket(*p, SM_SYSTEM_MESSAGE::STR_MSG_PvPZONE_MY_DEATH_TO_B(lastAttacker.getName()));
				else if (static_cast<Creature*>(p.get()) == &lastAttacker) // Java: p.equals(lastAttacker), identity
					PacketSendUtility::sendPacket(*p, SM_SYSTEM_MESSAGE::STR_MSG_PvPZONE_HOSTILE_DEATH_TO_ME(player->getName()));
				else
					PacketSendUtility::sendPacket(*p, SM_SYSTEM_MESSAGE::STR_MSG_PvPZONE_HOSTILE_DEATH_TO_B(lastAttacker.getName(), player->getName()));
			}
		});
		Player& dying = *player;
		player->getController().addTask(TaskId::TELEPORT, ThreadPoolManager::getInstance().schedule({this, &dying, &zone}, [this, &dying, &zone] {
			dying.getController().getAndRemoveTask(TaskId::TELEPORT); // remove manually as it won't get removed automatically
			PlayerReviveService::duelRevive(dying);
			doTeleport(dying, zone.getZoneTemplate()->getName());
			PacketSendUtility::broadcastToZone(zone, SM_SYSTEM_MESSAGE::STR_PvPZONE_OUT_MESSAGE(dying.getName()));
		}, 5000));
	}
	return true;
}

} // namespace aion::gameserver::handlers::zone::pvpZones
