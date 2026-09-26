#include "aion/gameserver/controllers/observer/AbstractCollisionObserver.h"

#include <cmath>

#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/math/Ray.h"
#include "aion/gameserver/geoEngine/models/GeoMap.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/BoundRadius.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::controllers::observer {

using geoEngine::math::Vector3f;
using model::gameobjects::Creature;
using model::gameobjects::player::Player;
using runtime::Ptr;

namespace {

/** Java: oldPos of the constructor: the player's last position from the client if known, else the creature's position */
Vector3f initialPosition(Creature& creature) {
	Ptr<world::WorldPosition> lastPos;
	if (Ptr<Player> player = runtime::as<Player>(creature); player && (lastPos = player->getMoveController()->getLastPositionFromClient()))
		return Vector3f(lastPos->getX(), lastPos->getY(), lastPos->getZ());
	return Vector3f(creature.getX(), creature.getY(), creature.getZ());
}

} // namespace

AbstractCollisionObserver::AbstractCollisionObserver(model::gameobjects::Creature& creatureValue,
	runtime::Ptr<geoEngine::scene::Spatial> geometryValue, int8_t intentionsValue, CheckType checkTypeValue)
	: ActionObserver(ObserverType::MOVE_OR_DIE), creature(runtime::Ref<model::gameobjects::Creature>(creatureValue)), oldPos(initialPosition(creatureValue)),
	  geometry(geometryValue), intentions(intentionsValue), checkType(checkTypeValue) {
}

AbstractCollisionObserver::~AbstractCollisionObserver() = default;

// Java: anonymous Runnable (fieldmap AbstractCollisionObserver_Runnable, capturing only this): a lambda pinned to this observer
void AbstractCollisionObserver::moved() {
	if (!isRunning.getAndSet(true)) {
		utils::ThreadPoolManager::getInstance().execute(this, [this] {
			auto running = runtime::finally([this]() noexcept { isRunning.set(false); });
			Ptr<Creature> observed = creature.get();
			Vector3f pos;
			Vector3f dir;
			if (checkType == CheckType::TOUCH) { // check if we are standing on the geometry (either top or bottom)
				float x = observed->getX();
				float y = observed->getY();
				float z = observed->getZ();
				float zMax = z + 0.05f + observed->getObjectTemplate()->getBoundRadius()->getUpper();
				float zMin = z - 0.11f;
				if (Ptr<Player> player = runtime::as<Player>(observed)) {
					if (player->getMoveController()->isJumping() || !player->isInGlidingState() && !observed->isFlying()) {
						float geoZ = world::geo::GeoService::getInstance().getZ(observed->getWorldId(), x, y, z, observed->getInstanceId());
						if (!std::isnan(geoZ)) {
							zMin = geoZ - 0.11f;
						}
					}
				}
				pos = Vector3f(x, y, zMax);
				dir = Vector3f(pos.getX(), pos.getY(), zMin);
			} else { // check if we passed the geometry (either entering or leaving)
				pos = Vector3f(observed->getX(), observed->getY(), observed->getZ() + geoEngine::models::GeoMap::COLLISION_CHECK_Z_OFFSET);
				dir = oldPos.get();
				dir.setZ(dir.getZ() + geoEngine::models::GeoMap::COLLISION_CHECK_Z_OFFSET);
			}
			float limit = pos.distance(dir);
			dir.subtractLocal(pos).normalizeLocal();
			geoEngine::math::Ray r(pos, dir);
			r.setLimit(limit);
			geoEngine::collision::CollisionResults results(intentions.get(), observed->getInstanceId(), true);
			geometry->collideWith(r, results);
			onMoved(results);
			oldPos = pos;
		});
	}
}

} // namespace aion::gameserver::controllers::observer
