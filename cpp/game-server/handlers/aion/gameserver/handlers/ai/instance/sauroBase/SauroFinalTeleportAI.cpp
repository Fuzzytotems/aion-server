#include "aion/gameserver/handlers/ai/instance/sauroBase/SauroFinalTeleportAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::ai::instance::sauroBase {

AION_AI(SauroFinalTeleportAI, "saurohiddenpassage");

// Java SauroFinalTeleportAI.java:25-28
void SauroFinalTeleportAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java SauroFinalTeleportAI.java:30-34
bool SauroFinalTeleportAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	switchWay(player, dialogActionId);
	return true;
}

// Java SauroFinalTeleportAI.java:36-45
void SauroFinalTeleportAI::switchWay(Player& player, int32_t dialogActionId) {
	switch (dialogActionId) {
		case model::DialogAction::SELECT_BOSS_LEVEL3:
			checkKeys(player, 1);
			break;
		case model::DialogAction::SELECT_BOSS_LEVEL4:
			checkKeys(player, 2);
			break;
		default:
			break;
	}
}

// Java SauroFinalTeleportAI.java:47-59
void SauroFinalTeleportAI::checkKeys(Player& player, int64_t keyCount) {
	runtime::Ptr<Item> keys = player.getInventory().getFirstItemByItemId(185000179);
	int32_t portal = static_cast<int32_t>(730875 + keyCount);
	if (keys != nullptr && keys->getItemCount() >= keyCount) {
		spawn(portal, 127.5f, 432.8f, 151.0f, int8_t{119});
		player.getInventory().decreaseByItemId(185000179, keys->getItemCount());
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
		AIActions::deleteOwner(*this);
		PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDVritra_Base_DoorOpen_09());
	} else {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1352));
	}
}

} // namespace aion::gameserver::handlers::ai::instance::sauroBase
