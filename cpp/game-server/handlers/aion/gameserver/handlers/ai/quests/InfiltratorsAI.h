#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::quests {

/**
 * The AI of the quest infiltrators ("infiltrator"): each one's death spawns the next infiltrator of its chain at its spawn point (npc id
 * 0, Java's default, for the last ones).
 * <p>
 * Java: data/handlers/ai/quests/InfiltratorsAI.java, @AIName("infiltrator") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class InfiltratorsAI : public AggressiveNpcAI {
public:
	explicit InfiltratorsAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::quests
