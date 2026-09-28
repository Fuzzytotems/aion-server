#include "aion/commons/database/StatementParameters.h"

#include <fmt/format.h>

#include "aion/commons/database/SQLException.h"

namespace aion::commons::database {

StatementParameters::Value& StatementParameters::at(int32_t parameterIndex) {
	if (parameterIndex < 1 || static_cast<size_t>(parameterIndex) > values.size()) {
		if (parameterIndex < 1)
			throw SQLException(fmt::format("Parameter index out of range ({} < 1 ).", parameterIndex), "S1009");
		throw SQLException(fmt::format("Parameter index out of range ({} > number of parameters, which is {}).", parameterIndex, values.size()), "S1009");
	}
	Value& value = values[static_cast<size_t>(parameterIndex) - 1];
	value = Value{};
	return value;
}

void StatementParameters::setNull(int32_t parameterIndex, int32_t sqlType) {
	Value& value = at(parameterIndex);
	value.kind = Kind::NULL_VALUE;
	value.sqlType = sqlType;
}

void StatementParameters::setBoolean(int32_t parameterIndex, bool value) {
	setByte(parameterIndex, value ? 1 : 0);
}

void StatementParameters::setByte(int32_t parameterIndex, int8_t value) {
	Value& v = at(parameterIndex);
	v.kind = Kind::TINY;
	v.integer = value;
}

void StatementParameters::setShort(int32_t parameterIndex, int16_t value) {
	Value& v = at(parameterIndex);
	v.kind = Kind::SHORT;
	v.integer = value;
}

void StatementParameters::setInt(int32_t parameterIndex, int32_t value) {
	Value& v = at(parameterIndex);
	v.kind = Kind::LONG;
	v.integer = value;
}

void StatementParameters::setLong(int32_t parameterIndex, int64_t value) {
	Value& v = at(parameterIndex);
	v.kind = Kind::LONGLONG;
	v.integer = value;
}

void StatementParameters::setUnsignedLong(int32_t parameterIndex, uint64_t value) {
	Value& v = at(parameterIndex);
	v.kind = Kind::LONGLONG;
	v.isUnsigned = true;
	v.integer = static_cast<int64_t>(value);
}

void StatementParameters::setFloat(int32_t parameterIndex, float value) {
	Value& v = at(parameterIndex);
	v.kind = Kind::FLOAT;
	v.floating = value;
}

void StatementParameters::setDouble(int32_t parameterIndex, double value) {
	Value& v = at(parameterIndex);
	v.kind = Kind::DOUBLE;
	v.floating = value;
}

void StatementParameters::setString(int32_t parameterIndex, std::string_view value) {
	Value& v = at(parameterIndex);
	v.kind = Kind::STRING;
	v.bytes = std::string(value);
}

void StatementParameters::setBytes(int32_t parameterIndex, std::span<const uint8_t> value) {
	Value& v = at(parameterIndex);
	v.kind = Kind::BYTES;
	v.bytes.assign(value.begin(), value.end());
}

void StatementParameters::setDateTime(int32_t parameterIndex, const DateTimeValue& value) {
	Value& v = at(parameterIndex);
	v.kind = Kind::DATETIME;
	v.dateTime = value;
	v.dateTime.kind = DateTimeValue::Kind::DATETIME;
}

void StatementParameters::setDate(int32_t parameterIndex, Date value) {
	Value& v = at(parameterIndex);
	v.kind = Kind::DATE;
	v.dateTime.kind = DateTimeValue::Kind::DATE;
	v.dateTime.year = static_cast<uint32_t>(static_cast<int>(value.year()));
	v.dateTime.month = static_cast<unsigned>(value.month());
	v.dateTime.day = static_cast<unsigned>(value.day());
}

void StatementParameters::clear() {
	for (Value& value : values)
		value = Value{};
}

void StatementParameters::checkAllSet(const std::vector<Value>& values) {
	for (size_t i = 0; i < values.size(); ++i) {
		if (values[i].kind == Kind::UNSET)
			throw SQLException(fmt::format("No value specified for parameter {}", i + 1), "07001");
	}
}

} // namespace aion::commons::database
