#pragma once

#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/commons/database/StatementParameters.h"

struct st_mysql_stmt; // MYSQL_STMT of MariaDB Connector/C

namespace aion::commons::database {

class Connection;

namespace detail {

template <typename T>
struct IsOptional : std::false_type {};
template <typename T>
struct IsOptional<std::optional<T>> : std::true_type {};

template <typename T>
concept OptionalString = std::same_as<T, std::optional<std::string>> || std::same_as<T, std::optional<std::string_view>>;

template <typename T>
concept OptionalBytes = std::same_as<T, std::optional<std::vector<uint8_t>>> || std::same_as<T, std::optional<std::span<const uint8_t>>>;

} // namespace detail

/**
 * Java: java.sql.PreparedStatement (also used for CallableStatement without OUT parameters). A server-side prepared statement using the binary
 * protocol of MariaDB Connector/C. Created by Connection::prepareStatement.
 * <p>
 * <b>Parameters</b> are 1-based. Every setter has an overload taking std::optional, where std::nullopt sets SQL NULL (Java passes null
 * references). setObject accepts any supported C++ type (and std::optional of it), replacing Java's setObject(index, Object[, sqlType]).
 * Parameters keep their values after execution until changed or clearParameters() is called.
 * <p>
 * <b>Results</b>: execute() returns true if the statement produced a result set, which getResultSet() returns (owned by the statement until the next execution); otherwise
 * getUpdateCount() holds the number of affected (by default: matched) rows. executeQuery() returns the result set directly. Result sets are
 * fully buffered and independent of the statement.
 * <p>
 * <b>Batches</b>: addBatch() stores a copy of the current parameters and addBatch(sql) a plain SQL statement; executeBatch() executes all
 * entries in order and returns their update counts. Like Connector/J (continueBatchOnError=true, executeBatchSerially), a failing entry does
 * not stop the batch: its count becomes EXECUTE_FAILED and a BatchUpdateException carrying the last failure is thrown after all entries were
 * attempted. Only a deadlock (or lock table overflow), which rolls back the transaction, aborts the batch immediately; the counts of the
 * preceding entries are then EXECUTE_FAILED as well.
 * <p>
 * Deviation: Connector/J emulates prepared statements on the client by default (useServerPrepStmts=false) and talks the text protocol, while
 * this class uses real server-side prepared statements. Visible differences: statements that the server cannot prepare fail in
 * prepareStatement(), FLOAT/DOUBLE parameters are sent as binary values instead of their decimal text, and getString() on FLOAT/DOUBLE
 * columns formats the binary value (shortest round-trip representation) instead of returning the server's text.
 * <p>
 * Not thread safe: use a statement from one thread at a time, together with its connection.
 */
class PreparedStatement : public Statement {
public:
	~PreparedStatement();
	PreparedStatement(const PreparedStatement&) = delete;
	PreparedStatement& operator=(const PreparedStatement&) = delete;

	// ---- parameters ----

	void setNull(int32_t parameterIndex, int32_t sqlType);

	void setBoolean(int32_t parameterIndex, bool value);
	void setBoolean(int32_t parameterIndex, std::optional<bool> value) { setOptional(parameterIndex, value, Types::BOOLEAN); }
	void setByte(int32_t parameterIndex, int8_t value);
	void setByte(int32_t parameterIndex, std::optional<int8_t> value) { setOptional(parameterIndex, value, Types::TINYINT); }
	void setShort(int32_t parameterIndex, int16_t value);
	void setShort(int32_t parameterIndex, std::optional<int16_t> value) { setOptional(parameterIndex, value, Types::SMALLINT); }
	void setInt(int32_t parameterIndex, int32_t value);
	void setInt(int32_t parameterIndex, std::optional<int32_t> value) { setOptional(parameterIndex, value, Types::INTEGER); }
	void setLong(int32_t parameterIndex, int64_t value);
	void setLong(int32_t parameterIndex, std::optional<int64_t> value) { setOptional(parameterIndex, value, Types::BIGINT); }
	void setFloat(int32_t parameterIndex, float value);
	void setFloat(int32_t parameterIndex, std::optional<float> value) { setOptional(parameterIndex, value, Types::FLOAT); }
	void setDouble(int32_t parameterIndex, double value);
	void setDouble(int32_t parameterIndex, std::optional<double> value) { setOptional(parameterIndex, value, Types::DOUBLE); }

	void setString(int32_t parameterIndex, std::string_view value);
	template <detail::OptionalString O>
	void setString(int32_t parameterIndex, const O& value) {
		if (value)
			setString(parameterIndex, std::string_view(*value));
		else
			setNull(parameterIndex, Types::VARCHAR);
	}

