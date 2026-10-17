#include "aion/gameserver/handlers/ai/instance/drakenspire/BeritraPortalAI.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(BeritraPortalAI, "beritra_portal");

// Java BeritraPortalAI.java:25-40
void BeritraPortalAI::handleUseItemFinish(Player& player) {
	switch (commons::utils::Rnd::get(1, 3)) {
		case 1:
			TeleportService::teleportTo(player, 301390000, 174.7f, 518.2f, 1749.6f, int8_t{59}, TeleportAnimation::FADE_OUT);
			break;
		case 2:
			TeleportService::teleportTo(player, 301390000, 173.4f, 517.9f, 1749.6f, int8_t{59}, TeleportAnimation::FADE_OUT);
			break;
		case 3:
			TeleportService::teleportTo(player, 301390000, 173.4f, 514.6f, 1749.6f, int8_t{59}, TeleportAnimation::FADE_OUT);
			break;
	}
	player.getController().startProtectionActiveTask();
	runtime::Ref<Player> captured(player);
	ThreadPoolManager::getInstance().schedule({this}, [captured] {
		PacketSendUtility::sendPacket(*captured, network::aion::serverpackets::SM_PLAY_MOVIE(false, 0, 0, 915, true));
	}, 1000);
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
