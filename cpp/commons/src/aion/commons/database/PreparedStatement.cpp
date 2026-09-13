#include "aion/commons/database/PreparedStatement.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include <mysql.h>
#include <mysqld_error.h>

#include "aion/commons/database/Connection.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::database {

namespace {

constexpr unsigned int BINARY_CHARSET_NUMBER = 63;

/** Skips whitespace and comments at the start of the statement (Connector/J: StringUtils.stripCommentsAndHints). */
std::string_view skipWhitespaceAndComments(std::string_view sql) noexcept {
	while (!sql.empty()) {
		char c = sql.front();
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v') {
			sql.remove_prefix(1);
		} else if (sql.starts_with("/*")) {
			size_t end = sql.find("*/", 2);
			sql = end == std::string_view::npos ? std::string_view() : sql.substr(end + 2);
		} else if (sql.starts_with("--") || c == '#') {
			size_t end = sql.find('\n');
			sql = end == std::string_view::npos ? std::string_view() : sql.substr(end + 1);
		} else {
			break;
		}
	}
	return sql;
}

bool startsWithKeyword(std::string_view sql, std::string_view keyword) noexcept {
	sql = skipWhitespaceAndComments(sql);
	return sql.size() >= keyword.size() && utils::StringUtils::equalsIgnoreCase(sql.substr(0, keyword.size()), keyword);
}

bool isDataManipulation(std::string_view sql) noexcept {
	for (std::string_view keyword : {"INSERT", "UPDATE", "DELETE", "DROP", "CREATE", "ALTER", "TRUNCATE", "RENAME"}) {
		if (startsWithKeyword(sql, keyword))
			return true;
	}
	return false;
}

ColumnType toColumnType(const MYSQL_FIELD& field) noexcept {
	bool binary = field.charsetnr == BINARY_CHARSET_NUMBER;
	switch (field.type) {
		case MYSQL_TYPE_TINY:
			return ColumnType::TINYINT;
		case MYSQL_TYPE_SHORT:
			return ColumnType::SMALLINT;
		case MYSQL_TYPE_INT24:
			return ColumnType::MEDIUMINT;
		case MYSQL_TYPE_LONG:
			return ColumnType::INT;
		case MYSQL_TYPE_LONGLONG:
			return ColumnType::BIGINT;
		case MYSQL_TYPE_YEAR:
			return ColumnType::YEAR;
		case MYSQL_TYPE_FLOAT:
			return ColumnType::FLOAT;
		case MYSQL_TYPE_DOUBLE:
			return ColumnType::DOUBLE;
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_NEWDECIMAL:
			return ColumnType::DECIMAL;
		case MYSQL_TYPE_BIT:
			return ColumnType::BIT;
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_NEWDATE:
			return ColumnType::DATE;
		case MYSQL_TYPE_TIME:
			return ColumnType::TIME;
		case MYSQL_TYPE_DATETIME:
			return ColumnType::DATETIME;
		case MYSQL_TYPE_TIMESTAMP:
			return ColumnType::TIMESTAMP;
		case MYSQL_TYPE_NULL:
			return ColumnType::NULL_TYPE;
		case MYSQL_TYPE_JSON:
			return ColumnType::JSON;
		case MYSQL_TYPE_ENUM:
			return ColumnType::ENUM;
		case MYSQL_TYPE_SET:
			return ColumnType::SET;
		case MYSQL_TYPE_GEOMETRY:
			return ColumnType::GEOMETRY;
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
		case MYSQL_TYPE_BLOB:
			return binary ? ColumnType::BLOB : ColumnType::TEXT;
		case MYSQL_TYPE_STRING:
			if (field.flags & ENUM_FLAG)
				return ColumnType::ENUM;
			if (field.flags & SET_FLAG)
				return ColumnType::SET;
			return binary ? ColumnType::BINARY : ColumnType::CHAR;
		default:
			return binary ? ColumnType::VARBINARY : ColumnType::VARCHAR;
	}
}

