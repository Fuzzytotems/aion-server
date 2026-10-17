#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

/**
 * The AI of Isbariya's servants ("isbariyaServants"): they go 10 s after their spawn, 281659 20 s.
 * <p>
 * Java: data/handlers/ai/instance/beshmundirTemple/IsbariyaServantsAI.java, @AIName("isbariyaServants") (the marker is in the .cpp).
 *
 * @author unknown
 */
class IsbariyaServantsAI : public AggressiveNpcAI {
public:
	explicit IsbariyaServantsAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void toDespawn(int32_t delay);
};

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
