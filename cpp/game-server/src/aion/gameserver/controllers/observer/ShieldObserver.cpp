#include "aion/gameserver/controllers/observer/ShieldObserver.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/templates/shield/ShieldTemplate.h"

namespace aion::gameserver::controllers::observer {

ShieldObserver::ShieldObserver(model::siege::FortressLocation& value, const model::templates::shield::ShieldTemplate* shieldValue,
	model::gameobjects::Creature& creatureValue)
	: ActionObserver(ObserverType{}), location(runtime::Ref<model::siege::FortressLocation>(value)),
	  creature(runtime::Ref<model::gameobjects::Creature>(creatureValue)), shield(shieldValue), oldPosition() {
	// Java: super(ObserverType.MOVE); WorldPosition lastPos; if (creature instanceof Player player && (lastPos =
	// player.getMoveController().getLastPositionFromClient()) != null) { this.oldPosition = new Point3D(lastPos.getX(), lastPos.getY(),
	// lastPos.getZ()); } else { this.oldPosition = new Point3D(creature.getX(), creature.getY(), creature.getZ()); }; super(...) arguments
	AION_UNPORTED();
}

runtime::Ref<ShieldObserver> ShieldObserver::create(model::siege::FortressLocation& value,
	const model::templates::shield::ShieldTemplate* shieldValue,
	model::gameobjects::Creature& creatureValue) {
	return runtime::makeRef<ShieldObserver>(value, shieldValue, creatureValue);
}

void ShieldObserver::moved() {
	AION_UNPORTED();
}

ShieldObserver::~ShieldObserver() = default;

} // namespace aion::gameserver::controllers::observer
