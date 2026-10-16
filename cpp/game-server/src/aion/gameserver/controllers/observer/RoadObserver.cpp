#include "aion/gameserver/controllers/observer/RoadObserver.h"

#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/road/Road.h"
#include "aion/gameserver/model/templates/road/RoadExit.h"
#include "aion/gameserver/model/templates/road/RoadTemplate.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldType.h"

namespace aion::gameserver::controllers::observer {

using geoEngine::math::Vector3f;

RoadObserver::RoadObserver(model::road::Road& roadValue, model::gameobjects::player::Player& playerValue)
	: ActionObserver(ObserverType::MOVE), player(playerValue), road(roadValue) {
	oldPosition.set(Vector3f(playerValue.getX(), playerValue.getY(), playerValue.getZ()));
}

RoadObserver::~RoadObserver() = default;

runtime::Ref<RoadObserver> RoadObserver::create(model::road::Road& roadValue, model::gameobjects::player::Player& playerValue) {
	return runtime::makeRef<RoadObserver>(roadValue, playerValue);
}

void RoadObserver::moved() {
	Vector3f newPosition(player->getX(), player->getY(), player->getZ());
	if (road->isCrossed(oldPosition.get(), newPosition)) {
		const model::templates::road::RoadExit* exit = road->getTemplate()->getRoadExit();
		using model::animations::TeleportAnimation;
		using services::teleport::TeleportService;

		world::WorldType type = road->getWorldType();
		if (type == world::WorldType::ELYSEA) {
			if (player->getRace() == model::Race::ELYOS) {
				TeleportService::teleportTo(*player, exit->getMap(), exit->getX(), exit->getY(), exit->getZ(), int8_t{0}, TeleportAnimation::FADE_OUT_BEAM);
			}
		} else if (type == world::WorldType::ASMODAE) {
			if (player->getRace() == model::Race::ASMODIANS) {
				TeleportService::teleportTo(*player, exit->getMap(), exit->getX(), exit->getY(), exit->getZ(), int8_t{0}, TeleportAnimation::FADE_OUT_BEAM);
			}
		} else {
			TeleportService::teleportTo(*player, exit->getMap(), exit->getX(), exit->getY(), exit->getZ(), int8_t{0}, TeleportAnimation::FADE_OUT_BEAM);
		}
	}
	oldPosition = newPosition;
}

} // namespace aion::gameserver::controllers::observer
