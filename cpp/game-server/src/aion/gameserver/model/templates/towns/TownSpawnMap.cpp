#include "aion/gameserver/model/templates/towns/TownSpawnMap.h"

#include <utility>

#include "aion/gameserver/model/templates/detail/JavaHashMap.h"

namespace aion::gameserver::model::templates::towns {

void TownSpawnMap::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	townSpawnsData.clear();
	std::vector<std::pair<int32_t, const TownSpawn*>> puts;
	puts.reserve(townSpawns.size());
	for (const TownSpawn& entry : townSpawns) {
		townSpawnsData.insertOrAssign(entry.getTownId(), &entry);
		puts.emplace_back(entry.getTownId(), &entry);
	}
	townSpawnsDataValues = ::aion::gameserver::model::templates::detail::javaIntegerHashMapValues(puts);
}

const TownSpawn* TownSpawnMap::getTownSpawn(int32_t townId) const {
	const TownSpawn* const* entry = townSpawnsData.find(townId);
	return entry == nullptr ? nullptr : *entry;
}

} // namespace aion::gameserver::model::templates::towns
