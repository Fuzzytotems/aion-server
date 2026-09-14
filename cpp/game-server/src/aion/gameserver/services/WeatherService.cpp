#include "aion/gameserver/services/WeatherService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/templates/world/WeatherEntry.h"

namespace aion::gameserver::services {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous Runnable at WeatherService.java:53 (com.aionemu.gameserver.services.WeatherService$1); argument 1 of schedule(); storage: task

WeatherService& WeatherService::getInstance() {
	static WeatherService instance; // Java SingletonHolder
	return instance;
}

WeatherService::WeatherService() {
	AION_UNPORTED();
}

void WeatherService::checkWeathersTime() {
	AION_UNPORTED();
}

void WeatherService::setNextWeather(int32_t mapId, std::span<const model::templates::world::WeatherEntry* const> weatherEntries,
	utils::time::gametime::GameTime& createdTime) {
	AION_UNPORTED();
}

const model::templates::world::WeatherEntry* WeatherService::nextWeather(const model::templates::world::WeatherEntry* oldEntry,
	const model::templates::world::WeatherTable* table, int32_t zoneId, utils::time::gametime::GameTime& time) {
	AION_UNPORTED();
}

const model::templates::world::WeatherEntry* WeatherService::getRandomWeather(utils::time::gametime::GameTime& time,
	const model::templates::world::WeatherTable* table, int32_t zoneId) {
	AION_UNPORTED();
}

bool WeatherService::checkSnowCondition(bool canSnow, const model::templates::world::WeatherEntry* entry) {
	AION_UNPORTED();
}

void WeatherService::loadWeather(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool WeatherService::changeWeather(int32_t mapId, int32_t weatherCode) {
	AION_UNPORTED();
}

const model::templates::world::WeatherEntry* WeatherService::getOrCreateWeatherEntry(int32_t zoneId, int32_t weatherCode,
	const model::templates::world::WeatherTable* table) {
	AION_UNPORTED();
}

const model::templates::world::WeatherEntry* WeatherService::findWeatherEntry(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

const model::templates::world::WeatherEntry* WeatherService::getWeatherEntry(int32_t mapId, int32_t weatherZoneId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
