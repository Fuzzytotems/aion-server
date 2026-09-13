#pragma once

#include <chrono>
#include <cstdint>

namespace aion::commons::database {

/**
 * Java: java.sql.Timestamp. A point in time with millisecond precision. DATETIME and TIMESTAMP values are converted between this instant and
 * the local date/time of the connection time zone (the serverTimezone URL parameter, or the system time zone if it is empty), like
 * Connector/J does.
 */
using Timestamp = std::chrono::sys_time<std::chrono::milliseconds>;

/** Java: java.sql.Date / java.time.LocalDate. DATE values are exchanged as calendar dates without any time zone conversion. */
using Date = std::chrono::year_month_day;

/** Java: java.sql.Types - generic SQL type codes, used for setNull/setObject and ResultSetMetaData::getColumnType. */
namespace Types {
inline constexpr int32_t BIT = -7;
inline constexpr int32_t TINYINT = -6;
inline constexpr int32_t SMALLINT = 5;
inline constexpr int32_t INTEGER = 4;
inline constexpr int32_t BIGINT = -5;
inline constexpr int32_t FLOAT = 6;
inline constexpr int32_t REAL = 7;
inline constexpr int32_t DOUBLE = 8;
inline constexpr int32_t NUMERIC = 2;
inline constexpr int32_t DECIMAL = 3;
inline constexpr int32_t CHAR = 1;
inline constexpr int32_t VARCHAR = 12;
inline constexpr int32_t LONGVARCHAR = -1;
inline constexpr int32_t DATE = 91;
inline constexpr int32_t TIME = 92;
inline constexpr int32_t TIMESTAMP = 93;
inline constexpr int32_t BINARY = -2;
inline constexpr int32_t VARBINARY = -3;
inline constexpr int32_t LONGVARBINARY = -4;
inline constexpr int32_t NULL_TYPE = 0; // Java: Types.NULL (NULL is a macro in C++)
inline constexpr int32_t OTHER = 1111;
inline constexpr int32_t BLOB = 2004;
inline constexpr int32_t CLOB = 2005;
inline constexpr int32_t BOOLEAN = 16;
} // namespace Types

/** Java: the constants of java.sql.Statement. PreparedStatement derives from it, so PreparedStatement::RETURN_GENERATED_KEYS works too. */
struct Statement {
	static constexpr int32_t SUCCESS_NO_INFO = -2;
	static constexpr int32_t EXECUTE_FAILED = -3;
	static constexpr int32_t RETURN_GENERATED_KEYS = 1;
	static constexpr int32_t NO_GENERATED_KEYS = 2;
};

} // namespace aion::commons::database
