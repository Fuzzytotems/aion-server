#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/VershakaStatueAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(VershakaStatueAI, "vershakastatue");

// Java VershakaStatueAI.java:23-26
void VershakaStatueAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java VershakaStatueAI.java:28-35
bool VershakaStatueAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t /*questId*/, int32_t /*extendedRewardIndex*/) {
	if (dialogActionId == model::DialogAction::SETPRO1) {
		SkillEngine::getInstance().applyEffectDirectly(300, player, player);
	}
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
	return true;
}

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
