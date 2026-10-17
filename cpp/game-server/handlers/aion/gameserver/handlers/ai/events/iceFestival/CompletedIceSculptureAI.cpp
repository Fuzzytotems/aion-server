#include "aion/gameserver/handlers/ai/events/iceFestival/CompletedIceSculptureAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::events::iceFestival {

AION_AI(CompletedIceSculptureAI, "icefestival_completed_ice_sculpture");

// Java CompletedIceSculptureAI.java:21-25
void CompletedIceSculptureAI::handleDialogStart(Player& player) {
	// custom dialog handling because the quest doesn't show up in the quest selection (incomplete quest data in the 4.8 game client)
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getOwner().getObjectId(), 1011));
}

// Java CompletedIceSculptureAI.java:27-36
bool CompletedIceSculptureAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	switch (dialogActionId) {
		case model::DialogAction::SETPRO1:
			// world_event_icebuff_drop
			SkillEngine::getInstance().getSkill(getOwner(), 22719, 1, runtime::Ptr<VisibleObject>(player))->useWithoutPropSkill();
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getOwner().getObjectId(), 0));
			return true;
	}
	return false;
}

} // namespace aion::gameserver::handlers::ai::events::iceFestival