/** Result buffer of one column while fetching. */
struct ColumnBuffer {
	enum class Read : uint8_t { INT8, INT16, INT32, INT64, FLOAT, DOUBLE, TIME, BYTES };
	Read read = Read::BYTES;
	/** Read::TIME only: the kind of value, taken from the column type (see fetchResultSet) */
	DateTimeValue::Kind timeKind = DateTimeValue::Kind::DATETIME;
	bool isUnsigned = false;
	std::vector<char> data;
	MYSQL_TIME time{};
	unsigned long length = 0;
	my_bool isNull = 0;
	my_bool error = 0;
};

template <typename T>
T readValue(const std::vector<char>& data) noexcept {
	T value{};
	std::memcpy(&value, data.data(), sizeof(T));
	return value;
}

struct FreeResultGuard {
	MYSQL_STMT* stmt;
	~FreeResultGuard() { mysql_stmt_free_result(stmt); }
};

} // namespace

PreparedStatement::PreparedStatement(Connection& connection, std::string_view sql, bool returnGeneratedKeys, int32_t resultSetType)
	: connection(&connection), sql(sql), returnGeneratedKeys(returnGeneratedKeys), resultSetType(resultSetType) {
	connection.checkOpen();
	stmt = mysql_stmt_init(connection.mysql);
	if (!stmt)
		connection.throwError();
	my_bool updateMaxLength = 1;
	mysql_stmt_attr_set(stmt, STMT_ATTR_UPDATE_MAX_LENGTH, &updateMaxLength);
	if (mysql_stmt_prepare(stmt, this->sql.data(), static_cast<unsigned long>(this->sql.size())) != 0) {
		unsigned int errorNumber = mysql_stmt_errno(stmt);
		std::string sqlState = mysql_stmt_sqlstate(stmt);
		std::string message = mysql_stmt_error(stmt);
		mysql_stmt_close(stmt);
		stmt = nullptr;
		connection.onError(errorNumber, sqlState);
		throw Connection::createException(errorNumber, sqlState, message);
	}
	parameters = StatementParameters(mysql_stmt_param_count(stmt));
	connection.registerStatement(this);
}

PreparedStatement::~PreparedStatement() {
	close();
}

void PreparedStatement::close() noexcept {
	if (connection)
		connection->unregisterStatement(this);
	detach();
	ownedConnection.reset();
}

void PreparedStatement::detach() noexcept {
	if (stmt) {
		mysql_stmt_close(stmt);
		stmt = nullptr;
	}
	connection = nullptr;
	batch.clear();
}

void PreparedStatement::checkOpen() const {
	if (!stmt || !connection)
		throw SQLException("No operations allowed after statement closed.", "S1009");
	connection->checkOpen();
}

void PreparedStatement::throwError() {
	unsigned int errorNumber = mysql_stmt_errno(stmt);
	std::string sqlState = mysql_stmt_sqlstate(stmt);
	std::string message = mysql_stmt_error(stmt);
	if (connection)
		connection->onError(errorNumber, sqlState);
	throw Connection::createException(errorNumber, sqlState, message);
}

Connection& PreparedStatement::getConnection() const {
	checkOpen();
	return *connection;
}

// ---- parameters ----

void PreparedStatement::setNull(int32_t parameterIndex, int32_t sqlType) {
	checkOpen();
	parameters.setNull(parameterIndex, sqlType);
}

void PreparedStatement::setBoolean(int32_t parameterIndex, bool value) {
	checkOpen();
	parameters.setBoolean(parameterIndex, value);
}

void PreparedStatement::setByte(int32_t parameterIndex, int8_t value) {
	checkOpen();
	parameters.setByte(parameterIndex, value);
}

void PreparedStatement::setShort(int32_t parameterIndex, int16_t value) {
	checkOpen();
	parameters.setShort(parameterIndex, value);
}

void PreparedStatement::setInt(int32_t parameterIndex, int32_t value) {
	checkOpen();
	parameters.setInt(parameterIndex, value);
}

void PreparedStatement::setLong(int32_t parameterIndex, int64_t value) {
	checkOpen();
	parameters.setLong(parameterIndex, value);
}

void PreparedStatement::setUnsignedLong(int32_t parameterIndex, uint64_t value) {
	checkOpen();
	parameters.setUnsignedLong(parameterIndex, value);
}

