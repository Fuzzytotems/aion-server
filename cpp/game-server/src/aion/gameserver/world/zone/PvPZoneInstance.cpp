#include "aion/gameserver/world/zone/PvPZoneInstance.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/templates/zone/ZoneInfo.h"
#include "aion/gameserver/model/templates/zone/ZoneType.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::world::zone {

using model::templates::zone::ZoneType;

PvPZoneInstance::PvPZoneInstance(int32_t mapIdValue, model::templates::zone::ZoneInfo& templateValue) : ZoneInstance(mapIdValue, templateValue) {
}

PvPZoneInstance::~PvPZoneInstance() = default;

runtime::Ref<PvPZoneInstance> PvPZoneInstance::create(int32_t mapIdValue, model::templates::zone::ZoneInfo& templateValue) {
	return runtime::makeRef<PvPZoneInstance>(mapIdValue, templateValue);
}

bool PvPZoneInstance::onEnter(model::gameobjects::Creature& creature) {
	SYNCHRONIZED(*this) {
		if (ZoneInstance::onEnter(creature)) {
			creature.setInsideZoneType(ZoneType::PVP);
			return true;
		}
		return false;
	}
}

bool PvPZoneInstance::onLeave(model::gameobjects::Creature& creature) {
	SYNCHRONIZED(*this) {
		if (ZoneInstance::onLeave(creature)) {
			creature.unsetInsideZoneType(ZoneType::PVP);
			return true;
		}
		return false;
	}
}

} // namespace aion::gameserver::world::zone
