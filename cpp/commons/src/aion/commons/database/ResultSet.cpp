#include "aion/commons/database/ResultSet.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>

#include <fmt/format.h>
#include <magic_enum/magic_enum.hpp>

#include "aion/commons/database/SQLException.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::database {

namespace {

using utils::StringUtils::equalsIgnoreCase;

// SQLSTATEs used by Connector/J for these errors
const char* SQL_STATE_GENERAL_ERROR = "S1000";
const char* SQL_STATE_ILLEGAL_ARGUMENT = "S1009";
const char* SQL_STATE_COLUMN_NOT_FOUND = "S0022";
const char* SQL_STATE_NUMERIC_VALUE_OUT_OF_RANGE = "22003";

enum class Storage { SIGNED, UNSIGNED, DOUBLE, BYTES, DATE_TIME };

Storage storageOf(const ColumnDefinition& column) noexcept {
	switch (column.type) {
		case ColumnType::TINYINT:
		case ColumnType::SMALLINT:
		case ColumnType::MEDIUMINT:
		case ColumnType::INT:
		case ColumnType::BIGINT:
		case ColumnType::YEAR:
			return column.isUnsigned ? Storage::UNSIGNED : Storage::SIGNED;
		case ColumnType::FLOAT:
		case ColumnType::DOUBLE:
			return Storage::DOUBLE;
		case ColumnType::DATE:
		case ColumnType::TIME:
		case ColumnType::DATETIME:
		case ColumnType::TIMESTAMP:
			return Storage::DATE_TIME;
		default:
			return Storage::BYTES;
	}
}

std::string typeName(const ColumnDefinition& column) {
	std::string name(column.type == ColumnType::NULL_TYPE ? "NULL" : magic_enum::enum_name(column.type));
	if (column.isUnsigned)
		name += " UNSIGNED";
	return name;
}

[[noreturn]] void throwOutOfRange(std::string_view value, std::string_view targetType) {
	throw SQLException(fmt::format("Value '{}' is outside of valid range for type {}", value, targetType), SQL_STATE_NUMERIC_VALUE_OUT_OF_RANGE);
}

[[noreturn]] void throwUnsupportedConversion(const ColumnDefinition& column, std::string_view targetType) {
	throw SQLException(fmt::format("Unsupported conversion from {} to {}", typeName(column), targetType), SQL_STATE_ILLEGAL_ARGUMENT);
}

[[noreturn]] void throwUninterpretable(std::string_view value) {
	throw SQLException(fmt::format("Cannot determine value type from string '{}'", value), SQL_STATE_ILLEGAL_ARGUMENT);
}

/** Java: Double.parseDouble for the strings that Connector/J passes to it. Values beyond the double range become 0 or infinity. */
double parseDouble(std::string_view text) {
	double value = 0;
	auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (end != text.data() + text.size() || (ec != std::errc() && ec != std::errc::result_out_of_range))
		throwUninterpretable(text);
	if (ec == std::errc::result_out_of_range) {
		bool negative = text.starts_with('-');
		size_t exponent = text.find_first_of("eE");
		bool tiny = exponent != std::string_view::npos && exponent + 1 < text.size() && text[exponent + 1] == '-';
		if (tiny)
			return negative ? -0.0 : 0.0;
		return negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
	}
	return value;
}

enum class NumberFormat { EMPTY, FLOATING, INTEGER, INVALID };

/** Connector/J AbstractNumericValueFactory.createFromBytes: contains e/E or matches -?\d*\.\d* is floating, -?\d+ is integer. */
NumberFormat classifyNumber(std::string_view s) noexcept {
	if (s.empty())
		return NumberFormat::EMPTY;
	if (s.find_first_of("eE") != std::string_view::npos)
		return NumberFormat::FLOATING;
	size_t pos = s.starts_with('-') ? 1 : 0;
	size_t digitsBefore = 0, dots = 0, digitsAfter = 0;
	for (; pos < s.size(); ++pos) {
		char c = s[pos];
		if (c >= '0' && c <= '9') {
			(dots == 0 ? digitsBefore : digitsAfter)++;
		} else if (c == '.' && dots == 0) {
			++dots;
		} else {
			return NumberFormat::INVALID;
		}
	}
	if (dots == 1)
		return NumberFormat::FLOATING;
	return digitsBefore > 0 ? NumberFormat::INTEGER : NumberFormat::INVALID;
}

/** Java: (long) d with the range check of Connector/J's createFromDouble. */
int64_t doubleToInteger(double d, int64_t min, int64_t max, std::string_view targetType) {
	if (d < static_cast<double>(min) || d > static_cast<double>(max))
		throwOutOfRange(fmt::format("{}", d), targetType);
	if (std::isnan(d))
		return 0;
	if (d >= 9223372036854775807.0) // 2^63: Java saturates
		return std::numeric_limits<int64_t>::max();
	return static_cast<int64_t>(d);
}

int64_t bitToLong(std::string_view bytes) noexcept {
	uint64_t value = 0;
	for (char b : bytes)
		value = (value << 8) | static_cast<uint8_t>(b);
	return static_cast<int64_t>(value);
}

struct DecimalParts {
	std::optional<int64_t> integerPart; // nullopt: beyond int64_t
	bool negative = false;
	bool fractionNonZero = false;
};

std::optional<DecimalParts> splitDecimal(std::string_view text) noexcept {
	DecimalParts parts;
	std::string_view s = text;
	if (s.starts_with('-')) {
		parts.negative = true;
		s.remove_prefix(1);
	} else if (s.starts_with('+')) {
		s.remove_prefix(1);
	}
	size_t dot = s.find('.');
	std::string_view integer = s.substr(0, dot);
	std::string_view fraction = dot == std::string_view::npos ? std::string_view() : s.substr(dot + 1);
	for (char c : fraction) {
		if (c < '0' || c > '9')
			return std::nullopt;
		if (c != '0')
			parts.fractionNonZero = true;
	}
	if (integer.empty()) {
		parts.integerPart = 0;
		return fraction.empty() ? std::nullopt : std::optional(parts);
	}
	uint64_t magnitude = 0;
	auto [end, ec] = std::from_chars(integer.data(), integer.data() + integer.size(), magnitude);
	if (end != integer.data() + integer.size() && ec != std::errc::result_out_of_range)
		return std::nullopt;
	if (ec == std::errc::result_out_of_range || magnitude > (parts.negative ? 9223372036854775808ull : 9223372036854775807ull))
		return parts; // integerPart stays empty
	parts.integerPart = parts.negative ? static_cast<int64_t>(0 - magnitude) : static_cast<int64_t>(magnitude);
	return parts;
}

/** Converts a cell to an integer in [min, max] like Connector/J's Byte/Short/Integer/LongValueFactory. */
int64_t toInteger(const ColumnDefinition& column, Storage storage, int64_t i, uint64_t u, double d, std::string_view bytes, int64_t min, int64_t max,
	std::string_view targetType) {
	auto checkRange = [&](int64_t value) {
		if (value < min || value > max)
			throwOutOfRange(std::to_string(value), targetType);
		return value;
	};
	switch (storage) {
		case Storage::SIGNED:
			return checkRange(i);
		case Storage::UNSIGNED:
			if (u > static_cast<uint64_t>(max))
				throwOutOfRange(std::to_string(u), targetType);
			return static_cast<int64_t>(u);
		case Storage::DOUBLE:
			return doubleToInteger(d, min, max, targetType);
		case Storage::DATE_TIME:
			throwUnsupportedConversion(column, targetType);
		case Storage::BYTES:
			break;
	}
	if (column.type == ColumnType::BIT)
		return checkRange(bitToLong(bytes));
	if (column.type == ColumnType::DECIMAL) {
		// Connector/J createFromBigDecimal: exact range check, then truncation towards zero
		auto parts = splitDecimal(bytes);
		if (!parts)
			throwUninterpretable(bytes);
		if (!parts->integerPart || *parts->integerPart > max || *parts->integerPart < min ||
			(*parts->integerPart == max && parts->fractionNonZero && !parts->negative) ||
			(*parts->integerPart == min && parts->fractionNonZero && parts->negative))
			throwOutOfRange(bytes, targetType);
		return *parts->integerPart;
	}
	switch (classifyNumber(bytes)) {
		case NumberFormat::EMPTY: // emptyStringsConvertToZero
			return 0;
		case NumberFormat::FLOATING:
			return doubleToInteger(parseDouble(bytes), min, max, targetType);
		case NumberFormat::INTEGER: {
			int64_t value = 0;
			auto [end, ec] = std::from_chars(bytes.data(), bytes.data() + bytes.size(), value);
			if (ec == std::errc::result_out_of_range)
				throwOutOfRange(bytes, targetType); // BigInteger path
			if (ec != std::errc() || end != bytes.data() + bytes.size())
				throwUninterpretable(bytes);
			return checkRange(value);
		}
		case NumberFormat::INVALID:
			break;
	}
	throwUninterpretable(bytes);
}

std::string formatDouble(const ColumnDefinition& column, double d) {
	if (column.type == ColumnType::FLOAT)
		return fmt::format("{}", static_cast<float>(d));
	return fmt::format("{}", d);
}

const char* TIMESTAMP_TYPE = "java.sql.Timestamp";
const char* DATE_TYPE = "java.sql.Date";

} // namespace

