#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::events::iceFestival {

/**
 * The AI of Ishrunerk of the ice festival ("ishrunerk"): the dialog also selects the first of its five quests that answers; a quest
 * finished without reward closes the window.
 * <p>
 * Java: data/handlers/ai/events/iceFestival/IshrunerkAI.java, @AIName("ishrunerk") (the marker is in the .cpp).
 *
 * @author (none)
 */
class IshrunerkAI : public GeneralNpcAI {
public:
	explicit IshrunerkAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::events::iceFestival
