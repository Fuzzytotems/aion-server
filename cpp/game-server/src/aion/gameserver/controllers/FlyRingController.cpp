#include "aion/gameserver/controllers/FlyRingController.h"

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/observer/FlyRingObserver.h"
#include "aion/gameserver/model/flyring/FlyRing.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::controllers {

using model::gameobjects::player::Player;
using observer::FlyRingObserver;
using runtime::Ptr;
using runtime::Ref;

FlyRingController::FlyRingController() {
	observed.set(runtime::RcConcurrentHashMap<int32_t, Ref<FlyRingObserver>>::create(AION_LOCK_CLASS(FlyRingController::observed#stripe)));
}

FlyRingController::~FlyRingController() = default;

model::flyring::FlyRing& FlyRingController::getOwner() const {
	return static_cast<model::flyring::FlyRing&>(VisibleObjectController::getOwner());
}

void FlyRingController::see(model::gameobjects::VisibleObject& object) {
	if (Ptr<Player> p = runtime::as<Player>(object)) {
		Ref<FlyRingObserver> observer = FlyRingObserver::create(getOwner(), *p);
		p->getObserveController()->addObserver(*observer);
		observed->put(p->getObjectId(), observer);
	}
}

void FlyRingController::notSee(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation) {
	if (Ptr<Player> p = runtime::as<Player>(object)) {
		Ptr<FlyRingObserver> observer = observed->remove(p->getObjectId());
		// Java: removeObserver(null) is a no-op (ArrayList.remove(null) finds nothing)
		if (observer)
			p->getObserveController()->removeObserver(*observer);
	}
}

} // namespace aion::gameserver::controllers
