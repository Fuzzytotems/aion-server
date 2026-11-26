#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/model/templates/world/WeatherTable.xml.h"

namespace aion::gameserver::model::templates::world {

/**
 * Weathers of one map. Zone 0 is the whole map, zones 1..zone_count are the weather zones of the map.
 * <p>
 * Java com.aionemu.gameserver.model.templates.world.WeatherTable (reworked by upstream 374336b3b). C++: the indexes point into the bound
 * `entries`, which never move after afterUnmarshal; a table that was not unmarshalled has empty indexes (Java: null maps).
 *
 * @author Rolandas
 */
class WeatherTable : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/world/WeatherTable.xml.inc"
private:
	std::unordered_map<std::string, const WeatherEntry*> entriesByName;
	std::unordered_map<int32_t, const WeatherEntry*> entriesByCode;
	std::unordered_map<int32_t, std::vector<const WeatherEntry*>> entriesByZone;

public:
	// C++: move-only (the bound weathers hold unique_ptr phases); without this the copy constructor that std::vector<WeatherEntry> claims would
	// be chosen when MapWeatherData's vector relocates its tables, because the unordered_map members make the move constructor not noexcept
	WeatherTable() = default;
	WeatherTable(const WeatherTable&) = delete;
	WeatherTable& operator=(const WeatherTable&) = delete;
	WeatherTable(WeatherTable&&) = default;
	WeatherTable& operator=(WeatherTable&&) = default;

	/** @return the weather of the given name, nullptr (Java null) if there is none */
	const WeatherEntry* getEntry(std::string_view name) const;

	/**
	 * @return The weather with the given code, null if there is no such code in this world. 0 is clear sky.
	 */
	const WeatherEntry* getEntry(int32_t code) const;

	const std::vector<WeatherEntry>& getEntries() const { return entries; }

	/**
	 * @return Weathers which can be active in the given zone (0 = the whole map), in the order they are rolled in.
	 */
	std::vector<const WeatherEntry*> getEntriesForZone(int32_t zoneId) const;
};

} // namespace aion::gameserver::model::templates::world
