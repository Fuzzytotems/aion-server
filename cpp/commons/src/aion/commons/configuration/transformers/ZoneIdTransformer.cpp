#include "aion/commons/configuration/transformers/ZoneIdTransformer.h"

#include <cstdlib>

#include <fmt/format.h>

#include "aion/commons/utils/Exception.h"

namespace aion::commons::configuration::transformers::ZoneIdTransformer {

namespace {

using utils::IllegalArgumentException;

const std::chrono::time_zone* locate(std::string_view name, std::string_view zoneId) {
	const std::chrono::tzdb& db = database(); // outside the try: a missing database is not an unknown ID
	try {
		return db.locate_zone(name);
	} catch (const std::exception&) {
		throw IllegalArgumentException(fmt::format("Unknown time-zone ID: {}", zoneId));
	}
}

/** Java: ZoneOffset.parseNumber */
int parseNumber(std::string_view offsetId, std::size_t pos, bool precededByColon) {
	if (precededByColon && offsetId[pos - 1] != ':')
		throw IllegalArgumentException(fmt::format("Invalid ID for ZoneOffset, colon not found when expected: {}", offsetId));
	char ch1 = offsetId[pos];
	char ch2 = offsetId[pos + 1];
	if (ch1 < '0' || ch1 > '9' || ch2 < '0' || ch2 > '9')
		throw IllegalArgumentException(fmt::format("Invalid ID for ZoneOffset, non numeric characters found: {}", offsetId));
	return (ch1 - '0') * 10 + (ch2 - '0');
}

/** Java: ZoneOffset.of(offsetId). @return the total offset in seconds */
int parseOffset(std::string offsetId) {
	if (offsetId == "Z")
		return 0;
	int hours, minutes = 0, seconds = 0;
	switch (offsetId.size()) {
		case 2:
			offsetId.insert(1, "0");
			[[fallthrough]];
		case 3:
			hours = parseNumber(offsetId, 1, false);
			break;
		case 5:
			hours = parseNumber(offsetId, 1, false);
			minutes = parseNumber(offsetId, 3, false);
			break;
		case 6:
			hours = parseNumber(offsetId, 1, false);
			minutes = parseNumber(offsetId, 4, true);
			break;
		case 7:
			hours = parseNumber(offsetId, 1, false);
			minutes = parseNumber(offsetId, 3, false);
			seconds = parseNumber(offsetId, 5, false);
			break;
		case 9:
			hours = parseNumber(offsetId, 1, false);
			minutes = parseNumber(offsetId, 4, true);
			seconds = parseNumber(offsetId, 7, true);
			break;
		default:
			throw IllegalArgumentException(fmt::format("Invalid ID for ZoneOffset, invalid format: {}", offsetId));
	}
	char first = offsetId[0];
	if (first != '+' && first != '-')
		throw IllegalArgumentException(fmt::format("Invalid ID for ZoneOffset, plus/minus not found when expected: {}", offsetId));
	// Java: ZoneOffset.validate
	if (hours > 18)
		throw IllegalArgumentException(
		  fmt::format("Zone offset hours not in valid range: value {} is not in the range -18 to 18", first == '-' ? -hours : hours));
	if (minutes > 59)
		throw IllegalArgumentException(fmt::format("Zone offset minutes not in valid range: value {} is not in the range -59 to 59", minutes));
	if (seconds > 59)
		throw IllegalArgumentException(fmt::format("Zone offset seconds not in valid range: value {} is not in the range -59 to 59", seconds));
	if (hours == 18 && (minutes | seconds) != 0)
		throw IllegalArgumentException("Zone offset not in valid range: -18:00 to +18:00");
	int totalSeconds = hours * 3600 + minutes * 60 + seconds;
	return first == '-' ? -totalSeconds : totalSeconds;
}

const std::chrono::time_zone* ofOffset(std::string_view offsetId, std::string_view zoneId) {
	int totalSeconds = parseOffset(std::string(offsetId));
	if (totalSeconds == 0)
		return locate("UTC", zoneId);
	// Deviation: std::chrono only knows the whole-hour Etc/GMT zones, whose sign is inverted (Etc/GMT-2 is UTC+2)
	if (totalSeconds % 3600 != 0 || totalSeconds < -12 * 3600 || totalSeconds > 14 * 3600)
		throw IllegalArgumentException(fmt::format("Fixed offset time zones other than whole hours between -12 and +14 are not supported: {}", zoneId));
	int hours = totalSeconds / 3600;
	return locate(fmt::format("Etc/GMT{}{}", hours > 0 ? '-' : '+', std::abs(hours)), zoneId);
}

/** Java: ZoneRegion.checkName, [A-Za-z][A-Za-z0-9~/._+-]+ */
bool isValidRegionId(std::string_view zoneId) noexcept {
	auto isAlpha = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
	if (zoneId.size() < 2 || !isAlpha(zoneId[0]))
		return false;
	for (char c : zoneId.substr(1)) {
		if (!isAlpha(c) && !(c >= '0' && c <= '9') && c != '~' && c != '/' && c != '.' && c != '_' && c != '+' && c != '-')
			return false;
	}
	return true;
}

const std::chrono::time_zone* ofRegion(std::string_view zoneId) {
	if (!isValidRegionId(zoneId))
		throw IllegalArgumentException(fmt::format("Invalid ID for region-based ZoneId, invalid format: {}", zoneId));
	return locate(zoneId, zoneId);
}

const std::chrono::time_zone* ofWithPrefix(std::string_view zoneId, std::size_t prefixLength) {
	if (zoneId.size() == prefixLength)
		return locate(zoneId == "GMT" ? "GMT" : "UTC", zoneId); // "UT" is not in the time zone database
	if (zoneId[prefixLength] != '+' && zoneId[prefixLength] != '-')
		return ofRegion(zoneId); // drop through to the time zone database
	try {
		return ofOffset(zoneId.substr(prefixLength), zoneId);
	} catch (const IllegalArgumentException&) {
		throw IllegalArgumentException(fmt::format("Invalid ID for offset-based ZoneId: {}", zoneId), std::current_exception());
	}
}

} // namespace

const std::chrono::tzdb& database() {
	try {
		return std::chrono::get_tzdb();
	} catch (...) {
		throw utils::IllegalStateException("The time zone database is not available. On Windows, std::chrono loads it from the system ICU library "
		                                   "(icu.dll), which requires Windows 10 version 1903 / Windows Server 2019 or later.",
		                                   std::current_exception());
	}
}

const std::chrono::time_zone* systemDefault() {
	const std::chrono::tzdb& db = database();
	try {
		return db.current_zone();
	} catch (...) {
		throw utils::IllegalStateException("The system time zone could not be determined. Please configure the time zone explicitly.",
		                                   std::current_exception());
	}
}

const std::chrono::time_zone* of(std::string_view zoneId) {
	if (zoneId.size() <= 1 || zoneId.starts_with('+') || zoneId.starts_with('-'))
		return ofOffset(zoneId, zoneId);
	if (zoneId.starts_with("UTC") || zoneId.starts_with("GMT"))
		return ofWithPrefix(zoneId, 3);
	if (zoneId.starts_with("UT"))
		return ofWithPrefix(zoneId, 2);
	return ofRegion(zoneId);
}

} // namespace aion::commons::configuration::transformers::ZoneIdTransformer
