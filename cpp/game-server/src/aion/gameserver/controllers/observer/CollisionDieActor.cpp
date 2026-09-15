#include "aion/gameserver/controllers/observer/CollisionDieActor.h"

#include <optional>
#include <string>

#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntention.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntentionInfo.h"
#include "aion/gameserver/geoEngine/collision/CollisionResult.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/scene/Geometry.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::controllers::observer {

using model::gameobjects::Creature;
using model::gameobjects::player::Player;
using runtime::Ptr;

CollisionDieActor::CollisionDieActor(model::gameobjects::Creature& creatureValue, runtime::Ptr<geoEngine::scene::Spatial> geometryValue,
	model::siege::FortressLocation& fortressLocationValue)
	: AbstractCollisionObserver(creatureValue, geometryValue, geoEngine::collision::getId(geoEngine::collision::CollisionIntention::MATERIAL),
		  CheckType::PASS),
	  fortressLocation(fortressLocationValue) {
}

CollisionDieActor::~CollisionDieActor() = default;

runtime::Ref<CollisionDieActor> CollisionDieActor::create(model::gameobjects::Creature& creatureValue,
	runtime::Ptr<geoEngine::scene::Spatial> geometryValue, model::siege::FortressLocation& fortressLocationValue) {
	return runtime::makeRef<CollisionDieActor>(creatureValue, geometryValue, fortressLocationValue);
}

void CollisionDieActor::onMoved(geoEngine::collision::CollisionResults& collisionResults) {
	if (collisionResults.size() != 0) {
		Ptr<Creature> observed = creature.get();
		if (Ptr<Player> player = runtime::as<Player>(observed); configs::main::GeoDataConfig::GEO_MATERIALS_SHOWDETAILS && player && player->isStaff()) {
			std::optional<geoEngine::collision::CollisionResult> result = collisionResults.getClosestCollision();
			utils::PacketSendUtility::sendMessage(*player, "Entered " + result.value().getGeometry()->getName());
		}
		if (fortressLocation->isUnderShield() && fortressLocation->getRace() != detail::siegeRaceByRace(observed->getRace()))
			kill(*observed);
	}
}

void CollisionDieActor::kill(model::gameobjects::Creature& creature) {
	if (creature.getController().die()) {
		if (Ptr<Player> player = runtime::as<Player>(creature))
			services::player::PlayerReviveService::scheduleReviveAtBase(*player, 2500, 0);
	}
}

} // namespace aion::gameserver::controllers::observer