void PreparedStatement::setFloat(int32_t parameterIndex, float value) {
	checkOpen();
	parameters.setFloat(parameterIndex, value);
}

void PreparedStatement::setDouble(int32_t parameterIndex, double value) {
	checkOpen();
	parameters.setDouble(parameterIndex, value);
}

void PreparedStatement::setString(int32_t parameterIndex, std::string_view value) {
	checkOpen();
	parameters.setString(parameterIndex, value);
}

void PreparedStatement::setBytes(int32_t parameterIndex, std::span<const uint8_t> value) {
	checkOpen();
	parameters.setBytes(parameterIndex, value);
}

void PreparedStatement::setTimestamp(int32_t parameterIndex, Timestamp value) {
	checkOpen();
	parameters.setDateTime(parameterIndex, connection->getProperties().timeZone.toDateTime(value));
}

void PreparedStatement::setDate(int32_t parameterIndex, Date value) {
	checkOpen();
	parameters.setDate(parameterIndex, value);
}

void PreparedStatement::clearParameters() {
	checkOpen();
	parameters.clear();
}

// ---- execution ----

void PreparedStatement::executeWith(const std::vector<StatementParameters::Value>& values) {
	checkOpen();
	StatementParameters::checkAllSet(values);
	currentResultSet.reset();
	updateCount = -1;
	mysql_stmt_free_result(stmt);

	struct Scratch {
		int8_t i8;
		int16_t i16;
		int32_t i32;
		int64_t i64;
		float f;
		double d;
		MYSQL_TIME time;
		unsigned long length;
	};
	size_t count = values.size();
	std::vector<MYSQL_BIND> binds(count);
	std::vector<Scratch> scratch(count);
	for (size_t i = 0; i < count; ++i) {
		const StatementParameters::Value& value = values[i];
		MYSQL_BIND& bind = binds[i];
		Scratch& s = scratch[i];
		std::memset(&bind, 0, sizeof(bind));
		std::memset(&s, 0, sizeof(s));
		using Kind = StatementParameters::Kind;
		switch (value.kind) {
			case Kind::UNSET: // rejected by checkAllSet
			case Kind::NULL_VALUE:
				bind.buffer_type = MYSQL_TYPE_NULL;
				break;
			case Kind::TINY:
				s.i8 = static_cast<int8_t>(value.integer);
				bind.buffer_type = MYSQL_TYPE_TINY;
				bind.buffer = &s.i8;
				break;
			case Kind::SHORT:
				s.i16 = static_cast<int16_t>(value.integer);
				bind.buffer_type = MYSQL_TYPE_SHORT;
				bind.buffer = &s.i16;
				break;
			case Kind::LONG:
				s.i32 = static_cast<int32_t>(value.integer);
				bind.buffer_type = MYSQL_TYPE_LONG;
				bind.buffer = &s.i32;
				break;
			case Kind::LONGLONG:
				s.i64 = value.integer;
				bind.buffer_type = MYSQL_TYPE_LONGLONG;
				bind.buffer = &s.i64;
				bind.is_unsigned = value.isUnsigned ? 1 : 0;
				break;
			case Kind::FLOAT:
				s.f = static_cast<float>(value.floating);
				bind.buffer_type = MYSQL_TYPE_FLOAT;
				bind.buffer = &s.f;
				break;
			case Kind::DOUBLE:
				s.d = value.floating;
				bind.buffer_type = MYSQL_TYPE_DOUBLE;
				bind.buffer = &s.d;
				break;
			case Kind::STRING:
			case Kind::BYTES:
				if (value.bytes.size() > std::numeric_limits<unsigned long>::max())
					throw SQLException("Parameter value too large", "22001");
				bind.buffer_type = value.kind == Kind::STRING ? MYSQL_TYPE_STRING : MYSQL_TYPE_BLOB;
				bind.buffer = const_cast<char*>(value.bytes.data());
				s.length = static_cast<unsigned long>(value.bytes.size());
				bind.buffer_length = s.length;
				bind.length = &s.length;
				break;
			case Kind::DATETIME:
			case Kind::DATE: {
				const DateTimeValue& dt = value.dateTime;
				s.time.year = dt.year;
				s.time.month = dt.month;
				s.time.day = dt.day;
				if (value.kind == Kind::DATETIME) {
					s.time.hour = dt.hour;
					s.time.minute = dt.minute;
					s.time.second = dt.second;
					s.time.second_part = dt.microsecond;
					s.time.time_type = MYSQL_TIMESTAMP_DATETIME;
					bind.buffer_type = MYSQL_TYPE_DATETIME;
				} else {
					s.time.time_type = MYSQL_TIMESTAMP_DATE;
					bind.buffer_type = MYSQL_TYPE_DATE;
				}
				bind.buffer = &s.time;
				bind.buffer_length = sizeof(MYSQL_TIME);
				break;
			}
		}
	}
	if (count > 0 && mysql_stmt_bind_param(stmt, binds.data()) != 0)
		throwError();
	if (mysql_stmt_execute(stmt) != 0)
		throwError();
}

