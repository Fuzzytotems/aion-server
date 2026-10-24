#include "aion/gameserver/handlers/ai/quests/QuestStartItemNpcAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"

namespace aion::gameserver::handlers::ai::quests {

AION_AI(QuestStartItemNpcAI, "quest_start_use_item");

// Java QuestStartItemNpcAI.java:27-33
void QuestStartItemNpcAI::handleUseItemFinish(Player& player) {
	runtime::HashSet<int32_t>& relatedQuests = QuestEngine::getInstance().getQuestNpc(getOwner().getNpcId())->getOnQuestStart();
	if (!QuestEngine::getInstance().onDialog(*QuestEnv::create(runtime::Ptr<VisibleObject>(getOwner()), player, 0,
			relatedQuests.isEmpty() ? model::DialogAction::USE_OBJECT : model::DialogAction::QUEST_SELECT)))
		if (getObjectTemplate()->isDialogNpc()) // show default dialog
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

} // namespace aion::gameserver::handlers::ai::quests
