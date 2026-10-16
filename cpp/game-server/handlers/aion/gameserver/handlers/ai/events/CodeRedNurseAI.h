#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::events {

/**
 * The AI of the Code Red event nurses ("code_red_nurse"): the eight nurses talk as quest npcs and give their buff with the first choice;
 * other npcs open page 1011.
 * <p>
 * Java: data/handlers/ai/events/CodeRedNurseAI.java, @AIName("code_red_nurse") (the marker is in the .cpp).
 *
 * @author xTz, bobobear
 */
class CodeRedNurseAI : public GeneralNpcAI {
public:
	explicit CodeRedNurseAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::events
