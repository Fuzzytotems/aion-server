#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/world/WeatherTable.xml.h"

namespace aion::gameserver::model::templates::world {

/** Java com.aionemu.gameserver.model.templates.world.WeatherTable. @author Rolandas */
class WeatherTable : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/world/WeatherTable.xml.inc"
public:
	/**
	 * @return the entry that follows `entry` (the plain weather after a "before" weather, the "after" weather after a plain one, of the same name
	 *         and zone), nullptr (Java null) if there is none
	 */
	const WeatherEntry* getWeatherAfter(const WeatherEntry* entry) const;

	std::vector<const WeatherEntry*> getWeathersForZone(int32_t zoneId) const;
};

} // namespace aion::gameserver::model::templates::world
