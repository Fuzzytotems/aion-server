#pragma once

// Shared test support of the login slice (P5-00 tests/login_slice and the P4-15 flow tests in tests/network; m5a-plan.md S-10):
// - the test database `aion_gs_test_login_slice` on the server of AION_TEST_GS_DATABASE_URL, recreated from game-server/sql/aion_gs.sql once per
//   process under the MariaDB lock of the same name (ctest runs every test case in its own process; the lock serializes them), tables emptied per
//   test;
// - small static data holders the slice reads (spawn locations and creation data, the experience table, the world maps);
// - SlicePlayer: the real Player with a construction and destruction counter (the destroy observer of the lifetime tests), installed into
//   PlayerService through its C++-only test factory; it falls back to stat container doubles while the P5-01 containers are unported;
// - SKIP_IF_UNPORTED: a body of another chunk that is still AION_UNPORTED skips the test (it runs once that body is ported).
// Included by name from tests/network (TEST_INCLUDES support) and by relative path from tests/login_slice.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
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

#include "aion/commons/configuration/ConfigurableProcessor.h"
#include "aion/commons/configuration/Properties.h"
#include "aion/commons/database/Connection.h"
#include "aion/commons/database/ConnectionProperties.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/configs/Config.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/SkillTreeData.bind.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/PlayerInitialData.bind.h"
#include "aion/gameserver/dataholders/PlayerInitialData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/services/player/PlayerService.h"

/** Runs the statement; an unported body of another chunk skips the test (it starts to run once that body is ported) */
#define SLICE_SKIP_IF_UNPORTED(statement)                                                                                                     \
	try {                                                                                                                                      \
		statement;                                                                                                                             \
	} catch (const ::aion::gameserver::runtime::UnportedException& unported) {                                                                  \
		GTEST_SKIP() << "another chunk is not ported yet: " << unported.what();                                                               \
	}

