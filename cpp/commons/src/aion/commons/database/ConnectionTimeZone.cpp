#include "aion/commons/database/ConnectionTimeZone.h"

#include <algorithm>
#include <limits>
#include <optional>

#include <fmt/format.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/DateTimeFormatter.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::database {

using namespace std::chrono;

namespace {

const logging::Logger& log() {
	static const logging::Logger logger = logging::LoggerFactory::getLogger("com.aionemu.commons.database.ConnectionTimeZone");
	return logger;
}

std::string formatOffset(seconds offset) {
	if (offset == seconds::zero())
		return "Z";
	auto total = offset.count();
	char sign = total < 0 ? '-' : '+';
	if (total < 0)
		total = -total;
	std::string text = fmt::format("{}{:02}:{:02}", sign, total / 3600, total / 60 % 60);
	if (total % 60 != 0)
		text += fmt::format(":{:02}", total % 60);
	return text;
}

bool parseNumber(std::string_view text, size_t& pos, size_t minDigits, size_t maxDigits, int& out) {
	size_t digits = 0;
	int value = 0;
	while (pos < text.size() && digits < maxDigits && text[pos] >= '0' && text[pos] <= '9') {
		value = value * 10 + (text[pos] - '0');
		++pos;
		++digits;
	}
	out = value;
	return digits >= minDigits;
}

/** Java ZoneId/ZoneOffset formats: Z, +h, +hh, +hh:mm, +hhmm, +hh:mm:ss, optionally prefixed with UTC, GMT or UT. */
bool parseOffset(std::string_view id, seconds& offset) {
	std::string_view text = id;
	for (std::string_view prefix : {"UTC", "GMT", "UT"}) {
		if (text.starts_with(prefix)) {
			text.remove_prefix(prefix.size());
			break;
		}
	}
	if (text.empty() || text == "Z") {
		offset = seconds::zero();
		return text.size() != id.size() || text == "Z";
	}
	if (text[0] != '+' && text[0] != '-')
		return false;
	bool negative = text[0] == '-';
	size_t pos = 1;
	int hours = 0, minutes = 0, secs = 0;
	if (!parseNumber(text, pos, 1, 2, hours))
		return false;
	if (pos < text.size()) {
		bool colon = text[pos] == ':';
		if (colon)
			++pos;
		if (!parseNumber(text, pos, 2, 2, minutes))
			return false;
		if (pos < text.size()) {
			if (colon && !(text[pos] == ':'))
				return false;
			if (colon)
				++pos;
			if (!parseNumber(text, pos, 2, 2, secs))
				return false;
		}
	}
	if (pos != text.size() || hours > 18 || minutes > 59 || secs > 59)
		return false;
	auto total = seconds(hours * 3600 + minutes * 60 + secs);
	offset = negative ? -total : total;
	return true;
}

// ---- time zone database lookups, cached per thread ----

/** Periods are only cached within this range, which keeps the cached bounds plus offsets far away from overflow. */
constexpr sys_seconds MIN_CACHED_INSTANT = sys_days(year{-9999} / January / 1);
constexpr sys_seconds MAX_CACHED_INSTANT = sys_days(year{9999} / December / 31);

/** The UTC offset period of the last toLocal lookup of this thread: instants in [begin, end) have the offset. */
struct SysPeriodCache {
	const time_zone* zone = nullptr;
	sys_seconds begin;
	sys_seconds end;
	seconds offset{0};
};

/** The last toSys lookup of this thread: local times in [begin, end) have exactly this offset as java.time's answer. */
struct LocalPeriodCache {
	const time_zone* zone = nullptr;
	local_seconds begin;
	local_seconds end;
	seconds offset{0};
};

thread_local SysPeriodCache sysPeriodCache;
thread_local LocalPeriodCache localPeriodCache;

seconds cachedOffsetOfInstant(const time_zone& zone, sys_seconds time) {
	SysPeriodCache& cache = sysPeriodCache;
	if (cache.zone != &zone || time < cache.begin || time >= cache.end) {
		sys_info info = zone.get_info(time);
		cache = SysPeriodCache{&zone, info.begin, info.end, info.offset};
	}
	return cache.offset;
}

seconds cachedOffsetOfLocalTime(const time_zone& zone, local_seconds time) {
	LocalPeriodCache& cache = localPeriodCache;
	if (cache.zone == &zone && time >= cache.begin && time < cache.end)
		return cache.offset;
	// For unique times first is the only offset, in a gap it is the offset before the transition (shifting the time forward by the gap length)
	// and in an overlap it is the earlier offset: exactly java.time's ZonedDateTime.of(LocalDateTime, zone).
	local_info info = zone.get_info(time);
	if (info.result == local_info::unique) {
		// Cache the local times of this period that can only belong to it (or for which its offset is the earlier one of an overlap):
		// [begin + max(offset, previous offset), end + offset). Local times before that fall into the overlap with, or the gap after, the
		// previous period. Overlaps with the next period at the end resolve to this period's (earlier) offset anyway.
		const sys_info& period = info.first;
		std::optional<local_seconds> begin;
		if (period.begin < MIN_CACHED_INSTANT) {
			begin = local_seconds((MIN_CACHED_INSTANT + days(1)).time_since_epoch() + period.offset);
		} else if (period.begin <= MAX_CACHED_INSTANT) {
			sys_info previous = zone.get_info(period.begin - seconds(1));
			// periods shorter than a day are not cached (their neighbours could overlap as well)
			if (previous.begin < MIN_CACHED_INSTANT || period.begin - previous.begin >= days(1))
				begin = local_seconds(period.begin.time_since_epoch() + std::max(period.offset, previous.offset));
		}
		sys_seconds end = std::min(period.end, MAX_CACHED_INSTANT);
		if (begin)
			cache = LocalPeriodCache{&zone, *begin, local_seconds(end.time_since_epoch() + period.offset), period.offset};
	}
	return info.first.offset;
}

// ---- operating system zone (without the time zone database) ----

#ifdef _WIN32

const std::optional<DYNAMIC_TIME_ZONE_INFORMATION>& windowsTimeZoneInformation() {
	static const std::optional<DYNAMIC_TIME_ZONE_INFORMATION> information = []() -> std::optional<DYNAMIC_TIME_ZONE_INFORMATION> {
		DYNAMIC_TIME_ZONE_INFORMATION info{};
		if (GetDynamicTimeZoneInformation(&info) == TIME_ZONE_ID_INVALID)
			return std::nullopt;
		return info;
	}();
	return information;
}

/** @return the UTC offset of the Windows system time zone at the instant (historical rules from the registry's dynamic DST data) */
seconds operatingSystemOffset(sys_seconds time) {
	const auto& info = windowsTimeZoneInformation();
	if (!info)
		return seconds::zero();
	static constexpr sys_seconds FILETIME_EPOCH = sys_days(year{1601} / January / 1);
	// FILETIME/SYSTEMTIME range: years 1601 to 30827
	time = std::clamp(time, FILETIME_EPOCH + days(2), sys_seconds(sys_days(year{30000} / January / 1)));
	uint64_t ticks = static_cast<uint64_t>((time - FILETIME_EPOCH).count()) * 10'000'000ULL;
	FILETIME utcFileTime{static_cast<DWORD>(ticks), static_cast<DWORD>(ticks >> 32)};
	SYSTEMTIME utc{};
	SYSTEMTIME local{};
	FILETIME localFileTime{};
	if (!FileTimeToSystemTime(&utcFileTime, &utc) || !SystemTimeToTzSpecificLocalTimeEx(&*info, &utc, &local) ||
		!SystemTimeToFileTime(&local, &localFileTime))
		return seconds(-static_cast<int64_t>(info->Bias) * 60);
	uint64_t localTicks = (static_cast<uint64_t>(localFileTime.dwHighDateTime) << 32) | localFileTime.dwLowDateTime;
	return seconds((static_cast<int64_t>(localTicks) - static_cast<int64_t>(ticks)) / 10'000'000);
}

std::string operatingSystemZoneName() {
	const auto& info = windowsTimeZoneInformation();
	if (!info)
		return "Z";
	std::u16string_view name(static_cast<const char16_t*>(static_cast<const void*>(info->TimeZoneKeyName)));
	if (name.empty())
		name = std::u16string_view(static_cast<const char16_t*>(static_cast<const void*>(info->StandardName)));
	return utils::StringUtils::toUtf8(name);
}

#else

seconds operatingSystemOffset(sys_seconds time) {
	return utils::getUtcOffset(time, nullptr);
}

std::string operatingSystemZoneName() {
	return "LOCAL";
}

#endif

/** java.time's local to instant resolution (see cachedOffsetOfLocalTime) for a zone that only provides the offset of an instant. */
seconds operatingSystemOffsetOfLocalTime(local_seconds time) {
	// offsets are within +-18 hours, so any transition relevant to this local time lies within that window
	sys_seconds asSys(time.time_since_epoch());
	seconds before = operatingSystemOffset(asSys - hours(18));
	seconds after = operatingSystemOffset(asSys + hours(18));
	if (before == after)
		return before;
	if (operatingSystemOffset(asSys - before) == before) // unique before the transition, or the earlier offset of an overlap
		return before;
	if (operatingSystemOffset(asSys - after) == after)
		return after;
	return before; // gap: shifted forward by the gap length
}

} // namespace

