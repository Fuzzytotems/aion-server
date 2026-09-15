#include "aion/gameserver/dataholders/WorldMapsData.h"

namespace aion::gameserver::dataholders {

void WorldMapsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::world::WorldMapTemplate& map : worldMaps)
		mapsById.insert_or_assign(map.getMapId(), &map);
	// Java: worldMaps = null (the C++ index points into the storage, which stays)
}

const model::templates::world::WorldMapTemplate* WorldMapsData::getTemplate(int32_t worldId) const {
	auto it = mapsById.find(worldId);
	return it != mapsById.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
