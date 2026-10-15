#include "aion/commons/database/DatabaseFactory.h"

#include <algorithm>
#include <limits>
#include <mutex>
#include <string>

#include "aion/commons/configs/DatabaseConfig.h"
#include "aion/commons/database/MariaDbLibrary.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"

namespace aion::commons::database {

namespace {

const auto log = logging::LoggerFactory::getLogger("com.aionemu.commons.database.DatabaseFactory");
const auto dataSourceLog = logging::LoggerFactory::getLogger("com.zaxxer.hikari.HikariDataSource");
const auto poolLog = logging::LoggerFactory::getLogger("com.zaxxer.hikari.pool.PoolBase");

/** HikariCP: SOFT_TIMEOUT_FLOOR */
constexpr int32_t MIN_CONNECTION_TIMEOUT_MILLIS = 250;
/** HikariCP: VALIDATION_TIMEOUT (5000 ms) in seconds */
constexpr int32_t VALIDATION_TIMEOUT_SECONDS = 5;

std::mutex factoryMutex;
std::shared_ptr<ConnectionPool<Connection>> dataSource;
std::chrono::milliseconds dataSourceSocketTimeout{0}; // guarded by factoryMutex, valid while dataSource is set

/** Applies Options to parsed connection properties (see DatabaseFactory::Options). */
void applySocketTimeout(ConnectionProperties& properties, const DatabaseFactory::Options& options) {
	std::chrono::milliseconds timeout = options.defaultSocketTimeout;
	if (options.socketTimeout)
		timeout = *options.socketTimeout;
	else if (properties.getParameter("socketTimeout"))
		timeout = properties.socketTimeout;
	if (timeout.count() < 0)
		throw utils::IllegalArgumentException("database.socket_timeout cannot be negative: " + std::to_string(timeout.count()));
	if (timeout.count() == 0 && options.requireSocketTimeout)
		throw utils::IllegalArgumentException(
			"This server requires a database socket timeout greater than 0 (set database.socket_timeout in milliseconds, or socketTimeout in database.url)");
	properties.socketTimeout = timeout;
}

/** Prepares a connection returned to the pool for the next user (HikariCP: ProxyConnection.close + PoolBase.resetConnectionState). */
bool resetConnection(Connection& connection) {
	connection.closeOpenStatements();
	if (connection.isClosed())
		return false;
	if (!connection.getAutoCommit()) {
		if (connection.isInTransaction()) {
			connection.rollback();
			poolLog.debug("Executed rollback on connection due to dirty commit state on close().");
		}
		connection.setAutoCommit(true);
	}
	return true;
}

} // namespace

void DatabaseFactory::init() {
	using configs::DatabaseConfig;
	init(DatabaseConfig::DATABASE_URL, DatabaseConfig::DATABASE_USER, DatabaseConfig::DATABASE_PASSWORD, DatabaseConfig::DATABASE_CONNECTIONS_MAX,
		DatabaseConfig::DATABASE_TIMEOUT);
}

void DatabaseFactory::init(Options options) {
	using configs::DatabaseConfig;
	if (!options.socketTimeout && DatabaseConfig::DATABASE_SOCKET_TIMEOUT)
		options.socketTimeout = std::chrono::milliseconds(*DatabaseConfig::DATABASE_SOCKET_TIMEOUT);
	init(DatabaseConfig::DATABASE_URL, DatabaseConfig::DATABASE_USER, DatabaseConfig::DATABASE_PASSWORD, DatabaseConfig::DATABASE_CONNECTIONS_MAX,
		DatabaseConfig::DATABASE_TIMEOUT, options);
}

void DatabaseFactory::init(std::string_view url, std::string_view user, std::string_view password, int32_t maxConnections, int32_t timeoutMillis) {
	init(url, user, password, maxConnections, timeoutMillis, Options{});
}

std::chrono::milliseconds DatabaseFactory::resolveSocketTimeout(std::string_view url, const Options& options) {
	ConnectionProperties properties = ConnectionProperties::parse(url);
	applySocketTimeout(properties, options);
	return properties.socketTimeout;
}

std::optional<std::chrono::milliseconds> DatabaseFactory::getSocketTimeout() {
	std::scoped_lock lock(factoryMutex);
	if (!dataSource)
		return std::nullopt;
	return dataSourceSocketTimeout;
}

void DatabaseFactory::init(std::string_view url, std::string_view user, std::string_view password, int32_t maxConnections, int32_t timeoutMillis,
	const Options& options) {
	std::scoped_lock lock(factoryMutex);
	if (dataSource)
		return;

	// validation like HikariConfig.setMaximumPoolSize / setConnectionTimeout
	if (maxConnections < 1)
		throw utils::IllegalArgumentException("maxPoolSize cannot be less than 1");
	if (timeoutMillis != 0 && timeoutMillis < MIN_CONNECTION_TIMEOUT_MILLIS)
		throw utils::IllegalArgumentException("connectionTimeout cannot be less than 250ms");

	ConnectionProperties properties = ConnectionProperties::parse(url, user, password);
	applySocketTimeout(properties, options);
	// HikariCP sets the login timeout to max(1, (500 + connectionTimeout) / 1000) seconds, which Connector/J uses if connectTimeout is not set
	if (properties.connectTimeout.count() == 0) {
		int64_t loginTimeoutSeconds = std::max<int64_t>(1, (500 + static_cast<int64_t>(timeoutMillis == 0 ? std::numeric_limits<int32_t>::max() : timeoutMillis)) / 1000);
		properties.connectTimeout = std::chrono::seconds(loginTimeoutSeconds);
	}

	ConnectionPool<Connection>::Config config;
	config.maximumPoolSize = maxConnections;
	config.connectionTimeout = std::chrono::milliseconds(timeoutMillis == 0 ? std::numeric_limits<int32_t>::max() : timeoutMillis);

	ConnectionPool<Connection>::Callbacks callbacks;
	callbacks.create = [properties] { return Connection::open(properties); };
	// HikariCP PoolBase.isConnectionDead: isValid(max(1000, validationTimeout) / 1000) with a network timeout of validationTimeout (default 5 s)
	callbacks.isValid = [](Connection& connection) { return connection.isValid(VALIDATION_TIMEOUT_SECONDS); };
	callbacks.reset = resetConnection;

	dataSourceLog.info("{} - Starting...", config.poolName);
	std::string poolName = config.poolName;
	auto pool = ConnectionPool<Connection>::create(std::move(config), std::move(callbacks));
	// HikariCP initializationFailTimeout=1 (default): fail fast if the database is not reachable
	try {
		pool->addIdleConnection(Connection::open(properties));
	} catch (const SQLException& e) {
		pool->shutdown();
		throw SQLException("Failed to initialize pool: " + std::string(e.what()), e.getSQLState(), e.getErrorCode(), std::current_exception());
	}
	dataSource = std::move(pool);
	dataSourceSocketTimeout = properties.socketTimeout;
	dataSourceLog.info("{} - Start completed.", poolName);
}

PooledConnection DatabaseFactory::getConnection() {
	std::shared_ptr<ConnectionPool<Connection>> pool = getPool();
	if (!pool) // Deviation: Java throws a NullPointerException
		throw SQLException("DatabaseFactory is not initialized", "08003");
	PooledConnection con = pool->getConnection();
	if (!con->getAutoCommit()) {
		log.error("Connection was not in auto-commit mode.", utils::IllegalStateException(""));
		con->setAutoCommit(true);
	}
	return con;
}

void DatabaseFactory::shutdown() {
	std::shared_ptr<ConnectionPool<Connection>> pool;
	{
		std::scoped_lock lock(factoryMutex);
		pool = std::move(dataSource);
		dataSource.reset();
	}
	if (!pool)
		return;
	const std::string& poolName = pool->getConfig().poolName;
	dataSourceLog.info("{} - Shutdown initiated...", poolName);
	pool->shutdown();
	dataSourceLog.info("{} - Shutdown completed.", poolName);
	if (!MariaDbLibrary::shutdownIfUnused())
		log.debug("Client library stays initialized: {} connection(s) still borrowed", pool->getActiveConnections());
}

bool DatabaseFactory::isInitialized() {
	std::scoped_lock lock(factoryMutex);
	return dataSource != nullptr;
}

std::shared_ptr<ConnectionPool<Connection>> DatabaseFactory::getPool() {
	std::scoped_lock lock(factoryMutex);
	return dataSource;
}

} // namespace aion::commons::database
