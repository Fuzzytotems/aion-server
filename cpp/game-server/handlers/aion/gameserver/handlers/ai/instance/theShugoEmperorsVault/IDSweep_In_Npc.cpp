#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/IDSweep_In_Npc.h"

#include "aion/gameserver/ai/handler/TalkEventHandler.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/services/teleport/TeleportService.h"

namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault {

AION_AI(IDSweep_In_Npc, "idsweep_in_npc");

// Java IDSweep_In_Npc.java:27-31
void IDSweep_In_Npc::handleDialogStart(Player& player) {
	TalkEventHandler::onSimpleTalk(dynamic_cast<NpcAI&>(getOwner().getAi()), player); // Java: (NpcAI) getOwner().getAi()
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getOwner().getObjectId(), 1011));
}

// Java IDSweep_In_Npc.java:33-39
bool IDSweep_In_Npc::onDialogSelect(Player& player, int32_t dialogActionId, int32_t /*questId*/, int32_t /*extendedRewardIndex*/) {
	if (dialogActionId == model::DialogAction::TELEPORT_SIMPLE) {
		TeleportService::teleportTo(player, 301400000, player.getInstanceId(), 423.715f, 700.375f, 399.0f, int8_t{44}, TeleportAnimation::FADE_OUT_BEAM);
	}
	return true;
}

} // namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault
