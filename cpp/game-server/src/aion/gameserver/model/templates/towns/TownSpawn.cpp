#include "aion/gameserver/model/templates/towns/TownSpawn.h"

#include <utility>

#include "aion/gameserver/model/templates/detail/JavaHashMap.h"

namespace aion::gameserver::model::templates::towns {

void TownSpawn::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	townLevelsData.clear();
	std::vector<std::pair<int32_t, const TownLevel*>> puts;
	puts.reserve(townLevels.size());
	for (const TownLevel& entry : townLevels) {
		townLevelsData.insertOrAssign(entry.getLevel(), &entry);
		puts.emplace_back(entry.getLevel(), &entry);
	}
	townLevelsDataValues = ::aion::gameserver::model::templates::detail::javaIntegerHashMapValues(puts);
}

const TownLevel* TownSpawn::getSpawnsForLevel(int32_t level) const {
	const TownLevel* const* entry = townLevelsData.find(level);
	return entry == nullptr ? nullptr : *entry;
}

} // namespace aion::gameserver::model::templates::towns
