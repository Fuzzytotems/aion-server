#include "aion/gameserver/model/templates/spawns/TemporarySpawn.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <string>
#include <vector>

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/GameTimeService.h"
#include "aion/gameserver/utils/time/ServerTime.h"
#include "aion/gameserver/utils/time/gametime/GameTime.h"

namespace aion::gameserver::model::templates::spawns {

namespace {

/** Java String.split("\\.") : the parts between dots, trailing empty strings removed */
std::vector<std::string_view> splitAtDots(std::string_view text) {
	std::vector<std::string_view> parts;
	size_t start = 0;
	while (true) {
		size_t dot = text.find('.', start);
		if (dot == std::string_view::npos) {
			parts.push_back(text.substr(start));
			break;
		}
		parts.push_back(text.substr(start, dot - start));
		start = dot + 1;
	}
	while (!parts.empty() && parts.back().empty())
		parts.pop_back();
	return parts;
}

} // namespace

void TemporarySpawn::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	try {
		if (!spawnTime.empty()) { // Java: spawnTime != null
			spawnHour = parseTime(spawnTime, 0);
			spawnDay = parseTime(spawnTime, 1);
			spawnMonth = parseTime(spawnTime, 2);
			spawnTime.clear(); // Java: spawnTime = null
		}
		if (!despawnTime.empty()) { // Java: despawnTime != null
			despawnHour = parseTime(despawnTime, 0);
			despawnDay = parseTime(despawnTime, 1);
			despawnMonth = parseTime(despawnTime, 2);
			despawnTime.clear(); // Java: despawnTime = null
		}
	} catch (const std::exception& e) { // Deviation: Java's exception fails the unmarshalling; StaticDataException with the location
		ctx.fail(e.what());
	}
}

std::optional<int32_t> TemporarySpawn::parseTime(std::string_view time, int32_t type) {
	std::vector<std::string_view> parts = splitAtDots(time);
	if (type >= static_cast<int32_t>(parts.size()))
		throw runtime::ArrayIndexOutOfBoundsException("Index " + std::to_string(type) + " out of bounds for length " + std::to_string(parts.size()));
	std::string result(parts[static_cast<size_t>(type)]);
	if (result == "*")
		return std::nullopt;
	if (result.starts_with("/"))
		result = "-" + result.substr(1); // parse negative for later expression handling (/2 means every 2 hours)
	return commons::utils::parseInt(result);
}

bool TemporarySpawn::isTime(std::optional<int32_t> hour, std::optional<int32_t> day, std::optional<int32_t> month, const GameTimeValues& gameTime) {
	if (hour.has_value()) {
		int32_t gameTimeHour = gameTime.hour;
		if ((*hour >= 0 && gameTimeHour != *hour) || (*hour < 0 && gameTimeHour % *hour != 0))
			return false;
	}
	if (day.has_value()) {
		int32_t gameTimeDay = gameTime.day;
		if ((*day >= 0 && gameTimeDay != *day) || (*day < 0 && gameTimeDay % *day != 0))
			return false;
	}
	if (month.has_value()) {
		int32_t gameTimeMonth = gameTime.month;
		if ((*month >= 0 && gameTimeMonth != *month) || (*month < 0 && gameTimeMonth % *month != 0))
			return false;
	}
	return true;
}

bool TemporarySpawn::isExcludedWeekday(java::time::DayOfWeek today) const {
	return weekdays.has_value() && !weekdays->empty() && std::ranges::find(*weekdays, today) == weekdays->end();
}

bool TemporarySpawn::canSpawn() const {
	if (weekdays.has_value() && !weekdays->empty() && isExcludedWeekday(serverDayOfWeek()))
		return false;
	return isTime(spawnHour, spawnDay, spawnMonth, currentGameTime());
}

bool TemporarySpawn::canSpawn(java::time::DayOfWeek today, const GameTimeValues& gameTime) const {
	if (isExcludedWeekday(today))
		return false;
	return isTime(spawnHour, spawnDay, spawnMonth, gameTime);
}

bool TemporarySpawn::canDespawn() const {
	if (weekdays.has_value() && !weekdays->empty() && isExcludedWeekday(serverDayOfWeek()))
		return true;
	return isTime(despawnHour, despawnDay, despawnMonth, currentGameTime());
}

