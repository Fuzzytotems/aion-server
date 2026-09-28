#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "aion/commons/database/ConnectionProperties.h"
#include "aion/commons/database/DateTimeValue.h"
#include "aion/commons/database/SqlTypes.h"

namespace aion::commons::database {

/** The MariaDB column type of a result set column (Connector/J: MysqlType, without the UNSIGNED variants). */
enum class ColumnType : uint8_t {
	NULL_TYPE,
	TINYINT,
	SMALLINT,
	MEDIUMINT,
	INT,
	BIGINT,
	YEAR,
	FLOAT,
	DOUBLE,
	DECIMAL,
	BIT,
	DATE,
	TIME,
	DATETIME,
	TIMESTAMP,
	CHAR,
	VARCHAR,
	TEXT,
	BINARY,
	VARBINARY,
	BLOB,
	JSON,
	ENUM,
	SET,
	GEOMETRY
};

/** Metadata of one result set column. */
struct ColumnDefinition {
	/** column label: the alias, or the column name if there is none */
	std::string label;
	/** original column name (empty for expressions) */
	std::string name;
	/** table alias */
	std::string table;
	/** database (schema) name */
	std::string database;
	ColumnType type = ColumnType::VARCHAR;
	bool isUnsigned = false;
	/** number of fractional digits (DECIMAL, TIME, DATETIME, TIMESTAMP, FLOAT, DOUBLE) */
	uint32_t decimals = 0;
	/** display length of the column */
	uint64_t length = 0;
};

/** Settings of the connection that affect value conversions. */
struct ResultSetOptions {
	ConnectionTimeZone timeZone;
	ZeroDateTimeBehavior zeroDateTimeBehavior = ZeroDateTimeBehavior::EXCEPTION;
	/** ResultSet::TYPE_FORWARD_ONLY or TYPE_SCROLL_INSENSITIVE/TYPE_SCROLL_SENSITIVE (which behave the same) */
	int32_t type = 1003;
};

class ResultSet;

/** Java: java.sql.ResultSetMetaData. Column indexes are 1-based. Valid as long as its ResultSet exists. */
class ResultSetMetaData {
public:
	int32_t getColumnCount() const noexcept;
	/** Java: getColumnLabel - the alias or column name */
	std::string getColumnLabel(int32_t column) const;
	/** Java: getColumnName - the original column name (Connector/J returns the original name unless useOldAliasMetadataBehavior) */
	std::string getColumnName(int32_t column) const;
	std::string getTableName(int32_t column) const;
	/** Java: getCatalogName - the database name */
	std::string getCatalogName(int32_t column) const;
	/** @return the java.sql.Types code, mapped like Connector/J (TINYINT(1) is BIT, YEAR is DATE) */
	int32_t getColumnType(int32_t column) const;
	/** @return the type name like Connector/J, e.g. "INT UNSIGNED" */
	std::string getColumnTypeName(int32_t column) const;
	bool isSigned(int32_t column) const;
	int32_t getScale(int32_t column) const;

	/** C++ addition: the full column definition */
	const ColumnDefinition& getColumn(int32_t column) const;

private:
	friend class ResultSet;
	explicit ResultSetMetaData(const ResultSet& resultSet) noexcept : resultSet(&resultSet) {}
	const ResultSet* resultSet;
};

/**
 * Java: java.sql.ResultSet (Connector/J ResultSetImpl with a fully buffered, static row set). The rows are completely copied from the server
 * response when the statement executes, so a ResultSet stays usable after its statement or connection is gone and scrolling is cheap.
 * <p>
 * Column indexes are 1-based. Column labels are matched case-insensitively, first against the labels (aliases), then against the original
 * column names, then against "table.column"; the first match wins, like Connector/J.
 * <p>
 * <b>NULL handling:</b>
 * <ul>
 * <li>getBoolean/getByte/getShort/getInt/getLong/getFloat/getDouble return false/0 for SQL NULL (like JDBC); check wasNull() afterwards.</li>
 * <li>getString and getBytes return an empty string/vector for SQL NULL (Deviation: JDBC returns null); wasNull() tells them apart.</li>
 * <li>getTimestamp and getDate return std::nullopt for SQL NULL (JDBC: null).</li>
 * <li>getObject&lt;T&gt; returns std::nullopt for SQL NULL for every supported T: bool, int8_t, int16_t, int32_t, int64_t, float, double,
 * std::string, std::vector&lt;uint8_t&gt;, Timestamp and Date. It replaces Java's <code>(Integer) rs.getObject("x")</code> casts and the
 * JDBC 4.1 <code>getObject(column, Class)</code>.</li>
 * </ul>
 * Conversions between column and target types follow Connector/J (jdbcCompliantTruncation for reads): numbers out of the target range throw
 * SQLException with SQLSTATE 22003, numeric strings are parsed, getString formats numbers and temporal values like the MariaDB text protocol,
 * getBoolean treats -1 and positive numbers as true as well as "Y"/"true", and unsupported conversions (e.g. getInt on a DATETIME) throw.
 * <p>
 * Not thread safe; a ResultSet is used by one thread at a time.
 */
class ResultSet {
public:
	static constexpr int32_t TYPE_FORWARD_ONLY = 1003;
	static constexpr int32_t TYPE_SCROLL_INSENSITIVE = 1004;
	static constexpr int32_t TYPE_SCROLL_SENSITIVE = 1005;
	static constexpr int32_t CONCUR_READ_ONLY = 1007;
	static constexpr int32_t CONCUR_UPDATABLE = 1008;

