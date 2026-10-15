#pragma once

// Shared fixture of the ascension lane's tests (m5f-plan.md §15.6): tests/instance (P5-13), and - by relative path, like tests/cm_lz includes
// tests/cm_ak/InWorldPacketRunSupport.h - tests/playersvc (P5-08) and tests/cm_lz (P5-16). It is InWorldPacketTest (a DeterministicExecutor on a
// ManualClock, real Players and a real AionConnection) on a World of the real map rows the route touches (AscensionTestData.h, verbatim):
// Poeta, Verteron, Ishalgen, Altgard (open world maps, one instance each with the server's twin defaults), Taloc's Hollow, Haramel, Kamar
// Battlefield, Karamatis and Ataxiar (instance maps), without zones, shields or materials, and with GeoService's empty GeoMaps (geo data off, as
// the server starts without geo data). The holders the instance paths read are published per test: the npc
// templates and the spawns of Karamatis B and Ataxiar B, their two instance_cooltimes rows, Haramel's two instance_exit rows, no static doors,
// player_initial_data.xml's two spawn locations (the bind fallback) and the tribe relations of the npcs' and the players' tribes (without them
// every npc a player meets throws in KnownList, Npc.getType -> TribeRelationService, and the two never see each other).
//
// World reads its maps once per process, so a process that published WorldTestSupport.h's small test maps cannot use this fixture (the test
// skips: run it on its own, as ctest does - every test is its own process), and publishing this set makes publishTestStaticData() answer false.
//
// Database: SpawnEngine.spawnInstance ends in HousingService.spawnHouses, whose singleton loads the houses and the used player ids from the
// database, so a created instance needs one. The fixture recreates `aion_gs_test_ascension` from the Java tree's aion_gs.sql on the server of
// AION_TEST_GS_DATABASE_URL, once per process under a named lock (the rules of tests/economy/P5-09a/EconomyTestSupport.h, whose helpers it
// reuses), and skips without that variable. HOUSE_DATA is empty, so no house is spawned and no row is read.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "AscensionTestData.h"
#include "../cm_ak/InWorldPacketRunSupport.h"
#include "../economy/P5-09a/EconomyTestSupport.h"
#include "../world/WorldTestSupport.h"

#include "aion/commons/database/Connection.h"
#include "aion/commons/database/ConnectionProperties.h"
#include "aion/commons/database/DatabaseFactory.h"

#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.bind.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/dataholders/InstanceExitData.bind.h"
#include "aion/gameserver/dataholders/InstanceExitData.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/PlayerInitialData.bind.h"
#include "aion/gameserver/dataholders/PlayerInitialData.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/SpawnsData.bind.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/dataholders/StaticDoorData.bind.h"
#include "aion/gameserver/dataholders/StaticDoorData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/zone/ZoneUpdateService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::instance::test {

namespace cptest = network::aion::clientpackets::testing;

inline constexpr int32_t POETA = 210010000;
inline constexpr int32_t VERTERON = 210030000;
inline constexpr int32_t ISHALGEN = 220010000;
inline constexpr int32_t ALTGARD = 220030000;
inline constexpr int32_t TALOCS_HOLLOW = 300190000;
inline constexpr int32_t HARAMEL = 300200000;
inline constexpr int32_t KAMAR_BATTLEFIELD = 301120000;
inline constexpr int32_t KARAMATIS_B = 310020000;
inline constexpr int32_t ATAXIAR_B = 320020000;

/** player_initial_data.xml:4, the Elyos spawn location: the bind fallback of an Elyos without a bind point */
inline constexpr float ELYOS_SPAWN_X = 1212.9423f;
inline constexpr float ELYOS_SPAWN_Y = 1044.8516f;
inline constexpr float ELYOS_SPAWN_Z = 140.75568f;
inline constexpr int8_t ELYOS_SPAWN_HEADING = 32;

/**
 * Publishes the ascension world once per process (the server's WorldConfig defaults: 128 m regions, one usual twin, no beginner twins).
 * @return false if this process published WorldTestSupport.h's test maps
 */