void PreparedStatement::readResults() {
	if (mysql_stmt_field_count(stmt) > 0) {
		currentResultSet = fetchResultSet();
		updateCount = -1;
	} else {
		my_ulonglong affected = mysql_stmt_affected_rows(stmt);
		updateCount = affected == static_cast<my_ulonglong>(-1) ? -1 : static_cast<int64_t>(std::min<my_ulonglong>(affected, std::numeric_limits<int64_t>::max()));
		if (returnGeneratedKeys) {
			uint64_t id = mysql_stmt_insert_id(stmt);
			if (id != 0) {
				for (int64_t i = 0; i < updateCount; ++i)
					generatedKeys.push_back(id + static_cast<uint64_t>(i) * connection->autoIncrementIncrement);
			}
		}
	}
	// drain further results (e.g. the status result of CALL), otherwise the connection is out of sync
	while (mysql_stmt_more_results(stmt)) {
		int next = mysql_stmt_next_result(stmt);
		if (next > 0)
			throwError();
		if (next < 0)
			break;
		if (mysql_stmt_field_count(stmt) > 0) {
			FreeResultGuard guard{stmt};
			if (mysql_stmt_store_result(stmt) != 0)
				throwError();
		}
	}
}

std::unique_ptr<ResultSet> PreparedStatement::fetchResultSet() {
	FreeResultGuard guard{stmt};
	if (mysql_stmt_store_result(stmt) != 0)
		throwError();
	// metadata after store_result, so the copied fields contain max_length (STMT_ATTR_UPDATE_MAX_LENGTH)
	std::unique_ptr<MYSQL_RES, decltype(&mysql_free_result)> metadata(mysql_stmt_result_metadata(stmt), &mysql_free_result);
	if (!metadata)
		throwError();
	unsigned int columnCount = mysql_num_fields(metadata.get());
	MYSQL_FIELD* fields = mysql_fetch_fields(metadata.get());

	std::vector<ColumnDefinition> columns(columnCount);
	std::vector<ColumnBuffer> buffers(columnCount);
	std::vector<MYSQL_BIND> binds(columnCount);
	for (unsigned int i = 0; i < columnCount; ++i) {
		const MYSQL_FIELD& field = fields[i];
		ColumnDefinition& column = columns[i];
		column.label.assign(field.name ? field.name : "", field.name_length);
		column.name.assign(field.org_name ? field.org_name : "", field.org_name_length);
		column.table.assign(field.table ? field.table : "", field.table_length);
		column.database.assign(field.db ? field.db : "", field.db_length);
		column.type = toColumnType(field);
		column.isUnsigned = (field.flags & UNSIGNED_FLAG) != 0 && column.type != ColumnType::DECIMAL && column.type != ColumnType::FLOAT &&
			column.type != ColumnType::DOUBLE;
		column.decimals = field.decimals;
		column.length = field.length;

		ColumnBuffer& buffer = buffers[i];
		MYSQL_BIND& bind = binds[i];
		std::memset(&bind, 0, sizeof(bind));
		buffer.isUnsigned = column.isUnsigned;
		auto useData = [&](ColumnBuffer::Read read, enum_field_types type, size_t size) {
			buffer.read = read;
			buffer.data.resize(size);
			bind.buffer_type = type;
			bind.buffer = buffer.data.data();
			bind.buffer_length = static_cast<unsigned long>(size);
		};
		switch (field.type) {
			case MYSQL_TYPE_TINY:
				useData(ColumnBuffer::Read::INT8, MYSQL_TYPE_TINY, 1);
				break;
			case MYSQL_TYPE_SHORT:
			case MYSQL_TYPE_YEAR:
				useData(ColumnBuffer::Read::INT16, MYSQL_TYPE_SHORT, 2);
				break;
			case MYSQL_TYPE_INT24:
			case MYSQL_TYPE_LONG:
				useData(ColumnBuffer::Read::INT32, MYSQL_TYPE_LONG, 4);
				break;
			case MYSQL_TYPE_LONGLONG:
				useData(ColumnBuffer::Read::INT64, MYSQL_TYPE_LONGLONG, 8);
				break;
			case MYSQL_TYPE_FLOAT:
				useData(ColumnBuffer::Read::FLOAT, MYSQL_TYPE_FLOAT, 4);
				break;
			case MYSQL_TYPE_DOUBLE:
				useData(ColumnBuffer::Read::DOUBLE, MYSQL_TYPE_DOUBLE, 8);
				break;
			case MYSQL_TYPE_DATE:
			case MYSQL_TYPE_NEWDATE:
			case MYSQL_TYPE_TIME:
			case MYSQL_TYPE_DATETIME:
			case MYSQL_TYPE_TIMESTAMP:
				buffer.read = ColumnBuffer::Read::TIME;
				// Not MYSQL_TIME::time_type: Connector/C reports MYSQL_TIMESTAMP_DATE for values sent in short form (a DATETIME at midnight, a zero
				// value or TIME 00:00:00). Connector/J formats and converts by column type as well.
				buffer.timeKind = field.type == MYSQL_TYPE_TIME ? DateTimeValue::Kind::TIME
					: field.type == MYSQL_TYPE_DATE || field.type == MYSQL_TYPE_NEWDATE ? DateTimeValue::Kind::DATE
																																								: DateTimeValue::Kind::DATETIME;
				bind.buffer_type = field.type == MYSQL_TYPE_NEWDATE ? MYSQL_TYPE_DATE : field.type;
				bind.buffer = &buffer.time;
				bind.buffer_length = sizeof(MYSQL_TIME);
				break;
			default: {
				bool blob = field.type == MYSQL_TYPE_TINY_BLOB || field.type == MYSQL_TYPE_MEDIUM_BLOB || field.type == MYSQL_TYPE_LONG_BLOB ||
					field.type == MYSQL_TYPE_BLOB || field.type == MYSQL_TYPE_GEOMETRY;
				// max_length is the longest value in the stored result; values that are longer anyway are fetched separately below
				size_t size = std::min<size_t>(static_cast<size_t>(field.max_length) + 1, std::numeric_limits<unsigned long>::max());
				useData(ColumnBuffer::Read::BYTES, blob ? MYSQL_TYPE_BLOB : MYSQL_TYPE_STRING, std::max<size_t>(size, 1));
				break;
			}
		}
		bind.is_unsigned = column.isUnsigned ? 1 : 0;
		bind.length = &buffer.length;
		bind.is_null = &buffer.isNull;
		bind.error = &buffer.error;
	}
	if (columnCount > 0 && mysql_stmt_bind_result(stmt, binds.data()) != 0)
		throwError();

	ResultSetOptions options{connection->getProperties().timeZone, connection->getProperties().zeroDateTimeBehavior, resultSetType};
	ResultSetBuilder builder(std::move(columns), std::move(options));
	builder.reserve(static_cast<size_t>(mysql_stmt_num_rows(stmt)));
	std::vector<char> longValue;
	while (true) {
		int status = mysql_stmt_fetch(stmt);
		if (status == MYSQL_NO_DATA)
			break;
		if (status == 1)
			throwError();
		for (unsigned int i = 0; i < columnCount; ++i) {
			ColumnBuffer& buffer = buffers[i];
			if (buffer.isNull) {
				builder.addNull();
				continue;
			}
			switch (buffer.read) {
				case ColumnBuffer::Read::INT8:
					if (buffer.isUnsigned)
						builder.addUnsignedLong(readValue<uint8_t>(buffer.data));
					else
						builder.addLong(readValue<int8_t>(buffer.data));
					break;
				case ColumnBuffer::Read::INT16:
					if (buffer.isUnsigned)
						builder.addUnsignedLong(readValue<uint16_t>(buffer.data));
					else
						builder.addLong(readValue<int16_t>(buffer.data));
					break;
				case ColumnBuffer::Read::INT32:
					if (buffer.isUnsigned)
						builder.addUnsignedLong(readValue<uint32_t>(buffer.data));
					else
						builder.addLong(readValue<int32_t>(buffer.data));
					break;
				case ColumnBuffer::Read::INT64:
					if (buffer.isUnsigned)
						builder.addUnsignedLong(readValue<uint64_t>(buffer.data));
					else
						builder.addLong(readValue<int64_t>(buffer.data));
					break;
				case ColumnBuffer::Read::FLOAT:
					builder.addDouble(readValue<float>(buffer.data));
					break;
				case ColumnBuffer::Read::DOUBLE:
					builder.addDouble(readValue<double>(buffer.data));
					break;
				case ColumnBuffer::Read::TIME: {
					const MYSQL_TIME& t = buffer.time;
					DateTimeValue value;
					value.kind = buffer.timeKind;
					value.negative = t.neg != 0;
					value.year = t.year;
					value.month = t.month;
					value.day = t.day;
					value.hour = t.hour;
					value.minute = t.minute;
					value.second = t.second;
					value.microsecond = static_cast<uint32_t>(t.second_part);
					builder.addDateTime(value);
					break;
				}
				case ColumnBuffer::Read::BYTES:
					if (buffer.length > binds[i].buffer_length) {
						// longer than the buffer: fetch the complete value separately
						longValue.resize(buffer.length);
						unsigned long length = 0;
						my_bool isNull = 0, error = 0;
						MYSQL_BIND column;
						std::memset(&column, 0, sizeof(column));
						column.buffer_type = binds[i].buffer_type;
						column.buffer = longValue.data();
						column.buffer_length = static_cast<unsigned long>(longValue.size());
						column.length = &length;
						column.is_null = &isNull;
						column.error = &error;
						if (mysql_stmt_fetch_column(stmt, &column, i, 0) != 0)
							throwError();
						builder.addBytes(std::string_view(longValue.data(), std::min<size_t>(length, longValue.size())));
					} else {
						builder.addBytes(std::string_view(buffer.data.data(), buffer.length));
					}
					break;
			}
		}
	}
	return builder.build();
}

