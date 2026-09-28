#include "aion/gameserver/world/zone/NoFlyZoneInstance.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/zone/ZoneInfo.h"
#include "aion/gameserver/model/templates/zone/ZoneType.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::world::zone {

using model::templates::zone::ZoneType;

NoFlyZoneInstance::NoFlyZoneInstance(int32_t mapIdValue, model::templates::zone::ZoneInfo& templateValue) : ZoneInstance(mapIdValue, templateValue) {
}

NoFlyZoneInstance::~NoFlyZoneInstance() = default;

runtime::Ref<NoFlyZoneInstance> NoFlyZoneInstance::create(int32_t mapIdValue, model::templates::zone::ZoneInfo& templateValue) {
	return runtime::makeRef<NoFlyZoneInstance>(mapIdValue, templateValue);
}

bool NoFlyZoneInstance::onEnter(model::gameobjects::Creature& creature) {
	SYNCHRONIZED(*this) {
		if (!ZoneInstance::onEnter(creature))
			return false;
		bool wasInNoFlyZone = creature.isInsideZoneType(ZoneType::NO_FLY);
		creature.setInsideZoneType(ZoneType::NO_FLY);
		if (!wasInNoFlyZone && creature.isInsideZoneType(ZoneType::FLY)) {
			if (auto* player = dynamic_cast<model::gameobjects::player::Player*>(&creature))
				player->getController().onLeaveFlyArea();
		}
		return true;
	}
}

bool NoFlyZoneInstance::onLeave(model::gameobjects::Creature& creature) {
	SYNCHRONIZED(*this) {
		if (!ZoneInstance::onLeave(creature))
			return false;
		creature.unsetInsideZoneType(ZoneType::NO_FLY);
		if (!creature.isInsideZoneType(ZoneType::NO_FLY) && creature.isInsideZoneType(ZoneType::FLY)) {
			if (auto* player = dynamic_cast<model::gameobjects::player::Player*>(&creature))
				player->getController().onEnterFlyArea();
		}
		return true;
	}
}

} // namespace aion::gameserver::world::zone
