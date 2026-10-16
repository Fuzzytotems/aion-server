#pragma once

#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>

namespace aion::gameserver::model::gameobjects::player::detail {

/**
 * C++ only, private to P4-12: Java numeric conversions and the server-date arithmetic of AbyssRank, NpcFactions, PlayerCommonData and
 * PetCommonData, as pure functions so the tests can check them against Java results. The time zone is a parameter (callers pass
 * GSConfig.TIME_ZONE_ID); replace the date helpers by utils::time::ServerTime (P4-05) and getExpLoss by the XPLossEnum companion (P5-01) once
 * they are used by the whole chunk.
 */

/** Java Math.round(double): floor(a + 0.5) with Java's bit-exact implementation (JDK 8+); NaN is 0, out-of-range values saturate */
inline int64_t javaRound(double a) {
	const auto longBits = std::bit_cast<int64_t>(a);
	const int64_t biasedExp = (longBits & 0x7FF0000000000000LL) >> 52;
	const int64_t shift = (53 - 2 + 1023) - biasedExp; // (DoubleConsts.SIGNIFICAND_WIDTH - 2 + DoubleConsts.EXP_BIAS) - biasedExp
	if ((shift & -64) == 0) {                          // shift >= 0 && shift < 64: a is a finite value within long range
		int64_t r = (longBits & 0x000FFFFFFFFFFFFFLL) | (0x000FFFFFFFFFFFFFLL + 1);
		if (longBits < 0)
			r = -r;
		return ((r >> shift) + 1) >> 1;
	}
	if (std::isnan(a))
		return 0;
	if (a >= 9223372036854775807.0)
		return std::numeric_limits<int64_t>::max();
	if (a <= -9223372036854775808.0)
		return std::numeric_limits<int64_t>::min();
	return static_cast<int64_t>(a);
}

/** Java Math.round(float) (JDK 8+ bit-exact implementation; NaN and out-of-range values saturate like the (int) cast) */
inline int32_t javaRound(float a) {
	const auto intBits = std::bit_cast<int32_t>(a);
	const int32_t biasedExp = (intBits & 0x7F800000) >> 23;
	const int32_t shift = (24 - 2 + 127) - biasedExp; // (FloatConsts.SIGNIFICAND_WIDTH - 2 + FloatConsts.EXP_BIAS) - biasedExp
	if ((shift & -32) == 0) {                         // shift >= 0 && shift < 32: a is a finite value within int range
		int32_t r = (intBits & 0x007FFFFF) | (0x007FFFFF + 1);
		if (intBits < 0)
			r = -r;
		return ((r >> shift) + 1) >> 1;
	}
	if (std::isnan(a))
		return 0;
	if (a >= 2147483648.0f)
		return std::numeric_limits<int32_t>::max();
	if (a <= -2147483648.0f)
		return std::numeric_limits<int32_t>::min();
	return static_cast<int32_t>(a);
}

/** Java (int) doubleValue: NaN is 0, out-of-range values saturate */
inline int32_t toInt(double value) {
	if (std::isnan(value))
		return 0;
	if (value >= 2147483647.0)
		return std::numeric_limits<int32_t>::max();
	if (value <= -2147483648.0)
		return std::numeric_limits<int32_t>::min();
	return static_cast<int32_t>(value);
}

/** Java (long) floatValue: NaN is 0, out-of-range values saturate */
inline int64_t toLong(float value) {
	if (std::isnan(value))
		return 0;
	if (value >= 9223372036854775807.0f)
		return std::numeric_limits<int64_t>::max();
	if (value <= -9223372036854775808.0f)
		return std::numeric_limits<int64_t>::min();
	return static_cast<int64_t>(value);
}

/** Java XPLossEnum.getExpLoss(level, expNeed): Math.round(expNeed / 100 * param) of the first constant whose level is not below `level` */
inline int64_t getExpLoss(int32_t level, int64_t expNeed) {
	struct XpLoss {
		int32_t level;
		double param;
	};
	static constexpr XpLoss XP_LOSS[]{{6, 1.0}, {30, 1.0}, {40, 0.35}, {50, 0.25}, {55, 0.25}, {60, 0.25}, {65, 0.25}};
	if (level < 6)
		return 0;
	for (const XpLoss& xpLoss : XP_LOSS) {
		if (level <= xpLoss.level)
			return javaRound(static_cast<double>(expNeed / 100) * xpLoss.param);
	}
	return 0;
}

/** The calendar fields of Java ServerTime.ofEpochMilli(millis) that AbyssRank.doUpdate compares */
struct ServerDate {
	std::chrono::year_month_day date;
	int32_t isoWeek; // IsoFields.WEEK_OF_WEEK_BASED_YEAR
};

/** Java ServerTime.ofEpochMilli(epochMilli): the local date in `zone` and its ISO 8601 week number */
inline ServerDate serverDateOf(int64_t epochMilli, const std::chrono::time_zone* zone) {
	const std::chrono::sys_time<std::chrono::milliseconds> instant{std::chrono::milliseconds(epochMilli)};
	const std::chrono::local_days day = std::chrono::floor<std::chrono::days>(zone->to_local(instant));
	// ISO 8601 week: the week (Monday to Sunday) belongs to the year of its Thursday
	const auto isoWeekday = static_cast<int32_t>(std::chrono::weekday(day).iso_encoding()); // 1 = Monday .. 7 = Sunday
	const std::chrono::local_days thursday = day + std::chrono::days(4 - isoWeekday);
	const std::chrono::local_days firstOfWeekYear{std::chrono::year_month_day(thursday).year() / std::chrono::January / 1};
	const auto isoWeek = static_cast<int32_t>((thursday - firstOfWeekYear).count() / 7 + 1);
	return ServerDate{std::chrono::year_month_day(day), isoWeek};
}

/**
 * Java NpcFactions.getNextTime() at the instant `epochMilli`: 9:00 AM tomorrow if the server time is 9:00 or later, otherwise 9:00 AM today, as
 * (int) epoch seconds
 */
inline int32_t npcFactionNextTime(int64_t epochMilli, const std::chrono::time_zone* zone) {
	const std::chrono::sys_time<std::chrono::milliseconds> now{std::chrono::milliseconds(epochMilli)};
	const std::chrono::local_time<std::chrono::milliseconds> localNow = zone->to_local(now);
	const std::chrono::local_days today = std::chrono::floor<std::chrono::days>(localNow);
	const auto hour = std::chrono::floor<std::chrono::hours>(localNow - today).count();
	std::chrono::local_seconds repeatDate = today + std::chrono::hours(9);
	if (hour >= 9)
		repeatDate += std::chrono::days(1); // tomorrow morning at 9:00 AM
	const std::chrono::sys_seconds repeatDateSys = zone->to_sys(repeatDate, std::chrono::choose::earliest);
	return static_cast<int32_t>(repeatDateSys.time_since_epoch().count());
}

} // namespace aion::gameserver::model::gameobjects::player::detail
