#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::quests {

/**
 * The AI of a quest object ("quest_use_item", 610 npc templates, 2,720 spots spawned at startup: grain sacks, eggs, cubes, ...): a click is
 * refused unless a quest registered for the object answers QuestEngine.onCanAct (ACTION_ITEM_USE: the quest is started and still needs what
 * the object drops); then ActionItemNpcAI's use bar runs, and at its end the object asks the quests with USE_OBJECT. When one takes it and the
 * object has quest drops, the drop is registered for the user (or the group or alliance members the quest drop names), the object dies by the
 * user and the loot window opens; when none takes it, a dialog object shows its default page 1011. Seeing a creature runs
 * CreatureEventHandler.onCreatureSee, so the quests' at-distance events reach a player who comes near the object.
 * <p>
 * Java: data/handlers/ai/quests/QuestItemNpcAI.java, @AIName("quest_use_item") (the marker is in the .cpp).
 *
 * @author xTz
 */
class QuestItemNpcAI : public ActionItemNpcAI {
public:
	explicit QuestItemNpcAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

	void handleUseItemFinish(Player& player) override;

	void handleCreatureSee(Creature& creature) override;
};

} // namespace aion::gameserver::handlers::ai::quests
