#pragma once

// Q09 (phase 6, route-hand lane, 2026-09-29): the fixture of the hand-ported starting-zone quests - 1002 and 1114 of Poeta (Q05), 2002, 2004,
// 2007 and 2136 of Ishalgen (Q09) - driven through the real QuestEngine on a real World.
//
// - InWorldPacketTest (tests/cm_ak/InWorldPacketRunSupport.h: a DeterministicExecutor on a ManualClock, real Players and AionConnections) with
//   a World of five real map rows: Poeta and Ishalgen (open world maps), the two solo instance maps the quests create, Karamatis
//   (310010000) and Ataxiar (320010000), and Pandaemonium (a static object's map, for teleportToNpc). World reads its maps once per
//   process, like the ascension fixture it follows (tests/instance/AscensionTestSupport.h, whose database and packet-lookup helpers it
//   reuses): every test is its own process under ctest.
// - The holders are verbatim excerpts of the shipped data (ZoneQuestTestData.h): the quest rows, the item rows, the npc templates of the two
//   instance spawn files and of the quests' npcs, the tribe rows of those npcs and of both races' players, the spawns of both instance maps,
//   Ulgorn's spawn in Ishalgen (the one TeleportService.teleportToNpc looks up for 2007), Rae's (203554, h 60) there and a static object of
//   Pandaemonium (a fifth map row) for teleportToNpc's heading and no-template arms, and the skill row of 2002's spirit stone (8343).
// - The handler under test is created through its AION_QUEST_HANDLER factory and handed to QuestEngine.addQuestHandler, as QuestEngine.init
//   does with the generated registry; npcs are spawned into the World by AbstractQuestHandler.spawn (SpawnEngine), as the quests do.
// - Database: only the instance cases need one (SpawnEngine.spawnInstance reads the houses, AscensionTestSupport.h); they call
//   needDatabase(), which skips without AION_TEST_GS_DATABASE_URL.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "ZoneQuestTestData.h"
#include "../instance/AscensionTestSupport.h"

#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/ItemSetData.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/TitleData.bind.h"
#include "aion/gameserver/dataholders/TitleData.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/cron/ThreadPoolManagerRunnableRunner.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::zones::test {

namespace cptest = network::aion::clientpackets::testing;
namespace asc = ::aion::gameserver::instance::test;
using model::QuestEnv;
using model::QuestState;
using model::QuestStatus;
using runtime::Ptr;
using runtime::Ref;
using Player = gameserver::model::gameobjects::player::Player;
using Npc = gameserver::model::gameobjects::Npc;

inline constexpr int32_t POETA = 210010000;
inline constexpr int32_t ISHALGEN = 220010000;
inline constexpr int32_t KARAMATIS = 310010000; // WorldMapType.KARAMATIS (1002's solo instance)
inline constexpr int32_t ATAXIAR = 320010000;   // WorldMapType.ATAXIAR (2002's solo instance)

inline constexpr int32_t SM_DIALOG_WINDOW_OPCODE = 60; // ServerPacketsOpcodes.java
inline constexpr int32_t SM_QUEST_ACTION_OPCODE = 124;

/**
 * Publishes the five maps once per process (the server's WorldConfig defaults, as publishAscensionWorldData does).
 * @return false if this process published another test world
 */
inline bool publishZoneWorldData() {
	std::scoped_lock lock(world::test::publishedDataMutex);
	static bool published = false;
	if (published)
		return true;
	if (world::test::publishedData != world::test::PublishedData::NONE)
		return false;
	configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
	configs::main::WorldConfig::WORLD_MAX_TWINS_USUAL.store(1);
	configs::main::WorldConfig::WORLD_MAX_TWINS_BEGINNER.store(-1);
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	static std::deque<xml::LoadContext> contexts;
	dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(contexts.emplace_back(), zoneWorldMapsXml()));
	dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(contexts.emplace_back(), "<zones/>"));
	dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(contexts.emplace_back(), "<shields/>"));
	dataholders::DataManager::MATERIAL_DATA.publish(xml::bindString<dataholders::MaterialData>(contexts.emplace_back(), "<material_templates/>"));
	world::geo::GeoService::getInstance().init(); // geo data off: one empty GeoMap per map (AscensionTestSupport.h)
	world::test::publishedData = world::test::PublishedData::REAL;
	published = true;
	return true;
}

