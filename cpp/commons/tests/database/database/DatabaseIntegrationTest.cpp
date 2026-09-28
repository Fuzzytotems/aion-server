#include <array>
#include <atomic>
#include <cstdlib>
#include <future>
#include <random>
#include <thread>

#include <asio/connect.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"

using namespace aion::commons::database;
using namespace std::chrono;
using namespace std::chrono_literals;

// Integration tests against a real MariaDB/MySQL server. They run only if these environment variables are set, otherwise they are skipped:
//   AION_TEST_DATABASE_URL       e.g. jdbc:mysql://localhost:3306/aion_test   (the database must exist)
//   AION_TEST_DATABASE_USER      e.g. root
//   AION_TEST_DATABASE_PASSWORD  (optional)
// The tests create and drop their own tables (named aion_cpp_test_*).

namespace {

std::string env(const char* name) {
	const char* value = std::getenv(name);
	return value ? value : "";
}

/**
 * Forwards one TCP connection to the database server on 127.0.0.1. After freeze() it silently discards all data in both directions while
 * keeping the sockets open, like a peer that vanished without a TCP reset (NAT/firewall idle timeout).
 */
class FreezableTcpProxy {
public:
	explicit FreezableTcpProxy(uint16_t targetPort) : acceptor(context, {asio::ip::address_v4::loopback(), 0}) {
		worker = std::thread([this, targetPort] {
			try {
				acceptor.accept(client);
				server.connect({asio::ip::address_v4::loopback(), targetPort});
				std::thread upstream([this] { pump(client, server); });
				pump(server, client);
				upstream.join();
			} catch (const std::exception&) {
				// closed before a client connected
			}
		});
	}

	~FreezableTcpProxy() {
		close();
		worker.join();
	}

	uint16_t port() const { return acceptor.local_endpoint().port(); }
	void freeze() { frozen = true; }

	void close() {
		std::error_code ec;
		acceptor.close(ec);
		client.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
		server.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
	}

private:
	void pump(asio::ip::tcp::socket& from, asio::ip::tcp::socket& to) {
		std::array<char, 16384> buffer;
		std::error_code ec;
		while (true) {
			size_t n = from.read_some(asio::buffer(buffer), ec);
			if (ec)
				break;
			if (!frozen)
				asio::write(to, asio::buffer(buffer.data(), n), ec);
			if (ec)
				break;
		}
		to.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
	}

	asio::io_context context;
	asio::ip::tcp::acceptor acceptor;
	asio::ip::tcp::socket client{context};
	asio::ip::tcp::socket server{context};
	std::atomic<bool> frozen{false};
	std::thread worker;
};

class DatabaseIntegrationTest : public ::testing::Test {
protected:
	void SetUp() override {
		url = env("AION_TEST_DATABASE_URL");
		if (url.empty())
			GTEST_SKIP() << "AION_TEST_DATABASE_URL is not set";
		user = env("AION_TEST_DATABASE_USER");
		password = env("AION_TEST_DATABASE_PASSWORD");
		properties = ConnectionProperties::parse(url, user, password);
		table = fmt::format("aion_cpp_test_{}", std::random_device()() % 1000000);
		con = Connection::open(properties);
		con->executeSimple("CREATE TABLE " + table +
			" (id INT AUTO_INCREMENT PRIMARY KEY, flag TINYINT(1), tiny TINYINT, small SMALLINT, num INT, big BIGINT, ubig BIGINT UNSIGNED, "
			"f FLOAT, d DOUBLE, dec_value DECIMAL(10,2), s VARCHAR(100) UNIQUE, txt MEDIUMTEXT, bin BLOB, big_bin LONGBLOB, ts DATETIME(3), "
			"ts2 TIMESTAMP NULL DEFAULT NULL, day DATE, tm TIME, bits BIT(16), e ENUM('A','B')) ENGINE=InnoDB");
	}

	void TearDown() override {
		DatabaseFactory::shutdown();
		if (url.empty())
			return;
		try {
			if (!con || con->isClosed())
				con = Connection::open(properties);
			con->executeSimple("DROP PROCEDURE IF EXISTS " + table + "_proc");
			con->executeSimple("DROP TABLE IF EXISTS " + table + "_child");
			con->executeSimple("DROP TABLE IF EXISTS " + table);
		} catch (const std::exception& e) {
			ADD_FAILURE() << "cleanup failed: " << e.what();
		}
	}

	static std::string sslCipher(Connection& connection) {
		auto rs = connection.prepareStatement("SELECT VARIABLE_VALUE FROM information_schema.SESSION_STATUS WHERE VARIABLE_NAME = 'SSL_CIPHER'")
								->executeQuery();
		return rs->next() ? rs->getString(1) : "";
	}

