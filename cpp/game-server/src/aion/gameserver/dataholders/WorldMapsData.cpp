#include "aion/gameserver/dataholders/WorldMapsData.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/runtime/sched/ForkJoinPool.h"

namespace aion::gameserver::dataholders {

using model::templates::world::WorldMapTemplate;

void WorldMapsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const WorldMapTemplate& map : worldMaps) {
		if (mapsById.insert_or_assign(map.getMapId(), &map).second)
			mapIdsInOrder.push_back(map.getMapId());
	}
	mapsInOrder.clear();
	for (int32_t mapId : mapIdsInOrder)
		mapsInOrder.push_back(mapsById.at(mapId));
	// Java: worldMaps = null (the C++ index points into the storage, which stays)
}

std::vector<const WorldMapTemplate*>::const_iterator WorldMapsData::begin() const {
	return mapsInOrder.begin();
}

std::vector<const WorldMapTemplate*>::const_iterator WorldMapsData::end() const {
	return mapsInOrder.end();
}

void WorldMapsData::forEachParalllel(const std::function<void(const WorldMapTemplate&)>& consumer) const {
	runtime::ForkJoinPool::commonPool().parallelForEach(mapsInOrder, [&consumer](const WorldMapTemplate* map) { consumer(*map); });
}

int32_t WorldMapsData::size() const {
	return static_cast<int32_t>(mapsById.size());
}

const WorldMapTemplate* WorldMapsData::getTemplate(int32_t worldId) const {
	auto it = mapsById.find(worldId);
	return it != mapsById.end() ? it->second : nullptr;
}

int32_t WorldMapsData::getWorldIdByCName(std::string_view name) const {
	for (const WorldMapTemplate* template_ : mapsInOrder) {
		if (commons::utils::StringUtils::equalsIgnoreCase(template_->getCName(), name))
			return template_->getMapId();
	}
	return 0;
}

} // namespace aion::gameserver::dataholders