namespace aion::gameserver::loginslice::test {

inline constexpr std::string_view TEST_DATABASE = "aion_gs_test_login_slice";

inline std::string env(const char* name) {
	const char* value = std::getenv(name); // NOLINT(concurrency-mt-unsafe): read by the test process before threads start
	return value ? value : "";
}

/** @return true if the database tests are enabled (AION_TEST_GS_DATABASE_URL is set) */
inline bool isDatabaseEnabled() {
	return !env("AION_TEST_GS_DATABASE_URL").empty();
}

inline std::string databaseUser() {
	std::string value = env("AION_TEST_GS_DATABASE_USER");
	return value.empty() ? "root" : value;
}

inline std::string databasePassword() {
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

/** Splits an SQL script into statements (';' outside quotes; "-- ", "#" and block comments removed) */
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

/** The table names of the test database, read after the schema was created */
inline std::vector<std::string>& databaseTables() {
	static std::vector<std::string> names;
	return names;
}

/** Takes the process lock, recreates the test database from aion_gs.sql and initializes DatabaseFactory with it (once per process) */
inline void setUpDatabaseOnce() {
	static std::once_flag once;
	std::call_once(once, [] {
		using commons::database::Connection;
		using commons::database::ConnectionProperties;
		// the lock connection stays open until the process exits: the server releases the named lock when it closes
		static Connection* lockConnection =
			Connection::open(ConnectionProperties::parse(env("AION_TEST_GS_DATABASE_URL"), databaseUser(), databasePassword())).release();
		auto lock = lockConnection->prepareStatement("SELECT GET_LOCK(?, 900)");
		lock->setString(1, std::string(TEST_DATABASE));
		auto locked = lock->executeQuery();
		if (!locked->next() || locked->getInt(1) != 1)
			throw commons::utils::IllegalStateException("Could not acquire the database lock " + std::string(TEST_DATABASE));

		lockConnection->executeSimple("DROP DATABASE IF EXISTS `" + std::string(TEST_DATABASE) + "`");
		lockConnection->executeSimple("CREATE DATABASE `" + std::string(TEST_DATABASE) + "` CHARACTER SET utf8mb4");

		std::unique_ptr<Connection> schema =
			Connection::open(ConnectionProperties::parse(urlWithDatabase(TEST_DATABASE), databaseUser(), databasePassword()));
		std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "sql" / "aion_gs.sql", std::ios::binary);
		if (!in)
			throw commons::utils::IOException("Cannot read aion_gs.sql");
		std::stringstream content;
		content << in.rdbuf();
		for (const std::string& statement : splitSqlStatements(content.str()))
			schema->executeSimple(statement);
		auto rs = schema->prepareStatement("SELECT table_name FROM information_schema.tables WHERE table_schema = ? AND table_type = 'BASE TABLE'");
		rs->setString(1, std::string(TEST_DATABASE));
		auto names = rs->executeQuery();
		while (names->next())
			databaseTables().push_back(names->getString(1));

		commons::database::DatabaseFactory::init(urlWithDatabase(TEST_DATABASE), databaseUser(), databasePassword(), 10, 5000);
	});
}

/** Deletes the rows of every table (foreign key checks off on a dedicated connection that is closed afterwards) */
inline void clearTables() {
	using commons::database::Connection;
	using commons::database::ConnectionProperties;
	std::unique_ptr<Connection> con = Connection::open(ConnectionProperties::parse(urlWithDatabase(TEST_DATABASE), databaseUser(), databasePassword()));
	con->executeSimple("SET FOREIGN_KEY_CHECKS=0");
	for (const std::string& table : databaseTables())
		con->executeSimple("DELETE FROM `" + table + "`");
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

/** @return the first column of the first row of a query, std::nullopt for NULL or no row */
inline std::optional<std::string> queryString(std::string_view sql) {
	auto con = commons::database::DatabaseFactory::getConnection();
	auto rs = con->prepareStatement(sql)->executeQuery();
	if (!rs->next())
		return std::nullopt;
	return rs->getObject<std::string>(1);
}

/** Inserts a players row */
inline void insertPlayer(int32_t id, std::string_view name, int32_t accountId, std::string_view race = "ELYOS", std::string_view playerClass = "WARRIOR") {
	auto con = commons::database::DatabaseFactory::getConnection();
	auto st = con->prepareStatement("INSERT INTO players (id, name, account_id, account_name, x, y, z, heading, world_id, gender, race, player_class, exp) "
									"VALUES (?, ?, ?, ?, 1, 2, 3, 4, 210010000, 'MALE', ?, ?, 0)");
	st->setInt(1, id);
	st->setString(2, name);
	st->setInt(3, accountId);
	st->setString(4, "account" + std::to_string(accountId));
	st->setString(5, race);
	st->setString(6, playerClass);
	st->execute();
}

// ---------------------------------------------------------------------------------------------------------------------------------- config

/** Applies the Java default of every game server config field once per process (the slice reads GSConfig, NameConfig, MembershipConfig, ...) */
inline void applyConfigDefaultsOnce() {
	static std::once_flag once;
	std::call_once(once, [] {
		commons::configuration::Properties empty;
		std::vector<commons::configuration::ConfigurableProcessor::Binder> binders;
		for (const configs::Config::ConfigClass& config : configs::Config::getClasses())
			binders.emplace_back(config.bind);
		commons::configuration::ConfigurableProcessor::process(empty, binders);
	});
}

// ------------------------------------------------------------------------------------------------------------------------------ static data

/** player_initial_data.xml of the tests: the real spawn locations, creation data without starting items */
inline const char* const PLAYER_INITIAL_DATA_XML = R"(<player_initial_data>
	<asmodian_spawn_location map_id="220010000" heading="32" x="571.0388" y="2787.3420" z="299.8750"/>
	<elyos_spawn_location map_id="210010000" heading="32" x="1212.9423" y="1044.8516" z="140.75568"/>
	<player_data class="WARRIOR"><items/></player_data>
	<player_data class="MAGE"><items/></player_data>
</player_initial_data>)";

/** the first 16 levels of player_experience_table.xml (PlayerCommonData.setExp reads the start of level 10) */
inline const char* const PLAYER_EXPERIENCE_TABLE_XML = "<player_experience_table><exp>0</exp><exp>400</exp><exp>1433</exp><exp>3820</exp><exp>9054</exp>"
	"<exp>17655</exp><exp>30978</exp><exp>52010</exp><exp>82982</exp><exp>126069</exp><exp>182252</exp><exp>260622</exp><exp>360825</exp>"
	"<exp>490331</exp><exp>649169</exp><exp>844378</exp></player_experience_table>";

inline const char* const WORLD_MAPS_XML = R"(<world_maps>
	<map id="210010000" cName="LF1" name="Poeta" name_id="400234" water_level="100" death_level="0" world_type="ELYSEA" world_size="3072" flags="BIND RECALL GLIDE PVP DUEL_SAME_RACE"/>
	<map id="220010000" cName="DF1" name="Ishalgen" name_id="400259" water_level="248" death_level="0" world_type="ASMODAE" world_size="3072" flags="BIND RECALL GLIDE PVP DUEL_SAME_RACE"/>
</world_maps>)";

