#include "aion/gameserver/services/WeatherService.h"

#include <vector>

#include "aion/commons/utils/Rnd.h"
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
#include "aion/gameserver/services/GameTimeService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/time/gametime/DayTime.h"
#include "aion/gameserver/utils/time/gametime/GameTime.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::services {

namespace {

using model::templates::world::WeatherEntry;
using model::templates::world::WeatherTable;
using WeatherArray = runtime::Array<const WeatherEntry*>;

/** Java: `new SM_WEATHER(weatherEntries)` reads the array while it writes; C++: the packet takes a span, so the slots are read once here */
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
	runtime::Ref<utils::time::gametime::GameTime> gameTime = GameTimeService::getInstance().getGameTime()->clone();
	for (const model::templates::world::WorldMapTemplate* worldMapTemplate : *dataholders::DataManager::WORLD_MAPS_DATA) {
		int32_t mapId = worldMapTemplate->getMapId();
		const WeatherTable* table = dataholders::DataManager::MAP_WEATHER_DATA->getWeather(mapId);
		if (table != nullptr) {
			runtime::Ref<WeatherArray> weatherEntries = WeatherArray::make(table->getZoneCount());
			worldZoneWeathers.put(mapId, weatherEntries);
			std::vector<const WeatherEntry*> entries = entriesOf(*weatherEntries);
			setNextWeather(mapId, entries, *gameTime);
		}
	}
}

void WeatherService::checkWeathersTime() {
	// Java: anonymous Runnable at WeatherService.java:53 (com.aionemu.gameserver.services.WeatherService$1), pin {this} (the immortal singleton)
	utils::ThreadPoolManager::getInstance().schedule({this}, [this] {
		runtime::Ref<utils::time::gametime::GameTime> weatherTime = GameTimeService::getInstance().getGameTime()->clone();
		for (auto [mapId, weatherEntries] : worldZoneWeathers.snapshot()) {
			// setNextWeather writes into worldZoneWeathers.get(mapId), this array; the span only carries the length (header request world-2 rejected)
			std::vector<const WeatherEntry*> entries = entriesOf(*weatherEntries);
			setNextWeather(mapId, entries, *weatherTime);
			int32_t worldId = mapId;
			network::aion::serverpackets::SM_WEATHER packet(entriesOf(*weatherEntries));
			utils::PacketSendUtility::broadcastToWorld(packet, [worldId](model::gameobjects::player::Player& p) {
				return p.isSpawned() && p.getWorldId() == worldId;
			});
		}
	}, commons::utils::Rnd::get(20000, 240000)); // change weather 20s to 4m after daytime change
}

void WeatherService::setNextWeather(int32_t mapId, std::span<const WeatherEntry* const> weatherEntries,
	utils::time::gametime::GameTime& createdTime) {
	const WeatherTable* table = dataholders::DataManager::MAP_WEATHER_DATA->getWeather(mapId);
	// Java passes the map's WeatherEntry[] and writes its elements; the span is read-only, so the elements are written to the array the span was
	// read from, worldZoneWeathers.get(mapId): every caller of this private method passes that array (header request world-2 rejected)
	runtime::Ptr<WeatherArray> array = worldZoneWeathers.get(mapId);
	SYNCHRONIZED(*array) {
		for (int32_t zoneId = 1; zoneId <= static_cast<int32_t>(weatherEntries.size()); zoneId++) {
			int32_t index = zoneId - 1;
			(*array)[index].set(nextWeather(array->get(index), table, zoneId, createdTime));
		}
	}
}

const WeatherEntry* WeatherService::nextWeather(const WeatherEntry* oldEntry, const WeatherTable* table, int32_t zoneId,
	utils::time::gametime::GameTime& time) {
	const WeatherEntry* nextWeather = table->getWeatherAfter(oldEntry);
	return nextWeather == nullptr ? getRandomWeather(time, table, zoneId) : nextWeather;
}