	int64_t countRows(Connection& connection) {
		auto st = connection.prepareStatement("SELECT COUNT(*) FROM " + table);
		auto rs = st->executeQuery();
		EXPECT_TRUE(rs->next());
		return rs->getLong(1);
	}

	void initFactory(int32_t maxConnections, int32_t timeoutMillis) {
		DatabaseFactory::init(url, user, password, maxConnections, timeoutMillis);
	}

	std::string url, user, password, table;
	ConnectionProperties properties;
	std::unique_ptr<Connection> con;
};

} // namespace

TEST_F(DatabaseIntegrationTest, AllTypesRoundTrip) {
	std::vector<uint8_t> blob(300000);
	for (size_t i = 0; i < blob.size(); ++i)
		blob[i] = static_cast<uint8_t>(i * 7);
	Timestamp timestamp = time_point_cast<milliseconds>(sys_days(2024y / May / 17) + 13h + 14min + 15s + 678ms);
	{
		auto st = con->prepareStatement("INSERT INTO " + table +
			" (flag, tiny, small, num, big, ubig, f, d, dec_value, s, txt, bin, big_bin, ts, ts2, day, tm, bits, e) "
			"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, '12:34:56', b'0000000100000010', ?)");
		EXPECT_EQ(st->getParameterCount(), 17);
		st->setBoolean(1, true);
		st->setByte(2, -128);
		st->setShort(3, 32767);
		st->setInt(4, -2147483647 - 1);
		st->setLong(5, 9007199254740993LL);
		st->setObject(6, 18446744073709551615ull);
		st->setFloat(7, 1.5f);
		st->setDouble(8, 0.1);
		st->setString(9, "12345678.91");
		st->setString(10, "Grüße 😀");
		st->setString(11, std::string(70000, 'x'));
		st->setBytes(12, std::span<const uint8_t>(blob.data(), 60000));
		st->setBytes(13, blob);
		st->setTimestamp(14, timestamp);
		st->setTimestamp(15, timestamp);
		st->setDate(16, Date(2024y / February / 29));
		st->setString(17, "B");
		EXPECT_EQ(st->executeUpdate(), 1);
		EXPECT_EQ(st->getUpdateCount(), 1);

		st->clearParameters();
		EXPECT_THROW(st->execute(), SQLException); // no values
		for (int i = 1; i <= 17; ++i)
			st->setNull(i, Types::NULL_TYPE);
		// nullable overloads (Java: passing null references)
		st->setBoolean(1, std::optional<bool>());
		st->setByte(2, std::optional<int8_t>());
		st->setShort(3, std::optional<int16_t>());
		st->setInt(4, std::optional<int32_t>());
		st->setLong(5, std::optional<int64_t>());
		st->setFloat(7, std::optional<float>());
		st->setDouble(8, std::optional<double>());
		st->setString(9, std::optional<std::string_view>());
		st->setObject(10, std::optional<std::string>(), Types::VARCHAR);
		st->setString(11, std::optional<std::string>());
		st->setBytes(12, std::optional<std::vector<uint8_t>>());
		st->setTimestamp(14, std::optional<Timestamp>());
		st->setDate(16, std::optional<Date>());
		st->setObject(17, std::nullopt);
		EXPECT_FALSE(st->execute());
	}
	auto st = con->prepareStatement("SELECT * FROM " + table + " ORDER BY id");
	auto rs = st->executeQuery();
	ASSERT_TRUE(rs->next());
	EXPECT_TRUE(rs->getBoolean("flag"));
	EXPECT_EQ(rs->getByte("tiny"), -128);
	EXPECT_EQ(rs->getShort("small"), 32767);
	EXPECT_EQ(rs->getInt("num"), -2147483647 - 1);
	EXPECT_EQ(rs->getLong("big"), 9007199254740993LL);
	EXPECT_EQ(rs->getString("ubig"), "18446744073709551615");
	EXPECT_THROW(rs->getLong("ubig"), SQLException);
	EXPECT_EQ(rs->getFloat("f"), 1.5f);
	EXPECT_EQ(rs->getDouble("d"), 0.1);
	EXPECT_EQ(rs->getString("dec_value"), "12345678.91");
	EXPECT_EQ(rs->getInt("dec_value"), 12345678);
	EXPECT_EQ(rs->getString("s"), "Grüße 😀");
	EXPECT_EQ(rs->getString("txt"), std::string(70000, 'x'));
	EXPECT_EQ(rs->getBytes("bin"), std::vector<uint8_t>(blob.begin(), blob.begin() + 60000));
	EXPECT_EQ(rs->getBytes("big_bin"), blob);
	EXPECT_EQ(rs->getTimestamp("ts"), timestamp);
	// TIMESTAMP without fraction: MariaDB truncates, MySQL rounds
	auto ts2 = rs->getTimestamp("ts2");
	ASSERT_TRUE(ts2);
	EXPECT_TRUE(*ts2 == timestamp - milliseconds(678) || *ts2 == timestamp + milliseconds(322));
	EXPECT_EQ(rs->getDate("day"), Date(2024y / February / 29));
	EXPECT_EQ(rs->getString("tm"), "12:34:56");
	EXPECT_EQ(rs->getInt("bits"), 258);
	EXPECT_EQ(rs->getString("e"), "B");
	EXPECT_FALSE(rs->wasNull());
	ResultSetMetaData md = rs->getMetaData();
	EXPECT_EQ(md.getColumnType(rs->findColumn("flag")), Types::BIT);
	EXPECT_EQ(md.getColumnType(rs->findColumn("ts")), Types::TIMESTAMP);
	EXPECT_EQ(md.getTableName(1), table);

	ASSERT_TRUE(rs->next());
	EXPECT_EQ(rs->getInt("num"), 0);
	EXPECT_TRUE(rs->wasNull());
	EXPECT_EQ(rs->getObject<int32_t>("num"), std::nullopt);
	EXPECT_EQ(rs->getString("s"), "");
	EXPECT_TRUE(rs->wasNull());
	EXPECT_EQ(rs->getTimestamp("ts"), std::nullopt);
	EXPECT_EQ(rs->getDate("day"), std::nullopt);
	EXPECT_TRUE(rs->getBytes("big_bin").empty());
	EXPECT_FALSE(rs->next());
}

