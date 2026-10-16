#include "aion/gameserver/handlers/ai/quests/QuestUseNpcAI.h"

#include "aion/gameserver/ai/handler/TalkEventHandler.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"

namespace aion::gameserver::handlers::ai::quests {

AION_AI(QuestUseNpcAI, "quest_use_npc");

// Java QuestUseNpcAI.java:20-24
void QuestUseNpcAI::handleUseItemFinish(Player& player) {
	if (getObjectTemplate()->isDialogNpc())
		gameserver::ai::handler::TalkEventHandler::onTalk(*this, player);
}

} // namespace aion::gameserver::handlers::ai::quests