/** Publishes the slice's static data holders once per process (DataManager holders are published once) */
inline void publishStaticDataOnce() {
	static std::once_flag once;
	std::call_once(once, [] {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		xml::LoadContext context;
		if (!dataholders::DataManager::PLAYER_INITIAL_DATA)
			dataholders::DataManager::PLAYER_INITIAL_DATA.publish(xml::bindString<dataholders::PlayerInitialData>(context, PLAYER_INITIAL_DATA_XML));
		if (!dataholders::DataManager::PLAYER_EXPERIENCE_TABLE)
			dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
				xml::bindString<dataholders::PlayerExperienceTable>(context, PLAYER_EXPERIENCE_TABLE_XML));
		if (!dataholders::DataManager::WORLD_MAPS_DATA) {
			// World::getInstance() reads these holders and the region size once (like tests/world/WorldTestSupport.h)
			configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
			configs::main::WorldConfig::WORLD_MAX_TWINS_USUAL.store(0);
			configs::main::WorldConfig::WORLD_MAX_TWINS_BEGINNER.store(0);
			dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(context, WORLD_MAPS_XML));
		}
		if (!dataholders::DataManager::ZONE_DATA)
			dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(context, "<zones/>"));
		if (!dataholders::DataManager::SHIELD_DATA)
			dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(context, "<shields/>"));
		if (!dataholders::DataManager::MATERIAL_DATA)
			dataholders::DataManager::MATERIAL_DATA.publish(xml::bindString<dataholders::MaterialData>(context, "<material_templates/>"));
		// GMService::getInstance() (AuditLogger, enter world) reads the skill templates
		if (!dataholders::DataManager::SKILL_DATA)
			dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(context, "<skill_data/>"));
		// PlayerService.newPlayer (SkillLearnService), the item factory and HousingService (character deletion) read these; the tests need no entries
		if (!dataholders::DataManager::SKILL_TREE_DATA)
			dataholders::DataManager::SKILL_TREE_DATA.publish(xml::bindString<dataholders::SkillTreeData>(context, "<skill_tree/>"));
		if (!dataholders::DataManager::ITEM_DATA)
			dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, "<item_templates/>"));
		if (!dataholders::DataManager::HOUSE_DATA)
			dataholders::DataManager::HOUSE_DATA.publish(xml::bindString<dataholders::HouseData>(context, "<house_lands/>"));
	});
}

// --------------------------------------------------------------------------------------------------------------------------------- players

/** CreatureGameStats double (the stat calculation belongs to P5-01) */
class SliceGameStats final : public model::stats::container::CreatureGameStats {
public:
	explicit SliceGameStats(model::gameobjects::Creature& owner) : CreatureGameStats(owner) {}
	const model::templates::stats::StatsTemplate* getStatsTemplate() override { return nullptr; }
	int32_t getBaseAttackSpeed() override { return 0; }
	std::unique_ptr<model::stats::calc::Stat2> getMovementSpeed() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getAttackRange() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getHpRegenRate() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getMpRegenRate() override { return nullptr; }
};

/** CreatureLifeStats double */
class SliceLifeStats final : public model::stats::container::CreatureLifeStats {
public:
	explicit SliceLifeStats(model::gameobjects::Creature& owner) : CreatureLifeStats(owner, 1000, 500) {}
};

/** Destroyed SlicePlayers since the last reset (the destroy observer of the lifetime tests) */
inline std::atomic<int32_t> destroyedSlicePlayers{0};
/** Created SlicePlayers since the last reset */
inline std::atomic<int32_t> createdSlicePlayers{0};

/** The real Player with stat container doubles; counts its construction and destruction */
class SlicePlayer final : public model::gameobjects::player::Player {
	AION_MAKE_REF_FRIEND
public:
	SlicePlayer(CreateKey key, model::account::PlayerAccountData& playerAccountData, model::account::Account& account)
		: Player(key, playerAccountData, account) {
		createdSlicePlayers.fetch_add(1);
	}

protected:
	~SlicePlayer() override { destroyedSlicePlayers.fetch_add(1); }

	void postConstruct() override {
		try {
			Player::postConstruct();
		} catch (const runtime::UnportedException&) {
			// the stat containers are not ported yet (P5-01): everything before them ran, the doubles take their place
			setGameStats(std::make_unique<SliceGameStats>(*this));
			setLifeStats(std::make_unique<SliceLifeStats>(*this));
		}
	}
};

/** PlayerService::PlayerFactory creating SlicePlayers */
inline runtime::Ref<model::gameobjects::player::Player> createSlicePlayer(model::account::PlayerAccountData& playerAccountData,
	model::account::Account& account) {
	return model::gameobjects::VisibleObject::create<SlicePlayer>(playerAccountData, account);
}

/** Installs the SlicePlayer factory into PlayerService for the scope and resets the counters */
class SlicePlayerFactoryScope {
public:
	SlicePlayerFactoryScope() {
		createdSlicePlayers = 0;
		destroyedSlicePlayers = 0;
		services::player::PlayerService::setPlayerFactoryForTests(&createSlicePlayer);
	}
	~SlicePlayerFactoryScope() {
		services::player::PlayerService::setPlayerFactoryForTests(nullptr);
		services::player::PlayerService::setLoadHookForTests(nullptr);
	}
	SlicePlayerFactoryScope(const SlicePlayerFactoryScope&) = delete;
	SlicePlayerFactoryScope& operator=(const SlicePlayerFactoryScope&) = delete;
};

/** Runs full Reclaimer scans until the backlog is empty (call outside any TaskScope) */
inline void drainReclaimer() {
	for (int i = 0; i < 4; i++)
		runtime::Reclaimer::getInstance().drain();
}

} // namespace aion::gameserver::loginslice::test