TEST_F(DatabaseIntegrationTest, TimestampsUseConnectionTimeZone) {
	ConnectionProperties plus5 = properties;
	plus5.timeZone = ConnectionTimeZone::of("+05:00");
	auto c = Connection::open(plus5);
	auto insert = c->prepareStatement("INSERT INTO " + table + " (ts) VALUES (?)");
	insert->setTimestamp(1, Timestamp(sys_days(2024y / January / 1) + 22h));
	insert->executeUpdate();
	auto rs = c->prepareStatement("SELECT ts, CAST(ts AS CHAR) FROM " + table)->executeQuery();
	ASSERT_TRUE(rs->next());
	EXPECT_EQ(rs->getString(2), "2024-01-02 03:00:00.000");
	EXPECT_EQ(rs->getTimestamp(1), Timestamp(sys_days(2024y / January / 1) + 22h));
}

TEST_F(DatabaseIntegrationTest, GeneratedKeys) {
	auto st = con->prepareStatement("INSERT INTO " + table + " (s) VALUES (?), (?)", Statement::RETURN_GENERATED_KEYS);
	st->setString(1, "a");
	st->setString(2, "b");
	EXPECT_EQ(st->executeUpdate(), 2);
	auto keys = st->getGeneratedKeys();
	std::vector<int32_t> ids;
	while (keys->next())
		ids.push_back(keys->getInt(1));
	ASSERT_EQ(ids.size(), 2u);
	EXPECT_EQ(ids[1], ids[0] + 1);

	auto single = con->prepareStatement("INSERT INTO " + table + " (s) VALUES (?)", Statement::RETURN_GENERATED_KEYS);
	for (const char* value : {"c", "d"}) {
		single->setString(1, value);
		single->addBatch();
	}
	EXPECT_EQ(single->executeBatch(), (std::vector<int32_t>{1, 1}));
	keys = single->getGeneratedKeys();
	int count = 0;
	while (keys->next()) {
		EXPECT_GT(keys->getLong("GENERATED_KEY"), ids[1]);
		++count;
	}
	EXPECT_EQ(count, 2);

	auto noKeys = con->prepareStatement("INSERT INTO " + table + " (s) VALUES ('e')");
	noKeys->executeUpdate();
	EXPECT_THROW(noKeys->getGeneratedKeys(), SQLException);
}

TEST_F(DatabaseIntegrationTest, BatchContinuesAfterFailure) {
	auto st = con->prepareStatement("INSERT INTO " + table + " (s) VALUES (?)");
	for (const char* value : {"x", "x", "y"}) {
		st->setString(1, value);
		st->addBatch();
	}
	try {
		st->executeBatch();
		FAIL();
	} catch (const BatchUpdateException& e) {
		EXPECT_EQ(e.getErrorCode(), 1062); // duplicate entry
		EXPECT_EQ(e.getUpdateCounts(), (std::vector<int32_t>{1, Statement::EXECUTE_FAILED, 1}));
	}
	EXPECT_EQ(countRows(*con), 2);
	EXPECT_TRUE(st->executeBatch().empty()); // the batch was cleared
}

