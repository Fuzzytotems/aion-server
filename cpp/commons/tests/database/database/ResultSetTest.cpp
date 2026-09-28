#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/utils/Exception.h"

using namespace aion::commons::database;
using namespace std::chrono;

namespace {

ColumnDefinition column(std::string label, ColumnType type, bool isUnsigned = false, std::string name = {}, std::string table = {}) {
	ColumnDefinition c;
	c.label = std::move(label);
	c.name = name.empty() ? c.label : std::move(name);
	c.table = std::move(table);
	c.type = type;
	c.isUnsigned = isUnsigned;
	return c;
}

ResultSetOptions utcOptions(int32_t type = ResultSet::TYPE_FORWARD_ONLY) {
	ResultSetOptions options;
	options.timeZone = ConnectionTimeZone::ofOffset(seconds(0));
	options.type = type;
	return options;
}

/** single row, single column */
std::unique_ptr<ResultSet> single(ColumnDefinition def, auto add, ResultSetOptions options = utcOptions()) {
	ResultSetBuilder builder({std::move(def)}, std::move(options));
	add(builder);
	auto rs = builder.build();
	EXPECT_TRUE(rs->next());
	return rs;
}

std::unique_ptr<ResultSet> text(std::string_view value, ColumnType type = ColumnType::VARCHAR) {
	return single(column("c", type), [&](ResultSetBuilder& b) { b.addBytes(value); });
}

std::unique_ptr<ResultSet> signedValue(int64_t value, ColumnType type = ColumnType::BIGINT) {
	return single(column("c", type), [&](ResultSetBuilder& b) { b.addLong(value); });
}

std::unique_ptr<ResultSet> doubleValue(double value, ColumnType type = ColumnType::DOUBLE) {
	return single(column("c", type), [&](ResultSetBuilder& b) { b.addDouble(value); });
}

std::string sqlStateOf(auto&& function) {
	try {
		function();
	} catch (const SQLException& e) {
		return e.getSQLState() + ": " + e.what();
	}
	return "no exception";
}

std::unique_ptr<ResultSet> threeRows(int32_t type) {
	ResultSetBuilder builder({column("id", ColumnType::INT)}, utcOptions(type));
	builder.addLong(10).addLong(20).addLong(30);
	return builder.build();
}

} // namespace

TEST(ResultSetTest, ForwardNavigation) {
	auto rs = threeRows(ResultSet::TYPE_FORWARD_ONLY);
	EXPECT_EQ(rs->getRowCount(), 3u);
	EXPECT_TRUE(rs->isBeforeFirst());
	EXPECT_FALSE(rs->isAfterLast());
	EXPECT_EQ(rs->getRow(), 0);
	EXPECT_EQ(sqlStateOf([&] { rs->getInt(1); }), "S1000: Before start of result set");
	std::vector<int32_t> ids;
	while (rs->next()) {
		ids.push_back(rs->getInt("ID"));
		if (ids.size() == 1)
			EXPECT_TRUE(rs->isFirst());
	}
	EXPECT_EQ(ids, (std::vector<int32_t>{10, 20, 30}));
	EXPECT_TRUE(rs->isAfterLast());
	EXPECT_FALSE(rs->next());
	EXPECT_EQ(rs->getRow(), 0);
	EXPECT_EQ(sqlStateOf([&] { rs->getInt(1); }), "S1000: After end of result set");
	const std::string forwardOnly = "S1009: Operation not allowed for a result set of type ResultSet.TYPE_FORWARD_ONLY.";
	EXPECT_EQ(sqlStateOf([&] { rs->beforeFirst(); }), forwardOnly);
	EXPECT_EQ(sqlStateOf([&] { rs->last(); }), forwardOnly);
	EXPECT_EQ(sqlStateOf([&] { rs->previous(); }), forwardOnly);
	EXPECT_EQ(sqlStateOf([&] { rs->absolute(1); }), forwardOnly);
}

