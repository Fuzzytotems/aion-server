#include "aion/gameserver/services/cron/CronExpression.h"

#include <array>
#include <bit>
#include <cctype>
#include <format>
#include <vector>

#include "aion/commons/configuration/transformers/ZoneIdTransformer.h"

namespace aion::gameserver::services::cron {

namespace {

using namespace std::chrono;

enum class FieldType : uint8_t { SECOND, MINUTE, HOUR, DAY_OF_MONTH, MONTH, DAY_OF_WEEK, YEAR };

struct FieldSpec {
	const char* name;
	int32_t min;
	int32_t max;
	/** Quartz checkIncrementRange limits */
	int32_t maxIncrement;
};

constexpr std::array<FieldSpec, 7> SPECS{{
	{"Second", 0, 59, 59},
	{"Minute", 0, 59, 59},
	{"Hour", 0, 23, 23},
	{"Day-of-Month", 1, 31, 31},
	{"Month", 1, 12, 12},
	{"Day-of-Week", 1, 7, 7},
	{"Year", CronExpression::MIN_YEAR, CronExpression::MAX_YEAR, CronExpression::MAX_YEAR - CronExpression::MIN_YEAR},
}};

constexpr std::array<std::string_view, 12> MONTH_NAMES{"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
constexpr std::array<std::string_view, 7> DAY_NAMES{"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

/** values of one field, indexed by value - offset (offset 0 except for the year) */
using ValueBits = std::bitset<CronExpression::MAX_YEAR - CronExpression::MIN_YEAR + 1>;

class Parser {
public:
	explicit Parser(std::string_view expression) : expression(expression) {}

	[[noreturn]] void fail(const std::string& message) const {
		throw CronExpressionParseException(std::format("{} (cron expression '{}')", message, expression));
	}

	[[noreturn]] void failUnsupported(std::string_view element) const {
		fail(std::format("Support for the special characters L, W and # is not implemented: '{}'", element));
	}

	/** parses one field into `bits`; @return true if the field is `?` */
	bool parseField(FieldType type, std::string_view field, ValueBits& bits) const {
		const FieldSpec& spec = SPECS[static_cast<size_t>(type)];
		if (field.find('#') != std::string_view::npos)
			failUnsupported(field);
		if (field == "?") {
			if (type != FieldType::DAY_OF_MONTH && type != FieldType::DAY_OF_WEEK)
				fail("'?' can only be specified for Day-of-Month or Day-of-Week.");
			return true;
		}
		size_t begin = 0;
		while (begin <= field.size()) {
			size_t end = field.find(',', begin);
			if (end == std::string_view::npos)
				end = field.size();
			std::string_view element = field.substr(begin, end - begin);
			if (element.empty())
				fail(std::format("Empty list element in the {} field: '{}'", spec.name, field));
			parseElement(type, spec, element, bits);
			begin = end + 1;
		}
		return false;
	}

private:
	void parseElement(FieldType type, const FieldSpec& spec, std::string_view element, ValueBits& bits) const {
		if (element.find('?') != std::string_view::npos)
			fail(std::format("'?' must be the only value of the Day-of-Month or Day-of-Week field: '{}'", element));
		size_t pos = 0;
		int32_t start = spec.min;
		int32_t stop = spec.max;
		int32_t step = 1;
		if (element[0] == '*' || element[0] == '/') {
			if (element[0] == '*')
				pos = 1;
			if (pos < element.size() || element[0] == '/')
				step = parseStep(spec, element, pos);
		} else {
			start = parseValue(type, spec, element, pos);
			stop = start;
			bool range = false;
			if (pos < element.size() && element[pos] == '-') {
				++pos;
				stop = parseValue(type, spec, element, pos);
				range = true;
			}
			if (pos < element.size() && element[pos] == '/') {
				step = parseStep(spec, element, pos);
				if (!range)
					stop = spec.max; // Quartz: `a/n` runs to the field maximum
			}
		}
		if (pos != element.size())
			fail(std::format("Illegal characters for the {} field: '{}'", spec.name, element));

		int32_t offset = type == FieldType::YEAR ? CronExpression::MIN_YEAR : 0;
		if (stop < start) {
			if (type == FieldType::YEAR)
				fail(std::format("Start year must be less than stop year: '{}'", element));
			int32_t span = spec.max - spec.min + 1;
			for (int32_t value = start; value <= stop + span; value += step)
				bits.set(static_cast<size_t>((value - spec.min) % span + spec.min - offset));
		} else {
			for (int32_t value = start; value <= stop; value += step)
				bits.set(static_cast<size_t>(value - offset));
		}
	}

	/** at '/' (or at the end for a bare `*`): parses the increment */
	int32_t parseStep(const FieldSpec& spec, std::string_view element, size_t& pos) const {
		if (pos >= element.size() || element[pos] != '/')
			fail(std::format("Illegal characters for the {} field: '{}'", spec.name, element));
		++pos;
		if (pos >= element.size() || !std::isdigit(static_cast<unsigned char>(element[pos])))
			fail(std::format("'/' must be followed by an integer: '{}'", element));
		int32_t step = parseNumber(element, pos);
		if (step < 1)
			fail(std::format("Increment must be at least 1: '{}'", element));
		if (step > spec.maxIncrement)
			fail(std::format("Increment > {} : {}", spec.maxIncrement, step));
		return step;
	}

	int32_t parseNumber(std::string_view element, size_t& pos) const {
		int64_t value = 0;
		size_t digits = 0;
		while (pos < element.size() && std::isdigit(static_cast<unsigned char>(element[pos]))) {
			value = value * 10 + (element[pos] - '0');
			++pos;
			if (++digits > 9)
				fail(std::format("Number too large: '{}'", element));
		}
		return static_cast<int32_t>(value);
	}

	int32_t parseValue(FieldType type, const FieldSpec& spec, std::string_view element, size_t& pos) const {
		if (pos >= element.size())
			fail(std::format("Unexpected end of the {} field: '{}'", spec.name, element));
		char c = element[pos];
		int32_t value;
		if (std::isdigit(static_cast<unsigned char>(c))) {
			value = parseNumber(element, pos);
			if (pos < element.size() && (element[pos] == 'L' || element[pos] == 'W'))
				failUnsupported(element);
		} else if (c >= 'A' && c <= 'Z') {
			size_t end = pos;
			while (end < element.size() && element[end] >= 'A' && element[end] <= 'Z')
				++end;
			std::string_view name = element.substr(pos, end - pos);
			const std::string_view* names = nullptr;
			size_t count = 0;
			int32_t first = 0;
			if (type == FieldType::MONTH) {
				names = MONTH_NAMES.data();
				count = MONTH_NAMES.size();
				first = 1;
			} else if (type == FieldType::DAY_OF_WEEK) {
				names = DAY_NAMES.data();
				count = DAY_NAMES.size();
				first = 1;
			}
			int32_t found = -1;
			for (size_t i = 0; i < count; ++i)
				if (names[i] == name)
					found = static_cast<int32_t>(i) + first;
			if (found < 0) {
				bool special = name == "L" || name == "LW" || name == "W" ||
					(name.size() == 4 && name.back() == 'L' && (type == FieldType::DAY_OF_WEEK || type == FieldType::DAY_OF_MONTH));
				if (special && (type == FieldType::DAY_OF_MONTH || type == FieldType::DAY_OF_WEEK))
					failUnsupported(element);
				if (names == nullptr)
					fail(std::format("Illegal characters for this position: '{}'", element));
				fail(std::format("Invalid {} value: '{}'", spec.name, name));
			}
			value = found;
			pos = end;
		} else {
			fail(std::format("Illegal characters for the {} field: '{}'", spec.name, element));
		}
		if (value < spec.min || value > spec.max)
			fail(std::format("{} values must be between {} and {}: '{}'", spec.name, spec.min, spec.max, element));
		return value;
	}

	std::string_view expression;
};

/** smallest set bit >= from, or -1 */
int32_t nextBit(uint64_t mask, int32_t from) noexcept {
	if (from >= 64)
		return -1;
	uint64_t remaining = mask & (~uint64_t{0} << from);
	return remaining == 0 ? -1 : std::countr_zero(remaining);
}

const time_zone* resolveZone(const time_zone* zone) {
	return zone != nullptr ? zone : commons::configuration::transformers::ZoneIdTransformer::systemDefault();
}

} // namespace

CronExpression::CronExpression(std::string_view expression) : expression_(expression) {
	std::string upper(expression);
	for (char& c : upper)
		if (c >= 'a' && c <= 'z')
			c = static_cast<char>(c - 'a' + 'A');
	Parser parser(expression);

	std::vector<std::string_view> fields;
	std::string_view text = upper;
	size_t pos = 0;
	while (pos < text.size()) {
		while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t'))
			++pos;
		size_t end = pos;
		while (end < text.size() && text[end] != ' ' && text[end] != '\t')
			++end;
		if (end > pos)
			fields.push_back(text.substr(pos, end - pos));
		pos = end;
	}
	if (fields.size() < 6)
		parser.fail("Unexpected end of expression.");
	if (fields.size() > 7)
		parser.fail("Unexpected tokens after the year field.");

	std::array<ValueBits, 7> bits{};
	bool dayOfMonthUnspecified = parser.parseField(FieldType::DAY_OF_MONTH, fields[3], bits[3]);
	bool dayOfWeekUnspecified = parser.parseField(FieldType::DAY_OF_WEEK, fields[5], bits[5]);
	if (dayOfMonthUnspecified == dayOfWeekUnspecified)
		parser.fail("Support for specifying both a day-of-week AND a day-of-month parameter is not implemented.");
	for (size_t field : {0u, 1u, 2u, 4u})
		(void)parser.parseField(static_cast<FieldType>(field), fields[field], bits[field]);
	if (fields.size() == 7 && fields[6] != "*") {
		(void)parser.parseField(FieldType::YEAR, fields[6], bits[6]);
		allYears_ = false;
		years_ = bits[6];
	}

	auto low64 = [](const ValueBits& value) {
		uint64_t result = 0;
		for (size_t i = 0; i < 64; ++i)
			if (value.test(i))
				result |= uint64_t{1} << i;
		return result;
	};
	seconds_ = low64(bits[0]);
	minutes_ = low64(bits[1]);
	hours_ = static_cast<uint32_t>(low64(bits[2]));
	daysOfMonth_ = static_cast<uint32_t>(low64(bits[3]));
	months_ = static_cast<uint16_t>(low64(bits[4]));
	daysOfWeek_ = static_cast<uint8_t>(low64(bits[5]));
	dayOfMonthUnspecified_ = dayOfMonthUnspecified;
}

bool CronExpression::isValidExpression(std::string_view expression) noexcept {
	try {
		CronExpression parsed(expression);
		return true;
	} catch (...) {
		return false;
	}
}

bool CronExpression::isSatisfiedBy(sys_seconds time, const time_zone* zone) const {
	std::optional<sys_seconds> next = getNextValidTimeAfter(time - seconds(1), zone);
	return next && *next == time;
}

bool CronExpression::matchesDay(local_days day) const noexcept {
	if (dayOfMonthUnspecified_) {
		unsigned dayOfWeek = weekday(day).c_encoding() + 1; // Quartz: 1 = SUN
		return ((daysOfWeek_ >> dayOfWeek) & 1u) != 0;
	}
	unsigned dayOfMonth = static_cast<unsigned>(year_month_day(day).day());
	return ((daysOfMonth_ >> dayOfMonth) & 1u) != 0;
}

std::optional<sys_seconds> CronExpression::getNextValidTimeAfter(sys_seconds after, const time_zone* zone) const {
	zone = resolveZone(zone);
	local_seconds t = zone->to_local(after) + seconds(1);
	for (;;) {
		local_days day = floor<days>(t);
		year_month_day date(day);
		int32_t y = static_cast<int32_t>(date.year());
		if (y > MAX_YEAR)
			return std::nullopt;
		if (y < MIN_YEAR) {
			t = local_days(year(MIN_YEAR) / 1 / 1);
			continue;
		}
		if (!allYears_ && !years_.test(static_cast<size_t>(y - MIN_YEAR))) {
			int32_t next = y + 1;
			while (next <= MAX_YEAR && !years_.test(static_cast<size_t>(next - MIN_YEAR)))
				++next;
			if (next > MAX_YEAR)
				return std::nullopt;
			t = local_days(year(next) / 1 / 1);
			continue;
		}
		int32_t m = static_cast<int32_t>(static_cast<unsigned>(date.month()));
		if (((months_ >> m) & 1u) == 0) {
			int32_t next = nextBit(months_, m + 1);
			t = next > 0 ? local_days(year(y) / month(static_cast<unsigned>(next)) / 1) : local_days(year(y + 1) / 1 / 1);
			continue;
		}
		if (!matchesDay(day)) {
			t = day + days(1);
			continue;
		}
		hh_mm_ss<seconds> time(t - day);
		int32_t h = static_cast<int32_t>(time.hours().count());
		int32_t minute = static_cast<int32_t>(time.minutes().count());
		int32_t second = static_cast<int32_t>(time.seconds().count());
		if (((hours_ >> h) & 1u) == 0) {
			int32_t next = nextBit(hours_, h + 1);
			t = next >= 0 ? day + hours(next) : day + days(1);
			continue;
		}
		if (((minutes_ >> minute) & 1u) == 0) {
			int32_t next = nextBit(minutes_, minute + 1);
			t = next >= 0 ? day + hours(h) + minutes(next) : day + hours(h + 1);
			continue;
		}
		if (((seconds_ >> second) & 1u) == 0) {
			int32_t next = nextBit(seconds_, second + 1);
			t = next >= 0 ? day + hours(h) + minutes(minute) + seconds(next) : day + hours(h) + minutes(minute + 1);
			continue;
		}

		local_info info = zone->get_info(t);
		if (info.result == local_info::nonexistent) {
			t += seconds(1);
			continue;
		}
		// unique, or ambiguous: the earlier instant (offset before the transition)
		sys_seconds instant(t.time_since_epoch() - info.first.offset);
		if (instant <= after) {
			t += seconds(1);
			continue;
		}
		return instant;
	}
}

} // namespace aion::gameserver::services::cron
