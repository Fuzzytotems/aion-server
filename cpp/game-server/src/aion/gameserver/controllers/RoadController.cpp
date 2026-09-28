#include "aion/gameserver/controllers/RoadController.h"

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/observer/RoadObserver.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/road/Road.h"

namespace aion::gameserver::controllers {

using model::gameobjects::player::Player;
using observer::RoadObserver;
using runtime::Ptr;
using runtime::Ref;

RoadController::RoadController() {
	observed.set(runtime::RcConcurrentHashMap<int32_t, Ref<RoadObserver>>::create(AION_LOCK_CLASS(RoadController::observed#stripe)));
}

RoadController::~RoadController() = default;

model::road::Road& RoadController::getOwner() const {
	return static_cast<model::road::Road&>(VisibleObjectController::getOwner());
}

void RoadController::see(model::gameobjects::VisibleObject& object) {
	if (Ptr<Player> p = runtime::as<Player>(object)) {
		Ref<RoadObserver> observer = RoadObserver::create(getOwner(), *p);
		p->getObserveController()->addObserver(*observer);
		observed->put(p->getObjectId(), observer);
	}
}

void RoadController::notSee(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation) {
	if (Ptr<Player> p = runtime::as<Player>(object)) {
		Ptr<RoadObserver> observer = observed->remove(p->getObjectId());
		// Java: removeObserver(null) is a no-op (ArrayList.remove(null) finds nothing)
		if (observer)
			p->getObserveController()->removeObserver(*observer);
	}
}

} // namespace aion::gameserver::controllers
