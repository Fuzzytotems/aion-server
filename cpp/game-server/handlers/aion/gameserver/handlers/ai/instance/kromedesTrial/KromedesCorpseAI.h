#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::kromedesTrial {

/**
 * The AI of the corpse in Kromede's Trial ("krcorpse"): the dialog gives its skill item once, while the player has none.
 * <p>
 * Java: data/handlers/ai/instance/kromedesTrial/KromedesCorpseAI.java, @AIName("krcorpse") (the marker is in the .cpp).
 *
 * @author Gigi
 */
class KromedesCorpseAI : public NpcAI {
public:
	explicit KromedesCorpseAI(Npc& owner) : NpcAI(owner) {}

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

protected:
	void handleDialogStart(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::kromedesTrial
