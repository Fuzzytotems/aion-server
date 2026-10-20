#include "aion/gameserver/handlers/ai/walkers/WalkGeneralRunnerAI.h"

namespace aion::gameserver::handlers::ai::walkers {

AION_AI(WalkGeneralRunnerAI, "generalrunner");

// Java WalkGeneralRunnerAI.java:19-23
void WalkGeneralRunnerAI::handleMoveArrived() {
	GeneralNpcAI::handleMoveArrived();
	getOwner().setState(CreatureState::WEAPON_EQUIPPED);
}

} // namespace aion::gameserver::handlers::ai::walkers
