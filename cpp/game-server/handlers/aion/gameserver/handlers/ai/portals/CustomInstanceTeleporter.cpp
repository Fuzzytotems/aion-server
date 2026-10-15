#include "aion/gameserver/handlers/ai/portals/CustomInstanceTeleporter.h"

#include "aion/gameserver/custom/instance/CustomInstanceService.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::ai::portals {

AION_AI(CustomInstanceTeleporter, "custom_instance_teleporter");

// Java CustomInstanceTeleporter.java:22-35
void CustomInstanceTeleporter::handleUseItemFinish(Player& player) {
	if (player.getLevel() < 65) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_ENTER_LEVEL());
		return;
	}
	if (!gameserver::custom::instance::CustomInstanceService::getInstance().canEnter(player.getObjectId())) {
		PacketSendUtility::sendMessage(player, "You have already done this instance for today. You can re-enter it after 9 AM.",
			ChatType::BRIGHT_YELLOW_CENTER);
		return;
	}
	gameserver::custom::instance::CustomInstanceService::getInstance().onEnter(player);
}

} // namespace aion::gameserver::handlers::ai::portals