TEST_F(DatabaseIntegrationTest, UpdateCountsAndStatementKinds) {
	con->executeSimple("INSERT INTO " + table + " (num) VALUES (1)");
	auto update = con->prepareStatement("UPDATE " + table + " SET num = 1");
	EXPECT_EQ(update->executeUpdate(), 1); // matched rows, like Connector/J (useAffectedRows=false)
	EXPECT_THROW(update->executeQuery(), SQLException);
	auto select = con->prepareStatement("SELECT num FROM " + table);
	EXPECT_THROW(select->executeUpdate(), SQLException);
	EXPECT_TRUE(select->execute());
	ASSERT_NE(select->getResultSet(), nullptr);
	EXPECT_TRUE(select->getResultSet()->next());
	EXPECT_EQ(select->getUpdateCount(), -1);
	EXPECT_THROW(con->prepareStatement("SELEKT 1"), SQLException);
	try {
		con->prepareStatement("SELECT missing_column FROM " + table);
		FAIL();
	} catch (const SQLException& e) {
		EXPECT_EQ(e.getErrorCode(), 1054);
		EXPECT_EQ(e.getSQLState(), "42S22");
	}
	EXPECT_FALSE(con->isClosed()); // errors do not break the connection
	EXPECT_EQ(con->getCatalog(), properties.database.empty() ? std::nullopt : std::optional(properties.database));
}

TEST_F(DatabaseIntegrationTest, TransactionsAndSavepoints) {
	EXPECT_TRUE(con->getAutoCommit());
	EXPECT_THROW(con->commit(), SQLException);
	EXPECT_THROW(con->rollback(), SQLException);
	con->setAutoCommit(false);
	con->executeSimple("INSERT INTO " + table + " (s) VALUES ('rolled back')");
	con->rollback();
	EXPECT_EQ(countRows(*con), 0);

	con->executeSimple("INSERT INTO " + table + " (s) VALUES ('kept')");
	Savepoint savepoint = con->setSavepoint("before`second");
	con->executeSimple("INSERT INTO " + table + " (s) VALUES ('undone')");
	con->rollback(savepoint);
	Savepoint unnamed = con->setSavepoint();
	con->releaseSavepoint(unnamed);
	con->commit();
	con->setAutoCommit(true);
	EXPECT_EQ(countRows(*con), 1);
}

TEST_F(DatabaseIntegrationTest, DbHelpersAndTransactionClass) {
	initFactory(2, 5000);
	EXPECT_TRUE(DB::insertUpdate("INSERT INTO " + table + " (s, num) VALUES ('a', 1)"));
	EXPECT_TRUE(DB::insertUpdate("INSERT INTO " + table + " (s, num) VALUES (?, ?)", [](PreparedStatement& st) {
		st.setString(1, "b");
		st.setInt(2, 2);
		st.execute();
	}));
	EXPECT_FALSE(DB::insertUpdate("INSERT INTO " + table + " (s) VALUES ('a')")); // duplicate, logged

	std::vector<std::string> names;
	EXPECT_TRUE(DB::select("SELECT s FROM " + table + " WHERE num >= ? ORDER BY s", [](PreparedStatement& st) { st.setInt(1, 1); },
		[&](ResultSet& rs) {
			while (rs.next())
				names.push_back(rs.getString("s"));
		}));
	EXPECT_EQ(names, (std::vector<std::string>{"a", "b"}));

	// scrollable result set idiom of the getUsedIDs DAOs
	{
		auto c = DatabaseFactory::getConnection();
		auto st = c->prepareStatement("SELECT id FROM " + table, ResultSet::TYPE_SCROLL_INSENSITIVE, ResultSet::CONCUR_READ_ONLY);
		auto rs = st->executeQuery();
		rs->last();
		EXPECT_EQ(rs->getRow(), 2);
		rs->beforeFirst();
		int n = 0;
		while (rs->next())
			++n;
		EXPECT_EQ(n, 2);
	}

	auto statement = DB::prepareStatement("DELETE FROM " + table + " WHERE s = ?");
	ASSERT_NE(statement, nullptr);
	statement->setString(1, "b");
	DB::executeUpdateAndClose(statement);
	EXPECT_EQ(statement, nullptr);
	EXPECT_EQ(DatabaseFactory::getPool()->getActiveConnections(), 0);

	Transaction tx = DB::beginTransaction();
	tx.insertUpdate("INSERT INTO " + table + " (s) VALUES ('tx1')");
	Savepoint sp = tx.setSavepoint("sp");
	tx.insertUpdate("INSERT INTO " + table + " (s) VALUES (?)", [](PreparedStatement& st) {
		st.setString(1, "tx2");
		st.executeUpdate();
	});
	tx.releaseSavepoint(sp);
	tx.commit();
	EXPECT_THROW(tx.commit(), SQLException);
	EXPECT_EQ(countRows(*con), 3);

	{
		Transaction abandoned = DB::beginTransaction();
		abandoned.insertUpdate("INSERT INTO " + table + " (s) VALUES ('never')");
	} // rolled back by the pool
	EXPECT_EQ(countRows(*con), 3);
}

