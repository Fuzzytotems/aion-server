#pragma once

// Shared fixture of the P5-11 legion and housing tests (m5a-plan.md E2-04, E2-05); a copy of tests/economy/EconomyTestSupport.h (P5-09), since a
// chunk's tests may only include its own test directories.
//
// Database: a fresh database `aion_gs_test_legionhouse` on the server named by AION_TEST_GS_DATABASE_URL (the database of the URL is only used for
// the lock connection), created from game-server/sql/aion_gs.sql of the Java tree, like the DAO tests (tests/dao/DaoTestDatabase.h, whose rules this
// copy follows). ctest runs every test case in its own process, so the singletons a test constructs (HousingService, LegionService, ...)
// see exactly the rows and the static data that test prepared; the tests must not be combined into one process (`--gtest_filter` one test).
//
// Test doubles, each standing in for a body of a later chunk:
// - the stat containers (P5-01): TestPlayer runs the real Player::postConstruct, which may stop at the unported PlayerGameStats constructor, and then
//   installs game and life stats doubles (like tests/dao/DaoTestSupport.h).

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/database/Connection.h"
#include "aion/commons/database/ConnectionProperties.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/dataholders/loadingutils/HolderRef.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::legionhouse::test {

inline constexpr std::string_view TEST_DATABASE = "aion_gs_test_legionhouse";

inline std::string env(const char* name) {
	const char* value = std::getenv(name); // NOLINT(concurrency-mt-unsafe): read by the test process before threads start
	return value ? value : "";
}

/** @return true if the database tests are enabled (AION_TEST_GS_DATABASE_URL is set) */
inline bool isDatabaseEnabled() {
	return !env("AION_TEST_GS_DATABASE_URL").empty();
}

inline std::string user() {
	std::string value = env("AION_TEST_GS_DATABASE_USER");
	return value.empty() ? "root" : value;
}

inline std::string password() {
	return env("AION_TEST_GS_DATABASE_PASSWORD");
}

/** @return the JDBC URL of the environment with its database replaced by `database` (the query string is kept) */
inline std::string urlWithDatabase(std::string_view database) {
	std::string url = env("AION_TEST_GS_DATABASE_URL");
	const size_t hostStart = url.find("//");
	const size_t query = url.find('?', hostStart == std::string::npos ? 0 : hostStart + 2);
	const size_t pathStart = url.find('/', hostStart == std::string::npos ? 0 : hostStart + 2);
	const size_t hostEnd = query == std::string::npos ? url.size() : query;
	std::string base = url.substr(0, pathStart != std::string::npos && pathStart < hostEnd ? pathStart : hostEnd);
	std::string suffix = query == std::string::npos ? "" : url.substr(query);
	return base + "/" + std::string(database) + suffix;
}

/** Splits an SQL script into statements (quotes, "-- " / "#" / block comments; the rules of tests/dao/DaoTestDatabase.h) */
inline std::vector<std::string> splitSqlStatements(std::string_view script) {
	std::vector<std::string> statements;
	std::string current;
	auto flush = [&] {
		size_t begin = current.find_first_not_of(" \t\r\n");
		if (begin != std::string::npos) {
			size_t end = current.find_last_not_of(" \t\r\n");
			statements.push_back(current.substr(begin, end - begin + 1));
		}
		current.clear();
	};
	for (size_t i = 0; i < script.size(); i++) {
		char c = script[i];
		if (c == '\'' || c == '"' || c == '`') {
			size_t end = i + 1;
			while (end < script.size() && script[end] != c) {
				if (script[end] == '\\' && c != '`')
					end++;
				end++;
			}
			end = std::min(end, script.size() - 1);
			current.append(script.substr(i, end - i + 1));
			i = end;
		} else if (c == '-' && i + 1 < script.size() && script[i + 1] == '-' &&
			(i + 2 == script.size() || script[i + 2] == ' ' || script[i + 2] == '\t' || script[i + 2] == '\r' || script[i + 2] == '\n')) {
			while (i < script.size() && script[i] != '\n')
				i++;
			current += '\n';
		} else if (c == '#') {
			while (i < script.size() && script[i] != '\n')
				i++;
			current += '\n';
		} else if (c == '/' && i + 1 < script.size() && script[i + 1] == '*') {
			size_t end = script.find("*/", i + 2);
			i = end == std::string_view::npos ? script.size() : end + 1;
			current += ' ';
		} else if (c == ';') {
			flush();
		} else {
			current += c;
		}
	}
	flush();
	return statements;
}

