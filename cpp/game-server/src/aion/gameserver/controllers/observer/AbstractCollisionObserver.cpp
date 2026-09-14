#include "aion/gameserver/controllers/observer/AbstractCollisionObserver.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"
#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::controllers::observer {

AbstractCollisionObserver::AbstractCollisionObserver(model::gameobjects::Creature& creatureValue,
	runtime::Ptr<geoEngine::scene::Spatial> geometryValue, int8_t intentionsValue, CheckType checkTypeValue)
	: ActionObserver(ObserverType::MOVE_OR_DIE), creature(runtime::Ref<model::gameobjects::Creature>(creatureValue)), geometry(geometryValue),
	  intentions(intentionsValue), checkType(checkTypeValue) {
	// Java: oldPos = the player's last position from the client if known, else the creature's position
	AION_UNPORTED();
}

AbstractCollisionObserver::~AbstractCollisionObserver() = default;

// callbacks: com.aionemu.gameserver.controllers.observer.AbstractCollisionObserver$1 (execute(), fieldmap AbstractCollisionObserver_Runnable)
void AbstractCollisionObserver::moved() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::observer
