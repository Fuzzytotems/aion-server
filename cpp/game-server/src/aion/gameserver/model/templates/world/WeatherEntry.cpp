#include "aion/gameserver/model/templates/world/WeatherEntry.h"

#include <string_view>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/model/templates/world/WeatherPhase.h"
#include "aion/gameserver/model/templates/world/WeatherTable.h"

namespace aion::gameserver::model::templates::world {

namespace {

/** Java String.trim() (code points <= U+0020) */
std::string_view trim(std::string_view s) {
	while (!s.empty() && static_cast<unsigned char>(s.front()) <= ' ')
		s.remove_prefix(1);
	while (!s.empty() && static_cast<unsigned char>(s.back()) <= ' ')
		s.remove_suffix(1);
	return s;
}

/** Java Integer.parseInt for the decimal numbers of the data (an optional sign, digits, no overflow check needed below 2^31) */
int32_t parseInt(std::string_view s) {
	size_t i = 0;
	bool negative = false;
	if (!s.empty() && (s[0] == '-' || s[0] == '+')) {
		negative = s[0] == '-';
		i = 1;
	}
	if (i == s.size())
		throw commons::utils::Exception("For input string: \"" + std::string(s) + "\"");
	int64_t value = 0;
	for (; i < s.size(); i++) {
		if (s[i] < '0' || s[i] > '9' || value > INT32_MAX)
			throw commons::utils::Exception("For input string: \"" + std::string(s) + "\"");
		value = value * 10 + (s[i] - '0');
	}
	return static_cast<int32_t>(negative ? -value : value);
}

/** Java `1 << n` (the shift distance is masked to five bits) */
int32_t bit(int32_t n) {
	return static_cast<int32_t>(1u << (static_cast<uint32_t>(n) & 31u));
}

} // namespace

// Java WeatherEntry.java:79-82: a weather without months may start all year round, an empty months never; a weather without hours never
// starts on its own
void WeatherEntry::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	try {
		monthMask = !months.has_value() ? ALL_MONTHS : parseMask(months, -1);
		hourMask = parseMask(hours.empty() ? std::nullopt : std::optional<std::string>(hours), 0); // absent and empty hours both give 0
	} catch (const commons::utils::Exception& e) {
		ctx.fail(e.what()); // Java: NumberFormatException out of afterUnmarshal fails the load
	}
}

// Java WeatherEntry.java:87-106
int32_t WeatherEntry::parseMask(const std::optional<std::string>& spec, int32_t shift) {
	if (!spec.has_value() || trim(*spec).empty()) // Java: spec == null || spec.isBlank()
		return 0;
	int32_t mask = 0;
	std::string_view rest = *spec;
	while (true) { // Java: spec.split(",") - trailing empty parts are dropped, which the isEmpty check below skips anyway
		size_t comma = rest.find(',');
		std::string_view part = trim(rest.substr(0, comma));
		if (!part.empty()) {
			size_t dash = part.find('-');
			if (dash == std::string_view::npos) {
				mask |= bit(parseInt(part) + shift);
			} else {
				int32_t from = parseInt(trim(part.substr(0, dash)));
				int32_t to = parseInt(trim(part.substr(dash + 1)));
				for (int32_t value = from; value <= to; value++)
					mask |= bit(value + shift);
			}
		}
		if (comma == std::string_view::npos)
			break;
		rest.remove_prefix(comma + 1);
	}
	return mask;
}

bool WeatherEntry::isDueIn(int32_t month, int32_t hour) const {
	return (hourMask & bit(hour)) != 0 && (monthMask & bit(month)) != 0;
}

// Java WeatherEntry.java:177-185
void WeatherEntry::resolveReferences(xml::LoadContext& ctx, const WeatherTable& table) {
	if (sign != nullptr)
		sign->resolve(ctx, table, *this, "sign");
	if (remain != nullptr)
		remain->resolve(ctx, table, *this, "remain");
	for (WeatherPhase& subWeather : subWeathers)
		subWeather.resolve(ctx, table, *this, "sub");
}

} // namespace aion::gameserver::model::templates::world
