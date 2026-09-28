#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "aion/gameserver/model/templates/spawns/TemporarySpawn.xml.h"

namespace aion::gameserver::model::templates::spawns {

/**
 * Java com.aionemu.gameserver.model.templates.spawns.TemporarySpawn.
 * <p>
 * C++: the checks read the server's day of week (Java ServerTime.now(), GSConfig.TIME_ZONE_ID) and the game time (GameTimeService). The
 * C++-only overloads with an explicit weekday and game time contain the whole logic, so it can be tested without the clocks.
 *
 * @author xTz
 */
class TemporarySpawn : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/spawns/TemporarySpawn.xml.inc"
public:
	/** The time of the game clock the checks compare with (Java GameTime.getHour/getDay/getMonth) */
	struct GameTimeValues {
		int32_t hour;
		int32_t day;
		int32_t month;
	};

	bool canSpawn() const;

	/** C++ only: canSpawn() at an explicit server day of week and game time */
	bool canSpawn(java::time::DayOfWeek today, const GameTimeValues& gameTime) const;

	bool canDespawn() const;

	/** C++ only: canDespawn() at an explicit server day of week and game time */
	bool canDespawn(java::time::DayOfWeek today, const GameTimeValues& gameTime) const;

	bool isInSpawnTime() const;

	/** C++ only: isInSpawnTime() at an explicit server day of week and game time */
	bool isInSpawnTime(java::time::DayOfWeek today, const GameTimeValues& gameTime) const;

private:
	// Java unbound fields (null when the expression part is "*"), parsed by afterUnmarshal
	std::optional<int32_t> spawnHour;
	std::optional<int32_t> spawnDay;
	std::optional<int32_t> spawnMonth;
	std::optional<int32_t> despawnHour;
	std::optional<int32_t> despawnDay;
	std::optional<int32_t> despawnMonth;

	/** @throws IllegalArgumentException (Java NumberFormatException) or IndexOutOfBoundsException for a malformed expression */
	static std::optional<int32_t> parseTime(std::string_view time, int32_t type);

	static bool isTime(std::optional<int32_t> hour, std::optional<int32_t> day, std::optional<int32_t> month, const GameTimeValues& gameTime);

	/** Java: weekdays != null && !weekdays.isEmpty() && !weekdays.contains(today) */
	bool isExcludedWeekday(java::time::DayOfWeek today) const;

	static bool checkDate(int32_t currentDate, int32_t spawnDate, std::optional<int32_t> despawnDate);

	static bool checkHour(int32_t currentHour, int32_t spawnHour, std::optional<int32_t> despawnHour);

	static bool checkWithDespawnExpression(int32_t currentDate, int32_t spawnTimeOrExpression, int32_t despawnExpression);

	/** the current server day of week and game time (ServerTime.now(), GameTimeService.getInstance().getGameTime()) */
	static java::time::DayOfWeek serverDayOfWeek();
	static GameTimeValues currentGameTime();
};

} // namespace aion::gameserver::model::templates::spawns