// ---------------------------------------------------------------------------------------------------------------------------------------------
// ResultSetMetaData

int32_t ResultSetMetaData::getColumnCount() const noexcept {
	return static_cast<int32_t>(resultSet->columns.size());
}

const ColumnDefinition& ResultSetMetaData::getColumn(int32_t column) const {
	return resultSet->column(column);
}

std::string ResultSetMetaData::getColumnLabel(int32_t column) const {
	return getColumn(column).label;
}

std::string ResultSetMetaData::getColumnName(int32_t column) const {
	const ColumnDefinition& def = getColumn(column);
	return def.name.empty() ? def.label : def.name;
}

std::string ResultSetMetaData::getTableName(int32_t column) const {
	return getColumn(column).table;
}

std::string ResultSetMetaData::getCatalogName(int32_t column) const {
	return getColumn(column).database;
}

int32_t ResultSetMetaData::getColumnType(int32_t column) const {
	const ColumnDefinition& def = getColumn(column);
	switch (def.type) {
		case ColumnType::NULL_TYPE:
			return Types::NULL_TYPE;
		case ColumnType::TINYINT:
			return def.length == 1 ? Types::BIT : Types::TINYINT; // tinyInt1isBit
		case ColumnType::SMALLINT:
			return Types::SMALLINT;
		case ColumnType::MEDIUMINT:
		case ColumnType::INT:
			return Types::INTEGER;
		case ColumnType::BIGINT:
			return Types::BIGINT;
		case ColumnType::YEAR:
			return Types::DATE; // yearIsDateType
		case ColumnType::FLOAT:
			return Types::REAL;
		case ColumnType::DOUBLE:
			return Types::DOUBLE;
		case ColumnType::DECIMAL:
			return Types::DECIMAL;
		case ColumnType::BIT:
			return Types::BIT;
		case ColumnType::DATE:
			return Types::DATE;
		case ColumnType::TIME:
			return Types::TIME;
		case ColumnType::DATETIME:
		case ColumnType::TIMESTAMP:
			return Types::TIMESTAMP;
		case ColumnType::CHAR:
		case ColumnType::ENUM:
		case ColumnType::SET:
			return Types::CHAR;
		case ColumnType::VARCHAR:
			return Types::VARCHAR;
		case ColumnType::TEXT:
		case ColumnType::JSON:
			return Types::LONGVARCHAR;
		case ColumnType::BINARY:
		case ColumnType::GEOMETRY:
			return Types::BINARY;
		case ColumnType::VARBINARY:
			return Types::VARBINARY;
		case ColumnType::BLOB:
			return Types::LONGVARBINARY;
	}
	return Types::OTHER;
}