TEST_F(DatabaseIntegrationTest, ReturnedConnectionsAreReset) {
	initFactory(1, 5000);
	std::unique_ptr<PreparedStatement> leaked;
	{
		auto c = DatabaseFactory::getConnection();
		c->setAutoCommit(false);
		c->executeSimple("INSERT INTO " + table + " (s) VALUES ('uncommitted')");
		leaked = c->prepareStatement("SELECT 1");
	}
	EXPECT_TRUE(leaked->isClosed());
	try {
		leaked->executeQuery();
		FAIL();
	} catch (const SQLException& e) {
		EXPECT_EQ(std::string(e.what()), "No operations allowed after statement closed.");
	}
	auto c = DatabaseFactory::getConnection();
	EXPECT_TRUE(c->getAutoCommit());
	EXPECT_EQ(countRows(*c), 0);
}

TEST_F(DatabaseIntegrationTest, PoolExhaustionTimesOut) {
	initFactory(1, 300);
	auto held = DatabaseFactory::getConnection();
	auto start = steady_clock::now();
	try {
		DatabaseFactory::getConnection();
		FAIL();
	} catch (const SQLTransientConnectionException& e) {
		EXPECT_GE(steady_clock::now() - start, 290ms);
		EXPECT_NE(std::string(e.what()).find("Connection is not available, request timed out after"), std::string::npos);
	}
	held.close();
	EXPECT_NO_THROW(DatabaseFactory::getConnection());
}

TEST_F(DatabaseIntegrationTest, BrokenConnectionIsReplaced) {
	initFactory(1, 5000);
	int64_t firstId;
	{
		auto c = DatabaseFactory::getConnection();
		auto rs = c->prepareStatement("SELECT CONNECTION_ID()")->executeQuery();
		ASSERT_TRUE(rs->next());
		firstId = rs->getLong(1);
	}
	con->executeSimple(fmt::format("KILL {}", firstId));
	std::this_thread::sleep_for(700ms); // longer than the alive bypass window, so the idle connection is validated
	auto c = DatabaseFactory::getConnection();
	auto rs = c->prepareStatement("SELECT CONNECTION_ID()")->executeQuery();
	ASSERT_TRUE(rs->next());
	EXPECT_NE(rs->getLong(1), firstId);
}

TEST_F(DatabaseIntegrationTest, ExportedKeys) {
	con->executeSimple("CREATE TABLE " + table + "_child (id INT PRIMARY KEY, parent_id INT, FOREIGN KEY (parent_id) REFERENCES " + table +
		" (id) ON DELETE CASCADE) ENGINE=InnoDB");
	auto keys = con->getMetaData().getExportedKeys(con->getCatalog(), std::nullopt, table);
	ASSERT_TRUE(keys->next());
	EXPECT_EQ(keys->getString("FKTABLE_NAME"), table + "_child");
	EXPECT_EQ(keys->getString("FKCOLUMN_NAME"), "parent_id");
	EXPECT_EQ(keys->getInt("DELETE_RULE"), 0);
	EXPECT_FALSE(keys->next());
}

