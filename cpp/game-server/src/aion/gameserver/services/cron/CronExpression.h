#pragma once

#include <bitset>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::services::cron {

/** Java: java.text.ParseException thrown by org.quartz.CronExpression for an invalid expression. */
class CronExpressionParseException : public runtime::IllegalArgumentException {
public:
	using runtime::IllegalArgumentException::IllegalArgumentException;
};

/**
 * Replacement of org.quartz.CronExpression (design §7.5): the Quartz syntax subset used by the Java server's configs, schedule XMLs, static
 * data (goods list sales times) and sources (research/critic.md "Quartz features actually used").
 *
 * Syntax. Fields separated by spaces or tabs: seconds minutes hours day-of-month month day-of-week [year]. Case-insensitive. Each field is a
 * comma-separated list of elements:
 * - `*` (every value), `?` (day-of-month or day-of-week only, as the whole field: "no specific value"),
 * - a value `v` (number or name), a range `a-b` (inclusive; a range whose end is below its start wraps around, e.g. `FRI-MON`, `22-2`),
 * - an increment `a/n`, `a-b/n`, `*`/`n` or `/n` (= `*`/`n`): every n-th value starting at a, up to b or the field maximum.
 * Ranges: seconds and minutes 0-59, hours 0-23, day-of-month 1-31, month 1-12 or JAN-DEC, day-of-week 1-7 or SUN-SAT (Quartz numbering:
 * 1 = SUN ... 7 = SAT), year 1970-2299 (a missing year field means every year). Leading zeros are allowed (`09-18`). Increments are limited
 * like Quartz (seconds/minutes 59, hours 23, day-of-month 31, month 12, day-of-week 7) and must be at least 1.
 * As in Quartz, exactly one of day-of-month and day-of-week must be `?`.
 *
 * Rejected with CronExpressionParseException (research/critic.md: no config or source uses them): `L`, `W`, `LW`, `#`, `L-n` and calendar
 * offsets. Deviations from Quartz, all stricter: tokens beyond the 7th field are an error (Quartz ignores them), trailing characters after
 * a name (`MONX`) are an error (Quartz ignores them), an increment of 0 is an error (Quartz treats `x/0` as `x`), and names may be combined
 * with increments (`MON-FRI/2`, which Quartz does not parse).
 *
 * Evaluation happens in local time of the given std::chrono::time_zone (nullptr = the system time zone, Java TimeZone.getDefault(); CronService
 * uses GSConfig::TIME_ZONE_ID). DST: a matching local time that does not exist (spring forward gap) is skipped; a local time that occurs twice
 * (fall back overlap) fires once, at its earlier instant, and the repeated hour produces no further fire times. Searches stop after year 2299
 * (Quartz MAX_YEAR is the current year + 100).
 *
 * Immutable after construction; thread-safe.
 */
class CronExpression {
public:
	static constexpr int32_t MIN_YEAR = 1970;
	static constexpr int32_t MAX_YEAR = 2299;

	/** @throws CronExpressionParseException */
	explicit CronExpression(std::string_view expression);

	/** Quartz isValidExpression */
	static bool isValidExpression(std::string_view expression) noexcept;

	/** Quartz getCronExpression / toString: the original text */
	const std::string& getCronExpression() const noexcept { return expression_; }
	const std::string& toString() const noexcept { return expression_; }

	/** Quartz isSatisfiedBy (second precision): true if `time` is a fire time, i.e. getNextValidTimeAfter(time - 1 s) == time. */
	bool isSatisfiedBy(std::chrono::sys_seconds time, const std::chrono::time_zone* zone) const;

	/** Quartz getNextValidTimeAfter: the first matching time strictly after `after`; empty when the year range is exhausted. */
	std::optional<std::chrono::sys_seconds> getNextValidTimeAfter(std::chrono::sys_seconds after, const std::chrono::time_zone* zone) const;

	/** Quartz getTimeAfter (AbstractCronTask.java:89,116): same as getNextValidTimeAfter. */
	std::optional<std::chrono::sys_seconds> getTimeAfter(std::chrono::sys_seconds after, const std::chrono::time_zone* zone) const {
		return getNextValidTimeAfter(after, zone);
	}

	friend bool operator==(const CronExpression& a, const CronExpression& b) noexcept { return a.expression_ == b.expression_; }

private:
	bool matchesDay(std::chrono::local_days day) const noexcept;

	std::string expression_;
	/** bit n = second/minute n (0-59) */
	uint64_t seconds_ = 0;
	uint64_t minutes_ = 0;
	/** bit n = hour n (0-23) */
	uint32_t hours_ = 0;
	/** bit n = day n (1-31) */
	uint32_t daysOfMonth_ = 0;
	/** bit n = month n (1-12) */
	uint16_t months_ = 0;
	/** bit n = Quartz day-of-week n (1 = SUN ... 7 = SAT) */
	uint8_t daysOfWeek_ = 0;
	/** day-of-month is `?`: days are selected by day-of-week (otherwise day-of-week is `?`) */
	bool dayOfMonthUnspecified_ = false;
	/** no year field or `*` */
	bool allYears_ = true;
	/** bit (year - MIN_YEAR) */
	std::bitset<MAX_YEAR - MIN_YEAR + 1> years_;
};

} // namespace aion::gameserver::services::cron
