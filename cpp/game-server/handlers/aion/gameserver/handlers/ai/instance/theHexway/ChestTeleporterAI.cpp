#include "aion/gameserver/handlers/ai/instance/theHexway/ChestTeleporterAI.h"

#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::handlers::ai::instance::theHexway {

AION_AI(ChestTeleporterAI, "chest_teleporter");

// Java ChestTeleporterAI.java:32-35
void ChestTeleporterAI::handleUseItemFinish(Player& player) {
	if (player.getWorldId() == 300700000)
		services::teleport::TeleportService::teleportTo(player, 300700000, 485.59f, 585.42f, 357.0f, int8_t{60});
}

// Java ChestTeleporterAI.java:38-51
void ChestTeleporterAI::handleCreatureMoved(Creature& creature) {
	runtime::Ptr<Player> player = runtime::as<Player>(runtime::Ptr<Creature>(creature));
	if (player == nullptr || creature.isDead() || !getOwner().canSee(runtime::Ptr<VisibleObject>(creature)) ||
		!getOwner().getPosition()->isMapRegionActive())
		return;
	SYNCHRONIZED(recievedShouts) {
		if (!recievedShouts.contains(creature.getObjectId())) {
			if (PositionUtil::isInRange(getOwner(), creature, 8) && GeoService::getInstance().canSee(getOwner(), creature)) {
				recievedShouts.add(creature.getObjectId());
				PacketSendUtility::sendPacket(*player,
					network::aion::serverpackets::SM_MESSAGE(getOwner(), "A mysterious orb of relocation is hidden in this chest", ChatType::NPC));
			}
		}
	}
}

} // namespace aion::gameserver::handlers::ai::instance::theHexway
