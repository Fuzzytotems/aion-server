#include "aion/gameserver/handlers/ai/instance/crucibleChallenge/AdministratorArminosAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::handlers::ai::instance::crucibleChallenge {

AION_AI(AdministratorArminosAI, "administratorarminos");

// Java AdministratorArminosAI.java:24-27
void AdministratorArminosAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java AdministratorArminosAI.java:29-44
bool AdministratorArminosAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (dialogActionId == model::DialogAction::SETPRO1) {
		if (player.isInsideZone(ZoneName::get("ILLUSION_STADIUM_1_300320000"))) {
			spawn(217827, 1250.1598f, 237.97736f, 405.3968f, int8_t{0});
			spawn(217828, 1250.1598f, 239.97736f, 405.3968f, int8_t{0});
			spawn(217829, 1250.1598f, 235.97736f, 405.3968f, int8_t{0});
		} else if (player.isInsideZone(ZoneName::get("ILLUSION_STADIUM_6_300320000"))) {
			spawn(217827, 1265.9661f, 793.5348f, 436.64008f, int8_t{0});
			spawn(217828, 1265.9661f, 789.5348f, 436.6402f, int8_t{0});
			spawn(217829, 1265.9661f, 791.5348f, 436.64014f, int8_t{0});
		}
		AIActions::deleteOwner(*this);
	}
	return true;
}

} // namespace aion::gameserver::handlers::ai::instance::crucibleChallenge
