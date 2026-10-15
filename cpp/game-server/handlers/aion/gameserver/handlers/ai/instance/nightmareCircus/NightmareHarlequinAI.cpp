#include "aion/gameserver/handlers/ai/instance/nightmareCircus/NightmareHarlequinAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/services/DialogService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

AION_AI(NightmareHarlequinAI, "nightmareharlequin");

// Java NightmareHarlequinAI.java:28-33
void NightmareHarlequinAI::handleDialogStart(Player& player) {
	// without an deny dialog we have to refrain from sending dialogs if the player is not allowed to talk to the npc. maybe it's invisible on retail?
	if (DialogService::isInteractionAllowed(player, getOwner()))
		GeneralNpcAI::handleDialogStart(player);
}

// Java NightmareHarlequinAI.java:35-49
bool NightmareHarlequinAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	const runtime::Ref<QuestEnv> env = QuestEnv::create(runtime::Ptr<VisibleObject>(getOwner()), player, questId, dialogActionId);
	env->setExtendedRewardIndex(extendedRewardIndex);
	if (QuestEngine::getInstance().onDialog(*env) && dialogActionId != model::DialogAction::SETPRO1) {
		return true;
	}
	if (dialogActionId == model::DialogAction::SETPRO1) {
		SkillEngine::getInstance().getSkill(getOwner(), player.getRace() == Race::ELYOS ? 21470 : 21472, 1, runtime::Ptr<VisibleObject>(player))
			->useWithoutPropSkill();
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
	} else if (dialogActionId == model::DialogAction::QUEST_SELECT && questId != 0) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 10, questId));
	}
	return true;
}

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
