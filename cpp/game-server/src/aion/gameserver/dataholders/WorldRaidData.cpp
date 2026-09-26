#include "aion/gameserver/dataholders/WorldRaidData.h"

#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"

namespace aion::gameserver::dataholders {

void WorldRaidData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	detail::JavaHashMapOrder<int32_t, const model::templates::worldraid::WorldRaidLocation*> order;
	for (const model::templates::worldraid::WorldRaidLocation& location : worldRaidLocations)
		order.putIfAbsent(location.getLocationId(), &location, detail::javaHashCode(location.getLocationId()));
	locationsById = detail::toLinkedMap<decltype(locationsById)>(order);
	// Java: worldRaidLocations.clear(); worldRaidLocations = null (the C++ index points into the storage, which stays)
}

const model::templates::worldraid::WorldRaidLocation* WorldRaidData::getLocationsById(int32_t locationId) const {
	auto* location = locationsById.get(locationId);
	return location != nullptr ? *location : nullptr;
}

const detail::LinkedMap<int32_t, const model::templates::worldraid::WorldRaidLocation*>& WorldRaidData::getLocations() const {
	return locationsById;
}

int32_t WorldRaidData::size() const {
	return static_cast<int32_t>(locationsById.size());
}

} // namespace aion::gameserver::dataholders
