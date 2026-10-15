#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

/**
 * The AI of the nightmare harlequins ("nightmareharlequin"): the dialog only where the player may interact; the quests first, then the
 * first choice gives the race's transformation and the quest choice opens the quest page.
 * <p>
 * Java: data/handlers/ai/instance/nightmareCircus/NightmareHarlequinAI.java, @AIName("nightmareharlequin") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class NightmareHarlequinAI : public GeneralNpcAI {
public:
	explicit NightmareHarlequinAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;
};

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
