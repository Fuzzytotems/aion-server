#include "aion/gameserver/model/templates/world/WeatherTable.h"

#include <algorithm>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/model/templates/world/WeatherEntry.h"
#include "aion/gameserver/model/templates/world/WeatherPhase.h"

namespace aion::gameserver::model::templates::world {

// Java WeatherTable.java:44-69
void WeatherTable::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	// Java: if (entries == null) entries = new ArrayList<>() - the bound vector is already empty
	entriesByName.clear();
	entriesByCode.clear();
	entriesByZone.clear();
	for (size_t index = 0; index < entries.size(); index++) {
		const WeatherEntry& entry = entries[index];
		if (entry.getCode() != static_cast<int32_t>(index) + 1)
			ctx.fail("Weather " + entry.getName() + " of map " + std::to_string(mapId) + " has code " + std::to_string(entry.getCode()) +
				" instead of " + std::to_string(index + 1));
		if (entry.getZoneId() < 0 || entry.getZoneId() > zoneCount)
			ctx.fail("Weather " + entry.getName() + " of map " + std::to_string(mapId) + " has zone " + std::to_string(entry.getZoneId()) +
				" but the map has " + std::to_string(zoneCount));
		if (entry.getOccurProbability() < 0 || entry.getOccurProbability() > 100)
			ctx.fail("Weather " + entry.getName() + " of map " + std::to_string(mapId) + " has occur_prob " + std::to_string(entry.getOccurProbability()));
		if (!entriesByName.insert_or_assign(entry.getName(), &entry).second)
			ctx.fail("Duplicated weather name " + entry.getName() + " in map " + std::to_string(mapId));
	}
	for (WeatherEntry& entry : entries) {
		entry.resolveReferences(ctx, *this);
		entriesByCode.insert_or_assign(entry.getCode(), &entry);
		entriesByZone[entry.getZoneId()].push_back(&entry);
	}
	// Weathers are rolled in priority order; equal priorities keep the order of the world's file.
	for (auto& [zoneId, zoneEntries] : entriesByZone)
		std::ranges::stable_sort(zoneEntries, {}, &WeatherEntry::getPriority);
}

const WeatherEntry* WeatherTable::getEntry(std::string_view name) const {
	auto it = entriesByName.find(std::string(name));
	return it != entriesByName.end() ? it->second : nullptr;
}

const WeatherEntry* WeatherTable::getEntry(int32_t code) const {
	auto it = entriesByCode.find(code);
	return it != entriesByCode.end() ? it->second : nullptr;
}

std::vector<const WeatherEntry*> WeatherTable::getEntriesForZone(int32_t zoneId) const {
	auto it = entriesByZone.find(zoneId);
	return it != entriesByZone.end() ? it->second : std::vector<const WeatherEntry*>{};
}

} // namespace aion::gameserver::model::templates::world
