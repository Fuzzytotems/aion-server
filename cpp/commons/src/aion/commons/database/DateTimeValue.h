#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace aion::commons::database {

/**
 * The broken-down value of a DATE, TIME, DATETIME or TIMESTAMP column or parameter, equivalent to MariaDB Connector/C's MYSQL_TIME but
 * without exposing <mysql.h>. No time zone is attached: it is a local date/time of the connection time zone (see ConnectionTimeZone).
 */
struct DateTimeValue {
	enum class Kind : uint8_t { DATE, TIME, DATETIME };

	Kind kind = Kind::DATETIME;
	/** TIME values only: negative duration */
	bool negative = false;
	uint32_t year = 0;
	uint32_t month = 0;
	uint32_t day = 0;
	/** 0-23 for DATETIME; TIME values may be up to 838 hours */
	uint32_t hour = 0;
	uint32_t minute = 0;
	uint32_t second = 0;
	uint32_t microsecond = 0;

	/** @return true for MySQL's "zero date" 0000-00-00 (DATE and DATETIME only) */
	bool isZeroDate() const noexcept { return kind != Kind::TIME && year == 0 && month == 0 && day == 0; }

	/**
	 * Formats the value like the MariaDB text protocol does: "yyyy-MM-dd", "[-]HH:mm:ss" or "yyyy-MM-dd HH:mm:ss", followed by the given number
	 * of fractional second digits (0-6, the column's decimals).
	 */
	std::string toString(uint32_t decimals = 0) const;

	/**
	 * Parses the textual formats produced by MariaDB: "yyyy-MM-dd", "yyyy-MM-dd HH:mm:ss[.f...]" (also with 'T' as separator) and
	 * "[-]H...H:mm:ss[.f...]". Fractions longer than 6 digits are truncated.
	 * @return the value, or nullopt if the text has none of these formats
	 */
	static std::optional<DateTimeValue> parse(std::string_view text);

	bool operator==(const DateTimeValue&) const = default;
};

} // namespace aion::commons::database
