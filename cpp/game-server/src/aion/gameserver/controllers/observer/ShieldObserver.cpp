#include "aion/gameserver/controllers/observer/ShieldObserver.h"

#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/controllers/observer/CollisionDieActor.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/geometry/Point3D.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/templates/shield/ShieldPoint.h"
#include "aion/gameserver/model/templates/shield/ShieldTemplate.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::controllers::observer {

using model::gameobjects::Creature;
using model::gameobjects::player::Player;
using runtime::Ptr;
using runtime::Ref;
using utils::PositionUtil;

namespace {

/** Java constructor: oldPosition = the player's last position from the client if known, else the creature's position */
Ref<model::geometry::Point3D> initialPosition(Creature& creature) {
	Ptr<world::WorldPosition> lastPos;
	if (Ptr<Player> player = runtime::as<Player>(creature); player && (lastPos = player->getMoveController()->getLastPositionFromClient())) {
		return model::geometry::Point3D::create(lastPos->getX(), lastPos->getY(), lastPos->getZ());
	} else {
		return model::geometry::Point3D::create(creature.getX(), creature.getY(), creature.getZ());
	}
}

} // namespace

ShieldObserver::ShieldObserver(model::siege::FortressLocation& value, const model::templates::shield::ShieldTemplate* shieldValue,
	model::gameobjects::Creature& creatureValue)
	: ActionObserver(ObserverType::MOVE), location(Ref<model::siege::FortressLocation>(value)), creature(Ref<Creature>(creatureValue)),
	  shield(shieldValue), oldPosition(initialPosition(creatureValue)) {
}

runtime::Ref<ShieldObserver> ShieldObserver::create(model::siege::FortressLocation& value,
	const model::templates::shield::ShieldTemplate* shieldValue,
	model::gameobjects::Creature& creatureValue) {
	return runtime::makeRef<ShieldObserver>(value, shieldValue, creatureValue);
}

void ShieldObserver::moved() {
	const model::templates::shield::ShieldPoint* shieldCenter = shield->getCenter();
	bool passedThrough = false;
	// only collide with upper half of sphere
	if (location->isUnderShield() && !(creature->getZ() < shieldCenter->getZ() && oldPosition->getZ() < shieldCenter->getZ())) {
		bool wasInside = PositionUtil::isInRange(oldPosition->getX(), oldPosition->getY(), oldPosition->getZ(), shieldCenter->getX(), shieldCenter->getY(),
			shieldCenter->getZ(), shield->getRadius());
		bool isInside = PositionUtil::isInRange(*creature, shieldCenter->getX(), shieldCenter->getY(), shieldCenter->getZ(), shield->getRadius());
		passedThrough = wasInside != isInside;
	}

	if (passedThrough) {
		CollisionDieActor::kill(*creature);
	} else {
		SYNCHRONIZED(*oldPosition) {
			oldPosition->setX(creature->getX());
			oldPosition->setY(creature->getY());
			oldPosition->setZ(creature->getZ());
		}
	}
}

ShieldObserver::~ShieldObserver() = default;

} // namespace aion::gameserver::controllers::observer
