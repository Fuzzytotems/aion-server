#include "ScenarioDatabase.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "LoginServerTestDatabase.h"
#include "aion/commons/database/Connection.h"
#include "aion/commons/database/ConnectionProperties.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"

namespace aion::gameserver::scenario {

namespace {

std::string env(const char* name) {
	const char* value = std::getenv(name); // NOLINT(concurrency-mt-unsafe): read by the test thread before any server process starts
	return value ? value : "";
}

/** @return true if `name` is `prefix` plus exactly 8 hex digits, the shape schemaSuffix() produces */
bool isScenarioSchemaName(std::string_view name, std::string_view prefix) {
	if (!name.starts_with(prefix) || name.size() != prefix.size() + 8)
		return false;
	for (char c : name.substr(prefix.size()))
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
			return false;
	return true;
}

std::string readFile(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	if (!in)
		throw std::runtime_error("cannot read " + file.string());
	std::stringstream content;
	content << in.rdbuf();
	return content.str();
}

} // namespace

std::optional<ScenarioEnvironment> ScenarioEnvironment::fromEnvironment() {
	ScenarioEnvironment environment;
	environment.gsUrl = env("AION_TEST_GS_DATABASE_URL");
	environment.lsUrl = env("AION_TEST_LS_DATABASE_URL");
	if (environment.gsUrl.empty() || environment.lsUrl.empty())
		return std::nullopt;
	environment.gsUser = env("AION_TEST_GS_DATABASE_USER");
	if (environment.gsUser.empty())
		environment.gsUser = "root";
	environment.gsPassword = env("AION_TEST_GS_DATABASE_PASSWORD");
	environment.lsUser = env("AION_TEST_DATABASE_USER");
	if (environment.lsUser.empty())
		environment.lsUser = "root";
	environment.lsPassword = env("AION_TEST_DATABASE_PASSWORD");
	return environment;
}

JdbcUrl JdbcUrl::parse(std::string_view url) {
	if (!url.starts_with("jdbc:"))
		throw std::invalid_argument("not a JDBC URL: " + std::string(url));
	size_t hostStart = url.find("://");
	if (hostStart == std::string_view::npos || hostStart + 3 >= url.size())
		throw std::invalid_argument("no host in JDBC URL: " + std::string(url));
	hostStart += 3;
	size_t queryStart = url.find('?', hostStart);
	size_t hostEnd = queryStart == std::string_view::npos ? url.size() : queryStart;
	size_t slash = url.find('/', hostStart);
	JdbcUrl parsed;
	if (slash != std::string_view::npos && slash < hostEnd) {
		parsed.server = std::string(url.substr(0, slash));
		parsed.database = std::string(url.substr(slash + 1, hostEnd - slash - 1));
	} else {
		parsed.server = std::string(url.substr(0, hostEnd));
	}
	if (parsed.server.size() == hostStart)
		throw std::invalid_argument("no host in JDBC URL: " + std::string(url));
	parsed.query = queryStart == std::string_view::npos ? "" : std::string(url.substr(queryStart));
	return parsed;
}

std::string JdbcUrl::toString() const {
	return server + "/" + database + query;
}

std::optional<std::string> JdbcUrl::parameter(std::string_view name) const {
	if (query.size() < 2)
		return std::nullopt;
	std::string_view rest = std::string_view(query).substr(1);
	while (!rest.empty()) {
		size_t amp = rest.find('&');
		std::string_view pair = rest.substr(0, amp);
		size_t eq = pair.find('=');
		if (pair.substr(0, eq) == name)
			return eq == std::string_view::npos ? std::string() : std::string(pair.substr(eq + 1));
		if (amp == std::string_view::npos)
			break;
		rest = rest.substr(amp + 1);
	}
	return std::nullopt;
}

std::string schemaUrl(std::string_view environmentUrl, std::string_view database, std::string_view configUrl) {
	JdbcUrl url = JdbcUrl::parse(environmentUrl);
	url.database = std::string(database);
	url.query = JdbcUrl::parse(configUrl).query;
	return url.toString();
}

std::string configuredDatabaseUrl(const std::filesystem::path& javaDir) {
	std::filesystem::path file = javaDir / "config" / "network" / "database.properties";
	std::istringstream lines(readFile(file));
	std::string line;
	while (std::getline(lines, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		size_t start = line.find_first_not_of(" \t");
		if (start == std::string::npos || line[start] == '#' || line[start] == '!')
			continue;
		size_t separator = line.find_first_of("=:", start);
		if (separator == std::string::npos)
			continue;
		std::string key = line.substr(start, line.find_last_not_of(" \t", separator - 1) - start + 1);
		if (key != "database.url")
			continue;
		size_t valueStart = line.find_first_not_of(" \t", separator + 1);
		return valueStart == std::string::npos ? "" : line.substr(valueStart, line.find_last_not_of(" \t") - valueStart + 1);
	}
	throw std::runtime_error("database.url is missing in " + file.string());
}

std::string schemaSuffix(std::string_view seed) {
	uint32_t hash = 2166136261u;
	for (char c : seed) {
		hash ^= static_cast<uint8_t>(c);
		hash *= 16777619u;
	}
	static constexpr const char* HEX = "0123456789abcdef";
	std::string text(8, '0');
	for (int i = 7; i >= 0; i--) {
		text[static_cast<size_t>(i)] = HEX[hash & 0xF];
		hash >>= 4;
	}
	return text;
}

SchemaLease::SchemaLease() noexcept = default;

SchemaLease::SchemaLease(std::unique_ptr<commons::database::Connection> connectionValue, std::string lockValue)
	: connection(std::move(connectionValue)), lock(std::move(lockValue)) {
}

SchemaLease::~SchemaLease() {
	release();
}

SchemaLease::SchemaLease(SchemaLease&& other) noexcept : connection(std::move(other.connection)), lock(std::move(other.lock)) {
	other.lock.clear();
}

SchemaLease& SchemaLease::operator=(SchemaLease&& other) noexcept {
	if (this != &other) {
		release();
		connection = std::move(other.connection);
		lock = std::move(other.lock);
		other.lock.clear();
	}
	return *this;
}

void SchemaLease::release() noexcept {
	if (connection) {
		try {
			auto statement = connection->prepareStatement("SELECT RELEASE_LOCK(?)");
			statement->setString(1, lock);
			statement->executeQuery();
		} catch (const std::exception&) {
			// closing the connection below releases the lock as well; nothing here may throw out of a destructor
		}
		connection.reset();
	}
	lock.clear();
}

ScenarioDatabase::ScenarioDatabase(std::string urlValue, std::string userValue, std::string passwordValue)
	: url(JdbcUrl::parse(urlValue)), user(std::move(userValue)), password(std::move(passwordValue)) {
}

void ScenarioDatabase::checkTestName(std::string_view database) {
	if (!database.starts_with("aion_gs_test") && !database.starts_with("aion_ls_test"))
		throw std::invalid_argument("refusing to change the database " + std::string(database) + " (only aion_gs_test* and aion_ls_test*)");
}

int32_t ScenarioDatabase::recreate(std::string_view database, const std::filesystem::path& sqlFile) const {
	checkTestName(database);
	const std::string script = readFile(sqlFile);
	std::unique_ptr<commons::database::Connection> admin = open(url.database);
	auto lock = admin->prepareStatement("SELECT GET_LOCK(?, 900)");
	lock->setString(1, database);
	auto locked = lock->executeQuery();
	if (!locked->next() || locked->getInt(1) != 1)
		throw std::runtime_error("could not acquire the database lock " + std::string(database));
	admin->executeSimple("DROP DATABASE IF EXISTS `" + std::string(database) + "`");
	admin->executeSimple("CREATE DATABASE `" + std::string(database) + "` CHARACTER SET utf8mb4");
	std::unique_ptr<commons::database::Connection> schema = open(database);
	int32_t statements = 0;
	for (const std::string& statement : loginserver::test::database::splitSqlStatements(script)) {
		schema->executeSimple(statement);
		statements++;
	}
	auto unlock = admin->prepareStatement("SELECT RELEASE_LOCK(?)");
	unlock->setString(1, database);
	unlock->executeQuery();
	return statements;
}

void ScenarioDatabase::drop(std::string_view database) const {
	checkTestName(database);
	open(url.database)->executeSimple("DROP DATABASE IF EXISTS `" + std::string(database) + "`");
}

std::string ScenarioDatabase::leaseLockName(std::string_view database) {
	return std::string(database) + ":in_use";
}

SchemaLease ScenarioDatabase::lease(std::string_view database) const {
	checkTestName(database);
	std::unique_ptr<commons::database::Connection> connection = open(url.database);
	const std::string lockName = leaseLockName(database);
	auto statement = connection->prepareStatement("SELECT GET_LOCK(?, 0)"); // 0: never wait, a held marker means a run is using the schema
	statement->setString(1, lockName);
	auto result = statement->executeQuery();
	if (!result->next() || result->getObject<int32_t>(1).value_or(0) != 1)
		return {};
	return SchemaLease(std::move(connection), lockName);
}

bool ScenarioDatabase::isLeaseFree(std::string_view database) const {
	std::unique_ptr<commons::database::Connection> connection = open(url.database);
	auto statement = connection->prepareStatement("SELECT IS_FREE_LOCK(?)");
	statement->setString(1, leaseLockName(database));
	auto result = statement->executeQuery();
	return result->next() && result->getObject<int32_t>(1).value_or(0) == 1;
}

std::vector<std::string> ScenarioDatabase::dropAbandonedSchemas(std::string_view prefix, std::chrono::minutes minimumAge) const {
	std::unique_ptr<commons::database::Connection> admin = open(url.database);
	std::vector<std::string> candidates;
	{
		auto statement = admin->prepareStatement("SELECT SCHEMA_NAME FROM information_schema.SCHEMATA WHERE SCHEMA_NAME LIKE ?");
		statement->setString(1, std::string(prefix) + "%");
		auto result = statement->executeQuery();
		while (result->next()) {
			std::optional<std::string> name = result->getObject<std::string>(1);
			if (name && isScenarioSchemaName(*name, prefix)) // LIKE treats '_' as a wildcard; the shape check is what decides
				candidates.push_back(*name);
		}
	}
	std::vector<std::string> dropped;
	for (const std::string& candidate : candidates) {
		auto age = admin->prepareStatement(
			"SELECT IFNULL(MIN(TIMESTAMPDIFF(MINUTE, CREATE_TIME, NOW())), ?) FROM information_schema.TABLES WHERE TABLE_SCHEMA = ?");
		age->setLong(1, minimumAge.count()); // no tables at all: the leftover of a run that died inside recreate(), old enough by definition
		age->setString(2, candidate);
		auto ageResult = age->executeQuery();
		if (!ageResult->next())
			continue;
		const std::optional<int64_t> minutes = ageResult->getObject<int64_t>(1);
		if (!minutes || *minutes < minimumAge.count())
			continue;
		if (!isLeaseFree(candidate)) // a run is using it right now: never touch it, whatever its age says
			continue;
		drop(candidate);
		dropped.push_back(candidate);
	}
	return dropped;
}

std::unique_ptr<commons::database::Connection> ScenarioDatabase::open(std::string_view database) const {
	JdbcUrl target = url;
	target.database = std::string(database);
	return commons::database::Connection::open(commons::database::ConnectionProperties::parse(target.toString(), user, password));
}

void ScenarioDatabase::execute(std::string_view database, std::string_view sql) const {
	open(database)->executeSimple(sql);
}

std::optional<std::string> ScenarioDatabase::queryString(std::string_view database, std::string_view sql) const {
	std::unique_ptr<commons::database::Connection> con = open(database);
	auto rs = con->prepareStatement(sql)->executeQuery();
	if (!rs->next())
		return std::nullopt;
	return rs->getObject<std::string>(1);
}

std::optional<int64_t> ScenarioDatabase::queryLong(std::string_view database, std::string_view sql) const {
	std::unique_ptr<commons::database::Connection> con = open(database);
	auto rs = con->prepareStatement(sql)->executeQuery();
	if (!rs->next())
		return std::nullopt;
	return rs->getObject<int64_t>(1);
}

std::vector<std::vector<std::optional<std::string>>> ScenarioDatabase::queryRows(std::string_view database, std::string_view sql, int32_t columns) const {
	std::unique_ptr<commons::database::Connection> con = open(database);
	auto rs = con->prepareStatement(sql)->executeQuery();
	std::vector<std::vector<std::optional<std::string>>> rows;
	while (rs->next()) {
		std::vector<std::optional<std::string>> row;
		for (int32_t column = 1; column <= columns; column++)
			row.push_back(rs->getObject<std::string>(column));
		rows.push_back(std::move(row));
	}
	return rows;
}

bool ScenarioDatabase::isReachable() const {
	try {
		open(url.database);
		return true;
	} catch (const std::exception&) {
		return false;
	}
}

bool ScenarioDatabase::isInvalidObjectId(int32_t id) noexcept {
	// IDFactory.java:46-47 INVALID_ID_BIT_MASK and INVALID_ID_BITCHECK (6484), compared as in isInvalidId (:152-154)
	constexpr int32_t INVALID_ID_BIT_MASK = 0b0010011111100110101111111111100;
	constexpr int32_t INVALID_ID_BITCHECK = 0b0000000000000000001100101010100;
	return (id & INVALID_ID_BIT_MASK) == INVALID_ID_BITCHECK;
}

int32_t ScenarioDatabase::seedInventoryItem(std::string_view database, const InventorySeed& seed) const {
	const int64_t highest =
	  queryLong(database, "SELECT COALESCE(MAX(item_unique_id), 0) FROM inventory WHERE item_unique_id >= " + std::to_string(SEEDED_OBJECT_ID_BASE))
		.value_or(0);
	int64_t id = highest >= SEEDED_OBJECT_ID_BASE ? highest + 1 : SEEDED_OBJECT_ID_BASE;
	while (id < SEEDED_OBJECT_ID_END && isInvalidObjectId(static_cast<int32_t>(id)))
		id++;
	if (id >= SEEDED_OBJECT_ID_END)
		throw std::runtime_error("the seeded object id range [" + std::to_string(SEEDED_OBJECT_ID_BASE) + ", " + std::to_string(SEEDED_OBJECT_ID_END) +
								 ") of " + std::string(database) + ".inventory is used up");
	execute(database, "INSERT INTO inventory (item_unique_id, item_id, item_count, item_owner, slot, item_location) VALUES (" + std::to_string(id) +
						  ", " + std::to_string(seed.itemId) + ", " + std::to_string(seed.count) + ", " + std::to_string(seed.ownerId) + ", " +
						  std::to_string(seed.slot) + ", " + std::to_string(seed.location) + ")");
	return static_cast<int32_t>(id);
}

void ScenarioDatabase::setLifeStatHp(std::string_view database, int32_t playerId, int32_t hp) const {
	const std::string player = std::to_string(playerId);
	if (queryLong(database, "SELECT COUNT(*) FROM player_life_stats WHERE player_id = " + player).value_or(0) != 1)
		throw std::runtime_error("player " + player + " has no player_life_stats row in " + std::string(database));
	execute(database, "UPDATE player_life_stats SET hp = " + std::to_string(hp) + " WHERE player_id = " + player);
}

} // namespace aion::gameserver::scenario