	void setBytes(int32_t parameterIndex, std::span<const uint8_t> value);
	template <detail::OptionalBytes O>
	void setBytes(int32_t parameterIndex, const O& value) {
		if (value)
			setBytes(parameterIndex, std::span<const uint8_t>(*value));
		else
			setNull(parameterIndex, Types::VARBINARY);
	}

	/** Sends the local date/time of the instant in the connection time zone (see ConnectionTimeZone). */
	void setTimestamp(int32_t parameterIndex, Timestamp value);
	template <std::same_as<std::optional<Timestamp>> O>
	void setTimestamp(int32_t parameterIndex, const O& value) {
		setOptional(parameterIndex, value, Types::TIMESTAMP);
	}
	void setDate(int32_t parameterIndex, Date value);
	template <std::same_as<std::optional<Date>> O>
	void setDate(int32_t parameterIndex, const O& value) {
		setOptional(parameterIndex, value, Types::DATE);
	}

	/**
	 * Java: setObject(index, value). Dispatches on the C++ type: bool, signed/unsigned integers, float, double, anything convertible to
	 * std::string_view, std::vector&lt;uint8_t&gt;, Timestamp, Date, std::nullopt and std::optional of these.
	 */
	template <typename T>
	void setObject(int32_t parameterIndex, const T& value);

	/**
	 * Java: setObject(index, value, targetSqlType). The SQL type is used for NULL values; non-null values are sent with the type of the C++ value
	 * and converted by the server.
	 */
	template <typename T>
	void setObject(int32_t parameterIndex, const T& value, int32_t targetSqlType);

	/** Java: clearParameters() */
	void clearParameters();

	/** @return the number of '?' parameters of the statement */
	int32_t getParameterCount() const noexcept { return static_cast<int32_t>(parameters.size()); }

	// ---- execution ----

	/** @return true if the statement produced a result set (getResultSet), false for an update count (getUpdateCount) */
	bool execute();

	/**
	 * @return the result set (empty without columns if the statement produced none)
	 * @throws SQLException "Can not issue data manipulation statements with executeQuery()." for INSERT/UPDATE/DELETE/... statements
	 */
	std::unique_ptr<ResultSet> executeQuery();

	/**
	 * @return the update count, clamped to int32_t
	 * @throws SQLException "Can not issue executeUpdate() or executeLargeUpdate() for SELECTs" for SELECT statements
	 */
	int32_t executeUpdate();
	int64_t executeLargeUpdate();

	/** Java: getResultSet() - the result set of the last execute(), owned by this statement (nullptr if there is none) */
	ResultSet* getResultSet() noexcept { return currentResultSet.get(); }

	/** @return the update count of the last execution, or -1 if it produced a result set */
	int32_t getUpdateCount() const noexcept;
	int64_t getLargeUpdateCount() const noexcept { return updateCount; }

	/** Java: addBatch() - adds a copy of the current parameters to the batch */
	void addBatch();
	/**
	 * Java: addBatch(String sql) - adds a plain SQL statement (without parameters) to the batch. It is executed via the text protocol on the same
	 * connection, in the order it was added relative to the parameter sets (used e.g. to set session variables between parameter sets).
	 */
	void addBatch(std::string_view sql);
	void clearBatch() noexcept { batch.clear(); }
	std::vector<int32_t> executeBatch();
	std::vector<int64_t> executeLargeBatch();

	/**
	 * Java: getGeneratedKeys() - one row per generated AUTO_INCREMENT value of the last execution or batch, column "GENERATED_KEY"
	 * (BIGINT UNSIGNED). For multi-row inserts the keys are derived from the first id and auto_increment_increment, like Connector/J.
	 * @throws SQLException if the statement was not prepared with Statement::RETURN_GENERATED_KEYS
	 */
	std::unique_ptr<ResultSet> getGeneratedKeys();

	/** @return the connection that created this statement @throws SQLException if the statement is closed */
	Connection& getConnection() const;

	/** Java: close() - releases the server-side statement (and the owned connection, see DB::prepareStatement). Idempotent. */
	void close() noexcept;
	bool isClosed() const noexcept { return stmt == nullptr; }

	/**
	 * Makes this statement keep a pooled connection handle alive and release it when the statement is closed or destroyed. Used by
	 * DB::prepareStatement, whose statements own their connection like the Java version.
	 */
	void setOwnedConnection(std::shared_ptr<void> connectionHandle) noexcept { ownedConnection = std::move(connectionHandle); }

private:
	friend class Connection;

