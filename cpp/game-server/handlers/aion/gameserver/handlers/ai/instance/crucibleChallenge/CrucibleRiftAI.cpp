#include "aion/gameserver/handlers/ai/instance/crucibleChallenge/CrucibleRiftAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::crucibleChallenge {

AION_AI(CrucibleRiftAI, "cruciblerift");

// Java CrucibleRiftAI.java:25-37
void CrucibleRiftAI::handleUseItemFinish(Player& player) {
	switch (getNpcId()) {
		case 730459:
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
			break;
		case 730460:
			TeleportService::teleportTo(player, 300320000, getPosition()->getInstanceId(), 1759.5004f, 1273.5414f, 389.11743f, int8_t{10});
			spawn(205679, 1765.522f, 1282.1051f, 389.11743f, int8_t{0});
			AIActions::deleteOwner(*this);
			break;
	}
}

// Java CrucibleRiftAI.java:39-45
void CrucibleRiftAI::handleSpawned() {
	ActionItemNpcAI::handleSpawned();
	if (getNpcId() == 730459) {
		announceRift();
	}
}

// Java CrucibleRiftAI.java:47-58
bool CrucibleRiftAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (dialogActionId == model::DialogAction::SETPRO1 && getNpcId() == 730459) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
		TeleportService::teleportTo(player, 300320000, getPosition()->getInstanceId(), 1759.5946f, 1768.6449f, 389.11758f, int8_t{16});
		spawn(218190, 1760.8701f, 1774.7711f, 389.11743f, int8_t{110});
		spawn(218185, 1762.6906f, 1773.863f, 389.11743f, int8_t{80});
		spawn(218191, 1763.9441f, 1775.2466f, 389.1175f, int8_t{80});
		AIActions::deleteOwner(*this);
	}
	return true;
}

// Java CrucibleRiftAI.java:60-62
void CrucibleRiftAI::announceRift() {
	getPosition()->getWorldMapInstance()->forEachPlayer([](Player& player) { PacketSendUtility::sendMonologue(player, 1111482); });
}

} // namespace aion::gameserver::handlers::ai::instance::crucibleChallenge
