#pragma once

#include <chrono>
#include <string>
#include <string_view>

#include "aion/commons/configuration/transformers/PropertyTransformer.h"

namespace aion::commons::configuration::transformers {

namespace ZoneIdTransformer {

/**
 * @return the std::chrono time zone database
 * @throws utils::IllegalStateException if the database cannot be loaded. Deviation: Java ships its own time zone data, whereas MSVC's std::chrono
 *           reads it from the Windows ICU library (icu.dll, Windows 10 version 1903 / Windows Server 2019 or later). The message names that
 *           requirement instead of the STL's "Internal error loading IANA database information".
 */
const std::chrono::tzdb& database();

/**
 * Java: ZoneId.systemDefault()
 *
 * @throws utils::IllegalStateException if the time zone database is not available (see database()) or the system time zone is not in it.
 *           Deviation: Java never fails here (TimeZone.getDefault falls back to the system GMT offset or GMT); without a database there is no
 *           std::chrono::time_zone to fall back to, and a silently wrong zone would shift all scheduled times, so this fails with a clear message.
 */
const std::chrono::time_zone* systemDefault();

/**
 * Java: ZoneId.of(zoneId) on top of the std::chrono time zone database.
 * <ul>
 * <li>Region IDs ("Europe/Berlin", "GMT0", ...) are looked up in the time zone database ("Unknown time-zone ID: x" if missing).</li>
 * <li>"Z", "UTC", "GMT" and "UT" are UTC.</li>
 * <li>Offsets ("+02:00", "-5", "+0130", "UTC+2", "GMT-03:00", ...) are validated like Java's ZoneOffset.of. Deviation: std::chrono has no
 * arbitrary fixed-offset zones, so only whole-hour offsets between -12 and +14 are supported (mapped to "Etc/GMT∓h"); others are an error.</li>
 * </ul>
 *
 * @throws utils::IllegalArgumentException if the ID is invalid or unknown
 * @throws utils::IllegalStateException if the time zone database is not available (see database())
 */
const std::chrono::time_zone* of(std::string_view zoneId);

} // namespace ZoneIdTransformer

/**
 * Transforms a time zone ID to a time zone (Java: ZoneId, see ZoneIdTransformer::of). An empty value yields the system time zone.
 * <p>
 * Java's TimeZoneTransformer (TimeZone.getTimeZone, which silently falls back to GMT for unknown IDs) has no separate C++ type and is not ported;
 * no config uses it.
 * <p>
 * Java: com.aionemu.commons.configuration.transformers.ZoneIdTransformer
 *
 * @author Neon
 */
template <>
struct PropertyTransformer<const std::chrono::time_zone*> {
	static std::string typeName() { return "ZoneId"; }

	static const std::chrono::time_zone* parseObject(std::string_view value) {
		return value.empty() ? ZoneIdTransformer::systemDefault() : ZoneIdTransformer::of(value);
	}
};

} // namespace aion::commons::configuration::transformers
