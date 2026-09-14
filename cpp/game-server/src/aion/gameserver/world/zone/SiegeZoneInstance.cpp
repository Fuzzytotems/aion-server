#include "aion/gameserver/world/zone/SiegeZoneInstance.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/templates/zone/ZoneInfo.h"
#include "aion/gameserver/world/zone/handler/ZoneHandler.h"

namespace aion::gameserver::world::zone {

SiegeZoneInstance::SiegeZoneInstance(int32_t mapId, model::templates::zone::ZoneInfo& template_Value) : ZoneInstance(mapId, template_Value) {
}

runtime::Ref<SiegeZoneInstance> SiegeZoneInstance::create(int32_t value, model::templates::zone::ZoneInfo& template_Value) {
	return runtime::makeRef<SiegeZoneInstance>(value, template_Value);
}

SiegeZoneInstance::~SiegeZoneInstance() = default;

} // namespace aion::gameserver::world::zone