TEST_F(DatabaseIntegrationTest, MidnightAndZeroTemporalValues) {
	// The binary protocol sends midnight DATETIME/TIMESTAMP values in 4 bytes and zero values or TIME 00:00:00 in 0 bytes.
	con->executeSimple("INSERT INTO " + table +
		" (s, ts, ts2, day, tm) VALUES ('midnight', '2024-01-01 00:00:00.000', '2024-05-17 00:00:00', '2024-01-01', '00:00:00')");
	con->executeSimple("INSERT INTO " + table + " (s, ts, day, tm) VALUES ('zero', '0000-00-00 00:00:00', '0000-00-00', '-00:00:01')");
	const ConnectionTimeZone& tz = properties.timeZone;
	{
		auto rs = con->prepareStatement("SELECT ts, ts2, day, tm FROM " + table + " ORDER BY id")->executeQuery();
		ASSERT_TRUE(rs->next());
		EXPECT_EQ(rs->getString("ts"), "2024-01-01 00:00:00.000");
		EXPECT_EQ(rs->getString("ts2"), "2024-05-17 00:00:00");
		EXPECT_EQ(rs->getString("day"), "2024-01-01");
		EXPECT_EQ(rs->getString("tm"), "00:00:00");
		EXPECT_EQ(rs->getTimestamp("ts"), tz.toTimestamp(DateTimeValue{.kind = DateTimeValue::Kind::DATETIME, .year = 2024, .month = 1, .day = 1}));
		EXPECT_EQ(rs->getTimestamp("ts2"), tz.toTimestamp(DateTimeValue{.kind = DateTimeValue::Kind::DATETIME, .year = 2024, .month = 5, .day = 17}));
		EXPECT_EQ(rs->getTimestamp("tm"), tz.toTimestamp(DateTimeValue{.kind = DateTimeValue::Kind::TIME}));
		EXPECT_EQ(rs->getDate("tm"), Date(1970y / January / 1));

		ASSERT_TRUE(rs->next());
		EXPECT_EQ(rs->getString("ts"), "0000-00-00 00:00:00.000");
		EXPECT_EQ(rs->getString("day"), "0000-00-00");
		EXPECT_EQ(rs->getString("tm"), "-00:00:01");
		EXPECT_THROW(rs->getTimestamp("ts"), SQLException); // zeroDateTimeBehavior=EXCEPTION
		EXPECT_THROW(rs->getDate("day"), SQLException);
	}
	ConnectionProperties convertToNull = properties;
	convertToNull.zeroDateTimeBehavior = ZeroDateTimeBehavior::CONVERT_TO_NULL;
	auto c = Connection::open(convertToNull);
	auto rs = c->prepareStatement("SELECT ts, day FROM " + table + " WHERE s = 'zero'")->executeQuery();
	ASSERT_TRUE(rs->next());
	EXPECT_EQ(rs->getTimestamp("ts"), std::nullopt);
	EXPECT_EQ(rs->getDate("day"), std::nullopt);
}

TEST_F(DatabaseIntegrationTest, BatchMixesPlainStatementsAndParameterSets) {
	con->executeSimple("INSERT INTO " + table + " (s, num, big) VALUES ('e1', 1, 30), ('e2', 1, 20), ('e3', 1, 10), ('a1', 2, 5), ('a2', 2, 50)");
	// the idiom of AbyssRankDAO.updateRankingLists: session variables set by plain statements between the parameter sets
	auto st = con->prepareStatement("UPDATE " + table + " SET small = @a:=@a+1 WHERE num = ? AND (@minBig IS NULL OR big >= @minBig) ORDER BY big DESC LIMIT ?");
	st->addBatch("SET @minBig = 10;");
	st->addBatch("SET @a = 0;");
	st->setInt(1, 1);
	st->setInt(2, 5);
	st->addBatch();
	st->addBatch("SET @a = 0;");
	st->setInt(1, 2);
	st->setInt(2, 5);
	st->addBatch();
	st->addBatch("SET @minBig = NULL;");
	EXPECT_EQ(st->executeBatch(), (std::vector<int32_t>{0, 0, 3, 0, 1, 0}));
	std::vector<std::string> ranks;
	auto rs = con->prepareStatement("SELECT s, small FROM " + table + " ORDER BY s")->executeQuery();
	while (rs->next())
		ranks.push_back(rs->getString(1) + "=" + rs->getString(2));
	EXPECT_EQ(ranks, (std::vector<std::string>{"a1=", "a2=1", "e1=1", "e2=2", "e3=3"}));
	auto check = con->prepareStatement("SELECT @minBig IS NULL, @a")->executeQuery();
	ASSERT_TRUE(check->next());
	EXPECT_TRUE(check->getBoolean(1));
	EXPECT_EQ(check->getInt(2), 1);

	// failures of plain statements are reported like those of parameter sets
	st->addBatch("SET @a = 0;");
	st->addBatch("SELECT 1");
	st->addBatch("UPDATE no_such_table_aion SET x = 1");
	try {
		st->executeBatch();
		FAIL();
	} catch (const BatchUpdateException& e) {
		EXPECT_EQ(e.getErrorCode(), 1146) << e.what(); // the last failure
		EXPECT_EQ(e.getUpdateCounts(), (std::vector<int32_t>{0, Statement::EXECUTE_FAILED, Statement::EXECUTE_FAILED}));
	}

	auto insert = con->prepareStatement("INSERT INTO " + table + " (s) VALUES (?)", Statement::RETURN_GENERATED_KEYS);
	insert->addBatch("INSERT INTO " + table + " (s) VALUES ('p1'), ('p2')");
	insert->setString(1, "p3");
	insert->addBatch();
	EXPECT_EQ(insert->executeBatch(), (std::vector<int32_t>{2, 1}));
	auto keys = insert->getGeneratedKeys();
	int keyCount = 0;
	while (keys->next())
		++keyCount;
	EXPECT_EQ(keyCount, 3);
}

