#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "aion/commons/database/Connection.h"
#include "aion/commons/database/ConnectionPool.h"

namespace aion::commons::database {

/**
 * A connection borrowed from the DatabaseFactory pool. Use it like a pointer to Connection (<code>con-&gt;prepareStatement(...)</code>); the
 * connection returns to the pool when the handle is destroyed or close() is called (Java: try-with-resources on the Connection).
 */
using PooledConnection = ConnectionPool<Connection>::Handle;

/**
 * This file is used for creating a pool of connections for the server.<br>
 * It utilizes database.properties and creates a pool of connections and automatically recycles them when closed.
 * <p>
 * C++ port: the HikariCP data source is replaced by ConnectionPool&lt;Connection&gt;, configured from DatabaseConfig: at most
 * DATABASE_CONNECTIONS_MAX connections (created on demand), DATABASE_TIMEOUT milliseconds to wait for a free connection (0: practically
 * unlimited, like HikariCP). Like HikariCP's default initializationFailTimeout, init() opens one connection right away and throws if that fails.
 * Returned connections get their open statements closed, an uncommitted transaction rolled back and auto-commit restored.
 * <p>
 * Thread safe.
 *
 * @author Disturbing, SoulKeeper
 */
class DatabaseFactory {
public:
	DatabaseFactory() = delete;

	/**
	 * Java: init() - creates the pool from DatabaseConfig (DATABASE_URL, DATABASE_USER, DATABASE_PASSWORD, DATABASE_CONNECTIONS_MAX,
	 * DATABASE_TIMEOUT). Does nothing if the pool already exists.
	 * @throws SQLException if the URL is invalid ("Cannot handle the connection string ...") or no connection could be opened
	 * ("Failed to initialize pool: ...", with the connection error as cause)
	 * @throws utils::IllegalArgumentException for invalid pool settings (HikariCP messages)
	 */
	static void init();

	/** C++ addition: init() with explicit settings instead of DatabaseConfig (tests, tools). */
	static void init(std::string_view url, std::string_view user, std::string_view password, int32_t maxConnections, int32_t timeoutMillis);

	/**
	 * Java: getConnection() - an active connection from the pool, in auto-commit mode.
	 * @throws SQLTransientConnectionException if no connection became available within DATABASE_TIMEOUT
	 * @throws SQLException if the factory is not initialized or was shut down
	 */
	static PooledConnection getConnection();

	/**
	 * C++ addition: closes the pool (idle connections immediately, borrowed ones when they are returned) and frees the client library if no
	 * connection is left. init() may be called again afterwards.
	 */
	static void shutdown();

	static bool isInitialized();

	/** C++ addition: the pool, e.g. for statistics; nullptr if not initialized */
	static std::shared_ptr<ConnectionPool<Connection>> getPool();
};

} // namespace aion::commons::database
