#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::kromedesTrial {

/**
 * The AI of Magas in Kromede's Trial ("krmagas"): with a relic key, the first choice plays the potion movie (whose end teleports the
 * player: KromedesTrialInstance.onPlayMovieEnd).
 * <p>
 * Java: data/handlers/ai/instance/kromedesTrial/KromedesMagasAI.java, @AIName("krmagas") (the marker is in the .cpp).
 *
 * @author Gigi
 */
class KromedesMagasAI : public NpcAI {
public:
	explicit KromedesMagasAI(Npc& owner) : NpcAI(owner) {}

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

protected:
	void handleDialogStart(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::kromedesTrial
