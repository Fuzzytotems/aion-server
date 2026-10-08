#include "aion/gameserver/handlers/admincommands/Weather.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/model/templates/world/WeatherEntry.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/services/WeatherService.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Weather);

Weather::Weather()
	: AdminCommand("weather", "Shows/changes the weather.",
		  "<info> - Shows info for the weather in the current zone.\n"
		  "<next> - Triggers a natural weather change on this map.\n"
		  "<set> <code> - Changes the weather on this map, according to the weather code between 0 (default) and 12.\n") {
}

// Java Weather.java:26-78
void Weather::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(admin);
		return;
	}
	const std::string command = commons::utils::StringUtils::toLowerCase(params[0]); // parity= switch (params[0].toLowerCase()) {
	if (command == "info") { // parity= case "info":
		for (const runtime::Ptr<ZoneInstance>& regionZone : admin.findZones()) {
			if (regionZone->getZoneTemplate()->getZoneType() == ZoneClassName::WEATHER) {
				int32_t weatherZoneId = DataManager::ZONE_DATA->getWeatherZoneId(*regionZone->getZoneTemplate());
				const WeatherEntry* weatherEntry = WeatherService::getInstance().getWeatherEntry(admin.getWorldId(), weatherZoneId);
				if (weatherEntry != nullptr) {
					std::string info = "Weather for region " + regionZone->getZoneTemplate()->getXmlName() + ":";
					if (weatherEntry == &WeatherEntry::NONE) {
						info += "\n\tcode: " + std::to_string(weatherEntry->getCode()) + " (no weather)";
					} else {
						if (weatherEntry->getZoneId() > 0)
							info += "\n\tzone: " + std::to_string(weatherEntry->getZoneId());
						if (!weatherEntry->getWeatherName().empty()) // parity= if (weatherEntry.getWeatherName() != null) // C++: an absent name is empty (docs/deviations/C1.md)
							info += "\n\tname: " + weatherEntry->getWeatherName();
						info += "\n\tcode: " + std::to_string(weatherEntry->getCode());
					}
					sendInfo(admin, info);
					return;
				}
			}
		}
		sendInfo(admin, "No weather found for this region.");
		return;
	}
	if (command == "set" || command == "next") { // parity= case "set": case "next":
		int32_t weatherCode;
		if (commons::utils::StringUtils::equalsIgnoreCase(params[0], "next")) {
			if (params.size() != 1) {
				sendInfo(admin);
				return;
			}
			weatherCode = -1;
		} else {
			weatherCode = params.size() > 1 ? commons::utils::parseInt(params[1]) : -1;
			if (weatherCode < 0 || weatherCode > 12) {
				sendInfo(admin, "Weather code must be between 0 and 12.");
				return;
			}
		}
		if (WeatherService::getInstance().changeWeather(admin.getWorldId(), weatherCode)) {
			// findWeatherEntry never returns null (it falls back to WeatherEntry.NONE); an absent name is empty (Java null)
			const std::string& weatherName = WeatherService::getInstance().findWeatherEntry(admin)->getWeatherName();
			sendInfo(admin, "Changed the weather" + (weatherName.empty() ? std::string(".") : " to " + weatherName + ".")); // parity= sendInfo(admin, "Changed the weather" + (weatherName == null ? "." : " to " + weatherName + "."));
		} else {
			sendInfo(admin, "This region has no weather defined.");
		}
	}
}

} // namespace aion::gameserver::handlers::admincommands
