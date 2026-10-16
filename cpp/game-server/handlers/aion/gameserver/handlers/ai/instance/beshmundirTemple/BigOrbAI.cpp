#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/BigOrbAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

AION_AI(BigOrbAI, "bigorb");

// Java BigOrbAI.java:22-29
void BigOrbAI::handleDialogStart(Player& player) {
	if (!isPortalSpawned()) { // Portal isn't spawned
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
	} else { // Portal is already spawned
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 10));
	}
}

// Java BigOrbAI.java:31-38
bool BigOrbAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (dialogActionId == model::DialogAction::SETPRO1) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
		spawn(730276, 1604.6683f, 1606.5886f, 306.8665f, int8_t{90});
	}
	return false;
}

// Java BigOrbAI.java:40-42
bool BigOrbAI::isPortalSpawned() {
	return !getPosition()->getWorldMapInstance()->getNpcs({730276}).empty();
}

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
