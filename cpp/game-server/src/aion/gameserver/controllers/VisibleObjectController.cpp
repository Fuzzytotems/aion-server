#include "aion/gameserver/controllers/VisibleObjectController.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/LogoutBreakers.h"

namespace aion::gameserver::controllers {

VisibleObjectController::VisibleObjectController() noexcept = default;

VisibleObjectController::~VisibleObjectController() = default;

void VisibleObjectController::setOwner(model::gameobjects::VisibleObject& value) {
	owner.set(&value);
	bindOwner(value);
}

bool VisibleObjectController::delete_() {
	AION_UNPORTED();
}

void VisibleObjectController::deleteAndScheduleRespawn() {
	AION_UNPORTED();
}

void VisibleObjectController::deleteIfAliveOrCancelRespawn() {
	AION_UNPORTED();
}

void VisibleObjectController::onBeforeSpawn() {
	AION_UNPORTED();
}

void VisibleObjectController::onDespawn() {
	AION_UNPORTED();
}

void VisibleObjectController::onDelete() {
	// Java: empty. C++ only: the delete breakers of the owner (LogoutBreakers.h class comment, runtime-architecture.md §5.3), the last statement
	// of every controller's onDelete chain (noexcept: a failing step is logged, P5-00 ports the steps).
	model::gameobjects::player::LogoutBreakers::onDelete(getOwner());
}

} // namespace aion::gameserver::controllers
