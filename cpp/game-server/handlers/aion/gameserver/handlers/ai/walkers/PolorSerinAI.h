#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/walkers/WalkGeneralRunnerAI.h"

namespace aion::gameserver::handlers::ai::walkers {

/**
 * The AI of Polor and Serin ("polorserin"): a runner that, with an adult (203129 or 203132) in its aggro range, arrives as a plain walker
 * and puts its weapon away.
 * <p>
 * Java: data/handlers/ai/walkers/PolorSerinAI.java, @AIName("polorserin") (the marker is in the .cpp).
 *
 * @author Rolandas
 */
class PolorSerinAI : public WalkGeneralRunnerAI {
public:
	explicit PolorSerinAI(Npc& owner) : WalkGeneralRunnerAI(owner) {}

protected:
	void handleMoveArrived() override;
};

} // namespace aion::gameserver::handlers::ai::walkers
