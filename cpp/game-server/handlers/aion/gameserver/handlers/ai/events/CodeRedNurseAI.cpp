#include "aion/gameserver/handlers/ai/events/CodeRedNurseAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::events {

AION_AI(CodeRedNurseAI, "code_red_nurse");

// Java CodeRedNurseAI.java:26-43
void CodeRedNurseAI::handleDialogStart(Player& player) {
	switch (getNpcId()) {
		case 831435: // Jorpine (MON-THU)
		case 831436: // Yennu (MON-THU)
		case 831437: // Dalloren (FRI-SUN)
		case 831518: // Dalliea (FRI-SUN)
		case 831441: // Hylian (MON-THU)
		case 831442: // Rordah (MON-THU)
		case 831443: // Mazka (FRI-SUN)
		case 831524: // Desha (FRI-SUN)
			GeneralNpcAI::handleDialogStart(player);
			break;
		default:
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
			break;
	}
}

// Java CodeRedNurseAI.java:45-67
bool CodeRedNurseAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	runtime::Ref<QuestEnv> env = QuestEnv::create(runtime::Ptr<VisibleObject>(getOwner()), player, questId, dialogActionId);
	env->setExtendedRewardIndex(extendedRewardIndex);
	if (QuestEngine::getInstance().onDialog(*env) && dialogActionId != model::DialogAction::SETPRO1) {
		return true;
	}
	if (dialogActionId == model::DialogAction::SETPRO1) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(0, 0));
		int32_t skillId = 0;
		switch (getNpcId()) {
			case 831435:
			case 831441:
				skillId = 21280; // Jorpine / Hylian (MON-THU)
				break;
			case 831436:
			case 831442:
				skillId = 21281; // Yennu / Rordah (MON-THU)
				break;
			case 831437:
			case 831443:
				skillId = 21283; // Dalloren / Mazka (FRI-SUN)
				break;
			case 831518:
			case 831524:
				skillId = 21309; // Dalliea / Desha (FRI-SUN)
				break;
			default:
				skillId = 0;
				break;
		}
		if (skillId != 0)
			SkillEngine::getInstance().getSkill(getOwner(), skillId, 1, runtime::Ptr<VisibleObject>(player))->useWithoutPropSkill();
	} else if (dialogActionId == model::DialogAction::QUEST_SELECT && questId != 0) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 10, questId));
	}
	return true;
}

} // namespace aion::gameserver::handlers::ai::events
