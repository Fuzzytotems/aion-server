#include "aion/gameserver/dataholders/HouseBuildingData.h"

#include <string>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"

namespace aion::gameserver::dataholders {

void HouseBuildingData::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	// Java: if (buildings == null) return; (an empty list builds no index either)
	buildingById.clear();
	for (const model::templates::housing::Building& building : buildings) {
		if (!buildingById.try_emplace(building.getId(), &building).second) // Java: buildingById.put(...) != null
			ctx.fail("Duplicate building ID " + std::to_string(building.getId()));
	}
	// Java: buildings = null (the C++ index points into the storage, which stays)
}

const model::templates::housing::Building* HouseBuildingData::getBuilding(int32_t buildingId) const {
	auto it = buildingById.find(buildingId);
	return it != buildingById.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
