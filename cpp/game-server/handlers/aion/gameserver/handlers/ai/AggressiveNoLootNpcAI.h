#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of an aggressive npc that leaves no loot and no corpse ("aggressive_no_loot": instance mobs).
 * <p>
 * Java: data/handlers/ai/AggressiveNoLootNpcAI.java, @AIName("aggressive_no_loot").
 */
class AggressiveNoLootNpcAI : public AggressiveNpcAI {
public:
	explicit AggressiveNoLootNpcAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai
