#include "aion/gameserver/world/zone/InvasionZoneInstance.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/templates/zone/ZoneInfo.h"
#include "aion/gameserver/world/zone/handler/ZoneHandler.h"

namespace aion::gameserver::world::zone {

InvasionZoneInstance::InvasionZoneInstance(int32_t mapId, model::templates::zone::ZoneInfo& template_Value) : ZoneInstance(mapId, template_Value) {
}

runtime::Ref<InvasionZoneInstance> InvasionZoneInstance::create(int32_t value, model::templates::zone::ZoneInfo& template_Value) {
	return runtime::makeRef<InvasionZoneInstance>(value, template_Value);
}

InvasionZoneInstance::~InvasionZoneInstance() = default;

} // namespace aion::gameserver::world::zone
