#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/world/WeatherEntry.xml.h"

namespace aion::gameserver::model::templates::world {

/** Java com.aionemu.gameserver.model.templates.world.WeatherEntry. @author Rolandas */
class WeatherEntry : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/world/WeatherEntry.xml.inc"
private:
	WeatherEntry() = default; // Java: private WeatherEntry()

public:
	/** Java: public static final WeatherEntry NONE = new WeatherEntry() */
	static const WeatherEntry NONE;

	WeatherEntry(int32_t zoneId, int32_t weatherCode);
};

} // namespace aion::gameserver::model::templates::world
