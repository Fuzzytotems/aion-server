#include "aion/gameserver/utils/time/gametime/GameTime.h"

#include <array>
#include <cstddef>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/WeatherService.h"
#include "aion/gameserver/spawnengine/TemporarySpawnEngine.h"
#include "aion/gameserver/utils/time/gametime/DayTime.h"

namespace aion::gameserver::utils::time::gametime {

namespace {

/** Java: GameTime.Month constructor data (days) in ordinal order; the enum's companion, private to GameTime */
constexpr std::array<int32_t, 12> MONTH_DAYS{{31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31}};
static_assert(static_cast<size_t>(GameTime_Month::DECEMBER) + 1 == MONTH_DAYS.size(), "one entry per GameTime.Month constant");

/** Java: Month.getDaysOfYear() */
constexpr int32_t getDaysOfYear() noexcept {
	int32_t daysOfYear = 0;
	for (int32_t days : MONTH_DAYS)
		daysOfYear += days;
	return daysOfYear;
}

/** Java int addition (wraps on overflow) */
constexpr int32_t addInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

/** Java int subtraction (wraps on overflow) */
constexpr int32_t subtractInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

} // namespace

static_assert(getDaysOfYear() * 24 * 60 == 12 * 31 * 24 * 60, "GameTime.h MINUTES_IN_YEAR equals Month.getDaysOfYear() * MINUTES_IN_DAY");

GameTime::GameTime(std::optional<int32_t> time) {
	if (time && *time < 0)
		throw runtime::IllegalArgumentException("Time must be >= 0");
	gameTime = time ? *time : 0;
	dayTime = calculateDayTime();
}

runtime::Ref<GameTime> GameTime::create(std::optional<int32_t> time) {
	return runtime::makeRef<GameTime>(time);
}

void GameTime::addMinutes(int32_t minutes) {
	if (minutes != 0) {
		gameTime = addInt(gameTime.get(), minutes);
		if (getMinute() == 0)
			onHourChange(minutes == 1);
	}
}

bool GameTime::setDayTime(DayTime value) {
	if (this->dayTime.get() == value)
		return false;
	this->dayTime = value;
	return true;
}

int32_t GameTime::getYear() {
	return gameTime.get() / MINUTES_IN_YEAR;
}

int32_t GameTime::getMonth() {
	int32_t month = 0;
	int32_t minutesOfThisYear = gameTime.get() % MINUTES_IN_YEAR;
	for (int32_t days : MONTH_DAYS) {
		month += 1;
		if ((minutesOfThisYear -= days * MINUTES_IN_DAY) < 0)
			break;
	}
	return month;
}

int32_t GameTime::getDay() {
	int32_t day = 1;
	int32_t minutesInYear = gameTime.get() % MINUTES_IN_YEAR;
	for (int32_t days : MONTH_DAYS) {
		int32_t minutesInMonth = days * MINUTES_IN_DAY;
		if (minutesInYear > minutesInMonth) {
			minutesInYear -= minutesInMonth;
		} else {
			if (minutesInYear < minutesInMonth) // if both are equal, it's day 1 of the following month
				day += minutesInYear / MINUTES_IN_DAY;
			break;
		}
	}
	return day;
}

int32_t GameTime::getHour() {
	return (gameTime.get() % MINUTES_IN_DAY) / MINUTES_IN_HOUR;
}

int32_t GameTime::getMinute() {
	return gameTime.get() % MINUTES_IN_HOUR;
}

void GameTime::onHourChange(bool changedByClock) {
	spawnengine::TemporarySpawnEngine::onHourChange();
	if (setDayTime(calculateDayTime()) && changedByClock) // don't change weather if time was changed by admin
		services::WeatherService::getInstance().checkWeathersTime();
}

DayTime GameTime::calculateDayTime() {
	int32_t hour = getHour();
	if (hour > 21 || hour < 4)
		return DayTime::NIGHT;
	else if (hour > 16)
		return DayTime::EVENING;
	else if (hour > 8)
		return DayTime::AFTERNOON;
	else
		return DayTime::MORNING;
}

runtime::Ref<GameTime> GameTime::minus(GameTime& gt) {
	return create(subtractInt(this->getTime(), gt.getTime()));
}

runtime::Ref<GameTime> GameTime::plus(GameTime& gt) {
	return create(addInt(this->getTime(), gt.getTime()));
}

bool GameTime::isGreaterThan(GameTime& gt) {
	return this->getTime() > gt.getTime();
}

bool GameTime::isLessThan(GameTime& gt) {
	return this->getTime() < gt.getTime();
}

int32_t GameTime::hashCode() const {
	const int32_t prime = 31;
	int32_t result = 1;
	result = addInt(prime * result, gameTime.get());
	return result;
}

bool GameTime::equals(const GameTime& obj) const {
	if (this == &obj)
		return true;
	return gameTime.get() == obj.gameTime.get();
}

runtime::Ref<GameTime> GameTime::clone() {
	return create(gameTime.get());
}

GameTime::~GameTime() = default;

} // namespace aion::gameserver::utils::time::gametime
