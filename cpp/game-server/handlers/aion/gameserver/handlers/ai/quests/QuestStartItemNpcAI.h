#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::quests {

/**
 * The AI of the objects that start quests ("quest_start_use_item"): the use is a quest dialog (the quest selection when the npc starts
 * quests); unhandled, a dialog npc opens its default page.
 * <p>
 * Java: data/handlers/ai/quests/QuestStartItemNpcAI.java, @AIName("quest_start_use_item") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class QuestStartItemNpcAI : public ActionItemNpcAI {
public:
	explicit QuestStartItemNpcAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::quests