TEST(ResultSetTest, EmptyResultSet) {
	auto rs = ResultSetBuilder({column("id", ColumnType::INT)}, utcOptions()).build();
	EXPECT_FALSE(rs->isBeforeFirst()); // Connector/J: false for empty result sets
	EXPECT_FALSE(rs->isAfterLast());
	EXPECT_FALSE(rs->next());
	EXPECT_FALSE(rs->isAfterLast());
	EXPECT_EQ(sqlStateOf([&] { rs->getInt(1); }), "S1000: Illegal operation on empty result set.");

	auto noColumns = ResultSetBuilder({}, utcOptions()).build();
	EXPECT_FALSE(noColumns->next());
	EXPECT_EQ(noColumns->getMetaData().getColumnCount(), 0);
}

TEST(ResultSetTest, ScrollingLikeGetUsedIds) {
	auto rs = threeRows(ResultSet::TYPE_SCROLL_INSENSITIVE);
	// the DAO idiom: rs.last(); count = rs.getRow(); rs.beforeFirst(); while (rs.next()) ...
	EXPECT_TRUE(rs->last());
	EXPECT_EQ(rs->getRow(), 3);
	EXPECT_TRUE(rs->isLast());
	rs->beforeFirst();
	EXPECT_TRUE(rs->next());
	EXPECT_EQ(rs->getInt(1), 10);

	EXPECT_TRUE(rs->absolute(-1));
	EXPECT_EQ(rs->getInt(1), 30);
	EXPECT_TRUE(rs->absolute(2));
	EXPECT_EQ(rs->getInt(1), 20);
	EXPECT_TRUE(rs->previous());
	EXPECT_EQ(rs->getInt(1), 10);
	EXPECT_FALSE(rs->previous());
	EXPECT_TRUE(rs->isBeforeFirst());
	EXPECT_FALSE(rs->absolute(4));
	EXPECT_TRUE(rs->isAfterLast());
	EXPECT_TRUE(rs->previous());
	EXPECT_EQ(rs->getInt(1), 30);
	EXPECT_FALSE(rs->absolute(-4));
	EXPECT_TRUE(rs->isBeforeFirst());
	EXPECT_FALSE(rs->absolute(0));
	EXPECT_TRUE(rs->first());
	EXPECT_TRUE(rs->relative(2));
	EXPECT_EQ(rs->getInt(1), 30);
	EXPECT_FALSE(rs->relative(5));
	EXPECT_TRUE(rs->isAfterLast());
	EXPECT_FALSE(rs->relative(-10));
	EXPECT_TRUE(rs->isBeforeFirst());
	rs->afterLast();
	EXPECT_TRUE(rs->isAfterLast());
}

TEST(ResultSetTest, ColumnLookup) {
	ResultSetBuilder builder({column("cnt", ColumnType::BIGINT, false, "id", "legions"), column("name", ColumnType::VARCHAR, false, "name", "l"),
														column("Name", ColumnType::VARCHAR)},
		utcOptions());
	builder.addLong(5).addBytes("first").addBytes("second");
	auto rs = builder.build();
	ASSERT_TRUE(rs->next());
	EXPECT_EQ(rs->findColumn("CNT"), 1);
	EXPECT_EQ(rs->findColumn("id"), 1); // original column name
	EXPECT_EQ(rs->findColumn("legions.id"), 1);
	EXPECT_EQ(rs->findColumn("NAME"), 2); // first match wins
	EXPECT_EQ(rs->getString("name"), "first");
	EXPECT_EQ(sqlStateOf([&] { rs->findColumn("missing"); }), "S0022: Column 'missing' not found.");
	EXPECT_EQ(sqlStateOf([&] { rs->getInt(0); }), "S1009: Column Index out of range, 0 < 1.");
	EXPECT_EQ(sqlStateOf([&] { rs->getInt(4); }), "S1009: Column Index out of range, 4 > 3.");

	ResultSetMetaData md = rs->getMetaData();
	EXPECT_EQ(md.getColumnCount(), 3);
	EXPECT_EQ(md.getColumnLabel(1), "cnt");
	EXPECT_EQ(md.getColumnName(1), "id");
	EXPECT_EQ(md.getTableName(1), "legions");
	EXPECT_EQ(md.getColumnType(1), Types::BIGINT);
	EXPECT_EQ(md.getColumnType(2), Types::VARCHAR);
	EXPECT_EQ(md.getColumnTypeName(1), "BIGINT");
	EXPECT_THROW(md.getColumnLabel(4), SQLException);
}

