#include "aion/gameserver/handlers/ai/HiddenTeleportNpcAI.h"

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/FlyPathData.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"

namespace aion::gameserver::handlers::ai {

AION_AI(HiddenTeleportNpcAI, "hidden_teleporter");

// Java HiddenTeleportNpcAI.java:27-30
void HiddenTeleportNpcAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java HiddenTeleportNpcAI.java:32-38
bool HiddenTeleportNpcAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t /*questId*/, int32_t /*extendedRewardIndex*/) {
	if (dialogActionId == model::DialogAction::SETPRO1)
		teleport(player);
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
	return true;
}

// Java HiddenTeleportNpcAI.java:40-52
void HiddenTeleportNpcAI::teleport(Player& player) {
	int32_t teleId = getTeleportId();
	if (teleId == 0)
		return;
	const FlyPathEntry* flypath = DataManager::FLY_PATH->getPathTemplate(teleId);
	player.setCurrentFlypath(flypath);
	player.unsetPlayerMode(PlayerMode::RIDE);
	player.setState(CreatureState::FLYING);
	player.unsetState(CreatureState::ACTIVE);
	player.setFlightTeleportId(teleId * 1000 + 1);
	PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, EmotionType::START_FLYTELEPORT, teleId * 1000 + 1, 0), true);
}

// Java HiddenTeleportNpcAI.java:54-75
int32_t HiddenTeleportNpcAI::getTeleportId() {
	switch (getOwner().getNpcId()) {
		case 804811:
			return 279;
		case 804812:
			return 281;
		case 804813:
			return 280;
		case 804814:
			return 282;
		case 804822:
			return 286;
		case 804823:
			return 284;
		case 804824:
			return 283;
		case 804825:
			return 285;
		default:
			break;
	}
	return 0;
}

} // namespace aion::gameserver::handlers::ai
