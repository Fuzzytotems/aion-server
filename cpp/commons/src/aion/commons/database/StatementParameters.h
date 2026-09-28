#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/database/DateTimeValue.h"
#include "aion/commons/database/SqlTypes.h"

namespace aion::commons::database {

/**
 * The parameter values of a PreparedStatement, independent of the MariaDB client library (so the rules can be unit tested). Indexes of the
 * public setters are 1-based like JDBC. Each value remembers the binary protocol type it is sent as (e.g. setByte sends a TINYINT).
 */
class StatementParameters {
public:
	enum class Kind : uint8_t { UNSET, NULL_VALUE, TINY, SHORT, LONG, LONGLONG, FLOAT, DOUBLE, STRING, BYTES, DATETIME, DATE };

	struct Value {
		Kind kind = Kind::UNSET;
		bool isUnsigned = false;
		int64_t integer = 0;
		double floating = 0;
		/** STRING and BYTES data */
		std::string bytes;
		/** DATETIME and DATE */
		DateTimeValue dateTime;
		/** NULL_VALUE: the java.sql.Types code passed to setNull */
		int32_t sqlType = Types::NULL_TYPE;
	};

	explicit StatementParameters(size_t count = 0) : values(count) {}

	size_t size() const noexcept { return values.size(); }

	/** @return the value of the 0-based parameter */
	const Value& operator[](size_t index) const noexcept { return values[index]; }
	const std::vector<Value>& getValues() const noexcept { return values; }

	void setNull(int32_t parameterIndex, int32_t sqlType);
	/** sent as TINYINT 1/0, like Connector/J */
	void setBoolean(int32_t parameterIndex, bool value);
	void setByte(int32_t parameterIndex, int8_t value);
	void setShort(int32_t parameterIndex, int16_t value);
	void setInt(int32_t parameterIndex, int32_t value);
	void setLong(int32_t parameterIndex, int64_t value);
	void setUnsignedLong(int32_t parameterIndex, uint64_t value);
	void setFloat(int32_t parameterIndex, float value);
	void setDouble(int32_t parameterIndex, double value);
	void setString(int32_t parameterIndex, std::string_view value);
	void setBytes(int32_t parameterIndex, std::span<const uint8_t> value);
	/** a DATETIME value (the caller converts Timestamps with the connection time zone) */
	void setDateTime(int32_t parameterIndex, const DateTimeValue& value);
	void setDate(int32_t parameterIndex, Date value);

	/** Java: clearParameters() - all parameters become unset */
	void clear();

	/**
	 * @throws SQLException "No value specified for parameter N" (SQLSTATE 07001) if a parameter was never set
	 */
	static void checkAllSet(const std::vector<Value>& values);

private:
	Value& at(int32_t parameterIndex);

	std::vector<Value> values;
};

} // namespace aion::commons::database
