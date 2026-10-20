#include "aion/gameserver/handlers/ai/instance/rakes/GeniesIncenseBurnerAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

AION_AI(GeniesIncenseBurnerAI, "geniesincenseburner");

// Java GeniesIncenseBurnerAI.java:20-24
void GeniesIncenseBurnerAI::handleUseItemFinish(Player& /*player*/) {
	AIActions::targetSelf(*this);
	AIActions::useSkill(*this, 18465);
}

} // namespace aion::gameserver::handlers::ai::instance::rakes
