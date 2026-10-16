#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb {

/**
 * The AI of the Shugo Imperial Tomb's bonus ghosts ("shugo_tomb_ghost"): it does not think and runs; the bonus stages' 219505-219507 neither
 * give loot nor decay, the others do.
 * <p>
 * Java: data/handlers/ai/instance/shugoImperialTomb/ShugoTombGhostAI.java, @AIName("shugo_tomb_ghost") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class ShugoTombGhostAI : public AggressiveNpcAI {
public:
	explicit ShugoTombGhostAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleSpawned() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb
