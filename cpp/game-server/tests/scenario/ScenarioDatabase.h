#pragma once

// Database helpers of the M5a scenario harness (m5a-plan.md F-04, §5.1): the test database settings of the environment, the JDBC URLs the
// server processes get (the server of the environment URL, a test schema and the query of the server's own config/network/database.properties,
// so serverTimezone and characterEncoding stay as configured), schema creation from the Java SQL scripts and direct queries for the DB
// assertions (commons::database::Connection::open, never the DatabaseFactory: the servers run as child processes, plan D4).
//
// Scope of "independent": the queries do not go through the game server's DAOs, but they DO go through the same C++ MariaDB connector the DAOs
// used to write the rows, so a layer-wide encoding or type defect in that connector would cancel out between the writer and this reader. The
// risk is small because the helpers read every column as a raw string, and the rows the gate compares are ids, numbers and positions; it is
// recorded here because §5 calls these assertions "direct MariaDB queries", which is true of the SQL and not of the client.

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aion::commons::database {
class Connection;
} // namespace aion::commons::database

namespace aion::gameserver::scenario {

/** The test database settings (m5a-plan.md §5.1 "Environment") */
struct ScenarioEnvironment {
	/** AION_TEST_GS_DATABASE_URL, AION_TEST_GS_DATABASE_USER (default root), AION_TEST_GS_DATABASE_PASSWORD */
	std::string gsUrl;
	std::string gsUser;
	std::string gsPassword;
	/** AION_TEST_LS_DATABASE_URL, AION_TEST_DATABASE_USER (default root), AION_TEST_DATABASE_PASSWORD */
	std::string lsUrl;
	std::string lsUser;
	std::string lsPassword;

	/** @return the settings, std::nullopt if AION_TEST_GS_DATABASE_URL or AION_TEST_LS_DATABASE_URL is not set (the scenario is skipped) */
	static std::optional<ScenarioEnvironment> fromEnvironment();
};

/** A JDBC URL split into its server part ("jdbc:mysql://host:port"), the database and the query ("?a=b&c=d" or empty) */
struct JdbcUrl {
	std::string server;
	std::string database;
	std::string query;

	/** @throws std::invalid_argument if the text is no jdbc:<subprotocol>://<host>[/<database>][?<query>] URL */
	static JdbcUrl parse(std::string_view url);

	std::string toString() const;

	/** @return the value of the query parameter as written (placeholders like ${gameserver.timezone} included), std::nullopt if absent */
	std::optional<std::string> parameter(std::string_view name) const;
};

/** The server of environmentUrl, the database and the query of configUrl (§5.1 "URLs") */
std::string schemaUrl(std::string_view environmentUrl, std::string_view database, std::string_view configUrl);

/** database.url of <javaDir>/config/network/database.properties. @throws std::runtime_error if the file or key is missing */
std::string configuredDatabaseUrl(const std::filesystem::path& javaDir);

/** A stable 8 hex digit suffix for schema names (FNV-1a of the seed, e.g. the output directory), so parallel build trees use their own schemas */
std::string schemaSuffix(std::string_view seed);

/**
 * The "this schema belongs to a running scenario" marker: a MariaDB session lock (GET_LOCK) held by its own connection for as long as this
 * object lives. `ScenarioDatabase::dropAbandonedSchemas` drops a scenario schema only while nothing holds its marker.
 * <p>
 * The point is what happens when nothing runs: MariaDB releases a session lock when the session ends, so the marker disappears by itself when
 * the test process is killed without running a single destructor - a CTest TIMEOUT or a crash of the gate (m5a-plan.md §5.1 "Run model"). That
 * is exactly the case in which the run cannot drop its own schemas, and the next run is then free to drop them.
 */
class SchemaLease {
public:
	// every special member is defined in the .cpp: std::unique_ptr<Connection> needs the complete type to destroy one, and it is forward
	// declared here so that the harness headers stay free of the database headers
	SchemaLease() noexcept;
	SchemaLease(std::unique_ptr<commons::database::Connection> connection, std::string lockName);
	~SchemaLease();

	SchemaLease(SchemaLease&& other) noexcept;
	SchemaLease& operator=(SchemaLease&& other) noexcept;

	/** @return true if this object holds the marker (false: another process holds it, or nothing was taken) */
	bool held() const noexcept {
		return connection != nullptr;
	}

	/** the lock name, "" if this object holds nothing */
	const std::string& name() const noexcept {
		return lock;
	}

	/** Releases the marker; does nothing if this object holds none */
	void release() noexcept;

private:
	std::unique_ptr<commons::database::Connection> connection;
	std::string lock;
};

/** Direct connections to one MariaDB server for schema setup and assertions */
class ScenarioDatabase {
public:
	/** @param url a JDBC URL of the server (its database is only used for the admin connection) */
	ScenarioDatabase(std::string url, std::string user, std::string password);

	/**
	 * Drops and creates `database` (utf8mb4) and executes the statements of the SQL script. Only databases named aion_gs_test* or aion_ls_test*
	 * are accepted. Takes the MariaDB lock named like the database while it works.
	 *
	 * @return the number of statements executed
	 */
	int32_t recreate(std::string_view database, const std::filesystem::path& sqlFile) const;

	void drop(std::string_view database) const;

	/** the name of the in-use marker of a schema: "<database>:in_use", never the name recreate() locks (a caller holds both at once) */
	static std::string leaseLockName(std::string_view database);