/** One server packet as Java writes it (ItemPacketTestSupport.h's javaPacket): AionServerPacket.writeOP and the body */
inline std::vector<uint8_t> javaPacket(int32_t opcode, const network::test::PacketWriter& body) {
	int32_t op = (opcode + 207) ^ 0xDF; // Crypt.encodeServerPacketOpcode: (opcode + SM_VERSION_CHECK.INTERNAL_VERSION) ^ 0xDF
	network::test::PacketWriter packet;
	packet.H(op).C(0x44).H(~op);
	packet.B(body.data);
	return packet.data;
}

/** SM_DIALOG_WINDOW (SM_DIALOG_WINDOW.java:29-40), a page other than MAIL and TOWN_CHALLENGE_TASK: D target, H page, D quest, H 0, H 0 */
inline std::vector<uint8_t> dialogWindow(int32_t targetObjectId, int32_t dialogPageId, int32_t questId) {
	return javaPacket(SM_DIALOG_WINDOW_OPCODE, network::test::PacketWriter().D(targetObjectId).H(dialogPageId).D(questId).H(0).H(0));
}

/** SM_QUEST_ACTION(UPDATE, qs) (SM_QUEST_ACTION.java:67-81): C 2, D quest, C status value, C 0, D vars | flags << 24, H 0 */
inline std::vector<uint8_t> questUpdate(int32_t questId, int32_t statusValue, int32_t vars) {
	return javaPacket(SM_QUEST_ACTION_OPCODE, network::test::PacketWriter().C(2).D(questId).C(statusValue).C(0).D(vars).H(0));
}

inline constexpr int32_t START = 3; // QuestStatus.value() (QuestStatus.java:11-14)
inline constexpr int32_t REWARD = 4;

