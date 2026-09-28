#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aion::commons::utils {

/**
 * The fields of an instant in a time zone (Java: ZonedDateTime's accessors), the input of DateTimeFormatter. Computing them is cheap once the
 * UTC offset is known; getUtcOffset is the expensive part.
 */
struct DateTimeFields {
	int32_t year = 1970;
	/** 1-12 */
	int32_t month = 1;
	/** 1-31 */
	int32_t day = 1;
	/** 1 (Monday) - 7 (Sunday), like Java's DayOfWeek */
	int32_t dayOfWeek = 4;
	/** 1-366 */
	int32_t dayOfYear = 1;
	int32_t hour = 0;
	int32_t minute = 0;
	int32_t second = 0;
	/** 0-999,999,999 */
	int32_t nano = 0;
	int32_t offsetSeconds = 0;

	/** @return the fields of the instant at the given UTC offset */
	static DateTimeFields of(std::chrono::system_clock::time_point time, std::chrono::seconds utcOffset);

	/** @return the fields of the instant in the time zone (nullptr: the system default time zone) */
	static DateTimeFields of(std::chrono::system_clock::time_point time, const std::chrono::time_zone* timeZone);
};

/**
 * @param timeZone the time zone, or nullptr for the system default time zone (determined by the C library, so it works without a time zone
 *          database)
 * @return the UTC offset of the time zone at the given instant (Java: ZoneRules.getOffset(Instant))
 */
std::chrono::seconds getUtcOffset(std::chrono::sys_seconds time, const std::chrono::time_zone* timeZone);

/**
 * @return the time zone with the given ID (e.g. "Europe/Berlin", "UTC"), nullptr for an empty ID (system default)
 * @throws IllegalArgumentException if the zone is unknown or no time zone database is available
 */
const std::chrono::time_zone* findTimeZone(std::string_view zoneId);

/**
 * Java: java.time.format.DateTimeFormatter.ofPattern(pattern).withZone(zone) for the pattern letters used by the server: y u M L d D E a H k K
 * h m s S (fraction) X x Z, plus quoted literals ('T', '' for a quote). Other letters throw IllegalArgumentException. Immutable and thread
 * safe.
 * <pre>
 * DateTimeFormatter::ofPattern("yyyy-MM-dd'T'HH:mm:ss,SSSXXX").format(time)   // 2026-09-12T15:42:30,123+02:00
 * </pre>
 */
class DateTimeFormatter {
public:
	/** @throws IllegalArgumentException for unsupported or invalid patterns */
	static DateTimeFormatter ofPattern(std::string_view pattern);

	/** @param timeZone the time zone, or nullptr for the system default time zone */
	DateTimeFormatter withZone(const std::chrono::time_zone* timeZone) const;

	const std::chrono::time_zone* getZone() const noexcept { return zone; }

	/** Formats the instant in this formatter's zone. */
	std::string format(std::chrono::system_clock::time_point time) const;

	/** Appends the formatted fields (this formatter's zone is not used). */
	void formatTo(std::string& out, const DateTimeFields& fields) const;

private:
	enum class Field : uint8_t {
		LITERAL,
		YEAR,
		YEAR_2_DIGITS,
		MONTH,
		MONTH_SHORT_NAME,
		MONTH_NAME,
		DAY_OF_MONTH,
		DAY_OF_YEAR,
		DAY_OF_WEEK_SHORT_NAME,
		DAY_OF_WEEK_NAME,
		AM_PM,
		HOUR_OF_DAY,       // H: 0-23
		CLOCK_HOUR_OF_DAY, // k: 1-24
		HOUR_OF_AM_PM,     // K: 0-11
		CLOCK_HOUR_OF_AM_PM, // h: 1-12
		MINUTE,
		SECOND,
		FRACTION,
		OFFSET_X, // X: Z for zero
		OFFSET_x, // x: +00 for zero
		OFFSET_Z,
	};

	struct Element {
		Field field;
		int32_t count = 0;
		std::string literal;
	};

	std::vector<Element> elements;
	const std::chrono::time_zone* zone = nullptr;
};

} // namespace aion::commons::utils
