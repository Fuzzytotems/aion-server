#include "aion/gameserver/model/templates/world/WeatherEntry.h"

namespace aion::gameserver::model::templates::world {

const WeatherEntry WeatherEntry::NONE{};

WeatherEntry::WeatherEntry(int32_t zoneIdValue, int32_t weatherCodeValue) : zoneId(zoneIdValue), weatherCode(weatherCodeValue) {
}

} // namespace aion::gameserver::model::templates::world
