#include "aion/gameserver/handlers/ai/instance/rentusBase/CollapsedReianBuildingAI.h"

#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

AION_AI(CollapsedReianBuildingAI, "collapsed_reian_building");

// Java CollapsedReianBuildingAI.java:18-22
void CollapsedReianBuildingAI::handleSpawned() {
	NpcAI::handleSpawned();
	SkillEngine::getInstance().getSkill(getOwner(), 20088, 60, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
}

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
