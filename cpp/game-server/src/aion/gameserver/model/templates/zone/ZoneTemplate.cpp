#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"

#include "aion/gameserver/world/zone/ZoneAttributesInfo.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::model::templates::zone {

const ::aion::gameserver::world::zone::ZoneName* ZoneTemplate::getZoneName() const {
	const ::aion::gameserver::world::zone::ZoneName* interned = zoneName.value.load(std::memory_order_acquire);
	if (interned == nullptr && !name.empty()) {
		interned = ::aion::gameserver::world::zone::ZoneName::createOrGet(name); // idempotent: every thread gets the same interned object
		zoneName.value.store(interned, std::memory_order_release);
	}
	return interned;
}

// Java ZoneTemplate.java:96-98 (upstream 4617c9fd2)
bool ZoneTemplate::hasZoneAttribute(::aion::gameserver::world::zone::ZoneAttributes attribute) const {
	return flags != -1 && (flags & ::aion::gameserver::world::zone::getId(attribute)) != 0;
}

} // namespace aion::gameserver::model::templates::zone
