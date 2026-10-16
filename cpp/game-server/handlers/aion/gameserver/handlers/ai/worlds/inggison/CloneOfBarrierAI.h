#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::worlds::inggison {

/**
 * The AI of Omega's barrier clone in Inggison ("omegaclone"): 3 s after its spawn it targets its creator Omega and casts 18671 on him; its
 * death takes 18671 off Omega again.
 * <p>
 * Java: data/handlers/ai/worlds/inggison/CloneOfBarrierAI.java, @AIName("omegaclone") (the marker is in the .cpp).
 *
 * @author Luzien
 */
class CloneOfBarrierAI : public AggressiveNpcAI {
public:
	explicit CloneOfBarrierAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::worlds::inggison
