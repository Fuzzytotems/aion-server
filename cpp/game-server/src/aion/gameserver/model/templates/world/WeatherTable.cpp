#include "aion/gameserver/model/templates/world/WeatherTable.h"

#include <memory>

namespace aion::gameserver::model::templates::world {

const WeatherEntry* WeatherTable::getWeatherAfter(const WeatherEntry* entry) const {
	// Java: entry.getWeatherName() == null; an absent name is the empty string (the data has no present-empty name)
	if (entry == nullptr || entry->getWeatherName().empty() || entry->isAfter())
		return nullptr;
	for (const std::unique_ptr<WeatherEntry>& we : getZoneData()) {
		if (we->getZoneId() != entry->getZoneId())
			continue;
		if (entry->getWeatherName() == we->getWeatherName()) {
			if (entry->isBefore() && !we->isBefore() && !we->isAfter())
				return we.get();
			else if (!entry->isBefore() && !entry->isAfter() && we->isAfter())
				return we.get();
		}
	}
	return nullptr;
}

std::vector<const WeatherEntry*> WeatherTable::getWeathersForZone(int32_t zoneId) const {
	std::vector<const WeatherEntry*> result;
	for (const std::unique_ptr<WeatherEntry>& entry : getZoneData()) {
		if (entry->getZoneId() == zoneId)
			result.push_back(entry.get());
	}
	return result;
}

} // namespace aion::gameserver::model::templates::world
