#pragma once

#include <cstdint>
#include <exception>
#include <stacktrace>
#include <string>
#include <vector>

#include "aion/commons/utils/Exception.h"

namespace aion::commons::database {

/**
 * Java: java.sql.SQLException. Thrown by all database classes. Carries the SQLSTATE (5 characters, e.g. "23000", or the Connector/J specific
 * "S1009"; empty if unknown) and the vendor error code (MariaDB/MySQL error number, e.g. 1062 for a duplicate key; 0 if not applicable).
 * Server error messages are passed through unchanged, like Connector/J does.
 */
class SQLException : public utils::Exception {
public:
	explicit SQLException(const std::string& reason, std::string sqlState = {}, int32_t vendorCode = 0,
		std::stacktrace trace = std::stacktrace::current());
	SQLException(const std::string& reason, std::exception_ptr cause, std::stacktrace trace = std::stacktrace::current());
	SQLException(const std::string& reason, std::string sqlState, int32_t vendorCode, std::exception_ptr cause,
		std::stacktrace trace = std::stacktrace::current());

	/** @return the SQLSTATE, or an empty string if none is known */
	const std::string& getSQLState() const noexcept { return sqlState; }

	/** @return the vendor error code (MariaDB error number), or 0 */
	int32_t getErrorCode() const noexcept { return vendorCode; }

private:
	std::string sqlState;
	int32_t vendorCode;
};

/**
 * Java: java.sql.SQLTransientConnectionException. Thrown by DatabaseFactory::getConnection when no connection became available within the
 * configured timeout (HikariCP throws the same type).
 */
class SQLTransientConnectionException : public SQLException {
public:
	using SQLException::SQLException;
};

/**
 * Java: java.sql.BatchUpdateException. Thrown by PreparedStatement::executeBatch. getUpdateCounts() holds one entry per batched parameter set
 * that was attempted, where failed ones are Statement::EXECUTE_FAILED (Connector/J continueBatchOnError semantics).
 */
class BatchUpdateException : public SQLException {
public:
	BatchUpdateException(const std::string& reason, std::string sqlState, int32_t vendorCode, std::vector<int64_t> updateCounts,
		std::exception_ptr cause, std::stacktrace trace = std::stacktrace::current());

	/** Java: getUpdateCounts() - counts clamped to the int32_t range like Connector/J */
	std::vector<int32_t> getUpdateCounts() const;

	/** Java: getLargeUpdateCounts() */
	const std::vector<int64_t>& getLargeUpdateCounts() const noexcept { return updateCounts; }

private:
	std::vector<int64_t> updateCounts;
};

} // namespace aion::commons::database