class ZoneQuestTest : public cptest::InWorldPacketTest {
protected:
	void SetUp() override {
		cptest::InWorldPacketTest::SetUp();
		if (!publishZoneWorldData())
			GTEST_SKIP() << "this process published another test world (run the test on its own)";
		prepared = true;
		// only the instance cases set up the database (needDatabase): it is recreated once per process under a lock the instance tests share.
		// Without it, the singletons the packets reach at first use (TownService for SM_NPC_INFO, ServerVariablesDAO) log that they could
		// not load and go on with nothing loaded, which no case reads
		missingAiHandlers = *configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn"); // no AI is registered in the test executables: DummyNpcAI instead
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(
			[](Player&) { return std::vector<Ref<gameserver::model::gameobjects::player::PetCommonData>>(); });
		savedQuestKinahRates = *configs::main::RatesConfig::QUEST_KINAH_RATES.get();
		savedXpQuestRates = *configs::main::RatesConfig::XP_QUEST_RATES.get();
		configs::main::RatesConfig::QUEST_KINAH_RATES.set({1.0f});
		configs::main::RatesConfig::XP_QUEST_RATES.set({1.0f});
		services::cron::CronService::resetForTests(); // QuestEngine::clear cancels the daily message in the cron service
		services::cron::CronService::initSingleton(std::make_unique<utils::cron::ThreadPoolManagerRunnableRunner>(), std::chrono::locate_zone("UTC"),
			services::cron::CronService::Driver::EXECUTOR);
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), zoneNpcTemplatesXml()));
		dataholders::DataManager::SPAWNS_DATA.publish(xml::bindString<dataholders::SpawnsData>(contexts.emplace_back(), zoneSpawnsXml()));
		dataholders::DataManager::INSTANCE_COOLTIME_DATA.publish(
			xml::bindString<dataholders::InstanceCooltimeData>(contexts.emplace_back(), zoneInstanceCooltimesXml()));
		dataholders::DataManager::INSTANCE_EXIT_DATA.publish(
			xml::bindString<dataholders::InstanceExitData>(contexts.emplace_back(), asc::instanceExitXml()));
		dataholders::DataManager::STATICDOOR_DATA.publish(
			xml::bindString<dataholders::StaticDoorData>(contexts.emplace_back(), "<staticdoor_templates/>"));
		dataholders::DataManager::PLAYER_INITIAL_DATA.publish(
			xml::bindString<dataholders::PlayerInitialData>(contexts.emplace_back(), asc::playerInitialDataXml()));
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(
			xml::bindString<dataholders::TribeRelationsData>(contexts.emplace_back(), zoneTribeRelationsXml()));
		if (!dataholders::DataManager::HOUSE_DATA) // HousingService's singleton keeps reading it: published once per process, never reset
			dataholders::DataManager::HOUSE_DATA.publish(xml::bindString<dataholders::HouseData>(houseContext(), "<house_lands/>"));
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(contexts.emplace_back(), zoneQuestsXml()));
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(contexts.emplace_back(), zoneItemTemplatesXml()));
		dataholders::DataManager::TITLE_DATA.publish(xml::bindString<dataholders::TitleData>(contexts.emplace_back(), zoneTitlesXml()));
		// the base fixture's empty skill holder, replaced by the row of the skill 2002's spirit stone applies (ItemPacketTest's way); the base
		// TearDown resets it
		dataholders::DataManager::SKILL_DATA.resetForTests();
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(contexts.emplace_back(), zoneSkillTemplatesXml()));
		// what ItemPacketTest publishes beside its items: no cleanup rows and no item sets (none of the rows belongs to one)
		dataholders::DataManager::ITEM_CLEAN_UP.publish(std::make_unique<dataholders::ItemRestrictionCleanupData>());
		dataholders::DataManager::ITEM_SET_DATA.publish(std::make_unique<dataholders::ItemSetData>());
		lookups.activeHouseOfPlayer = &asc::noHouse;
		lookups.cpInfoForCurrentMap = &asc::noCpInfo;
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
			for (Ref<gameserver::model::gameobjects::VisibleObject>& npc : spawned)
				npc->getController().delete_(); // World.removeObject answers false for an npc the case deleted already
			spawned.clear();
			envs.clear();
			for (int32_t mapId : {KARAMATIS, ATAXIAR}) {
				for (const Ptr<world::WorldMapInstance>& instance : *world::World::getInstance().getWorldMap(mapId)) {
					if (instance->getInstanceId() > 1)
						services::instance::InstanceService::destroyInstance(*instance);
				}
			}
			QuestEngine::getInstance().clear(); // the handlers themselves are Immortal (RT-11)
			services::cron::CronService::resetForTests();
			network::aion::serverpackets::detail::setPacketLookupsForTests(nullptr);
			gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(missingAiHandlers);
			configs::main::RatesConfig::QUEST_KINAH_RATES.set(savedQuestKinahRates);
			configs::main::RatesConfig::XP_QUEST_RATES.set(savedXpQuestRates);
			dataholders::DataManager::TITLE_DATA.resetForTests();
			dataholders::DataManager::ITEM_SET_DATA.resetForTests();
			dataholders::DataManager::ITEM_CLEAN_UP.resetForTests();
			dataholders::DataManager::ITEM_DATA.resetForTests();
			dataholders::DataManager::QUEST_DATA.resetForTests();
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

	/** Skips a case that creates an instance when no test database is configured */
	bool needDatabase() {
		if (!economy::test::isDatabaseEnabled())
			return false;
		asc::setUpAscensionDatabaseOnce();
		return true;
	}

	/** The installed DeterministicExecutor (InWorldPacketTest installs it and keeps no pointer) */
	static runtime::DeterministicExecutor& executor() {
		return dynamic_cast<runtime::DeterministicExecutor&>(*utils::ThreadPoolManager::installedBackend());
	}

	/**
	 * A connected character of the race and level, online, stored in the World and spawned at the place (the level is set before the Player
	 * exists, so no level change runs; QuestHandlerTestSupport.h's makeQuester and AscensionWorldTest's spawnActor together)
	 */
	void spawnActor(gameserver::model::Race race, int32_t level, int32_t mapId, float x, float y, float z) {
		cptest::PlayerFixture& pf = actor;
		pf.account = gameserver::model::account::Account::create(9411);
		pf.commonData = gameserver::model::gameobjects::player::PlayerCommonData::create(420001);
		pf.commonData->setName("Quester");
		pf.commonData->setRace(race);
		pf.commonData->setPlayerClass(gameserver::model::PlayerClass::WARRIOR);
		pf.commonData->setLevel(level);
		pf.appearance = gameserver::model::gameobjects::player::PlayerAppearance::create();
		pf.account->addPlayerAccountData(std::make_unique<gameserver::model::account::PlayerAccountData>(*pf.account, *pf.commonData, *pf.appearance));
		pf.account->setAccountWarehouse(std::make_unique<gameserver::model::items::storage::PlayerStorage>(*pf.account,
			gameserver::model::items::storage::StorageType::ACCOUNT_WAREHOUSE));
		pf.player = gameserver::model::gameobjects::VisibleObject::create<cptest::TestPlayer>(*pf.account->getPlayerAccountData(420001), *pf.account);
		pf.player->setKnownlist(std::make_unique<cptest::TestKnownList>(*pf.player));
		pf.player->setFriendList(std::make_unique<gameserver::model::gameobjects::player::FriendList>(*pf.player,
			std::vector<Ptr<gameserver::model::gameobjects::player::Friend>>{}));
		pf.player->setBlockList(gameserver::model::gameobjects::player::BlockList::create());
		pf.player->setPlayerSettings(gameserver::model::gameobjects::player::PlayerSettings::create());
		pf.player->setAbyssRank(gameserver::model::gameobjects::player::AbyssRank::create(0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0));
		pf.player->setEffectController(std::make_unique<controllers::effect::PlayerEffectController>(*pf.player));
		pf.player->setFlyController(std::make_unique<controllers::FlyController>(*pf.player));
		pf.player->setEmotions(std::make_unique<gameserver::model::gameobjects::player::emotion::EmotionList>(*pf.player));
		pf.player->setMotions(std::make_unique<gameserver::model::gameobjects::player::motion::MotionList>(*pf.player));
		pf.player->setQuestStateList(gameserver::model::gameobjects::player::QuestStateList::create());
		pf.player->setPosition(world::WorldPosition::create(mapId, x, y, z, int8_t{0}));
		pf.commonData->setOnline(true); // PlayerEnterWorldService
		world::World& world = world::World::getInstance();
		world.storeObject(*pf.player);
		ASSERT_TRUE(world.setPosition(Ptr<gameserver::model::gameobjects::VisibleObject>(*pf.player), mapId, 1, x, y, z, int8_t{0}));
		world.spawn(Ptr<gameserver::model::gameobjects::VisibleObject>(*pf.player));
		ASSERT_TRUE(pf.player->isSpawned());
		client = std::make_unique<cptest::TestClient>();
		client->enterWorld(pf);
		clearSent();
	}

	Player& player() { return *actor.player; }

	/** An npc of the template spawned into the World beside the player (AbstractQuestHandler.spawn: SpawnEngine and World.spawn) */
	Npc& spawnNpc(int32_t npcId, float dx = 2.0f, int8_t heading = 0) {
		Ptr<gameserver::model::gameobjects::VisibleObject> object =
			AbstractQuestHandler::spawn(npcId, *player().getWorldMapInstance(), player().getX() + dx, player().getY(), player().getZ(), heading);
		EXPECT_TRUE(object) << npcId;
		spawned.push_back(Ref<gameserver::model::gameobjects::VisibleObject>(object));
		return *runtime::cast<Npc>(object);
	}

	/** Hands the handler of the factory to the engine (addQuestHandler calls its register_); `handler` keeps it for the direct hook calls */
	void install(std::unique_ptr<AbstractQuestHandler> created) {
		handler = created.get();
		QuestEngine::getInstance().addQuestHandler(std::move(created));
	}

	/** A quest state as the DAO loads it (stored), in the player's list */
	Ref<QuestState> hold(int32_t questId, QuestStatus status, int32_t vars = 0) {
		int32_t completeCount = status == QuestStatus::COMPLETE ? 1 : 0;
		Ref<QuestState> qs = QuestState::create(questId, status, vars, 0, completeCount, std::nullopt, std::nullopt, std::nullopt);
		qs->setPersistentState(gameserver::model::gameobjects::Persistable::PersistentState::UPDATED);
		player().getQuestStateList()->addQuest(questId, *qs);
		return qs;
	}

	Ptr<QuestState> stateOf(int32_t questId) { return player().getQuestStateList()->getQuestState(questId); }

	int32_t varOf(int32_t questId) {
		Ptr<QuestState> qs = stateOf(questId);
		return qs ? qs->getQuestVarById(0) : -1;
	}

	/** An item of the template given the way the quests give them (ItemService through AbstractQuestHandler.giveQuestItem) */
	void give(int32_t itemId, int64_t count) {
		services::item::ItemService::addItem(player(), itemId, count, true);
	}

	int64_t held(int32_t itemId) { return player().getInventory().getItemCountByItemId(itemId); }

	/** QuestEngine.onDialog with a fresh env (CM_DIALOG_SELECT's way): target, quest and dialog action */
	bool dialog(Ptr<gameserver::model::gameobjects::VisibleObject> target, int32_t questId, int32_t dialogActionId) {
		Ref<QuestEnv> env = QuestEnv::create(target, player(), questId, dialogActionId);
		envs.push_back(env);
		return QuestEngine::getInstance().onDialog(*env);
	}

	bool kill(Npc& npc) {
		Ref<QuestEnv> env = QuestEnv::create(Ptr<gameserver::model::gameobjects::VisibleObject>(npc), player(), 0, 0);
		envs.push_back(env);
		return QuestEngine::getInstance().onKill(*env);
	}

	/** The handler's own onKillEvent answer (QuestEngine.onKill answers true whatever the handlers say) */
	bool killHook(Npc& npc) {
		Ref<QuestEnv> env = QuestEnv::create(Ptr<gameserver::model::gameobjects::VisibleObject>(npc), player(), 0, 0);
		envs.push_back(env);
		return handler->onKillEvent(*env);
	}

	/** The whole var field of the quest (QuestVars.getQuestVars: all slots) */
	int32_t rawVarsOf(int32_t questId) { return stateOf(questId)->getQuestVars()->getQuestVars(); }

	std::vector<std::vector<uint8_t>> sent() { return (*client)->sentBytes(); }

	void clearSent() { (*client)->clearSent(); }

	bool wasSent(const std::vector<uint8_t>& packet) {
		std::vector<std::vector<uint8_t>> bytes = sent();
		return std::find(bytes.begin(), bytes.end(), packet) != bytes.end();
	}

	std::vector<uint8_t> serializedFor(network::aion::AionServerPacket&& packet) { return cptest::serialized(std::move(packet), client->con()); }

	/** The 5 byte opcode header of a serialized packet: it identifies the packet class */
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
	/** The handler under test, owned by the engine (Immortal, RT-11) */
	AbstractQuestHandler* handler = nullptr;
	std::string missingAiHandlers;
	std::vector<float> savedQuestKinahRates;
	std::vector<float> savedXpQuestRates;
	std::deque<xml::LoadContext> contexts;
	cptest::PlayerFixture actor;
	std::unique_ptr<cptest::TestClient> client;
	std::vector<Ref<gameserver::model::gameobjects::VisibleObject>> spawned;
	/** The envs the cases handed the engine: a scheduled quest task pins its env, the fixture keeps them until the TearDown */
	std::vector<Ref<QuestEnv>> envs;
	network::aion::serverpackets::detail::PacketLookupsForTests lookups{};
};

} // namespace aion::gameserver::questEngine::handlers::zones::test
