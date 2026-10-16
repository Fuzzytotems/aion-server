#include "aion/gameserver/questEngine/task/checker/CoordinateDestinationChecker.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::questEngine::task::checker {

CoordinateDestinationChecker::CoordinateDestinationChecker(gameserver::model::gameobjects::Creature& followerValue, float xValue, float yValue,
	float zValue)
	: DestinationChecker(followerValue), x(xValue), y(yValue), z(zValue) {
}

CoordinateDestinationChecker::~CoordinateDestinationChecker() = default;

runtime::Ref<CoordinateDestinationChecker> CoordinateDestinationChecker::create(gameserver::model::gameobjects::Creature& follower, float x,
	float y, float z) {
	return runtime::makeRef<CoordinateDestinationChecker>(follower, x, y, z);
}

bool CoordinateDestinationChecker::check() {
	return utils::PositionUtil::isInRange(*follower, x, y, z, 20);
}

} // namespace aion::gameserver::questEngine::task::checker
