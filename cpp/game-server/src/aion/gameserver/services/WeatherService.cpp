#include "aion/gameserver/services/WeatherService.h"

#include <vector>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MapWeatherData.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/world/WeatherEntry.h"
#include "aion/gameserver/model/templates/world/WeatherTable.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneClassName.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_WEATHER.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::services {

namespace {

using model::templates::world::WeatherEntry;
using model::templates::world::WeatherTable;
using WeatherArray = runtime::Array<const WeatherEntry*>;

/** the zone weathers in one read; SM_WEATHER takes a span */
std::vector<const WeatherEntry*> entriesOf(const WeatherArray& weatherEntries) {
	std::vector<const WeatherEntry*> entries;
	entries.reserve(static_cast<size_t>(weatherEntries.length()));
	for (const WeatherEntry* entry : weatherEntries)
		entries.push_back(entry);
	return entries;
}

} // namespace

WeatherService& WeatherService::getInstance() {
	static WeatherService instance; // Java singleton
	return instance;
}

WeatherService::WeatherService() {
	for (const model::templates::world::WorldMapTemplate* worldMapTemplate : *dataholders::DataManager::WORLD_MAPS_DATA) {
		int32_t mapId = worldMapTemplate->getMapId();
		const WeatherTable* table = dataholders::DataManager::MAP_WEATHER_DATA->getWeather(mapId);
		if (table != nullptr)
			worldZoneWeathers.put(mapId, WeatherArray::make(table->getZoneCount())); // clear sky in every zone (interim)
	}
}

void WeatherService::checkWeathersTime() {
	// interim: the retail schedule of upstream 374336b3b replaces the 51d7a19cb roll at each day time change (wave 4)
}

void WeatherService::loadWeather(model::gameobjects::player::Player& player) {
	runtime::Ptr<WeatherArray> weatherEntries = worldZoneWeathers.get(player.getWorldId());
	if (weatherEntries)
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_WEATHER(entriesOf(*weatherEntries)));
}

bool WeatherService::changeWeather(int32_t mapId, int32_t weatherCode) {
	runtime::Ptr<WeatherArray> weatherEntries = worldZoneWeathers.get(mapId);
	if (!weatherEntries)
		return false;
	if (weatherCode == -1)
		return true; // interim: no natural weather change
	const WeatherTable* table = dataholders::DataManager::MAP_WEATHER_DATA->getWeather(mapId);
	const WeatherEntry* weather = weatherCode == 0 ? nullptr : table->getEntry(weatherCode); // 0 means sunny aka. no weather
	SYNCHRONIZED(*weatherEntries) {
		for (int32_t zoneId = 1; zoneId <= weatherEntries->length(); zoneId++)
			(*weatherEntries)[zoneId - 1].set(weather != nullptr && weather->getZoneId() == zoneId ? weather : nullptr);
	}
	network::aion::serverpackets::SM_WEATHER packet(entriesOf(*weatherEntries));
	utils::PacketSendUtility::broadcastToWorld(packet, [mapId](model::gameobjects::player::Player& p) { return p.isSpawned() && p.getWorldId() == mapId; });
	return true;
}

const WeatherEntry* WeatherService::findWeatherEntry(model::gameobjects::Creature& creature) {
	for (runtime::Ptr<world::zone::ZoneInstance> regionZone : creature.findZones()) {
		if (regionZone->getZoneTemplate()->getZoneType() == model::templates::zone::ZoneClassName::WEATHER) {
			int32_t weatherZoneId = dataholders::DataManager::ZONE_DATA->getWeatherZoneId(*regionZone->getZoneTemplate());
			const WeatherEntry* weatherEntry = getWeatherEntry(creature.getWorldId(), weatherZoneId);
			if (weatherEntry != nullptr)
				return weatherEntry;
		}
	}
	return nullptr;
}

const WeatherEntry* WeatherService::getWeatherEntry(int32_t mapId, int32_t weatherZoneId) {
	runtime::Ptr<WeatherArray> weatherEntries = worldZoneWeathers.get(mapId);
	if (!weatherEntries)
		return nullptr;
	return weatherZoneId <= 0 || weatherZoneId > weatherEntries->length() ? nullptr : weatherEntries->get(weatherZoneId - 1);
}

} // namespace aion::gameserver::services