std::string ResultSetMetaData::getColumnTypeName(int32_t column) const {
	return typeName(getColumn(column));
}

bool ResultSetMetaData::isSigned(int32_t column) const {
	const ColumnDefinition& def = getColumn(column);
	Storage storage = storageOf(def);
	return !def.isUnsigned && (storage == Storage::SIGNED || storage == Storage::DOUBLE || def.type == ColumnType::DECIMAL);
}

int32_t ResultSetMetaData::getScale(int32_t column) const {
	return static_cast<int32_t>(getColumn(column).decimals);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// ResultSet

ResultSet::ResultSet(std::vector<ColumnDefinition> columns, ResultSetOptions options) : columns(std::move(columns)), options(std::move(options)) {
}

void ResultSet::checkScrollable() const {
	if (options.type == TYPE_FORWARD_ONLY)
		throw SQLException("Operation not allowed for a result set of type ResultSet.TYPE_FORWARD_ONLY.", SQL_STATE_ILLEGAL_ARGUMENT);
}

bool ResultSet::next() {
	const auto count = static_cast<int64_t>(rowCount);
	if (position < count)
		++position;
	return position < count;
}

bool ResultSet::previous() {
	checkScrollable();
	if (position - 1 >= 0) {
		--position;
		return true;
	}
	if (position - 1 == -1)
		position = -1;
	return false;
}

bool ResultSet::first() {
	checkScrollable();
	if (rowCount == 0)
		return false;
	position = 0;
	return true;
}

bool ResultSet::last() {
	checkScrollable();
	if (rowCount == 0)
		return false;
	position = static_cast<int64_t>(rowCount) - 1;
	return true;
}

void ResultSet::beforeFirst() {
	checkScrollable();
	position = -1;
}

void ResultSet::afterLast() {
	checkScrollable();
	if (rowCount != 0)
		position = static_cast<int64_t>(rowCount);
}

bool ResultSet::absolute(int32_t row) {
	checkScrollable();
	if (rowCount == 0)
		return false;
	const auto count = static_cast<int64_t>(rowCount);
	int64_t target = row;
	if (target < 0) {
		target = count + target + 1;
		if (target <= 0) {
			position = -1;
			return false;
		}
	}
	if (target == 0) {
		position = -1;
		return false;
	}
	if (target > count) {
		position = count;
		return false;
	}
	position = target - 1;
	return true;
}

bool ResultSet::relative(int32_t rows) {
	checkScrollable();
	if (rowCount == 0)
		return false;
	// Deviation: Connector/J does not clamp the position (moving far before the first row reports a valid row); here it stops before the first
	// or after the last row.
	position = std::clamp<int64_t>(position + rows, -1, static_cast<int64_t>(rowCount));
	return position >= 0 && position < static_cast<int64_t>(rowCount);
}

bool ResultSet::isBeforeFirst() const noexcept {
	return rowCount != 0 && position == -1;
}

bool ResultSet::isAfterLast() const noexcept {
	return rowCount != 0 && position >= static_cast<int64_t>(rowCount);
}

bool ResultSet::isFirst() const noexcept {
	return rowCount != 0 && position == 0;
}

bool ResultSet::isLast() const noexcept {
	return rowCount != 0 && position == static_cast<int64_t>(rowCount) - 1;
}

int32_t ResultSet::getRow() const noexcept {
	if (position < 0 || position >= static_cast<int64_t>(rowCount))
		return 0;
	return static_cast<int32_t>(std::min<int64_t>(position + 1, std::numeric_limits<int32_t>::max()));
}

int32_t ResultSet::findColumn(std::string_view columnLabel) const {
	for (size_t i = 0; i < columns.size(); ++i) {
		if (equalsIgnoreCase(columns[i].label, columnLabel))
			return static_cast<int32_t>(i + 1);
	}
	for (size_t i = 0; i < columns.size(); ++i) {
		if (!columns[i].name.empty() && equalsIgnoreCase(columns[i].name, columnLabel))
			return static_cast<int32_t>(i + 1);
	}
	for (size_t i = 0; i < columns.size(); ++i) {
		const ColumnDefinition& c = columns[i];
		if (!c.table.empty() && columnLabel.size() == c.table.size() + 1 + c.name.size() && columnLabel[c.table.size()] == '.' &&
			equalsIgnoreCase(columnLabel.substr(0, c.table.size()), c.table) && equalsIgnoreCase(columnLabel.substr(c.table.size() + 1), c.name))
			return static_cast<int32_t>(i + 1);
	}
	throw SQLException(fmt::format("Column '{}' not found.", columnLabel), SQL_STATE_COLUMN_NOT_FOUND);
}

const ColumnDefinition& ResultSet::column(int32_t columnIndex) const {
	if (columnIndex < 1)
		throw SQLException(fmt::format("Column Index out of range, {} < 1.", columnIndex), SQL_STATE_ILLEGAL_ARGUMENT);
	if (static_cast<size_t>(columnIndex) > columns.size())
		throw SQLException(fmt::format("Column Index out of range, {} > {}.", columnIndex, columns.size()), SQL_STATE_ILLEGAL_ARGUMENT);
	return columns[static_cast<size_t>(columnIndex) - 1];
}

const ResultSet::Cell& ResultSet::cell(int32_t columnIndex) const {
	if (rowCount == 0)
		throw SQLException("Illegal operation on empty result set.", SQL_STATE_GENERAL_ERROR);
	if (position < 0)
		throw SQLException("Before start of result set", SQL_STATE_GENERAL_ERROR);
	if (position >= static_cast<int64_t>(rowCount))
		throw SQLException("After end of result set", SQL_STATE_GENERAL_ERROR);
	column(columnIndex);
	const Cell& c = cells[static_cast<size_t>(position) * columns.size() + static_cast<size_t>(columnIndex) - 1];
	lastWasNull = c.isNull;
	return c;
}

bool ResultSet::getBoolean(int32_t columnIndex) const {
	const Cell& c = cell(columnIndex);
	if (c.isNull)
		return false;
	const ColumnDefinition& def = column(columnIndex);
	// Connector/J BooleanValueFactory: -1 and positive numbers are true
	auto fromLong = [](int64_t v) { return v == -1 || v > 0; };
	auto fromDouble = [](double d) { return d == -1.0 || d > 0; };
	switch (storageOf(def)) {
		case Storage::SIGNED:
			return fromLong(c.i);
		case Storage::UNSIGNED:
			return c.u > 0;
		case Storage::DOUBLE:
			return fromDouble(c.d);
		case Storage::DATE_TIME:
			throwUnsupportedConversion(def, "java.lang.Boolean");
		case Storage::BYTES:
			break;
	}
	std::string_view bytes = bytesOf(c);
	if (def.type == ColumnType::BIT)
		return fromLong(bitToLong(bytes));
	if (def.type == ColumnType::DECIMAL)
		return fromDouble(parseDouble(bytes));
	if (bytes.empty())
		return false;
	std::string_view s = utils::StringUtils::trim(bytes);
	if (equalsIgnoreCase(s, "Y") || equalsIgnoreCase(s, "true"))
		return true;
	if (equalsIgnoreCase(s, "N") || equalsIgnoreCase(s, "false"))
		return false;
	switch (classifyNumber(s)) {
		case NumberFormat::FLOATING:
			return fromDouble(parseDouble(s));
		case NumberFormat::INTEGER: {
			int64_t value = 0;
			auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
			if (ec == std::errc::result_out_of_range)
				return !s.starts_with('-'); // BigInteger: only huge positive values are true
			return fromLong(value);
		}
		default:
			throwUninterpretable(s);
	}
}

int8_t ResultSet::getByte(int32_t columnIndex) const {
	const Cell& c = cell(columnIndex);
	if (c.isNull)
		return 0;
	return static_cast<int8_t>(toInteger(column(columnIndex), storageOf(column(columnIndex)), c.i, c.u, c.d, bytesOf(c), std::numeric_limits<int8_t>::min(),
		std::numeric_limits<int8_t>::max(), "java.lang.Byte"));
}

int16_t ResultSet::getShort(int32_t columnIndex) const {
	const Cell& c = cell(columnIndex);
	if (c.isNull)
		return 0;
	return static_cast<int16_t>(toInteger(column(columnIndex), storageOf(column(columnIndex)), c.i, c.u, c.d, bytesOf(c), std::numeric_limits<int16_t>::min(),
		std::numeric_limits<int16_t>::max(), "java.lang.Short"));
}

int32_t ResultSet::getInt(int32_t columnIndex) const {
	const Cell& c = cell(columnIndex);
	if (c.isNull)
		return 0;
	return static_cast<int32_t>(toInteger(column(columnIndex), storageOf(column(columnIndex)), c.i, c.u, c.d, bytesOf(c), std::numeric_limits<int32_t>::min(),
		std::numeric_limits<int32_t>::max(), "java.lang.Integer"));
}

int64_t ResultSet::getLong(int32_t columnIndex) const {
	const Cell& c = cell(columnIndex);
	if (c.isNull)
		return 0;
	return toInteger(column(columnIndex), storageOf(column(columnIndex)), c.i, c.u, c.d, bytesOf(c), std::numeric_limits<int64_t>::min(),
		std::numeric_limits<int64_t>::max(), "java.lang.Long");
}

double ResultSet::getDouble(int32_t columnIndex) const {
	const Cell& c = cell(columnIndex);
	if (c.isNull)
		return 0;
	const ColumnDefinition& def = column(columnIndex);
	switch (storageOf(def)) {
		case Storage::SIGNED:
			return static_cast<double>(c.i);
		case Storage::UNSIGNED:
			return static_cast<double>(c.u);
		case Storage::DOUBLE:
			return c.d;
		case Storage::DATE_TIME:
			throwUnsupportedConversion(def, "java.lang.Double");
		case Storage::BYTES:
			break;
	}
	std::string_view bytes = bytesOf(c);
	if (def.type == ColumnType::BIT)
		return static_cast<double>(bitToLong(bytes));
	if (def.type == ColumnType::DECIMAL)
		return parseDouble(bytes);
	switch (classifyNumber(bytes)) {
		case NumberFormat::EMPTY:
			return 0;
		case NumberFormat::FLOATING:
		case NumberFormat::INTEGER:
			return parseDouble(bytes);
		case NumberFormat::INVALID:
			break;
	}
	throwUninterpretable(bytes);
}

float ResultSet::getFloat(int32_t columnIndex) const {
	const Cell& c = cell(columnIndex);
	if (c.isNull)
		return 0;
	const ColumnDefinition& def = column(columnIndex);
	Storage storage = storageOf(def);
	if (storage == Storage::SIGNED)
		return static_cast<float>(c.i);
	if (storage == Storage::UNSIGNED)
		return static_cast<float>(c.u);
	if (storage == Storage::DATE_TIME)
		throwUnsupportedConversion(def, "java.lang.Float");
	double d = getDouble(columnIndex);
	// Connector/J FloatValueFactory.createFromDouble
	if (d < -std::numeric_limits<float>::max() || d > std::numeric_limits<float>::max())
		throwOutOfRange(storage == Storage::BYTES ? std::string(bytesOf(c)) : fmt::format("{}", d), "java.lang.Float");
	return static_cast<float>(d);
}

std::string ResultSet::getString(int32_t columnIndex) const {
	const Cell& c = cell(columnIndex);
	if (c.isNull)
		return {};
	const ColumnDefinition& def = column(columnIndex);
	switch (storageOf(def)) {
		case Storage::SIGNED:
			return std::to_string(c.i);
		case Storage::UNSIGNED:
			return std::to_string(c.u);
		case Storage::DOUBLE:
			return formatDouble(def, c.d);
		case Storage::DATE_TIME:
			return dateTimes[c.offset].toString(def.decimals);
		case Storage::BYTES:
			break;
	}
	if (def.type == ColumnType::BIT)
		return std::to_string(bitToLong(bytesOf(c)));
	return std::string(bytesOf(c));
}

std::vector<uint8_t> ResultSet::getBytes(int32_t columnIndex) const {
	const Cell& c = cell(columnIndex);
	if (c.isNull)
		return {};
	if (storageOf(column(columnIndex)) == Storage::BYTES) {
		std::string_view data = bytesOf(c);
		return std::vector<uint8_t>(data.begin(), data.end());
	}
	std::string text = getString(columnIndex);
	return std::vector<uint8_t>(text.begin(), text.end());
}

std::optional<Timestamp> ResultSet::getTimestamp(int32_t columnIndex) const {
	const Cell& c = cell(columnIndex);
	if (c.isNull)
		return std::nullopt;
	const ColumnDefinition& def = column(columnIndex);
	DateTimeValue value;
	switch (storageOf(def)) {
		case Storage::DATE_TIME:
			value = dateTimes[c.offset];
			break;
		case Storage::BYTES: {
			if (def.type == ColumnType::BIT || def.type == ColumnType::DECIMAL)
				throwUnsupportedConversion(def, TIMESTAMP_TYPE);
			auto parsed = DateTimeValue::parse(bytesOf(c));
			if (!parsed)
				throw SQLException(fmt::format("Cannot convert string '{}' to {} value", bytesOf(c), TIMESTAMP_TYPE), SQL_STATE_ILLEGAL_ARGUMENT);
			value = *parsed;
			break;
		}
		case Storage::SIGNED:
		case Storage::UNSIGNED:
			if (def.type != ColumnType::YEAR)
				throwUnsupportedConversion(def, TIMESTAMP_TYPE);
			value.kind = DateTimeValue::Kind::DATE;
			value.year = static_cast<uint32_t>(def.isUnsigned ? c.u : static_cast<uint64_t>(c.i));
			value.month = 1;
			value.day = 1;
			break;
		case Storage::DOUBLE:
			throwUnsupportedConversion(def, TIMESTAMP_TYPE);
	}
	if (value.isZeroDate()) {
		switch (options.zeroDateTimeBehavior) {
			case ZeroDateTimeBehavior::EXCEPTION:
				throw SQLException("Zero date value prohibited", SQL_STATE_ILLEGAL_ARGUMENT);
			case ZeroDateTimeBehavior::CONVERT_TO_NULL:
				return std::nullopt;
			case ZeroDateTimeBehavior::ROUND:
				value = DateTimeValue{.kind = DateTimeValue::Kind::DATETIME, .year = 1, .month = 1, .day = 1};
				break;
		}
	}
	if (value.kind == DateTimeValue::Kind::TIME && (value.negative || value.hour > 23))
		throw SQLException(fmt::format("The value '{}' is an invalid TIME value. JDBC Time objects represent a wall-clock time and not a duration as MySQL treats them. If you are treating this type as a duration, consider retrieving this value as a string and dealing with it according to your requirements.",
			value.toString()), SQL_STATE_ILLEGAL_ARGUMENT);
	return options.timeZone.toTimestamp(value);
}

std::optional<Date> ResultSet::getDate(int32_t columnIndex) const {
	const Cell& c = cell(columnIndex);
	if (c.isNull)
		return std::nullopt;
	const ColumnDefinition& def = column(columnIndex);
	DateTimeValue value;
	switch (storageOf(def)) {
		case Storage::DATE_TIME:
			value = dateTimes[c.offset];
			break;
		case Storage::BYTES: {
			if (def.type == ColumnType::BIT || def.type == ColumnType::DECIMAL)
				throwUnsupportedConversion(def, DATE_TYPE);
			auto parsed = DateTimeValue::parse(bytesOf(c));
			if (!parsed)
				throw SQLException(fmt::format("Cannot convert string '{}' to {} value", bytesOf(c), DATE_TYPE), SQL_STATE_ILLEGAL_ARGUMENT);
			value = *parsed;
			break;
		}
		case Storage::SIGNED:
		case Storage::UNSIGNED:
			if (def.type != ColumnType::YEAR)
				throwUnsupportedConversion(def, DATE_TYPE);
			value.kind = DateTimeValue::Kind::DATE;
			value.year = static_cast<uint32_t>(def.isUnsigned ? c.u : static_cast<uint64_t>(c.i));
			value.month = 1;
			value.day = 1;
			break;
		case Storage::DOUBLE:
			throwUnsupportedConversion(def, DATE_TYPE);
	}
	if (value.kind == DateTimeValue::Kind::TIME)
		return Date(std::chrono::year(1970), std::chrono::January, std::chrono::day(1));
	if (value.isZeroDate()) {
		switch (options.zeroDateTimeBehavior) {
			case ZeroDateTimeBehavior::EXCEPTION:
				throw SQLException("Zero date value prohibited", SQL_STATE_ILLEGAL_ARGUMENT);
			case ZeroDateTimeBehavior::CONVERT_TO_NULL:
				return std::nullopt;
			case ZeroDateTimeBehavior::ROUND:
				return Date(std::chrono::year(1), std::chrono::January, std::chrono::day(1));
		}
	}
	return Date(std::chrono::year(static_cast<int>(value.year)), std::chrono::month(value.month), std::chrono::day(value.day));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// ResultSetBuilder

ResultSetBuilder::ResultSetBuilder(std::vector<ColumnDefinition> columns, ResultSetOptions options)
	: resultSet(new ResultSet(std::move(columns), std::move(options))) {
}

ResultSetBuilder::~ResultSetBuilder() = default;

const ColumnDefinition& ResultSetBuilder::nextColumn() {
	if (!resultSet)
		throw utils::IllegalStateException("ResultSet was already built");
	if (resultSet->columns.empty())
		throw utils::IllegalStateException("ResultSet has no columns");
	auto& cells = resultSet->cells;
	cells.emplace_back();
	return resultSet->columns[(cells.size() - 1) % resultSet->columns.size()];
}

ResultSetBuilder& ResultSetBuilder::addNull() {
	nextColumn();
	return *this;
}

ResultSetBuilder& ResultSetBuilder::addLong(int64_t value) {
	Storage storage = storageOf(nextColumn());
	auto& cell = resultSet->cells.back();
	if (storage == Storage::SIGNED) {
		cell.i = value;
	} else if (storage == Storage::UNSIGNED && value >= 0) {
		cell.u = static_cast<uint64_t>(value);
	} else {
		resultSet->cells.pop_back();
		throw utils::IllegalArgumentException("Column does not store signed integers");
	}
	cell.isNull = false;
	return *this;
}

ResultSetBuilder& ResultSetBuilder::addUnsignedLong(uint64_t value) {
	Storage storage = storageOf(nextColumn());
	auto& cell = resultSet->cells.back();
	if (storage == Storage::UNSIGNED) {
		cell.u = value;
	} else if (storage == Storage::SIGNED && value <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
		cell.i = static_cast<int64_t>(value);
	} else {
		resultSet->cells.pop_back();
		throw utils::IllegalArgumentException("Column does not store unsigned integers");
	}
	cell.isNull = false;
	return *this;
}

ResultSetBuilder& ResultSetBuilder::addDouble(double value) {
	if (storageOf(nextColumn()) != Storage::DOUBLE) {
		resultSet->cells.pop_back();
		throw utils::IllegalArgumentException("Column does not store floating point numbers");
	}
	auto& cell = resultSet->cells.back();
	cell.d = value;
	cell.isNull = false;
	return *this;
}

ResultSetBuilder& ResultSetBuilder::addBytes(std::string_view value) {
	if (storageOf(nextColumn()) != Storage::BYTES) {
		resultSet->cells.pop_back();
		throw utils::IllegalArgumentException("Column does not store bytes");
	}
	if (value.size() > std::numeric_limits<uint32_t>::max()) {
		resultSet->cells.pop_back();
		throw utils::IllegalArgumentException("Value too large");
	}
	auto& cell = resultSet->cells.back();
	cell.offset = resultSet->byteData.size();
	cell.size = static_cast<uint32_t>(value.size());
	cell.isNull = false;
	resultSet->byteData.append(value);
	return *this;
}

ResultSetBuilder& ResultSetBuilder::addDateTime(const DateTimeValue& value) {
	if (storageOf(nextColumn()) != Storage::DATE_TIME) {
		resultSet->cells.pop_back();
		throw utils::IllegalArgumentException("Column does not store temporal values");
	}
	auto& cell = resultSet->cells.back();
	cell.offset = resultSet->dateTimes.size();
	cell.isNull = false;
	resultSet->dateTimes.push_back(value);
	return *this;
}

void ResultSetBuilder::reserve(size_t rows) {
	if (resultSet)
		resultSet->cells.reserve(rows * resultSet->columns.size());
}

std::unique_ptr<ResultSet> ResultSetBuilder::build() {
	if (!resultSet)
		throw utils::IllegalStateException("ResultSet was already built");
	size_t columnCount = resultSet->columns.size();
	if (columnCount != 0 && resultSet->cells.size() % columnCount != 0)
		throw utils::IllegalStateException("The last row is incomplete");
	resultSet->rowCount = columnCount == 0 ? 0 : resultSet->cells.size() / columnCount;
	return std::move(resultSet);
}

} // namespace aion::commons::database