/** Takes the process lock, recreates the test database from aion_gs.sql and initializes DatabaseFactory with it. Runs once per process. */
inline void setUpDatabaseOnce() {
	static std::once_flag once;
	std::call_once(once, [] {
		using commons::database::Connection;
		using commons::database::ConnectionProperties;
		// the lock connection stays open until the process exits: the server releases the named lock when it closes
		static Connection* lockConnection =
			Connection::open(ConnectionProperties::parse(env("AION_TEST_GS_DATABASE_URL"), user(), password())).release();
		auto lock = lockConnection->prepareStatement("SELECT GET_LOCK(?, 900)");
		lock->setString(1, std::string(TEST_DATABASE));
		auto locked = lock->executeQuery();
		if (!locked->next() || locked->getInt(1) != 1)
			throw commons::utils::IllegalStateException("Could not acquire the database lock " + std::string(TEST_DATABASE));
		lockConnection->executeSimple("DROP DATABASE IF EXISTS `" + std::string(TEST_DATABASE) + "`");
		lockConnection->executeSimple("CREATE DATABASE `" + std::string(TEST_DATABASE) + "` CHARACTER SET utf8mb4");

		std::unique_ptr<Connection> schema = Connection::open(ConnectionProperties::parse(urlWithDatabase(TEST_DATABASE), user(), password()));
		std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "sql" / "aion_gs.sql", std::ios::binary);
		if (!in)
			throw commons::utils::IOException("Cannot read aion_gs.sql");
		std::stringstream script;
		script << in.rdbuf();
		for (const std::string& statement : splitSqlStatements(script.str()))
			schema->executeSimple(statement);
		commons::database::DatabaseFactory::init(urlWithDatabase(TEST_DATABASE), user(), password(), 10, 5000);
	});
}

/** Executes a statement without parameters on a pooled connection */
inline void execute(std::string_view sql) {
	auto con = commons::database::DatabaseFactory::getConnection();
	con->executeSimple(sql);
}

/** @return the first column of the first row of a query as a number, std::nullopt for NULL or no row */
inline std::optional<int64_t> queryLong(std::string_view sql) {
	auto con = commons::database::DatabaseFactory::getConnection();
	auto rs = con->prepareStatement(sql)->executeQuery();
	if (!rs->next())
		return std::nullopt;
	return rs->getObject<int64_t>(1);
}

/** Inserts a minimal players row (the foreign key target of the broker and mail tables) */
inline void insertPlayer(int32_t id, std::string_view name, int32_t accountId, std::string_view race = "ELYOS") {
	auto con = commons::database::DatabaseFactory::getConnection();
	auto st = con->prepareStatement("INSERT INTO players (id, name, account_id, account_name, x, y, z, heading, world_id, gender, race, player_class, exp) "
									"VALUES (?, ?, ?, ?, 1, 2, 3, 4, 210010000, 'MALE', ?, 'WARRIOR', 0)");
	st->setInt(1, id);
	st->setString(2, name);
	st->setInt(3, accountId);
	st->setString(4, "account" + std::to_string(accountId));
	st->setString(5, race);
	st->execute();
}

/** Binds a static data class from XML text */
template <class T>
std::unique_ptr<T> bindXml(std::string_view text) {
	xml::LoadContext context;
	return xml::bindString<T>(context, text);
}

/** Publishes a holder into DataManager for one test and forgets it again, also when an assertion ends the test early */
template <class H>
class PublishedHolder {
public:
	PublishedHolder(xml::HolderRef<H>& holderRef, std::unique_ptr<H> holder) : ref(holderRef) { ref.publish(std::move(holder)); }
	~PublishedHolder() { ref.resetForTests(); }
	PublishedHolder(const PublishedHolder&) = delete;
	PublishedHolder& operator=(const PublishedHolder&) = delete;

private:
	xml::HolderRef<H>& ref;
};

/** Sets GSConfig.TIME_ZONE_ID for the scope (ServerTime reads it) */
class TimeZoneScope {
public:
	explicit TimeZoneScope(const char* zoneName) : previous(configs::main::GSConfig::TIME_ZONE_ID.load()) {
		configs::main::GSConfig::TIME_ZONE_ID.store(std::chrono::locate_zone(zoneName));
	}
	~TimeZoneScope() { configs::main::GSConfig::TIME_ZONE_ID.store(previous); }
	TimeZoneScope(const TimeZoneScope&) = delete;
	TimeZoneScope& operator=(const TimeZoneScope&) = delete;

private:
	const std::chrono::time_zone* const previous;
};