TEST_F(DatabaseIntegrationTest, BatchReportsLastErrorAndContinuesAfterLockWaitTimeout) {
	auto locker = Connection::open(properties);
	locker->setAutoCommit(false);
	locker->executeSimple("INSERT INTO " + table + " (s) VALUES ('locked')");
	con->executeSimple("SET SESSION innodb_lock_wait_timeout = 1");
	auto st = con->prepareStatement("INSERT INTO " + table + " (s) VALUES (?)");
	for (const char* value : {"a", "a", "locked", "b"}) {
		st->setString(1, value);
		st->addBatch();
	}
	try {
		st->executeBatch();
		FAIL();
	} catch (const BatchUpdateException& e) {
		// Connector/J keeps the last failure and does not abort on lock wait timeouts (the server only rolls back the statement)
		EXPECT_EQ(e.getErrorCode(), 1205) << e.what();
		EXPECT_EQ(e.getUpdateCounts(), (std::vector<int32_t>{1, Statement::EXECUTE_FAILED, Statement::EXECUTE_FAILED, 1}));
	}
	locker->rollback();
	EXPECT_EQ(countRows(*con), 2);
}

TEST_F(DatabaseIntegrationTest, BatchDeadlockVoidsPrecedingCounts) {
	con->executeSimple("INSERT INTO " + table + " (s, num) VALUES ('A', 0), ('B', 0)");
	auto other = Connection::open(properties);
	other->setAutoCommit(false);
	// make the other transaction heavier, so that InnoDB picks the batch's transaction as deadlock victim
	std::string rows;
	for (int i = 0; i < 50; ++i)
		rows += fmt::format("{}('other{}')", i == 0 ? "" : ", ", i);
	other->executeSimple("INSERT INTO " + table + " (s) VALUES " + rows);
	other->executeSimple("UPDATE " + table + " SET num = 1 WHERE s = 'B'");

	con->setAutoCommit(false);
	con->executeSimple("UPDATE " + table + " SET num = 2 WHERE s = 'A'");
	auto st = con->prepareStatement("UPDATE " + table + " SET num = 3 WHERE s = ?");
	for (const char* value : {"A", "B", "A"}) {
		st->setString(1, value);
		st->addBatch();
	}
	std::thread closeCycle([&] {
		std::this_thread::sleep_for(500ms); // the batch is waiting for B by now
		try {
			other->executeSimple("UPDATE " + table + " SET num = 1 WHERE s = 'A'");
		} catch (const SQLException&) {
		}
	});
	try {
		st->executeBatch();
		ADD_FAILURE() << "no deadlock";
	} catch (const BatchUpdateException& e) {
		EXPECT_EQ(e.getErrorCode(), 1213) << e.what();
		// Connector/J: the transaction was rolled back, so the count of the first (successful) command is void as well
		EXPECT_EQ(e.getUpdateCounts(), (std::vector<int32_t>{Statement::EXECUTE_FAILED}));
	}
	closeCycle.join();
	other->rollback();
	con->setAutoCommit(true);
}

TEST_F(DatabaseIntegrationTest, AuthenticationErrorIsNotMaskedByOtherAddresses) {
	ConnectionProperties wrong = properties;
	wrong.password = "definitely-not-the-password";
	auto start = steady_clock::now();
	try {
		Connection::open(wrong);
		FAIL();
	} catch (const SQLException& e) {
		EXPECT_EQ(e.getErrorCode(), 1045) << e.what(); // access denied, not the connection failure of another resolved address
		EXPECT_EQ(e.getSQLState(), "28000");
	}
	EXPECT_LT(steady_clock::now() - start, 1500ms);
}