TEST(ResultSetTest, NullHandling) {
	std::vector<ColumnDefinition> columns = {column("i", ColumnType::INT), column("s", ColumnType::VARCHAR), column("t", ColumnType::DATETIME),
		column("d", ColumnType::DATE), column("b", ColumnType::BLOB), column("f", ColumnType::FLOAT)};
	ResultSetBuilder builder(columns, utcOptions());
	for (size_t i = 0; i < columns.size(); ++i)
		builder.addNull();
	auto rs = builder.build();
	ASSERT_TRUE(rs->next());
	EXPECT_EQ(rs->getInt(1), 0);
	EXPECT_TRUE(rs->wasNull());
	EXPECT_EQ(rs->getLong("i"), 0);
	EXPECT_FALSE(rs->getBoolean(1));
	EXPECT_EQ(rs->getObject<int32_t>("i"), std::nullopt);
	EXPECT_TRUE(rs->wasNull());
	EXPECT_EQ(rs->getString(2), "");
	EXPECT_TRUE(rs->wasNull());
	EXPECT_EQ(rs->getObject<std::string>(2), std::nullopt);
	EXPECT_EQ(rs->getTimestamp(3), std::nullopt);
	EXPECT_EQ(rs->getDate(4), std::nullopt);
	EXPECT_TRUE(rs->getBytes(5).empty());
	EXPECT_EQ(rs->getObject<std::vector<uint8_t>>(5), std::nullopt);
	EXPECT_EQ(rs->getFloat(6), 0.0f);
	EXPECT_TRUE(rs->wasNull());
}

TEST(ResultSetTest, WasNullResetsOnNonNull) {
	ResultSetBuilder builder({column("a", ColumnType::INT), column("b", ColumnType::INT)}, utcOptions());
	builder.addNull().addLong(7);
	auto rs = builder.build();
	ASSERT_TRUE(rs->next());
	rs->getInt(1);
	EXPECT_TRUE(rs->wasNull());
	EXPECT_EQ(rs->getObject<int32_t>(2), 7);
	EXPECT_FALSE(rs->wasNull());
}

TEST(ResultSetTest, IntegerRangeChecks) {
	auto rs = signedValue(3000000000LL);
	EXPECT_EQ(rs->getLong(1), 3000000000LL);
	EXPECT_EQ(rs->getDouble(1), 3e9);
	EXPECT_EQ(sqlStateOf([&] { rs->getInt(1); }), "22003: Value '3000000000' is outside of valid range for type java.lang.Integer");
	EXPECT_EQ(sqlStateOf([&] { rs->getShort(1); }), "22003: Value '3000000000' is outside of valid range for type java.lang.Short");

	rs = signedValue(-129, ColumnType::SMALLINT);
	EXPECT_EQ(rs->getShort(1), -129);
	EXPECT_EQ(sqlStateOf([&] { rs->getByte(1); }), "22003: Value '-129' is outside of valid range for type java.lang.Byte");

	rs = single(column("u", ColumnType::BIGINT, true), [](ResultSetBuilder& b) { b.addUnsignedLong(18446744073709551615ull); });
	EXPECT_EQ(rs->getString(1), "18446744073709551615");
	EXPECT_EQ(sqlStateOf([&] { rs->getLong(1); }), "22003: Value '18446744073709551615' is outside of valid range for type java.lang.Long");
	EXPECT_TRUE(rs->getBoolean(1));

	rs = single(column("u", ColumnType::TINYINT, true), [](ResultSetBuilder& b) { b.addUnsignedLong(200); });
	EXPECT_EQ(rs->getInt(1), 200);
	EXPECT_THROW(rs->getByte(1), SQLException);
}