	PreparedStatement(Connection& connection, std::string_view sql, bool returnGeneratedKeys, int32_t resultSetType);

	template <typename T>
	void setOptional(int32_t parameterIndex, const std::optional<T>& value, int32_t sqlType) {
		if (value)
			setObject(parameterIndex, *value);
		else
			setNull(parameterIndex, sqlType);
	}

	void setUnsignedLong(int32_t parameterIndex, uint64_t value);
	void checkOpen() const;
	[[noreturn]] void throwError();
	void executeWith(const std::vector<StatementParameters::Value>& values);
	/** executes a plain SQL statement of the batch via the text protocol @return its update count */
	int64_t executePlainUpdate(std::string_view plainSql);
	void readResults();
	std::unique_ptr<ResultSet> fetchResultSet();
	void detach() noexcept;

	Connection* connection;
	st_mysql_stmt* stmt = nullptr;
	std::string sql;
	bool returnGeneratedKeys;
	int32_t resultSetType;
	StatementParameters parameters;
	/** batched parameter sets and plain SQL statements, in the order they were added */
	std::vector<std::variant<std::vector<StatementParameters::Value>, std::string>> batch;
	std::unique_ptr<ResultSet> currentResultSet;
	int64_t updateCount = -1;
	std::vector<uint64_t> generatedKeys;
	std::shared_ptr<void> ownedConnection;
};

template <typename T>
void PreparedStatement::setObject(int32_t parameterIndex, const T& value) {
	using U = std::remove_cvref_t<T>;
	if constexpr (std::is_same_v<U, std::nullopt_t> || std::is_same_v<U, std::nullptr_t>)
		setNull(parameterIndex, Types::NULL_TYPE);
	else if constexpr (detail::IsOptional<U>::value) {
		if (value)
			setObject(parameterIndex, *value);
		else
			setNull(parameterIndex, Types::NULL_TYPE);
	} else if constexpr (std::is_same_v<U, bool>)
		setBoolean(parameterIndex, value);
	else if constexpr (std::is_integral_v<U> && std::is_signed_v<U> && sizeof(U) == 1)
		setByte(parameterIndex, static_cast<int8_t>(value));
	else if constexpr (std::is_integral_v<U> && std::is_signed_v<U> && sizeof(U) == 2)
		setShort(parameterIndex, static_cast<int16_t>(value));
	else if constexpr (std::is_integral_v<U> && std::is_signed_v<U> && sizeof(U) == 4)
		setInt(parameterIndex, static_cast<int32_t>(value));
	else if constexpr (std::is_integral_v<U> && std::is_signed_v<U>)
		setLong(parameterIndex, static_cast<int64_t>(value));
	else if constexpr (std::is_integral_v<U> && sizeof(U) < 4)
		setInt(parameterIndex, static_cast<int32_t>(value));
	else if constexpr (std::is_integral_v<U> && sizeof(U) == 4)
		setLong(parameterIndex, static_cast<int64_t>(value));
	else if constexpr (std::is_integral_v<U>)
		setUnsignedLong(parameterIndex, static_cast<uint64_t>(value));
	else if constexpr (std::is_same_v<U, float>)
		setFloat(parameterIndex, value);
	else if constexpr (std::is_floating_point_v<U>)
		setDouble(parameterIndex, static_cast<double>(value));
	else if constexpr (std::is_same_v<U, Timestamp>)
		setTimestamp(parameterIndex, value);
	else if constexpr (std::is_same_v<U, Date>)
		setDate(parameterIndex, value);
	else if constexpr (std::is_convertible_v<const T&, std::string_view>)
		setString(parameterIndex, std::string_view(value));
	else if constexpr (std::is_convertible_v<const T&, std::span<const uint8_t>>)
		setBytes(parameterIndex, std::span<const uint8_t>(value));
	else
		static_assert(sizeof(T) == 0, "unsupported setObject type");
}

template <typename T>
void PreparedStatement::setObject(int32_t parameterIndex, const T& value, int32_t targetSqlType) {
	using U = std::remove_cvref_t<T>;
	if constexpr (std::is_same_v<U, std::nullopt_t> || std::is_same_v<U, std::nullptr_t>)
		setNull(parameterIndex, targetSqlType);
	else if constexpr (detail::IsOptional<U>::value) {
		if (value)
			setObject(parameterIndex, *value);
		else
			setNull(parameterIndex, targetSqlType);
	} else
		setObject(parameterIndex, value);
}

} // namespace aion::commons::database