	ResultSet(const ResultSet&) = delete;
	ResultSet& operator=(const ResultSet&) = delete;

	// ---- navigation (the scrolling methods throw for TYPE_FORWARD_ONLY result sets, like Connector/J) ----

	bool next();
	bool previous();
	bool first();
	bool last();
	void beforeFirst();
	void afterLast();
	bool absolute(int32_t row);
	bool relative(int32_t rows);
	/** @return true if the cursor is before the first row and the result set is not empty */
	bool isBeforeFirst() const noexcept;
	/** @return true if the cursor is after the last row and the result set is not empty */
	bool isAfterLast() const noexcept;
	bool isFirst() const noexcept;
	bool isLast() const noexcept;
	/** @return the 1-based current row number, or 0 if there is no current row */
	int32_t getRow() const noexcept;
	int32_t getType() const noexcept { return options.type; }
	int32_t getConcurrency() const noexcept { return CONCUR_READ_ONLY; }

	/** C++ addition: the number of rows */
	size_t getRowCount() const noexcept { return rowCount; }

	// ---- columns ----

	/** @return the 1-based index of the column with the given label @throws SQLException "Column 'x' not found." */
	int32_t findColumn(std::string_view columnLabel) const;
	ResultSetMetaData getMetaData() const noexcept { return ResultSetMetaData(*this); }
	/** @return true if the last column read had the value SQL NULL */
	bool wasNull() const noexcept { return lastWasNull; }

	// ---- getters by 1-based index ----

	bool getBoolean(int32_t columnIndex) const;
	int8_t getByte(int32_t columnIndex) const;
	int16_t getShort(int32_t columnIndex) const;
	int32_t getInt(int32_t columnIndex) const;
	int64_t getLong(int32_t columnIndex) const;
	float getFloat(int32_t columnIndex) const;
	double getDouble(int32_t columnIndex) const;
	std::string getString(int32_t columnIndex) const;
	std::vector<uint8_t> getBytes(int32_t columnIndex) const;
	std::optional<Timestamp> getTimestamp(int32_t columnIndex) const;
	std::optional<Date> getDate(int32_t columnIndex) const;

	template <typename T>
	std::optional<T> getObject(int32_t columnIndex) const;

	// ---- getters by column label ----

	bool getBoolean(std::string_view columnLabel) const { return getBoolean(findColumn(columnLabel)); }
	int8_t getByte(std::string_view columnLabel) const { return getByte(findColumn(columnLabel)); }
	int16_t getShort(std::string_view columnLabel) const { return getShort(findColumn(columnLabel)); }
	int32_t getInt(std::string_view columnLabel) const { return getInt(findColumn(columnLabel)); }
	int64_t getLong(std::string_view columnLabel) const { return getLong(findColumn(columnLabel)); }
	float getFloat(std::string_view columnLabel) const { return getFloat(findColumn(columnLabel)); }
	double getDouble(std::string_view columnLabel) const { return getDouble(findColumn(columnLabel)); }
	std::string getString(std::string_view columnLabel) const { return getString(findColumn(columnLabel)); }
	std::vector<uint8_t> getBytes(std::string_view columnLabel) const { return getBytes(findColumn(columnLabel)); }
	std::optional<Timestamp> getTimestamp(std::string_view columnLabel) const { return getTimestamp(findColumn(columnLabel)); }
	std::optional<Date> getDate(std::string_view columnLabel) const { return getDate(findColumn(columnLabel)); }