inline bool publishAscensionWorldData() {
	std::scoped_lock lock(world::test::publishedDataMutex);
	if (world::test::publishedData == world::test::PublishedData::TEST)
		return false;
	static bool published = false;
	if (!published) {
		if (world::test::publishedData == world::test::PublishedData::REAL)
			return false; // another file's real-data world: not this one
		configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
		configs::main::WorldConfig::WORLD_MAX_TWINS_USUAL.store(1);
		configs::main::WorldConfig::WORLD_MAX_TWINS_BEGINNER.store(-1);
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		static std::deque<xml::LoadContext> contexts;
		dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(contexts.emplace_back(), worldMapsXml()));
		dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(contexts.emplace_back(), "<zones/>"));
		dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(contexts.emplace_back(), "<shields/>"));
		dataholders::DataManager::MATERIAL_DATA.publish(
			xml::bindString<dataholders::MaterialData>(contexts.emplace_back(), "<material_templates/>"));
		// the server's startup with geo data off (GameServer -> GeoService.init): one empty GeoMap per map, so a spawn with a static id reaches
		// GeoMap.spawnPlaceableObject and finds no placeable object, instead of Java's NullPointerException of a map without a GeoMap
		world::geo::GeoService::getInstance().init();
		world::test::publishedData = world::test::PublishedData::REAL;
		published = true;
	}
	return true;
}

inline constexpr std::string_view ASCENSION_TEST_DATABASE = "aion_gs_test_ascension";

/** Takes the database's named lock for the life of the process, recreates it from aion_gs.sql and initializes DatabaseFactory. Once per process. */
inline void setUpAscensionDatabaseOnce() {
	static std::once_flag once;
	std::call_once(once, [] {
		using commons::database::Connection;
		using commons::database::ConnectionProperties;
		namespace db = economy::test;
		static Connection* lockConnection =
			Connection::open(ConnectionProperties::parse(db::env("AION_TEST_GS_DATABASE_URL"), db::user(), db::password())).release();
		auto lock = lockConnection->prepareStatement("SELECT GET_LOCK(?, 900)");
		lock->setString(1, std::string(ASCENSION_TEST_DATABASE));
		auto locked = lock->executeQuery();
		if (!locked->next() || locked->getInt(1) != 1)
			throw commons::utils::IllegalStateException("Could not acquire the database lock " + std::string(ASCENSION_TEST_DATABASE));
		lockConnection->executeSimple("DROP DATABASE IF EXISTS `" + std::string(ASCENSION_TEST_DATABASE) + "`");
		lockConnection->executeSimple("CREATE DATABASE `" + std::string(ASCENSION_TEST_DATABASE) + "` CHARACTER SET utf8mb4");
		std::unique_ptr<Connection> schema =
			Connection::open(ConnectionProperties::parse(db::urlWithDatabase(ASCENSION_TEST_DATABASE), db::user(), db::password()));
		std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "sql" / "aion_gs.sql", std::ios::binary);
		if (!in)
			throw commons::utils::IOException("Cannot read aion_gs.sql");
		std::stringstream script;
		script << in.rdbuf();
		for (const std::string& statement : db::splitSqlStatements(script.str()))
			schema->executeSimple(statement);
		commons::database::DatabaseFactory::init(db::urlWithDatabase(ASCENSION_TEST_DATABASE), db::user(), db::password(), 10, 5000);
	});
}

inline runtime::Ptr<model::house::House> noHouse(model::gameobjects::player::Player&) {
	return nullptr;
}

inline runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo> noCpInfo(model::gameobjects::player::Player&) {
	return nullptr;
}

