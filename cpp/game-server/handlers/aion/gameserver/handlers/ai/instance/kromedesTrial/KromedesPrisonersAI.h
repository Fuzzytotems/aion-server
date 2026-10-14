#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::kromedesTrial {

/**
 * The AI of the prisoners in Kromede's Trial ("krprisoners"): the first choice frees (removes) the prisoner.
 * <p>
 * Java: data/handlers/ai/instance/kromedesTrial/KromedesPrisonersAI.java, @AIName("krprisoners") (the marker is in the .cpp).
 *
 * @author Gigi
 */
class KromedesPrisonersAI : public NpcAI {
public:
	explicit KromedesPrisonersAI(Npc& owner) : NpcAI(owner) {}

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

protected:
	void handleDialogStart(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::kromedesTrial
