#include "aion/gameserver/world/zone/FlyZoneInstance.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/zone/ZoneInfo.h"
#include "aion/gameserver/model/templates/zone/ZoneType.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::world::zone {

using model::templates::zone::ZoneType;

FlyZoneInstance::FlyZoneInstance(int32_t mapIdValue, model::templates::zone::ZoneInfo& templateValue) : ZoneInstance(mapIdValue, templateValue) {
}

FlyZoneInstance::~FlyZoneInstance() = default;

runtime::Ref<FlyZoneInstance> FlyZoneInstance::create(int32_t mapIdValue, model::templates::zone::ZoneInfo& templateValue) {
	return runtime::makeRef<FlyZoneInstance>(mapIdValue, templateValue);
}

bool FlyZoneInstance::onEnter(model::gameobjects::Creature& creature) {
	SYNCHRONIZED(*this) {
		if (ZoneInstance::onEnter(creature)) {
			creature.setInsideZoneType(ZoneType::FLY);
			if (auto* player = dynamic_cast<model::gameobjects::player::Player*>(&creature)) {
				player->getController().onEnterFlyArea();
			}
			return true;
		} else {
			return false;
		}
	}
}

bool FlyZoneInstance::onLeave(model::gameobjects::Creature& creature) {
	SYNCHRONIZED(*this) {
		if (ZoneInstance::onLeave(creature)) {
			creature.unsetInsideZoneType(ZoneType::FLY);
			if (!creature.isInsideZoneType(ZoneType::FLY)) {
				if (auto* player = dynamic_cast<model::gameobjects::player::Player*>(&creature))
					player->getController().onLeaveFlyArea();
			}
			return true;
		} else
			return false;
	}
}

} // namespace aion::gameserver::world::zone
