#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>

#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/utils/time/fwd.h"

namespace aion::gameserver::utils::time {

/**
 * This class is used to tell the actual server time (not game time!), independent of the systems or JVM's time zone settings. It should be
 * used wherever simple timestamp checks aren't sufficient, to ensure time zone consistency throughout all classes.
 * <p>
 * C++: a static-only class (fieldmap K5). java.time.ZonedDateTime is `std::chrono::zoned_time` with millisecond precision in the zone of
 * GSConfig.TIME_ZONE_ID (hub-headers.md §6), LocalDateTime is `std::chrono::local_time<std::chrono::milliseconds>` and Date a
 * `commons::database::Timestamp`. Local times in a daylight saving gap are moved forward by the gap and ambiguous ones take the earlier
 * offset, like ZonedDateTime.of. Every method throws NullPointerException while GSConfig.TIME_ZONE_ID is null (before the configuration is
 * loaded), as Java's ZonedDateTime methods do for a null zone.
 *
 * @author Neon
 */
class ServerTime final {
public:
	using ZonedDateTime = std::chrono::zoned_time<std::chrono::milliseconds, const std::chrono::time_zone*>;
	using LocalDateTime = std::chrono::local_time<std::chrono::milliseconds>;

	ServerTime() = delete;

	/** @return The current server time (not game time!) */
	static ZonedDateTime now();

	/** @return The server time at the given date */
	static ZonedDateTime of(LocalDateTime localDateTime);

	/** @return The server time at the given (UTC) date */
	static ZonedDateTime atDate(commons::database::Timestamp date);

	/**
	 * @param epochMilli
	 *          - the milliseconds since the epoch of 1970-01-01T00:00:00 UTC
	 * @return The server time at the given date
	 */
	static ZonedDateTime ofEpochMilli(int64_t epochMilli);

	/**
	 * @param epochSecond
	 *          - the seconds since the epoch of 1970-01-01T00:00:00 UTC
	 * @return The server time at the given date
	 */
	static ZonedDateTime ofEpochSecond(int64_t epochSecond);

	/**
	 * Java: ZonedDateTime.parse(text).withZoneSameInstant(zone) for ISO_ZONED_DATE_TIME text: "2025-03-30T02:30:00+01:00", optionally with seconds
	 * fraction and a trailing "[Region/City]" (an offset of "Z" means UTC; the instant always comes from the offset, a region id only has
	 * to exist, as in java.time.format.Parsed.resolveInstant).
	 *
	 * @throws IllegalArgumentException
	 *           (Java: DateTimeParseException) if the text cannot be parsed
	 */
	static ZonedDateTime parse(std::string_view text);

	/**
	 * Java: of(LocalDateTime.parse(text)) for ISO_LOCAL_DATE_TIME text: "2025-03-30T02:30", "2025-03-30T02:30:15" or with a fraction of up to 9
	 * digits (truncated to milliseconds).
	 *
	 * @throws IllegalArgumentException
	 *           (Java: DateTimeParseException) if the text cannot be parsed
	 */
	static ZonedDateTime parseLocal(std::string_view text);

	/** @return The daylight savings offset in seconds at the current date. */
	static int32_t getDaylightSavings();

	/** @return The offset to UTC in seconds at the current date (including daylight savings). */
	static int32_t getOffset();

	/** @return The standard offset to UTC in seconds (excluding daylight savings). */
	static int32_t getStandardOffset();

private:
	/** C++ only: GSConfig.TIME_ZONE_ID, @throws NullPointerException if it is null */
	static const std::chrono::time_zone* zone();

	/** C++ only: Java LocalDateTime.parse (ISO_LOCAL_DATE_TIME) of text[pos..], advancing pos */
	static LocalDateTime parseLocalDateTime(std::string_view text, size_t& pos);
};

} // namespace aion::gameserver::utils::time
