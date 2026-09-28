#include "aion/gameserver/controllers/observer/StartMovingListener.h"

#include "aion/gameserver/controllers/observer/ObserverType.h"

namespace aion::gameserver::controllers::observer {

StartMovingListener::StartMovingListener() : ActionObserver(ObserverType::MOVE) {
}

StartMovingListener::~StartMovingListener() = default;

runtime::Ref<StartMovingListener> StartMovingListener::create() {
	return runtime::makeRef<StartMovingListener>();
}

} // namespace aion::gameserver::controllers::observer
