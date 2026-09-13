#pragma once

#include <cstdint>
#include <string>

namespace aion::commons::configuration {
class ConfigurableProcessor;
}

namespace aion::commons::configs {

/**
 * This class holds all configuration of database.
 * <p>
 * The fields are plain values: the game server rebinds them at runtime (Config.load()), but they are only read when the connection pool is
 * created at startup (DatabaseFactory::init), so no other thread reads them during a reload (see ConfigurableProcessor, "Threads and
 * reloading").
 *
 * @author SoulKeeper
 */
struct DatabaseConfig {
	/** database.url, e.g. jdbc:mysql://localhost:3306/aion_ls?serverTimezone=&characterEncoding=UTF-8 */
	static inline std::string DATABASE_URL;

	/** database.user */
	static inline std::string DATABASE_USER;

	/** database.password */
	static inline std::string DATABASE_PASSWORD;

	/** Maximum amount of connections kept in connection pool (database.connectionpool.connections.max, default 5) */
	static inline int32_t DATABASE_CONNECTIONS_MAX = 0;

	/** Maximum wait time in milliseconds when getting a DB connection, before throwing a timeout error (database.connectionpool.timeout, default 5000)
	 */
	static inline int32_t DATABASE_TIMEOUT = 0;

	/** Binds the fields above to their property keys (Java: the @Property annotations). Implemented in the configuration library. */
	static void bind(configuration::ConfigurableProcessor& processor);
};

} // namespace aion::commons::configs