bool PreparedStatement::execute() {
	checkOpen();
	generatedKeys.clear();
	executeWith(parameters.getValues());
	readResults();
	return currentResultSet != nullptr;
}

std::unique_ptr<ResultSet> PreparedStatement::executeQuery() {
	checkOpen();
	if (isDataManipulation(sql))
		throw SQLException("Can not issue data manipulation statements with executeQuery().", "S1009");
	execute();
	if (!currentResultSet) { // Connector/J returns its (row-less) update result here
		ResultSetOptions options{connection->getProperties().timeZone, connection->getProperties().zeroDateTimeBehavior, resultSetType};
		return ResultSetBuilder({}, std::move(options)).build();
	}
	return std::move(currentResultSet);
}

int64_t PreparedStatement::executeLargeUpdate() {
	checkOpen();
	if (startsWithKeyword(sql, "SELECT"))
		throw SQLException("Can not issue executeUpdate() or executeLargeUpdate() for SELECTs", "01S03");
	execute();
	return updateCount;
}

int32_t PreparedStatement::executeUpdate() {
	return static_cast<int32_t>(std::min<int64_t>(executeLargeUpdate(), std::numeric_limits<int32_t>::max()));
}

int32_t PreparedStatement::getUpdateCount() const noexcept {
	return static_cast<int32_t>(std::min<int64_t>(updateCount, std::numeric_limits<int32_t>::max()));
}

