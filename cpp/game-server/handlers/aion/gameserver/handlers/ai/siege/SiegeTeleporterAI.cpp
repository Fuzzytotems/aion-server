#include "aion/gameserver/handlers/ai/siege/SiegeTeleporterAI.h"

#include <string>

#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORTRESS_INFO.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(SiegeTeleporterAI, "siege_teleporter");

// Java SiegeTeleporterAI.java:23-27
void SiegeTeleporterAI::handleDespawned() {
	canTeleport(false);
	GeneralNpcAI::handleDespawned();
}

// Java SiegeTeleporterAI.java:29-33
void SiegeTeleporterAI::handleDied() {
	canTeleport(false);
	GeneralNpcAI::handleDied();
}

// Java SiegeTeleporterAI.java:35-39
void SiegeTeleporterAI::handleSpawned() {
	canTeleport(true);
	GeneralNpcAI::handleSpawned();
}

// Java SiegeTeleporterAI.java:41-51 (Java's cast (SiegeNpc) getOwner() and its anonymous Consumer)
void SiegeTeleporterAI::canTeleport(bool status) {
	const int32_t id = runtime::cast<SiegeNpc>(getOwner())->getSiegeId();
	runtime::Ptr<model::siege::SiegeLocation> location = SiegeService::getInstance().getSiegeLocation(id);
	if (location == nullptr) // Java: getSiegeLocation(id).setCanTeleport(status) on null
		throw runtime::NullPointerException("SiegeLocation " + std::to_string(id));
	location->setCanTeleport(status);
	getPosition()->getWorldMapInstance()->forEachPlayer(
		[id, status](Player& player) { PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_FORTRESS_INFO(id, status)); });
}

} // namespace aion::gameserver::handlers::ai::siege
