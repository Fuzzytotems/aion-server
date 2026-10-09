#include "aion/gameserver/handlers/ai/HouseSignAI.h"

#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::handlers::ai {

AION_AI(HouseSignAI, "housesign");

// Java HouseSignAI.java:22-29
bool HouseSignAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	static_cast<void>(questId);
	static_cast<void>(extendedRewardIndex);
	model::DialogPage page = model::getByActionId(dialogActionId);
	if (page == model::DialogPage::NULL_)
		return false;
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_DIALOG_WINDOW(getOwner().getObjectId(), id(page)));
	return true;
}

} // namespace aion::gameserver::handlers::ai
