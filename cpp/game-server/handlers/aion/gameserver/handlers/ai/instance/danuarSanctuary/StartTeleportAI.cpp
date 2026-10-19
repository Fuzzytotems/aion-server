#include "aion/gameserver/handlers/ai/instance/danuarSanctuary/StartTeleportAI.h"

#include "aion/gameserver/model/DialogPage.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/services/teleport/TeleportService.h"

namespace aion::gameserver::handlers::ai::instance::danuarSanctuary {

AION_AI(StartTeleportAI, "dsiportal");

// Java StartTeleportAI.java:24-27
void StartTeleportAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java StartTeleportAI.java:29-33
bool StartTeleportAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	switchWay(player);
	return true;
}

// Java StartTeleportAI.java:35-71
void StartTeleportAI::switchWay(Player& player) {
	runtime::Ptr<Item> key = nullptr;
	float x = 0;
	float y = 0;
	float z = 0;
	int8_t h = 0;
	switch (getOwner().getNpcId()) {
		case 701871:
			key = player.getInventory().getFirstItemByItemId(185000183);
			x = 1003.27f;
			y = 1371.26f;
			z = 338;
			h = 101;
			break;
		case 701872:
			key = player.getInventory().getFirstItemByItemId(185000182);
			x = 716.89f;
			y = 980.95f;
			z = 318.71f;
			h = 108;
			break;
		case 701873:
			key = player.getInventory().getFirstItemByItemId(185000181);
			x = 1014.81f;
			y = 275.87f;
			z = 310.47f;
			h = 0;
			break;
	}
	if (key != nullptr) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
		TeleportService::teleportTo(player, player.getWorldId(), player.getInstanceId(), x, y, z, h, TeleportAnimation::FADE_OUT_BEAM);
	} else {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), model::id(DialogPage::NO_RIGHT)));
	}
}

} // namespace aion::gameserver::handlers::ai::instance::danuarSanctuary
