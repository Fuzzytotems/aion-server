#include "aion/gameserver/handlers/ai/instance/crucibleChallenge/ArbiterAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::crucibleChallenge {

AION_AI(ArbiterAI, "arbiter");

// Java ArbiterAI.java:23-30
void ArbiterAI::handleDialogStart(Player& player) {
	if (player.getInventory().getFirstItemByItemId(186000134) != nullptr) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
	} else {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
	}
}

// Java ArbiterAI.java:32-62
bool ArbiterAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	int32_t instanceId = getPosition()->getInstanceId();
	if (dialogActionId == model::DialogAction::SETPRO1 && player.getInventory().decreaseByItemId(186000134, 1)) {
		switch (getNpcId()) {
			case 205682:
				TeleportService::teleportTo(player, 300320000, instanceId, 357.10208f, 1662.702f, 95.9803f, int8_t{60});
				break;
			case 205683:
				TeleportService::teleportTo(player, 300320000, instanceId, 1796.5513f, 306.9967f, 469.25f, int8_t{60});
				break;
			case 205684:
				TeleportService::teleportTo(player, 300320000, instanceId, 1324.433f, 1738.2279f, 316.476f, int8_t{70});
				break;
			case 205663:
				TeleportService::teleportTo(player, 300320000, instanceId, 1270.8877f, 237.93307f, 405.38028f, int8_t{60});
				break;
			case 205686:
				TeleportService::teleportTo(player, 300320000, instanceId, 357.98798f, 349.19116f, 96.09108f, int8_t{60});
				break;
			case 205687:
				TeleportService::teleportTo(player, 300320000, instanceId, 1759.5004f, 1273.5414f, 389.11743f, int8_t{10});
				break;
			case 205685:
				TeleportService::teleportTo(player, 300320000, instanceId, 1283.1246f, 791.6683f, 436.6403f, int8_t{60});
				break;
		}
	}
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
	return true;
}

} // namespace aion::gameserver::handlers::ai::instance::crucibleChallenge
