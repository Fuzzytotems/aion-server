#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/VaultKeeper.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/services/teleport/TeleportService.h"

namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault {

AION_AI(VaultKeeper, "IDSweep_Treasure_Room");

// Java VaultKeeper.java:31-35
bool VaultKeeper::onDialogSelect(Player& player, int32_t dialogActionId, int32_t /*questId*/, int32_t /*extendedRewardIndex*/) {
	checkEntryConditions(player, dialogActionId);
	return true;
}

// Java VaultKeeper.java:37-61 (synchronized; Java bug J-I6-1 ported: room counts up for a returning player too)
void VaultKeeper::checkEntryConditions(Player& player, int32_t dialogActionId) {
	SYNCHRONIZED(monitor) {
		if (dialogActionId == model::DialogAction::SETPRO1) {
			int32_t roomNo = room.get();

			if (playerAndRoom.containsKey(player.getObjectId())) {
				roomNo = *playerAndRoom.get(player.getObjectId());
			}

			if (roomNo == 0) {
				playerAndRoom.put(player.getObjectId(), 0);
				room = room.get() + 1;
				TeleportService::teleportTo(player, 301400000, player.getInstanceId(), 171.741f, 230.466f, 395.0f, int8_t{88}, TeleportAnimation::FADE_OUT_BEAM);
			} else if (roomNo == 1) {
				playerAndRoom.put(player.getObjectId(), 1);
				room = room.get() + 1;
				TeleportService::teleportTo(player, 301400000, player.getInstanceId(), 171.741f, 384.591f, 395.0f, int8_t{88}, TeleportAnimation::FADE_OUT_BEAM);
			} else if (roomNo == 2) {
				playerAndRoom.put(player.getObjectId(), 2);
				room = room.get() + 1;
				TeleportService::teleportTo(player, 301400000, player.getInstanceId(), 171.741f, 541.754f, 395.0f, int8_t{88}, TeleportAnimation::FADE_OUT_BEAM);
			} else {
				PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
			}
		}
	}
}

} // namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault
