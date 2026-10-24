#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::walkers {

/**
 * The AI of the walking runners ("generalrunner"): each arrival equips the weapon.
 * <p>
 * Java: data/handlers/ai/walkers/WalkGeneralRunnerAI.java, @AIName("generalrunner") (the marker is in the .cpp).
 *
 * @author Rolandas
 */
class WalkGeneralRunnerAI : public GeneralNpcAI {
public:
	explicit WalkGeneralRunnerAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleMoveArrived() override;
};

} // namespace aion::gameserver::handlers::ai::walkers
