#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/Door3AI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

AION_AI(Door3AI, "door3");

// Java Door3AI.java:23-30
void Door3AI::handleDialogStart(Player& player) {
	if (player.getInventory().getItemCountByItemId(185000091) > 0) {
		ActionItemNpcAI::handleDialogStart(player);
	} else {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), model::id(model::DialogPage::NO_RIGHT)));
	}
}

// Java Door3AI.java:32-35
void Door3AI::handleUseItemFinish(Player& player) {
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