TEST(ResultSetTest, FloatingPointConversions) {
	auto rs = doubleValue(2147483647.9);
	EXPECT_THROW(rs->getInt(1), SQLException); // Connector/J checks the double value itself
	rs = doubleValue(-12.9);
	EXPECT_EQ(rs->getInt(1), -12); // truncation towards zero
	EXPECT_EQ(rs->getLong(1), -12);
	EXPECT_FALSE(rs->getBoolean(1));
	rs = doubleValue(std::nan(""));
	EXPECT_EQ(rs->getInt(1), 0);
	EXPECT_EQ(rs->getLong(1), 0);
	rs = doubleValue(9223372036854775807.0);
	EXPECT_EQ(rs->getLong(1), std::numeric_limits<int64_t>::max());
	rs = doubleValue(1e300);
	EXPECT_EQ(sqlStateOf([&] { rs->getFloat(1); }).substr(0, 6), "22003:");
	EXPECT_EQ(rs->getDouble(1), 1e300);

	rs = doubleValue(static_cast<float>(1.1f), ColumnType::FLOAT);
	EXPECT_EQ(rs->getFloat(1), 1.1f);
	EXPECT_EQ(rs->getString(1), "1.1");
	rs = doubleValue(0.1);
	EXPECT_EQ(rs->getString(1), "0.1");
	EXPECT_TRUE(rs->getBoolean(1));
	rs = doubleValue(-1.0);
	EXPECT_TRUE(rs->getBoolean(1));
}

TEST(ResultSetTest, BooleanLikeConnectorJ) {
	EXPECT_TRUE(signedValue(1)->getBoolean(1));
	EXPECT_TRUE(signedValue(-1)->getBoolean(1));
	EXPECT_FALSE(signedValue(-2)->getBoolean(1));
	EXPECT_FALSE(signedValue(0)->getBoolean(1));
	EXPECT_TRUE(text("Y")->getBoolean(1));
	EXPECT_TRUE(text(" true ")->getBoolean(1));
	EXPECT_FALSE(text("n")->getBoolean(1));
	EXPECT_FALSE(text("FALSE")->getBoolean(1));
	EXPECT_TRUE(text("-1")->getBoolean(1));
	EXPECT_TRUE(text("0.5")->getBoolean(1));
	EXPECT_FALSE(text("")->getBoolean(1));
	EXPECT_TRUE(text("99999999999999999999")->getBoolean(1));
	EXPECT_EQ(sqlStateOf([] { text("yes")->getBoolean(1); }), "S1009: Cannot determine value type from string 'yes'");
	EXPECT_THROW(single(column("t", ColumnType::DATETIME), [](ResultSetBuilder& b) { b.addDateTime({}); })->getBoolean(1), SQLException);
}

TEST(ResultSetTest, NumericStrings) {
	EXPECT_EQ(text("42")->getInt(1), 42);
	EXPECT_EQ(text("-42")->getByte(1), -42);
	EXPECT_EQ(text("")->getInt(1), 0); // emptyStringsConvertToZero
	EXPECT_EQ(text("12.75")->getInt(1), 12);
	EXPECT_EQ(text("1e3")->getShort(1), 1000);
	EXPECT_EQ(text(".5")->getDouble(1), 0.5);
	EXPECT_EQ(text("1e400")->getDouble(1), std::numeric_limits<double>::infinity());
	EXPECT_EQ(sqlStateOf([] { text(" 42")->getInt(1); }), "S1009: Cannot determine value type from string ' 42'");
	EXPECT_EQ(sqlStateOf([] { text("abc")->getLong(1); }), "S1009: Cannot determine value type from string 'abc'");
	EXPECT_EQ(sqlStateOf([] { text("99999999999999999999")->getLong(1); }),
		"22003: Value '99999999999999999999' is outside of valid range for type java.lang.Long");
	EXPECT_EQ(sqlStateOf([] { text("128")->getByte(1); }), "22003: Value '128' is outside of valid range for type java.lang.Byte");
	EXPECT_EQ(text("123", ColumnType::TEXT)->getFloat(1), 123.0f);
}

