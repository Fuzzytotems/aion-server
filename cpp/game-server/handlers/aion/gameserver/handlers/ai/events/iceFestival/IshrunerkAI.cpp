#include "aion/gameserver/handlers/ai/events/iceFestival/IshrunerkAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"

namespace aion::gameserver::handlers::ai::events::iceFestival {

AION_AI(IshrunerkAI, "ishrunerk");

// Java IshrunerkAI.java:23-31
void IshrunerkAI::handleDialogStart(Player& player) {
	GeneralNpcAI::handleDialogStart(player);
	// custom dialog handling because the quests don't show up in the quest selection (incomplete quest data in the 4.8 game client)
	for (int32_t questId : {80715, 80716, 80717, 80718, 80720}) {
		runtime::Ref<QuestEnv> env = QuestEnv::create(runtime::Ptr<VisibleObject>(getOwner()), player, questId, model::DialogAction::QUEST_SELECT);
		if (QuestEngine::getInstance().onDialog(*env))
			break;
	}
}

// Java IshrunerkAI.java:33-43
bool IshrunerkAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (dialogActionId == model::DialogAction::SELECTED_QUEST_NOREWARD) {
		runtime::Ref<QuestEnv> env = QuestEnv::create(runtime::Ptr<VisibleObject>(getOwner()), player, questId, dialogActionId);
		env->setExtendedRewardIndex(extendedRewardIndex);
		if (QuestEngine::getInstance().onDialog(*env))
			PacketSendUtility::sendPacket(*env->getPlayer(), SM_DIALOG_WINDOW(getObjectId(), 0, questId)); // close window
		return true;
	}
	return GeneralNpcAI::onDialogSelect(player, dialogActionId, questId, extendedRewardIndex);
}

} // namespace aion::gameserver::handlers::ai::events::iceFestival
