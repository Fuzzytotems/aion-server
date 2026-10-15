#include "aion/gameserver/handlers/ai/instance/linkgateFoundry/LinkgateFoundrySecretTeleportAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/services/teleport/TeleportService.h"

namespace aion::gameserver::handlers::ai::instance::linkgateFoundry {

AION_AI(LinkgateFoundrySecretTeleportAI, "linkgateFoundrySecretRoomTeleport");

// Java LinkgateFoundrySecretTeleportAI.java:25-28
void LinkgateFoundrySecretTeleportAI::handleUseItemFinish(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java LinkgateFoundrySecretTeleportAI.java:30-34
bool LinkgateFoundrySecretTeleportAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	switchFloor(player, dialogActionId);
	return true;
}

// Java LinkgateFoundrySecretTeleportAI.java:36-48
void LinkgateFoundrySecretTeleportAI::switchFloor(Player& player, int32_t dialogActionId) {
	switch (dialogActionId) {
		case model::DialogAction::SETPRO1:
			TeleportService::teleportTo(player, 301270000, 177.65f, 258.15f, 313.0f, int8_t{60}, TeleportAnimation::FADE_OUT_BEAM);
			break;
		case model::DialogAction::SETPRO2:
			TeleportService::teleportTo(player, 301270000, 176.11f, 258.44f, 354.0f, int8_t{60}, TeleportAnimation::FADE_OUT_BEAM);
			break;
		case model::DialogAction::SETPRO3:
			TeleportService::teleportTo(player, 301270000, 176.11f, 258.44f, 394.0f, int8_t{60}, TeleportAnimation::FADE_OUT_BEAM);
			break;
		default:
			break;
	}
}

} // namespace aion::gameserver::handlers::ai::instance::linkgateFoundry