const WeatherEntry* WeatherService::getRandomWeather(utils::time::gametime::GameTime& time, const WeatherTable* table, int32_t zoneId) {
	std::vector<const WeatherEntry*> weathers = table->getWeathersForZone(zoneId);

	int32_t rankChance = commons::utils::Rnd::get(0, 6);
	// rank 2 occurs twice often than rank 1
	// rank 1 occurs twice often than rank 0
	int32_t rank;
	if (rankChance == 0)
		rank = 0;
	else if (rankChance <= 2)
		rank = 1;
	else
		rank = 2;

	bool canSnow = time.getMonth() <= 3 || time.getMonth() >= 11;
	std::vector<const WeatherEntry*> possibleWeathers;
	while (rank >= 0) {
		for (const WeatherEntry* entry : weathers) {
			if (entry->getRank() == -1)
				return entry; // constant weather, maybe completely random ?

			if (entry->getRank() == rank && checkSnowCondition(canSnow, entry))
				possibleWeathers.push_back(entry);
		}
		if (possibleWeathers.size() > 0) {
			rank = -1;
			break;
		}
		rank--;
	}

	const WeatherEntry* newWeather;
	if (possibleWeathers.empty()) {
		newWeather = &WeatherEntry::NONE;
	} else {
		// almost all weather types have after and before weathers, so chances to pick up are almost equal
		newWeather = *commons::utils::Rnd::get(possibleWeathers);
		// now find "before" weather if such exists
		if (!newWeather->isBefore()) {
			for (const WeatherEntry* entry : weathers) {
				if (newWeather->getWeatherName() == entry->getWeatherName() && entry->isBefore()) {
					newWeather = entry;
					break;
				}
			}
		}

		// now to be or not to be -- we don't want weather present every time :P
		// rank 2 is strongest to appear, rank 0 is the weakest
		int32_t dayTimeCorrection = 1;
		if (time.getDayTime() == utils::time::gametime::DayTime::AFTERNOON && !canSnow)
			dayTimeCorrection *= 2; // sunny days more often :)
		float chance = commons::utils::Rnd::chance();
		// Java: int divisions (33 / dayTimeCorrection) compared with a float
		if ((newWeather->getRank() == 0 && chance >= static_cast<float>(33 / dayTimeCorrection)) ||
			(newWeather->getRank() == 1 && chance >= static_cast<float>(50 / dayTimeCorrection)) ||
			(newWeather->getRank() == 2 && chance >= static_cast<float>(66 / dayTimeCorrection)))
			newWeather = &WeatherEntry::NONE;
	}
	return newWeather;
}

bool WeatherService::checkSnowCondition(bool canSnow, const WeatherEntry* entry) {
	// Java: entry.getWeatherName() != null (C++: an absent name is empty)
	if (!canSnow && !entry->getWeatherName().empty()) {
		if (entry->getWeatherName() == "SNOW" || entry->getWeatherName() == "SNOW_BEACH")
			return false; // ALTGARD_SNOW (Altgard) and SNOW_x_WZ0x (Beluslan) are always valid
	}
	return true;
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
	const WeatherTable* table = dataholders::DataManager::MAP_WEATHER_DATA->getWeather(mapId);
	runtime::Ref<utils::time::gametime::GameTime> time = GameTimeService::getInstance().getGameTime()->clone();
	SYNCHRONIZED(*weatherEntries) {
		for (int32_t zoneId = 1; zoneId <= weatherEntries->length(); zoneId++) {
			int32_t index = zoneId - 1;
			if (weatherCode == -1)
				(*weatherEntries)[index].set(nextWeather(weatherEntries->get(index), table, zoneId, *time));
			else
				(*weatherEntries)[index].set(getOrCreateWeatherEntry(zoneId, weatherCode, table));
		}
	}
	network::aion::serverpackets::SM_WEATHER packet(entriesOf(*weatherEntries));
	utils::PacketSendUtility::broadcastToWorld(packet, [mapId](model::gameobjects::player::Player& p) { return p.isSpawned() && p.getWorldId() == mapId; });
	return true;
}

const WeatherEntry* WeatherService::getOrCreateWeatherEntry(int32_t zoneId, int32_t weatherCode, const WeatherTable* table) {
	if (weatherCode == 0) // 0 means sunny aka. no weather
		return &WeatherEntry::NONE;
	for (const std::unique_ptr<WeatherEntry>& w : table->getZoneData()) {
		if (w->getZoneId() == zoneId && w->getCode() == weatherCode)
			return w.get();
	}
	// Java: new WeatherEntry(zoneId, weatherCode). Deviation: weather entries are immortal templates (`const WeatherEntry*` in the arrays and in
	// packets sent later), so the entry is never freed; only admin weather changes to a code the zone does not define create one
	return new WeatherEntry(zoneId, weatherCode);
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
	return &WeatherEntry::NONE;
}

const WeatherEntry* WeatherService::getWeatherEntry(int32_t mapId, int32_t weatherZoneId) {
	runtime::Ptr<WeatherArray> weatherEntries = worldZoneWeathers.get(mapId);
	if (!weatherEntries)
		return nullptr;
	return weatherZoneId <= 0 || weatherZoneId > weatherEntries->length() ? nullptr : weatherEntries->get(weatherZoneId - 1);
}

} // namespace aion::gameserver::services
