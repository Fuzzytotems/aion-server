#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/MapWeatherData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.MapWeatherData.
 * <p>
 * C++: the index points into the bound `weatherData` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author Rolandas
 */
class MapWeatherData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/MapWeatherData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::world::WeatherTable*> mapWeather;

public:
	/** @return the weather table of the map, nullptr (Java null) if there is none */
	const model::templates::world::WeatherTable* getWeather(int32_t mapId) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
