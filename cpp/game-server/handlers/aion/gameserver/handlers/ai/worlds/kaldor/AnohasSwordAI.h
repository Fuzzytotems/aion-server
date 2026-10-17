#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::worlds::kaldor {

/**
 * The AI of Anoha's sword in Kaldor ("anohas_sword"): the brigade general of the legion holding Anoha's fortress (7011) gets dialog 10, everyone
 * else 1011. Its first choice takes one 185000215 (dialog NO_RIGHT without it), tells the player and the world, and kills the sword, whose
 * decay task replays its death every 10 s instead; a map icon (702618) appears, and 30 minutes later Berserk Anoha (855263), while the sword
 * is deleted. It never respawns.
 * <p>
 * Java: data/handlers/ai/worlds/kaldor/AnohasSwordAI.java, @AIName("anohas_sword") (the marker is in the .cpp).
 *
 * @author Ritsu, Estrayl, Neon
 */
class AnohasSwordAI : public NpcAI {
public:
	explicit AnohasSwordAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::worlds::kaldor
