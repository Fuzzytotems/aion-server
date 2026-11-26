#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/world/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * Java com.aionemu.gameserver.services.WeatherService.
 * <p>
 * C++ interim (upstream sync foundation, docs/deviations/P4-10.md): upstream 374336b3b replaced the weather data (weather_table.xml) and this
 * service with the retail schedule (services/weather/WeatherMap, WeatherZone), which wave 4 ports. The old random weather of 51d7a19cb cannot
 * run on the new data (ranks and before/after weathers are gone), so until then every zone has clear sky (nullptr, code 0) unless //weather set
 * picks a weather of the new table; there is no natural weather change. The public API of 51d7a19cb stays for its callers.
 *
 * @author ATracer, Kwazar, Rolandas
 */
class WeatherService : public runtime::Immortal {
private:
	/** the weather of zones 1..zone_count of each map with a weather table, nullptr for clear sky */
	runtime::HashMap<int32_t, runtime::Ref<runtime::Array<const model::templates::world::WeatherEntry*>>> worldZoneWeathers{
		AION_LOCK_CLASS(WeatherService::worldZoneWeathers)};
public:
	static WeatherService& getInstance(); // Java singleton
private:
	WeatherService();
public:
	/** Triggered on every day time change. Interim: no natural weather change (see the class comment) */
	void checkWeathersTime();

	void loadWeather(model::gameobjects::player::Player& player);
	/**
	 * Changes the weather to the given weather code on the specified map. -1 has a special meaning and would trigger a natural weather change
	 * (interim: it changes nothing). Each weather zone gets the weather of that code if it belongs to the zone, else clear sky.
	 */
	bool changeWeather(int32_t mapId, int32_t weatherCode);

	/** @return the weather of the creature's weather zone, nullptr for clear sky (51d7a19cb returned WeatherEntry.NONE, which is gone) */
	const model::templates::world::WeatherEntry* findWeatherEntry(model::gameobjects::Creature& creature);
	/** @return the weather of the zone, nullptr for clear sky or a zone the map does not have */
	const model::templates::world::WeatherEntry* getWeatherEntry(int32_t mapId, int32_t weatherZoneId);
};

} // namespace aion::gameserver::services
