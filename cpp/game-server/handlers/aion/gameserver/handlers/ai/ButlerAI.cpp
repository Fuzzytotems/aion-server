#include "aion/gameserver/handlers/ai/ButlerAI.h"

#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::handlers::ai {

AION_AI(ButlerAI, "butler");

// Java ButlerAI.java:24-27
bool ButlerAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	static_cast<void>(questId);
	static_cast<void>(extendedRewardIndex);
	return kickDialog(player, model::getByActionId(dialogActionId));
}

// Java ButlerAI.java:29-34
bool ButlerAI::kickDialog(Player& player, model::DialogPage page) {
	if (page == model::DialogPage::NULL_)
		return false;
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_DIALOG_WINDOW(getOwner().getObjectId(), id(page)));
	return true;
}

// Java ButlerAI.java:36-40
void ButlerAI::handleCreatureSee(Creature& creature) {
	if (const runtime::Ptr<Player> player = runtime::as<Player>(creature))
		if (const runtime::Ptr<model::house::House> house = runtime::as<model::house::House>(getCreator()))
			house->sendScripts(*player);
}

} // namespace aion::gameserver::handlers::ai