void PreparedStatement::addBatch() {
	checkOpen();
	batch.emplace_back(parameters.getValues());
}

void PreparedStatement::addBatch(std::string_view plainSql) {
	checkOpen();
	batch.emplace_back(std::string(plainSql));
}

int64_t PreparedStatement::executePlainUpdate(std::string_view plainSql) {
	checkOpen();
	if (startsWithKeyword(plainSql, "SELECT"))
		throw SQLException("Can not issue SELECT via executeUpdate() or executeLargeUpdate().", "S1009");
	currentResultSet.reset();
	updateCount = -1;
	Connection::TextResult result = connection->executeText(plainSql);
	updateCount = result.updateCount;
	if (returnGeneratedKeys && result.insertId != 0) {
		for (int64_t i = 0; i < updateCount; ++i)
			generatedKeys.push_back(result.insertId + static_cast<uint64_t>(i) * connection->autoIncrementIncrement);
	}
	return updateCount;
}

std::vector<int64_t> PreparedStatement::executeLargeBatch() {
	checkOpen();
	generatedKeys.clear();
	auto entries = std::move(batch);
	batch.clear();
	bool isSelect = startsWithKeyword(sql, "SELECT");

	// Connector/J ClientPreparedStatement.executeBatchSerially with continueBatchOnError=true
	std::vector<int64_t> counts;
	counts.reserve(entries.size());
	std::exception_ptr lastError;
	for (const auto& entry : entries) {
		try {
			if (const std::string* plainSql = std::get_if<std::string>(&entry)) {
				counts.push_back(executePlainUpdate(*plainSql));
			} else {
				if (isSelect)
					throw SQLException("Can not issue SELECT via executeUpdate() or executeLargeUpdate().", "S1009");
				executeWith(std::get<std::vector<StatementParameters::Value>>(entry));
				readResults();
				counts.push_back(updateCount);
			}
		} catch (const SQLException& e) {
			// StatementImpl.hasDeadlockOrTimeoutRolledBackTx: the server rolled back the whole transaction, so the preceding counts are void too.
			// Lock wait timeouts only roll back the statement on MySQL 5.0.13+ / MariaDB, so the batch continues like for other errors.
			if (e.getErrorCode() == ER_LOCK_DEADLOCK || e.getErrorCode() == ER_LOCK_TABLE_FULL)
				throw BatchUpdateException(e.what(), e.getSQLState(), e.getErrorCode(), std::vector<int64_t>(counts.size(), EXECUTE_FAILED),
					std::current_exception());
			counts.push_back(EXECUTE_FAILED);
			lastError = std::current_exception(); // Connector/J reports the last failure
		}
	}
	if (lastError) {
		try {
			std::rethrow_exception(lastError);
		} catch (const SQLException& e) {
			throw BatchUpdateException(e.what(), e.getSQLState(), e.getErrorCode(), std::move(counts), lastError);
		}
	}
	return counts;
}

