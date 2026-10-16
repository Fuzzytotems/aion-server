#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::quests {

/**
 * The AI of the quest npcs used like objects ("quest_use_npc"): the use is a talk, for a dialog npc.
 * <p>
 * Java: data/handlers/ai/quests/QuestUseNpcAI.java, @AIName("quest_use_npc") (the marker is in the .cpp).
 *
 * @author Majka
 */
class QuestUseNpcAI : public ActionItemNpcAI {
public:
	explicit QuestUseNpcAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::quests
