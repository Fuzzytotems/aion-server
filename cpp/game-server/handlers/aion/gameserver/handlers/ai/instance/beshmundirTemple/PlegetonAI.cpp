#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/PlegetonAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

AION_AI(PlegetonAI, "plegeton");

// Java PlegetonAI.java:28-69
bool PlegetonAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	using network::aion::serverpackets::SM_PLAY_MOVIE;
	if (dialogActionId == model::DialogAction::SETPRO1) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
		switch (getNpcId()) {
			case 799517:
				PacketSendUtility::sendPacket(player, SM_PLAY_MOVIE(false, 0, 0, 448, false));
				if (!isStartTimer.get()) {
					isStartTimer = true;
					sendTimer();
					runtime::Ref<Player> captured(player);
					ThreadPoolManager::getInstance().schedule({this}, [this, captured] {
						runtime::Ptr<Npc> npc = getPosition()->getWorldMapInstance()->getNpc(216586);
						if (npc != nullptr && !npc->isDead()) {
							npc->getController().delete_();
							PacketSendUtility::sendPacket(*captured, SM_QUEST_ACTION(0, 0));
							getPosition()->getWorldMapInstance()->setDoorState(467, true);
						}
					}, 420000);
				}
				TeleportService::teleportTo(player, 300170000, 958.45233f, 430.4892f, 219.80301f);
				break;
			case 799518:
				PacketSendUtility::sendPacket(player, SM_PLAY_MOVIE(false, 0, 0, 449, false));
				TeleportService::teleportTo(player, 300170000, 822.0199f, 465.1819f, 220.29918f);
				break;
			case 799519:
				PacketSendUtility::sendPacket(player, SM_PLAY_MOVIE(false, 0, 0, 450, false));
				TeleportService::teleportTo(player, 300170000, 777.1054f, 300.39005f, 219.89926f);
				break;
			case 799520:
				PacketSendUtility::sendPacket(player, SM_PLAY_MOVIE(false, 0, 0, 451, false));
				TeleportService::teleportTo(player, 300170000, 942.3092f, 270.91855f, 219.86185f);
				break;
		}
	}
	return true;
}

// Java PlegetonAI.java:71-73
void PlegetonAI::sendTimer() {
	PacketSendUtility::broadcastToMap(getOwner(), SM_QUEST_ACTION(0, 420));
}

// Java PlegetonAI.java:75-78
void PlegetonAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