TEST_F(DatabaseIntegrationTest, SslModes) {
	auto rs = con->prepareStatement("SELECT @@have_ssl")->executeQuery();
	ASSERT_TRUE(rs->next());
	if (rs->getString(1) != "YES")
		GTEST_SKIP() << "the server has no TLS support";
	ConnectionProperties disabled = properties;
	disabled.sslMode = SslMode::DISABLED;
	EXPECT_EQ(sslCipher(*Connection::open(disabled)), "");
	ConnectionProperties preferred = properties;
	preferred.sslMode = SslMode::PREFERRED; // Connector/J: TLS whenever the server supports it, without certificate verification
	EXPECT_NE(sslCipher(*Connection::open(preferred)), "");
	ConnectionProperties required = properties;
	required.sslMode = SslMode::REQUIRED;
	EXPECT_NE(sslCipher(*Connection::open(required)), "");
}

TEST_F(DatabaseIntegrationTest, ValidationOfUnresponsiveConnectionTimesOut) {
	FreezableTcpProxy proxy(properties.port);
	ConnectionProperties viaProxy = properties;
	viaProxy.host = "127.0.0.1";
	viaProxy.port = proxy.port();
	auto c = Connection::open(viaProxy);
	EXPECT_TRUE(c->isValid(1));
	proxy.freeze();
	auto start = steady_clock::now();
	auto valid = std::async(std::launch::async, [&] { return c->isValid(1); });
	bool finished = valid.wait_for(10s) == std::future_status::ready;
	if (!finished)
		proxy.close(); // unblock the ping
	EXPECT_TRUE(finished) << "isValid(1) did not return within 10 s";
	EXPECT_FALSE(valid.get());
	EXPECT_LT(steady_clock::now() - start, 4s);
	EXPECT_TRUE(c->isClosed());
}

TEST_F(DatabaseIntegrationTest, PoolValidationOfUnresponsiveIdleConnectionTimesOut) {
	FreezableTcpProxy proxy(properties.port);
	std::string proxyUrl = fmt::format("jdbc:mysql://127.0.0.1:{}/{}", proxy.port(), properties.database);
	DatabaseFactory::init(proxyUrl, user, password, 1, 2000);
	std::this_thread::sleep_for(700ms); // longer than the alive bypass window, so the idle connection is validated
	proxy.freeze();
	auto start = steady_clock::now();
	auto borrow = std::async(std::launch::async, [] {
		try {
			DatabaseFactory::getConnection();
			return std::string("connected");
		} catch (const SQLException& e) {
			return std::string(e.what());
		}
	});
	bool finished = borrow.wait_for(15s) == std::future_status::ready;
	if (!finished)
		proxy.close(); // unblock the validation
	EXPECT_TRUE(finished) << "getConnection did not return within 15 s";
	// validation gives up after HikariCP's validation timeout (5 s); the replacement cannot connect through the proxy and times out as well
	EXPECT_NE(borrow.get().find("Connection is not available"), std::string::npos);
	EXPECT_LT(steady_clock::now() - start, 12s);
}

TEST_F(DatabaseIntegrationTest, CallDrainsProcedureResults) {
	con->executeSimple("CREATE PROCEDURE " + table + "_proc(IN n INT) BEGIN SELECT n + 1 AS v UNION ALL SELECT n + 2; END");
	initFactory(1, 5000);
	std::vector<int32_t> values;
	for (int i = 0; i < 2; ++i) {
		values.clear();
		EXPECT_TRUE(DB::call("CALL " + table + "_proc(?)", CallReadStH{.setParams = [](PreparedStatement& st) { st.setInt(1, 10); },
			.handleRead = [&](ResultSet& rs) {
				while (rs.next())
					values.push_back(rs.getInt("v"));
			}}));
		EXPECT_EQ(values, (std::vector<int32_t>{11, 12}));
	}
	// the single pooled connection must still be in sync
	auto c = DatabaseFactory::getConnection();
	auto rs = c->prepareStatement("SELECT 42")->executeQuery();
	ASSERT_TRUE(rs->next());
	EXPECT_EQ(rs->getInt(1), 42);
	auto call = c->prepareCall("CALL " + table + "_proc(1)");
	EXPECT_TRUE(call->execute());
	EXPECT_EQ(countRows(*c), 0);
}

TEST_F(DatabaseIntegrationTest, FactoryCanBeReinitializedAfterClientLibraryShutdown) {
	con.reset(); // no live connection handles, so shutdown() ends the client library
	for (int round = 0; round < 2; ++round) {
		initFactory(1, 5000);
		{
			auto c = DatabaseFactory::getConnection();
			auto rs = c->prepareStatement("SELECT 1")->executeQuery();
			EXPECT_TRUE(rs->next());
		}
		DatabaseFactory::shutdown();
		EXPECT_FALSE(DatabaseFactory::isInitialized());
	}
}
