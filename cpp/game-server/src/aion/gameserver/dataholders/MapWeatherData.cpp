#include "aion/gameserver/dataholders/MapWeatherData.h"

namespace aion::gameserver::dataholders {

void MapWeatherData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::world::WeatherTable& table : weatherData)
		mapWeather.insert_or_assign(table.getMapId(), &table);
	// Java: weatherData = null (the C++ index points into the storage, which stays)
}

const model::templates::world::WeatherTable* MapWeatherData::getWeather(int32_t mapId) const {
	auto it = mapWeather.find(mapId);
	return it != mapWeather.end() ? it->second : nullptr;
}

int32_t MapWeatherData::size() const {
	return static_cast<int32_t>(mapWeather.size());
}

} // namespace aion::gameserver::dataholders
