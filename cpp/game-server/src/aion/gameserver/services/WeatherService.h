#pragma once

#include <cstdint>
#include <span>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/world/fwd.h"
#include "aion/gameserver/services/fwd.h"
#include "aion/gameserver/utils/time/gametime/fwd.h"

namespace aion::gameserver::services {

/**
 * @author ATracer, Kwazar, Rolandas
 */
class WeatherService : public runtime::Immortal {
private:
	runtime::HashMap<int32_t, runtime::Ref<runtime::Array<const model::templates::world::WeatherEntry*>>> worldZoneWeathers{
		AION_LOCK_CLASS(WeatherService::worldZoneWeathers)};
public:
	static WeatherService& getInstance(); // Java singleton
private:
	WeatherService();
public:
	/** Triggered on every day time change (4x every two hours, since an in-game day equals 120 minutes) */
	void checkWeathersTime();
private:
	void setNextWeather(int32_t mapId, std::span<const model::templates::world::WeatherEntry* const> weatherEntries,
		utils::time::gametime::GameTime& createdTime);
	const model::templates::world::WeatherEntry* nextWeather(const model::templates::world::WeatherEntry* oldEntry,
		const model::templates::world::WeatherTable* table, int32_t zoneId, utils::time::gametime::GameTime& time);
	const model::templates::world::WeatherEntry* getRandomWeather(utils::time::gametime::GameTime& time,
		const model::templates::world::WeatherTable* table, int32_t zoneId);
	bool checkSnowCondition(bool canSnow, const model::templates::world::WeatherEntry* entry);
public:
	void loadWeather(model::gameobjects::player::Player& player);
	/**
	 * Changes the weather to the given weather code on the specified map. -1 has a special meaning and will trigger a natural weather change.
	 */
	bool changeWeather(int32_t mapId, int32_t weatherCode);
private:
	const model::templates::world::WeatherEntry* getOrCreateWeatherEntry(int32_t zoneId, int32_t weatherCode,
		const model::templates::world::WeatherTable* table);
public:
	const model::templates::world::WeatherEntry* findWeatherEntry(model::gameobjects::Creature& creature);
	const model::templates::world::WeatherEntry* getWeatherEntry(int32_t mapId, int32_t weatherZoneId);
};

} // namespace aion::gameserver::services