	template <typename T>
	std::optional<T> getObject(std::string_view columnLabel) const {
		return getObject<T>(findColumn(columnLabel));
	}

private:
	friend class ResultSetBuilder;
	friend class ResultSetMetaData;

	struct Cell {
		union {
			int64_t i;
			uint64_t u;
			double d;
			uint64_t offset; // into bytes or dateTimes
		};
		uint32_t size = 0;
		bool isNull = true;
		Cell() : u(0) {}
	};

	ResultSet(std::vector<ColumnDefinition> columns, ResultSetOptions options);

	void checkScrollable() const;
	const ColumnDefinition& column(int32_t columnIndex) const;
	/** checks the cursor and index, sets wasNull and returns the cell */
	const Cell& cell(int32_t columnIndex) const;
	/** the bytes of a cell of BYTES storage (for other cells the union holds no offset and the result is meaningless, but safe) */
	std::string_view bytesOf(const Cell& c) const noexcept {
		if (c.offset >= byteData.size())
			return {};
		return std::string_view(byteData).substr(static_cast<size_t>(c.offset), c.size);
	}

	std::vector<ColumnDefinition> columns;
	ResultSetOptions options;
	std::vector<Cell> cells;
	std::string byteData;
	std::vector<DateTimeValue> dateTimes;
	size_t rowCount = 0;
	/** -1: before first, rowCount: after last */
	int64_t position = -1;
	mutable bool lastWasNull = false;
};

template <typename T>
std::optional<T> ResultSet::getObject(int32_t columnIndex) const {
	if (cell(columnIndex).isNull)
		return std::nullopt;
	if constexpr (std::is_same_v<T, bool>)
		return getBoolean(columnIndex);
	else if constexpr (std::is_same_v<T, int8_t>)
		return getByte(columnIndex);
	else if constexpr (std::is_same_v<T, int16_t>)
		return getShort(columnIndex);
	else if constexpr (std::is_same_v<T, int32_t>)
		return getInt(columnIndex);
	else if constexpr (std::is_same_v<T, int64_t>)
		return getLong(columnIndex);
	else if constexpr (std::is_same_v<T, float>)
		return getFloat(columnIndex);
	else if constexpr (std::is_same_v<T, double>)
		return getDouble(columnIndex);
	else if constexpr (std::is_same_v<T, std::string>)
		return getString(columnIndex);
	else if constexpr (std::is_same_v<T, std::vector<uint8_t>>)
		return getBytes(columnIndex);
	else if constexpr (std::is_same_v<T, Timestamp>)
		return getTimestamp(columnIndex);
	else if constexpr (std::is_same_v<T, Date>)
		return getDate(columnIndex);
	else
		static_assert(sizeof(T) == 0, "unsupported getObject type");
}

/**
 * Fills a ResultSet row by row, cell by cell. Used by PreparedStatement to copy results from the server; also handy to create result sets
 * in tests. The value passed for a cell must match the storage of its column type:
 * <ul>
 * <li>integer types and YEAR: addLong (signed columns) or addUnsignedLong (unsigned columns)</li>
 * <li>FLOAT, DOUBLE: addDouble</li>
 * <li>DATE, TIME, DATETIME, TIMESTAMP: addDateTime</li>
 * <li>everything else (strings, DECIMAL as text, BIT as big endian bytes, BLOBs, JSON, ...): addBytes</li>
 * </ul>
 * addNull works for any column.
 */
class ResultSetBuilder {
public:
	ResultSetBuilder(std::vector<ColumnDefinition> columns, ResultSetOptions options);
	~ResultSetBuilder();

	ResultSetBuilder& addNull();
	ResultSetBuilder& addLong(int64_t value);
	ResultSetBuilder& addUnsignedLong(uint64_t value);
	ResultSetBuilder& addDouble(double value);
	ResultSetBuilder& addBytes(std::string_view value);
	ResultSetBuilder& addDateTime(const DateTimeValue& value);

	/** reserves memory for the given number of rows */
	void reserve(size_t rows);

	/** @throws IllegalStateException if the last row is incomplete */
	std::unique_ptr<ResultSet> build();

private:
	const ColumnDefinition& nextColumn();

	std::unique_ptr<ResultSet> resultSet;
};

} // namespace aion::commons::database
