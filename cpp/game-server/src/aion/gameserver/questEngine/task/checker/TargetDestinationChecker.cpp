#include "aion/gameserver/questEngine/task/checker/TargetDestinationChecker.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::questEngine::task::checker {

TargetDestinationChecker::TargetDestinationChecker(gameserver::model::gameobjects::Creature& followerValue,
	gameserver::model::gameobjects::Creature& targetValue)
	: DestinationChecker(followerValue), target(targetValue) {
}

TargetDestinationChecker::~TargetDestinationChecker() = default;

runtime::Ref<TargetDestinationChecker> TargetDestinationChecker::create(gameserver::model::gameobjects::Creature& follower,
	gameserver::model::gameobjects::Creature& target) {
	return runtime::makeRef<TargetDestinationChecker>(follower, target);
}

bool TargetDestinationChecker::check() {
	return utils::PositionUtil::isInRange(*target, *follower, 20);
}

} // namespace aion::gameserver::questEngine::task::checker