/** CreatureGameStats double (the stat calculation belongs to P5-01) */
class TestGameStats final : public model::stats::container::CreatureGameStats {
public:
	explicit TestGameStats(model::gameobjects::Creature& owner) : CreatureGameStats(owner) {}
	const model::templates::stats::StatsTemplate* getStatsTemplate() override { return nullptr; }
	int32_t getBaseAttackSpeed() override { return 0; }
	std::unique_ptr<model::stats::calc::Stat2> getMovementSpeed() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getAttackRange() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getHpRegenRate() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getMpRegenRate() override { return nullptr; }
};

/** CreatureLifeStats double */
class TestLifeStats final : public model::stats::container::CreatureLifeStats {
public:
	explicit TestLifeStats(model::gameobjects::Creature& owner) : CreatureLifeStats(owner, 1000, 500) {}
};

/** The real Player with the stat containers of the doubles above */
class TestPlayer final : public model::gameobjects::player::Player {
	AION_MAKE_REF_FRIEND
public:
	TestPlayer(CreateKey key, model::account::PlayerAccountData& playerAccountData, model::account::Account& account)
		: Player(key, playerAccountData, account) {}

protected:
	~TestPlayer() override = default;

	void postConstruct() override {
		try {
			Player::postConstruct();
		} catch (const runtime::UnportedException&) {
			// PlayerGameStats(Player&) is P5-01: everything before it ran
		}
		setGameStats(std::make_unique<TestGameStats>(*this));
		setLifeStats(std::make_unique<TestLifeStats>(*this));
	}
};

/** Java PlayerService.getPlayer without the DAO loads: account, common data, appearance, account data and the account warehouse */
struct PlayerFixture {
	runtime::Ref<model::account::Account> account;
	runtime::Ref<model::gameobjects::player::PlayerCommonData> commonData;
	runtime::Ref<TestPlayer> player;
};

inline PlayerFixture makePlayer(int32_t objectId, int32_t accountId, std::string_view name = "Tester", model::Race race = model::Race::ELYOS) {
	PlayerFixture f;
	f.account = model::account::Account::create(accountId);
	f.commonData = model::gameobjects::player::PlayerCommonData::create(objectId);
	f.commonData->setName(name);
	f.commonData->setRace(race);
	runtime::Ref<model::gameobjects::player::PlayerAppearance> appearance = model::gameobjects::player::PlayerAppearance::create();
	f.account->addPlayerAccountData(std::make_unique<model::account::PlayerAccountData>(*f.account, *f.commonData, *appearance));
	f.account->setAccountWarehouse(std::make_unique<model::items::storage::PlayerStorage>(*f.account, model::items::storage::StorageType::ACCOUNT_WAREHOUSE));
	f.player = model::gameobjects::VisibleObject::create<TestPlayer>(*f.account->getPlayerAccountData(objectId), *f.account);
	return f;
}

/** Fixture: a task scope, a deterministic thread pool and UTC as the server time zone; database tests call requireDatabase() first */
class LegionHouseTest : public testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		auto backend = std::make_unique<runtime::DeterministicExecutor>(clock, 11);
		executor = backend.get();
		utils::ThreadPoolManager::installBackend(std::move(backend));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		scope.emplace(AION_TASK_INFO(runtime::TaskKind::TEST));
	}

	void TearDown() override {
		scope.reset();
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	/** @return false (and marks the test skipped) without the test database */
	bool requireDatabase() {
		if (!isDatabaseEnabled())
			return false;
		setUpDatabaseOnce();
		return true;
	}

	runtime::ManualClock clock{0};
	/** the installed backend (retired backends are kept alive by ThreadPoolManager, so the pointer stays valid) */
	runtime::DeterministicExecutor* executor = nullptr;
	std::optional<runtime::TaskScope> scope;
	TimeZoneScope utc{"UTC"};
};

/** Skips the test without the test database (use at the start of a TEST_F body) */
#define LEGIONHOUSE_REQUIRE_DATABASE()                                                                                                                \
	if (!requireDatabase())                                                                                                                           \
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";

} // namespace aion::gameserver::legionhouse::test
