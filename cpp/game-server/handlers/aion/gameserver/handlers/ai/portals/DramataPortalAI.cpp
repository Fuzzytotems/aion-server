#include "aion/gameserver/handlers/ai/portals/DramataPortalAI.h"

#include <string>

#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::portals {

AION_AI(DramataPortalAI, "dramata_portal");

// Java DramataPortalAI.java:30-47
void DramataPortalAI::handleSpawned() {
	ActionItemNpcAI::handleSpawned();
	switch (getPosition()->getMapId()) {
		case 110070000:
			PacketSendUtility::broadcastToWorld(network::aion::serverpackets::SM_MESSAGE(getOwner(),
				"A dangerous rift is materializing inside [pos:Kaisinel's Academy;0 110070000 485.69 407.96 0.0 0]. Use it and destroy the source of this "
				"anomaly!",
				ChatType::BRIGHT_YELLOW_CENTER), [](Player& p) { return p.getLevel() >= 65 && p.getRace() == Race::ELYOS; });
			targetLocation = WorldPosition::create(220070000, 2928.98f, 919.95f, 35.289f, int8_t{94});
			break;
		case 120080000:
			PacketSendUtility::broadcastToWorld(network::aion::serverpackets::SM_MESSAGE(getOwner(),
				"A dangerous rift is materializing inside [pos:Marchutan Priory;1 120080000 392.58 231.57 0.0 0]. Use it and destroy the source of this "
				"anomaly!",
				ChatType::BRIGHT_YELLOW_CENTER), [](Player& p) { return p.getLevel() >= 65 && p.getRace() == Race::ASMODIANS; });
			targetLocation = WorldPosition::create(210050000, 141.7f, 2066.3f, 438.66f, int8_t{34});
			break;
	}
}

// Java DramataPortalAI.java:49-67
void DramataPortalAI::handleUseItemFinish(Player& player) {
	runtime::Ptr<WorldPosition> target = targetLocation.get();
	if (target == nullptr)
		return;
	if (player.getLevel() < 60) {
		PacketSendUtility::sendPacket(player,
			SM_SYSTEM_MESSAGE::STR_CANNOT_USE_DIRECT_PORTAL_LEVEL_LIMIT_COMMON(getOwner().getObjectTemplate()->getL10n()));
		return;
	}
	int64_t timeDelta = getOwner().getMillisSinceSpawn();
	if (timeDelta < 10 * 60 * 1000) { // Java: TimeUnit.MINUTES.toMillis(10)
		int64_t approxMin = 10 - timeDelta / 1000 / 60;
		PacketSendUtility::sendMessage(player,
			"Wait Daeva! The rift is not stable enough to enter. We estimate it is safe to use in about " + std::to_string(approxMin) + " minutes.");
		return;
	}
	TeleportService::teleportTo(player, *target);
	ActionItemNpcAI::handleUseItemFinish(player);
}

} // namespace aion::gameserver::handlers::ai::portals