	/**
	 * Takes the in-use marker of `database` (SchemaLease). Never waits.
	 *
	 * @return a lease that holds nothing if another connection holds the marker, i.e. if a scenario run is using that schema right now
	 */
	SchemaLease lease(std::string_view database) const;

	/** @return true if no connection holds the in-use marker of `database` */
	bool isLeaseFree(std::string_view database) const;

	/**
	 * Drops the schemas of scenario runs that were killed before they could drop their own (m5a-plan.md §5.1: a CTest TIMEOUT or a crash runs
	 * no destructor, so the ChildProcess job object reaps the servers but nothing drops the schemas). A schema is dropped when all three hold:
	 * <ul>
	 * <li>its name is `prefix` plus exactly 8 hex digits (the schemaSuffix() shape), so no hand-made or other test's schema is touched;</li>
	 * <li>no SchemaLease holds it, so a run that is using it right now is safe;</li>
	 * <li>its youngest table is older than `minimumAge` (a schema without tables counts as old).</li>
	 * </ul>
	 * The age is the second guard, for a run of an older binary that takes no lease at all: a gate run cannot outlive CTest's TIMEOUT 900, so
	 * anything that has not been touched for an hour is not a live run whatever its lease says.
	 *
	 * @return the names that were dropped
	 */
	std::vector<std::string> dropAbandonedSchemas(std::string_view prefix, std::chrono::minutes minimumAge) const;

	/** a connection to `database` on the same server, with the query of the URL */
	std::unique_ptr<commons::database::Connection> open(std::string_view database) const;

	void execute(std::string_view database, std::string_view sql) const;

	/** @return the first column of the first row, std::nullopt for NULL or no row */
	std::optional<std::string> queryString(std::string_view database, std::string_view sql) const;

	/** @return the first column of the first row as a number, std::nullopt for NULL or no row */
	std::optional<int64_t> queryLong(std::string_view database, std::string_view sql) const;

	/** @return every row with `columns` columns as strings (NULL: std::nullopt) */
	std::vector<std::vector<std::optional<std::string>>> queryRows(std::string_view database, std::string_view sql, int32_t columns) const;

	/** One `inventory` row a gate seeds while its character is logged out (seedInventoryItem); the other columns keep their SQL defaults */
	struct InventorySeed {
		int32_t ownerId = 0;
		int32_t itemId = 0;
		int64_t count = 1;
		/** item_location: StorageType.getId() - 0 the cube, 1 the regular warehouse (StorageType.java:7-8) */
		int32_t location = 0;
		/** slot: ItemStorage.FIRST_AVAILABLE_SLOT (ItemStorage.java:16), the slot every new Item carries (Item.java:45) */
		int64_t slot = 65535;
	};

	/**
	 * The first object id seedInventoryItem hands out, and the end of its range: gameserver.idfactory.wrap_at's default 2^27 (IDFactory.h).
	 * <p>
	 * Why this range is safe to write into while the game server runs (m5b3-plan.md §12 named it an unchecked inference): the server locks the
	 * ids InventoryDAO and the other DAOs report at startup and then allocates upward from a monotone cursor that wraps only at wrap_at
	 * (IDFactory.h: "Monotone cursor", "DAO seeding"). A startup with the whole world takes about 100,000 ids (82,000 npcs, m5b-plan.md Q3) and
	 * a gate run a few thousand more, so no id at or above 0x07000000 (117,440,512) is allocated in a run. A seeded row inserted after the startup
	 * is not locked in IDFactory, and nothing asks IDFactory about an item's id later: an Item is created without autoReleaseObjectId, so its id
	 * is never released (AionObject.java:27-35), not even when the item is consumed.
	 */
	static constexpr int32_t SEEDED_OBJECT_ID_BASE = 0x07000000;
	static constexpr int32_t SEEDED_OBJECT_ID_END = 1 << 27;

	/**
	 * Java IDFactory.isInvalidId (IDFactory.java:46-47, 152-154): the bit pattern IDFactory never allocates, so a seeded id avoids it too
	 */
	static bool isInvalidObjectId(int32_t id) noexcept;

	/**
	 * m5b3-plan.md G-02 and §10.1 "Character": inserts `seed` into `database`.inventory with the next object id of the seeded range - above
	 * every inventory row at or above SEEDED_OBJECT_ID_BASE, skipping isInvalidObjectId - which the character's next enter world loads
	 * (InventoryDAO.loadStorage). Call it while the owner is logged out: a logged-in character's storage is not reloaded, and its shutdown store
	 * would not know the row.
	 *
	 * @return the object id of the row
	 * @throws std::runtime_error when the seeded range is used up
	 */
	int32_t seedInventoryItem(std::string_view database, const InventorySeed& seed) const;

	/**
	 * Sets `player_life_stats.hp` of `playerId`, which the next enter world restores (the M5b gate's D12 death and the M5b-3 potion case, both
	 * with the character logged out). @throws std::runtime_error if the player has no player_life_stats row
	 */
	void setLifeStatHp(std::string_view database, int32_t playerId, int32_t hp) const;

	/** @return true if the server accepts a connection */
	bool isReachable() const;

private:
	static void checkTestName(std::string_view database);

	JdbcUrl url;
	std::string user;
	std::string password;
};

} // namespace aion::gameserver::scenario