TEST(ResultSetTest, DecimalColumns) {
	EXPECT_EQ(text("123.99", ColumnType::DECIMAL)->getInt(1), 123);
	EXPECT_EQ(text("-0.5", ColumnType::DECIMAL)->getInt(1), 0);
	EXPECT_EQ(text("2147483647.00", ColumnType::DECIMAL)->getInt(1), 2147483647);
	EXPECT_THROW(text("2147483647.01", ColumnType::DECIMAL)->getInt(1), SQLException);
	EXPECT_THROW(text("-2147483648.5", ColumnType::DECIMAL)->getInt(1), SQLException);
	EXPECT_EQ(text("-2147483648.0", ColumnType::DECIMAL)->getInt(1), -2147483648);
	EXPECT_THROW(text("123456789012345678901234.5", ColumnType::DECIMAL)->getLong(1), SQLException);
	EXPECT_EQ(text("123.25", ColumnType::DECIMAL)->getDouble(1), 123.25);
	EXPECT_EQ(text("123.25", ColumnType::DECIMAL)->getString(1), "123.25");
	EXPECT_TRUE(text("0.01", ColumnType::DECIMAL)->getBoolean(1));
}

TEST(ResultSetTest, BitColumns) {
	auto rs = text(std::string_view("\x01\x02", 2), ColumnType::BIT);
	EXPECT_EQ(rs->getInt(1), 258);
	EXPECT_EQ(rs->getString(1), "258");
	EXPECT_EQ(rs->getBytes(1), (std::vector<uint8_t>{1, 2}));
	EXPECT_TRUE(text(std::string_view("\x01", 1), ColumnType::BIT)->getBoolean(1));
	EXPECT_FALSE(text(std::string_view("\x00", 1), ColumnType::BIT)->getBoolean(1));
}

TEST(ResultSetTest, StringsAndBytes) {
	std::string binary("a\0b\xff", 4);
	auto rs = text(binary, ColumnType::BLOB);
	EXPECT_EQ(rs->getBytes(1), (std::vector<uint8_t>{'a', 0, 'b', 0xff}));
	EXPECT_EQ(rs->getString(1), binary);
	EXPECT_EQ(rs->getObject<std::vector<uint8_t>>(1)->size(), 4u);
	EXPECT_EQ(signedValue(-5)->getString(1), "-5");
	EXPECT_EQ(signedValue(-5)->getBytes(1), (std::vector<uint8_t>{'-', '5'}));
	EXPECT_EQ(text("Grüße")->getString(1), "Grüße");
}

