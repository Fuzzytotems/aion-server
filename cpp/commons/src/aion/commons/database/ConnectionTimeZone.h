#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

#include "aion/commons/database/DateTimeValue.h"
#include "aion/commons/database/SqlTypes.h"

namespace aion::commons::database {

/**
 * The time zone used to convert Timestamp instants to and from the local date/time values stored in DATETIME and TIMESTAMP columns.
 * Equivalent to Connector/J's connectionTimeZone (alias serverTimezone) with preserveInstants=true: an empty value means the system default
 * time zone ("LOCAL"), otherwise an IANA name ("Europe/Berlin") or a fixed offset ("UTC", "+02:00", "GMT-5").
 * <p>
 * Local times that do not exist (DST gap) are shifted forward by the length of the gap and ambiguous local times (DST overlap) use the
 * earlier offset, both like java.time.LocalDateTime.atZone.
 * <p>
 * Conversions in named zones remember the last UTC offset period per thread, so that converting many values of the same period (the usual
 * case when loading rows) does not query the time zone database each time (MSVC's implementation calls ICU for every lookup).
 * <p>
 * The system default zone is determined once. MSVC's time zone database needs icu.dll (Windows 10 1903 / Windows Server 2022 or later);
 * where it is not available, the system zone is taken from the operating system instead (see operatingSystemZone).
 */
class ConnectionTimeZone {
public:
	using MicrosecondsSysTime = std::chrono::sys_time<std::chrono::microseconds>;
	using MicrosecondsLocalTime = std::chrono::local_time<std::chrono::microseconds>;

	/** The system default time zone. */
	ConnectionTimeZone();

	static ConnectionTimeZone systemDefault();
	static ConnectionTimeZone ofOffset(std::chrono::seconds offset);

	/**
	 * The system time zone as seen by the operating system (Windows: the dynamic time zone information, C library elsewhere) without the
	 * C++ time zone database. Used as system default if the time zone database is not available; public for tests.
	 * Deviation: the rules of the operating system are used, which may differ from the IANA database for historical dates.
	 */
	static ConnectionTimeZone operatingSystemZone();

	/**
	 * Resolves a Connector/J connectionTimeZone/serverTimezone value. Empty, "LOCAL" and "SERVER" (not supported, see implementation) resolve to
	 * the system default.
	 * @throws SQLException if the zone is unknown (Connector/J: "The server time zone value '...' is unrecognized")
	 */
	static ConnectionTimeZone of(std::string_view id);

	/** @return the zone name, e.g. "Europe/Berlin" or "+02:00" (the Windows zone name, e.g. "W. Europe Standard Time", for operatingSystemZone) */
	const std::string& getId() const noexcept { return id; }

	MicrosecondsLocalTime toLocal(MicrosecondsSysTime time) const;
	MicrosecondsSysTime toSys(MicrosecondsLocalTime time) const;

	/** Converts an instant to a DATETIME value in this zone. */
	DateTimeValue toDateTime(Timestamp timestamp) const;

	/**
	 * Converts a DATE (local midnight), TIME (on 1970-01-01, like Connector/J) or DATETIME value of this zone to an instant. Sub-millisecond
	 * digits are truncated.
	 */
	Timestamp toTimestamp(const DateTimeValue& value) const;

private:
	enum class Source : uint8_t { FIXED_OFFSET, TIME_ZONE_DATABASE, OPERATING_SYSTEM };

	explicit ConnectionTimeZone(Source source) noexcept : source(source) {}

	/** @return the system default zone, determined on first use */
	static const ConnectionTimeZone& determineSystemZone();

	Source source = Source::FIXED_OFFSET;
	const std::chrono::time_zone* zone = nullptr; // Source::TIME_ZONE_DATABASE only
	std::chrono::seconds offset{0};								// Source::FIXED_OFFSET only
	std::string id = "Z";
};

} // namespace aion::commons::database