ConnectionTimeZone::ConnectionTimeZone() : ConnectionTimeZone(determineSystemZone()) {
}

const ConnectionTimeZone& ConnectionTimeZone::determineSystemZone() {
	static const ConnectionTimeZone systemZone = []() {
		try {
			ConnectionTimeZone tz(Source::TIME_ZONE_DATABASE);
			tz.zone = current_zone();
			tz.id = std::string(tz.zone->name());
			return tz;
		} catch (const std::exception& e) {
			ConnectionTimeZone tz = operatingSystemZone();
			log().warn("The time zone database is not available ({}), using the time zone rules of the operating system for {}", e.what(), tz.id);
			return tz;
		}
	}();
	return systemZone;
}

ConnectionTimeZone ConnectionTimeZone::systemDefault() {
	return {};
}

ConnectionTimeZone ConnectionTimeZone::ofOffset(seconds offset) {
	ConnectionTimeZone tz(Source::FIXED_OFFSET);
	tz.offset = offset;
	tz.id = formatOffset(offset);
	return tz;
}

ConnectionTimeZone ConnectionTimeZone::operatingSystemZone() {
	ConnectionTimeZone tz(Source::OPERATING_SYSTEM);
	tz.id = operatingSystemZoneName();
	return tz;
}

ConnectionTimeZone ConnectionTimeZone::of(std::string_view id) {
	std::string_view trimmed = utils::StringUtils::trim(id);
	if (trimmed.empty() || utils::StringUtils::equalsIgnoreCase(trimmed, "LOCAL"))
		return systemDefault();
	if (utils::StringUtils::equalsIgnoreCase(trimmed, "SERVER")) {
		// Deviation: Connector/J would query the server's time zone. The servers of this project run next to their database, so use the local zone.
		log().warn("connectionTimeZone=SERVER is not supported, using the system time zone");
		return systemDefault();
	}
	try {
		ConnectionTimeZone tz(Source::TIME_ZONE_DATABASE);
		tz.zone = locate_zone(trimmed);
		tz.id = std::string(tz.zone->name());
		return tz;
	} catch (const std::runtime_error&) {
		// not an IANA name (or no time zone database), try an offset
	}
	seconds parsedOffset;
	if (parseOffset(trimmed, parsedOffset))
		return ofOffset(parsedOffset);
	throw SQLException(fmt::format("The server time zone value '{}' is unrecognized or represents more than one time zone.", trimmed), "01S00");
}