std::vector<int32_t> PreparedStatement::executeBatch() {
	std::vector<int64_t> counts = executeLargeBatch();
	std::vector<int32_t> result;
	result.reserve(counts.size());
	for (int64_t count : counts)
		result.push_back(static_cast<int32_t>(std::min<int64_t>(count, std::numeric_limits<int32_t>::max())));
	return result;
}

std::unique_ptr<ResultSet> PreparedStatement::getGeneratedKeys() {
	checkOpen();
	if (!returnGeneratedKeys)
		throw SQLException("Generated keys not requested. You need to specify Statement.RETURN_GENERATED_KEYS to Statement.executeUpdate(), "
			"Statement.executeLargeUpdate() or Connection.prepareStatement().", "S1009");
	ColumnDefinition column;
	column.label = "GENERATED_KEY";
	column.name = "GENERATED_KEY";
	column.type = ColumnType::BIGINT;
	column.isUnsigned = true;
	column.length = 20;
	ResultSetOptions options{connection->getProperties().timeZone, connection->getProperties().zeroDateTimeBehavior, ResultSet::TYPE_FORWARD_ONLY};
	ResultSetBuilder builder({column}, std::move(options));
	for (uint64_t key : generatedKeys)
		builder.addUnsignedLong(key);
	return builder.build();
}

} // namespace aion::commons::database
