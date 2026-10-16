#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::worlds {

/**
 * A blackened grave ("blackened_grave"): it does not think; its death leaves 284262 where it stood and deletes the grave.
 * <p>
 * Java: data/handlers/ai/worlds/BlackenedGraveAI.java, @AIName("blackened_grave") (the marker is in the .cpp).
 *
 * @author Tibald
 */
class BlackenedGraveAI : public GeneralNpcAI {
public:
	explicit BlackenedGraveAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::worlds