class AscensionWorldTest : public cptest::InWorldPacketTest {
protected:
	void SetUp() override {
		cptest::InWorldPacketTest::SetUp();
		if (!economy::test::isDatabaseEnabled())
			GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the ascension tests (SpawnEngine.spawnInstance reads the houses)";
		if (!publishAscensionWorldData())
			GTEST_SKIP() << "this process published another test world (run the test on its own)";
		setUpAscensionDatabaseOnce();
		prepared = true;
		missingAiHandlers = *configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn"); // no AI is registered in the test executables: DummyNpcAI instead
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, npcTemplatesXml()));
		dataholders::DataManager::SPAWNS_DATA.publish(xml::bindString<dataholders::SpawnsData>(context, spawnsXml()));
		dataholders::DataManager::INSTANCE_COOLTIME_DATA.publish(
			xml::bindString<dataholders::InstanceCooltimeData>(context, instanceCooltimesXml()));
		dataholders::DataManager::INSTANCE_EXIT_DATA.publish(xml::bindString<dataholders::InstanceExitData>(context, instanceExitXml()));
		dataholders::DataManager::STATICDOOR_DATA.publish(xml::bindString<dataholders::StaticDoorData>(context, "<staticdoor_templates/>"));
		dataholders::DataManager::PLAYER_INITIAL_DATA.publish(xml::bindString<dataholders::PlayerInitialData>(context, playerInitialDataXml()));
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(
			xml::bindString<dataholders::TribeRelationsData>(context, tribeRelationsXml()));
		if (!dataholders::DataManager::HOUSE_DATA) // HousingService's singleton keeps reading it: published once per process, never reset
			dataholders::DataManager::HOUSE_DATA.publish(xml::bindString<dataholders::HouseData>(houseContext(), "<house_lands/>"));
		// the two reads of SM_PLAYER_INFO that need services these tests have not got (HousingService loads from the database)
		lookups.activeHouseOfPlayer = &noHouse;
		lookups.cpInfoForCurrentMap = &noCpInfo;
		network::aion::serverpackets::detail::setPacketLookupsForTests(&lookups);
	}

	void TearDown() override {
		if (prepared) {
			if (actor.player) {
				world::zone::ZoneUpdateService::getInstance().run();
				actor.player->getController().cancelAllTasks();
				if (actor.player->isSpawned())
					world::World::getInstance().despawn(*actor.player);
				world::World::getInstance().removeObject(*actor.player);
				actor.player->setTarget(nullptr);
				actor.player->setClientConnection(nullptr);
			}
			client.reset();
			actor = {};
			// the instances a test created stay in their maps (World is per process): destroy them, so their checkers and npcs go too
			for (int32_t mapId : {KARAMATIS_B, ATAXIAR_B, TALOCS_HOLLOW, HARAMEL, KAMAR_BATTLEFIELD}) {
				for (const runtime::Ptr<world::WorldMapInstance>& instance : *world::World::getInstance().getWorldMap(mapId)) {
					if (instance->getInstanceId() > 1)
						services::instance::InstanceService::destroyInstance(*instance);
				}
			}
			network::aion::serverpackets::detail::setPacketLookupsForTests(nullptr);
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(missingAiHandlers);
			dataholders::DataManager::NPC_DATA.resetForTests();
			dataholders::DataManager::SPAWNS_DATA.resetForTests();
			dataholders::DataManager::INSTANCE_COOLTIME_DATA.resetForTests();
			dataholders::DataManager::INSTANCE_EXIT_DATA.resetForTests();
			dataholders::DataManager::STATICDOOR_DATA.resetForTests();
			dataholders::DataManager::PLAYER_INITIAL_DATA.resetForTests();
			dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		}
		cptest::InWorldPacketTest::TearDown();
	}

	static xml::LoadContext& houseContext() {
		static xml::LoadContext context;
		return context;
	}

	/** The installed DeterministicExecutor (InWorldPacketTest installs it and keeps no pointer) */
	static runtime::DeterministicExecutor& executor() {
		return dynamic_cast<runtime::DeterministicExecutor&>(*utils::ThreadPoolManager::installedBackend());
	}

	/** A connected level-1 character with motions, stored in the World and spawned at the given place (TeleportStatementsTest's way) */
	void spawnActor(int32_t mapId, int32_t instanceId, float x, float y, float z, int8_t h, model::Race race = model::Race::ELYOS) {
		actor = cptest::makePlayer(420001, 9411, "Ascender", race);
		actor.player->setMotions(std::make_unique<model::gameobjects::player::motion::MotionList>(*actor.player));
		world::World& world = world::World::getInstance();
		world.storeObject(*actor.player);
		ASSERT_TRUE(world.setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player), mapId, instanceId, x, y, z, h));
		world.spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
		ASSERT_TRUE(actor.player->isSpawned());
		client = std::make_unique<cptest::TestClient>();
		client->enterWorld(actor);
		(*client)->clearSent();
	}

	std::vector<std::vector<uint8_t>> sent() { return (*client)->sentBytes(); }

	bool wasSent(const std::vector<uint8_t>& packet) {
		std::vector<std::vector<uint8_t>> bytes = sent();
		return std::find(bytes.begin(), bytes.end(), packet) != bytes.end();
	}

	/** The 5 byte opcode header of a serialized packet: it identifies the packet class (TeleportStatementsTest.cpp:172-181) */
	static std::vector<uint8_t> headerOf(const std::vector<uint8_t>& packet) {
		return std::vector<uint8_t>(packet.begin(), packet.begin() + std::min<size_t>(5, packet.size()));
	}

	bool wasSentClass(const std::vector<uint8_t>& packet) {
		for (const std::vector<uint8_t>& bytes : sent())
			if (headerOf(bytes) == headerOf(packet))
				return true;
		return false;
	}

	bool prepared = false;
	std::string missingAiHandlers;
	xml::LoadContext context;
	cptest::PlayerFixture actor;
	std::unique_ptr<cptest::TestClient> client;
	network::aion::serverpackets::detail::PacketLookupsForTests lookups{};
};

} // namespace aion::gameserver::instance::test