ConnectionTimeZone::MicrosecondsLocalTime ConnectionTimeZone::toLocal(MicrosecondsSysTime time) const {
	switch (source) {
		case Source::TIME_ZONE_DATABASE:
			return MicrosecondsLocalTime(time.time_since_epoch() + cachedOffsetOfInstant(*zone, floor<seconds>(time)));
		case Source::OPERATING_SYSTEM:
			return MicrosecondsLocalTime(time.time_since_epoch() + operatingSystemOffset(floor<seconds>(time)));
		case Source::FIXED_OFFSET:
			break;
	}
	return MicrosecondsLocalTime(time.time_since_epoch() + offset);
}

ConnectionTimeZone::MicrosecondsSysTime ConnectionTimeZone::toSys(MicrosecondsLocalTime time) const {
	switch (source) {
		case Source::TIME_ZONE_DATABASE:
			return MicrosecondsSysTime(time.time_since_epoch() - cachedOffsetOfLocalTime(*zone, floor<seconds>(time)));
		case Source::OPERATING_SYSTEM:
			return MicrosecondsSysTime(time.time_since_epoch() - operatingSystemOffsetOfLocalTime(floor<seconds>(time)));
		case Source::FIXED_OFFSET:
			break;
	}
	return MicrosecondsSysTime(time.time_since_epoch() - offset);
}

DateTimeValue ConnectionTimeZone::toDateTime(Timestamp timestamp) const {
	MicrosecondsLocalTime local = toLocal(time_point_cast<microseconds>(timestamp));
	local_days days = floor<std::chrono::days>(local);
	year_month_day ymd(days);
	hh_mm_ss<microseconds> time(local - days);
	DateTimeValue value;
	value.kind = DateTimeValue::Kind::DATETIME;
	value.year = static_cast<uint32_t>(static_cast<int>(ymd.year()));
	value.month = static_cast<unsigned>(ymd.month());
	value.day = static_cast<unsigned>(ymd.day());
	value.hour = static_cast<uint32_t>(time.hours().count());
	value.minute = static_cast<uint32_t>(time.minutes().count());
	value.second = static_cast<uint32_t>(time.seconds().count());
	value.microsecond = static_cast<uint32_t>(time.subseconds().count());
	return value;
}

Timestamp ConnectionTimeZone::toTimestamp(const DateTimeValue& value) const {
	local_days days = value.kind == DateTimeValue::Kind::TIME
		? local_days(sys_days(year{1970} / January / 1).time_since_epoch())
		: local_days(year_month_day(year(static_cast<int>(value.year)), month(value.month), day(value.day)));
	microseconds timeOfDay = hours(value.hour) + minutes(value.minute) + seconds(value.second) + microseconds(value.microsecond);
	if (value.kind == DateTimeValue::Kind::DATE)
		timeOfDay = microseconds::zero();
	else if (value.negative)
		timeOfDay = -timeOfDay;
	MicrosecondsSysTime sys = toSys(MicrosecondsLocalTime(days.time_since_epoch()) + timeOfDay);
	return floor<milliseconds>(sys);
}

} // namespace aion::commons::database