bool TemporarySpawn::canDespawn(java::time::DayOfWeek today, const GameTimeValues& gameTime) const {
	if (isExcludedWeekday(today))
		return true;
	return isTime(despawnHour, despawnDay, despawnMonth, gameTime);
}

bool TemporarySpawn::isInSpawnTime() const {
	if (weekdays.has_value() && !weekdays->empty() && isExcludedWeekday(serverDayOfWeek()))
		return false;
	GameTimeValues gameTime = currentGameTime();
	if (spawnMonth.has_value() && !checkDate(gameTime.month, *spawnMonth, despawnMonth))
		return false;
	if (spawnDay.has_value() && !checkDate(gameTime.day, *spawnDay, despawnDay))
		return false;
	if (spawnHour.has_value() && !checkHour(gameTime.hour, *spawnHour, despawnHour))
		return false;
	return true;
}

bool TemporarySpawn::isInSpawnTime(java::time::DayOfWeek today, const GameTimeValues& gameTime) const {
	if (isExcludedWeekday(today))
		return false;
	if (spawnMonth.has_value() && !checkDate(gameTime.month, *spawnMonth, despawnMonth))
		return false;
	if (spawnDay.has_value() && !checkDate(gameTime.day, *spawnDay, despawnDay))
		return false;
	if (spawnHour.has_value() && !checkHour(gameTime.hour, *spawnHour, despawnHour))
		return false;
	return true;
}

bool TemporarySpawn::checkDate(int32_t currentDate, int32_t spawnDate, std::optional<int32_t> despawnDate) {
	if (despawnDate.has_value() && *despawnDate < 0) // check "every nth month/day" expression
		return checkWithDespawnExpression(currentDate, spawnDate, -*despawnDate);
	if (spawnDate < 0)
		spawnDate = -spawnDate; // make the expression a positive spawn time, works just fine
	if (!despawnDate.has_value()) // any date
		return currentDate >= spawnDate;
	if (spawnDate <= *despawnDate)
		return currentDate >= spawnDate && currentDate <= *despawnDate;
	else
		return currentDate >= spawnDate || currentDate <= *despawnDate;
}

bool TemporarySpawn::checkHour(int32_t currentHour, int32_t spawnHourValue, std::optional<int32_t> despawnHourValue) {
	if (despawnHourValue.has_value() && *despawnHourValue < 0) // check "every nth month/day" expression
		return checkWithDespawnExpression(currentHour, spawnHourValue, -*despawnHourValue);
	if (spawnHourValue < 0)
		spawnHourValue = -spawnHourValue; // make the expression a positive spawn time, works just fine
	if (!despawnHourValue.has_value()) // any hour
		return currentHour >= spawnHourValue;
	if (spawnHourValue < *despawnHourValue)
		return currentHour >= spawnHourValue && currentHour < *despawnHourValue;
	if (spawnHourValue > *despawnHourValue)
		return currentHour >= spawnHourValue || currentHour < *despawnHourValue;
	return true;
}

bool TemporarySpawn::checkWithDespawnExpression(int32_t currentDate, int32_t spawnTimeOrExpression, int32_t despawnExpression) {
	// proper handling would be really complex, so for now some spawn/despawn combinations don't spawn directly on server start
	if (spawnTimeOrExpression < 0) // change expression to time
		spawnTimeOrExpression = -spawnTimeOrExpression;
	return currentDate >= spawnTimeOrExpression && spawnTimeOrExpression == despawnExpression;
}

java::time::DayOfWeek TemporarySpawn::serverDayOfWeek() {
	// Java: ServerTime.now().getDayOfWeek()
	std::chrono::weekday weekday{std::chrono::floor<std::chrono::days>(utils::time::ServerTime::now().get_local_time())};
	// ISO encoding: Monday 1 ... Sunday 7; DayOfWeek ordinals: MONDAY 0 ... SUNDAY 6
	return static_cast<java::time::DayOfWeek>(weekday.iso_encoding() - 1);
}

TemporarySpawn::GameTimeValues TemporarySpawn::currentGameTime() {
	runtime::Ptr<utils::time::gametime::GameTime> gameTime = services::GameTimeService::getInstance().getGameTime();
	return GameTimeValues{gameTime->getHour(), gameTime->getDay(), gameTime->getMonth()};
}

} // namespace aion::gameserver::model::templates::spawns
