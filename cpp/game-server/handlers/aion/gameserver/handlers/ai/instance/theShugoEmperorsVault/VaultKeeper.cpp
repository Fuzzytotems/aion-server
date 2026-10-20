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

// Java VaultKeeper.java:37-61 (synchronized). Correction of the Java code (owner's decision 2026-10-19, both branches; was J-I6-1): `room`
// counts up only for a new player; Java counted up in every branch, so a returning player took the next room from the next new player
void VaultKeeper::checkEntryConditions(Player& player, int32_t dialogActionId) {
	SYNCHRONIZED(monitor) {
		if (dialogActionId == model::DialogAction::SETPRO1) {
			int32_t roomNo = room.get();
			const bool newPlayer = !playerAndRoom.containsKey(player.getObjectId()); // the correction (was J-I6-1)

			if (!newPlayer) {
				roomNo = *playerAndRoom.get(player.getObjectId());
			}

			if (roomNo == 0) {
				playerAndRoom.put(player.getObjectId(), 0);
				if (newPlayer)
					room = room.get() + 1;
				TeleportService::teleportTo(player, 301400000, player.getInstanceId(), 171.741f, 230.466f, 395.0f, int8_t{88}, TeleportAnimation::FADE_OUT_BEAM);
			} else if (roomNo == 1) {
				playerAndRoom.put(player.getObjectId(), 1);
				if (newPlayer)
					room = room.get() + 1;
				TeleportService::teleportTo(player, 301400000, player.getInstanceId(), 171.741f, 384.591f, 395.0f, int8_t{88}, TeleportAnimation::FADE_OUT_BEAM);
			} else if (roomNo == 2) {
				playerAndRoom.put(player.getObjectId(), 2);
				if (newPlayer)
					room = room.get() + 1;
				TeleportService::teleportTo(player, 301400000, player.getInstanceId(), 171.741f, 541.754f, 395.0f, int8_t{88}, TeleportAnimation::FADE_OUT_BEAM);
			} else {
				PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
			}
		}
	}
}

} // namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault
