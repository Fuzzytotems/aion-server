#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

/**
 * The base of Eternal Bastion's aggressive npcs ("eternal_bastion_aggressive"): no loot or AP; the shatter mine (284685) draws no aggro.
 * <p>
 * Java: data/handlers/ai/instance/eternalBastion/EternalBastionAggressiveNpcAI.java, @AIName("eternal_bastion_aggressive")
 * (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class EternalBastionAggressiveNpcAI : public AggressiveNpcAI {
public:
	explicit EternalBastionAggressiveNpcAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool ask(AIQuestion question) override;

protected:
	void handleCreatureAggro(Creature& creature) override;
};

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
