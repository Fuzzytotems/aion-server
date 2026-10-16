#include "aion/gameserver/questEngine/task/checker/DestinationChecker.h"

#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::questEngine::task::checker {

DestinationChecker::DestinationChecker(gameserver::model::gameobjects::Creature& followerValue) : follower(followerValue) {
}

DestinationChecker::~DestinationChecker() = default;

} // namespace aion::gameserver::questEngine::task::checker