TEST(ResultSetTest, TemporalColumns) {
	ResultSetOptions options = utcOptions();
	options.timeZone = ConnectionTimeZone::ofOffset(hours(2));
	DateTimeValue dt{.kind = DateTimeValue::Kind::DATETIME, .year = 2024, .month = 5, .day = 1, .hour = 12, .minute = 0, .second = 1, .microsecond = 123456};
	ColumnDefinition def = column("t", ColumnType::DATETIME);
	def.decimals = 3;
	auto rs = single(def, [&](ResultSetBuilder& b) { b.addDateTime(dt); }, options);
	Timestamp expected = sys_days(2024y / May / 1) + hours(10) + seconds(1) + milliseconds(123);
	EXPECT_EQ(rs->getTimestamp(1), expected);
	EXPECT_EQ(rs->getObject<Timestamp>(1), expected);
	EXPECT_EQ(rs->getDate(1), Date(2024y / May / 1));
	EXPECT_EQ(rs->getString(1), "2024-05-01 12:00:01.123");
	EXPECT_EQ(sqlStateOf([&] { rs->getInt(1); }), "S1009: Unsupported conversion from DATETIME to java.lang.Integer");

	rs = single(column("d", ColumnType::DATE), [](ResultSetBuilder& b) { b.addDateTime({.kind = DateTimeValue::Kind::DATE, .year = 2024, .month = 2, .day = 29}); }, options);
	EXPECT_EQ(rs->getDate(1), Date(2024y / February / 29));
	EXPECT_EQ(rs->getTimestamp(1), sys_days(2024y / February / 28) + hours(22));
	EXPECT_EQ(rs->getString(1), "2024-02-29");

	rs = single(column("tm", ColumnType::TIME), [](ResultSetBuilder& b) { b.addDateTime({.kind = DateTimeValue::Kind::TIME, .hour = 25}); }, options);
	EXPECT_EQ(rs->getString(1), "25:00:00");
	EXPECT_EQ(sqlStateOf([&] { rs->getTimestamp(1); }).substr(0, 37), "S1009: The value '25:00:00' is an inv");

	rs = text("2024-05-01 12:00:01");
	EXPECT_EQ(rs->getTimestamp(1), sys_days(2024y / May / 1) + hours(12) + seconds(1));
	EXPECT_EQ(rs->getDate(1), Date(2024y / May / 1));
	EXPECT_EQ(sqlStateOf([] { text("yesterday")->getTimestamp(1); }), "S1009: Cannot convert string 'yesterday' to java.sql.Timestamp value");
	EXPECT_THROW(signedValue(5)->getTimestamp(1), SQLException);

	rs = single(column("y", ColumnType::YEAR, true), [](ResultSetBuilder& b) { b.addUnsignedLong(2024); });
	EXPECT_EQ(rs->getDate(1), Date(2024y / January / 1));
	EXPECT_EQ(rs->getInt(1), 2024);
}

TEST(ResultSetTest, ZeroDateBehaviour) {
	DateTimeValue zero{.kind = DateTimeValue::Kind::DATETIME};
	auto rs = single(column("t", ColumnType::DATETIME), [&](ResultSetBuilder& b) { b.addDateTime(zero); });
	EXPECT_EQ(sqlStateOf([&] { rs->getTimestamp(1); }), "S1009: Zero date value prohibited");
	EXPECT_EQ(rs->getString(1), "0000-00-00 00:00:00");

	ResultSetOptions options = utcOptions();
	options.zeroDateTimeBehavior = ZeroDateTimeBehavior::CONVERT_TO_NULL;
	rs = single(column("t", ColumnType::DATETIME), [&](ResultSetBuilder& b) { b.addDateTime(zero); }, options);
	EXPECT_EQ(rs->getTimestamp(1), std::nullopt);
	EXPECT_EQ(rs->getDate(1), std::nullopt);

	options.zeroDateTimeBehavior = ZeroDateTimeBehavior::ROUND;
	rs = single(column("t", ColumnType::DATETIME), [&](ResultSetBuilder& b) { b.addDateTime(zero); }, options);
	EXPECT_EQ(rs->getDate(1), Date(year(1) / January / 1));
	EXPECT_EQ(rs->getTimestamp(1), Timestamp(sys_days(year(1) / January / 1)));
}

TEST(ResultSetTest, BuilderValidatesStorage) {
	ResultSetBuilder builder({column("i", ColumnType::INT), column("s", ColumnType::VARCHAR)}, utcOptions());
	EXPECT_THROW(builder.addBytes("x"), aion::commons::utils::IllegalArgumentException);
	EXPECT_THROW(builder.addDouble(1.0), aion::commons::utils::IllegalArgumentException);
	builder.addLong(1);
	EXPECT_THROW(builder.build(), aion::commons::utils::IllegalStateException);
	builder.addBytes("x");
	auto rs = builder.build();
	EXPECT_EQ(rs->getRowCount(), 1u);
	EXPECT_THROW(builder.addNull(), aion::commons::utils::IllegalStateException);
}
