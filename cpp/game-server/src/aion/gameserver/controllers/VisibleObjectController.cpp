#include "aion/gameserver/controllers/VisibleObjectController.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"

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
	// Java: empty. The port of LogoutBreakers (P5-00) adds `model::gameobjects::player::LogoutBreakers::onDelete(getOwner());` here; until then
	// no breaker runs (LogoutBreakers::onDelete is an unported noexcept stub and would terminate).
}

} // namespace aion::gameserver::controllers
