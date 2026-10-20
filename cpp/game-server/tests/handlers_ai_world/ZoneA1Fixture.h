#pragma once

// Lane C's fixture of A1's world AIs (the zone bosses and world NPCs: tests/handlers_ai_world/<Slice>A1AiTest.cpp), made from the instance
// fixture of I1-I6 (tests/handlers_inst_1/InstanceFixture.h), whose spawn paths the AIs share: AbstractAI::spawn goes through SpawnEngine into
// the World singleton, which tests/ai/AiWorldTestSupport.h's AiWorldTest (a map instance of its own, outside the World) does not reach. The
// zone maps of world_maps.xml in this process's World (published once, before the first World::getInstance(); geo data off, no zones,
// shields or materials), plain npc templates for the npc ids the AIs spawn, the zone map's main instance (the npcs a case
// spawns there are deleted after it), and a connected player standing in it. No database: nothing on these paths reads one.
// Its own names (ZoneAiTest, zoneMapsXml, ...), so the A1 test executable can also hold InstanceFixture.h's or AiWorldTestSupport.h's users.
// The World is built once per process from the first published maps: run each case in its own process (ctest does) or before any AiWorldTest.

#include "../cm_ak/InWorldPacketRunSupport.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapInstanceFactory.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/world/zone/ZoneUpdateService.h"

namespace aion::gameserver::handlers::ai::worlds::test {

namespace cptest = ::aion::gameserver::network::aion::clientpackets::testing;

/** The zone maps of world_maps.xml (defined in ZoneA1TestSupport.h) */
inline const char* zoneMapsXml();

/** The npc ids the AIs under test spawn, find or kill (defined in ZoneA1TestSupport.h) */
inline const std::vector<int32_t>& zoneNpcIds();

/** The ai name of an npc id's template, empty for none (defined in ZoneA1TestSupport.h) */
inline std::string zoneNpcAi(int32_t npcId);

/** One plain monster row per npc id (npc_templates.xml's shape, the attributes a spawn reads; no ai: a DummyAI) */
inline std::string zoneNpcTemplatesXml() {
	std::string xml = "<npc_templates>";
	for (int32_t id : zoneNpcIds())
		xml += R"(<npc_template npc_id=")" + std::to_string(id) +
			R"(" level="30" name="instance npc" name_id="1" height="1" rank="SEASONED" rating="NORMAL" race="DRAKAN" tribe="GENERAL")" +
			// a named AI the test executable has not gives a DummyAI<Npc> (zoneNpcAi)
			(zoneNpcAi(id).empty() ? std::string(">") : R"( ai=")" + zoneNpcAi(id) + R"(">)") +
			R"(<stats maxHp="1000"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2"/></stats>)"
			R"(<bound_radius front="0.5" side="0.5" upper="1"/></npc_template>)";
	return xml + "</npc_templates>";
}

/** Publishes the instance maps once per process, before the World is created from them */
inline void publishZoneWorldOnce() {
	static const bool published = [] {
		configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
		configs::main::GeoDataConfig::CANSEE_ENABLE.store(false);
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		static std::deque<xml::LoadContext> contexts;
		dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(contexts.emplace_back(), zoneMapsXml()));
		dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(contexts.emplace_back(), "<zones/>"));
		dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(contexts.emplace_back(), "<shields/>"));
		dataholders::DataManager::MATERIAL_DATA.publish(
			xml::bindString<dataholders::MaterialData>(contexts.emplace_back(), "<material_templates/>"));
		world::geo::GeoService::getInstance().init(); // geo data off: one empty GeoMap per map, as the server's startup
		// HousingService's singleton keeps reading it (the revive's teleport asks it): published once per process, never reset
		if (!dataholders::DataManager::HOUSE_DATA)
			dataholders::DataManager::HOUSE_DATA.publish(xml::bindString<dataholders::HouseData>(contexts.emplace_back(), "<house_lands/>"));
		return true;
	}();
	static_cast<void>(published);
}

/** The two reads of SM_PLAYER_INFO that need services these tests have not got (HousingService loads from the database) */
inline runtime::Ptr<model::house::House> zoneNoHouse(model::gameobjects::player::Player&) {
	return nullptr;
}

inline runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo> zoneNoCpInfo(model::gameobjects::player::Player&) {
	return nullptr;
}

/** KnownList's protected pairing (tests/ai/AiWorldTestSupport.h's AiKnownListPairing): the unit tests run no known-list updates */
struct ZoneKnownListPairing : world::knownlist::KnownList {
	static bool pair(model::gameobjects::VisibleObject& a, model::gameobjects::VisibleObject& b) { return addPair(a, b); }
};

class ZoneAiTest : public cptest::InWorldPacketTest {
protected:
	void SetUp() override {
		cptest::InWorldPacketTest::SetUp();
		publishZoneWorldOnce();
		missingAiHandlers = *configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn"); // no AI is registered in the test executables: a DummyAI instead
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, zoneNpcTemplatesXml()));
		// the aggro list asks the tribes' relations (AggroList.isAware): the npcs' GENERAL tribe is hostile to both player tribes
		lookups.activeHouseOfPlayer = &zoneNoHouse;
		lookups.cpInfoForCurrentMap = &zoneNoCpInfo;
		network::aion::serverpackets::detail::setPacketLookupsForTests(&lookups);
		// SM_QUEST_ACTION and the npcs' deaths ask the quest data: one row (quest_data.xml:65-67, RoahCustomInstanceHandlerTest's)
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context, R"x(<quests>
	<quest id="1006" name="Ascension" nameId="1102006" quest_zone="Ascension Quests" minlevel_permitted="9" max_repeat_count="1" cannot_share="true" cannot_giveup="true" race_permitted="ELYOS" category="MISSION">
		<rewards exp="73200"/>
	</quest>
</quests>)x"));
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(xml::bindString<dataholders::TribeRelationsData>(context, R"(<tribe_relations><tribe name="GENERAL"><hostile>PC PC_DARK</hostile></tribe><tribe name="PC"><none>GENERAL</none></tribe>)"
			R"(<tribe name="PC_DARK"><none>GENERAL</none></tribe></tribe_relations>)"));
	}

	void TearDown() override {
		leave();
		network::aion::serverpackets::detail::setPacketLookupsForTests(nullptr);
		configs::main::AIConfig::MISSING_AI_HANDLERS.set(missingAiHandlers);
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.resetForTests();
		cptest::InWorldPacketTest::TearDown();
	}

	/** The player out of the World and every instance of the case destroyed (TearDown's first half; a case may call it between rounds) */
	void leave() {
		if (actor.player) {
			actor.player->getController().cancelAllTasks();
			if (actor.player->isSpawned())
				world::World::getInstance().despawn(*actor.player);
			world::World::getInstance().removeObject(*actor.player);
			actor.player->setTarget(nullptr);
			actor.player->setClientConnection(nullptr);
		}
		client.reset();
		actor = {};
		// a deleted AI may spawn again (BollvigAI.handleDespawned brings 204655 back): delete until the instance is empty
		for (runtime::Ref<world::WorldMapInstance>& instance : instances) {
			for (int32_t pass = 0; pass < 10 && !npcs(*instance).empty(); pass++) {
				for (runtime::Ptr<model::gameobjects::Npc>& npc : npcs(*instance))
					npc->getController().delete_();
			}
			EXPECT_TRUE(npcs(*instance).empty()) << "npcs left in map " << instance->getMapId();
		}
		instances.clear();
	}

	/**
	 * The zone map's main instance, which the World created with its handler. A world map has no instances beyond its twin count
	 * (WorldMap::getWorldMapInstance), so the cases share it: leave() deletes the npcs a case spawned in it.
	 */
	world::WorldMapInstance& create(int32_t mapId) {
		runtime::Ptr<world::WorldMap> map = world::World::getInstance().getWorldMap(mapId);
		EXPECT_NE(map, nullptr) << "map " << mapId << " is not in this process's World (published first by another fixture?)";
		runtime::Ref<world::WorldMapInstance> instance(*map->getMainWorldMapInstance());
		instances.push_back(instance);
		return *instance;
	}

	/** A connected player of the class, stored in the World and spawned in the instance (AscensionWorldTest.spawnActor's way) */
	void enter(world::WorldMapInstance& instance, float x, float y, float z, model::PlayerClass playerClass = model::PlayerClass::WARRIOR) {
		actor = cptest::makePlayer(430001, 9431, "Delver");
		actor.commonData->setPlayerClass(playerClass);
		world::World& world = world::World::getInstance();
		world.storeObject(*actor.player);
		ASSERT_TRUE(world.setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player), instance.getMapId(), instance.getInstanceId(), x,
			y, z, int8_t{0}));
		world.spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
		ASSERT_TRUE(actor.player->isSpawned());
		client = std::make_unique<cptest::TestClient>();
		client->enterWorld(actor);
		(*client)->clearSent();
	}

	model::gameobjects::player::Player& player() { return *actor.player; }

	std::vector<std::vector<uint8_t>> sent() { return (*client)->sentBytes(); }

	int64_t countSent(const std::vector<uint8_t>& packet) {
		std::vector<std::vector<uint8_t>> bytes = sent();
		return std::count(bytes.begin(), bytes.end(), packet);
	}

	std::vector<uint8_t> serialized(network::aion::AionServerPacket&& packet) { return cptest::serialized(std::move(packet), client->con()); }

	/** The installed DeterministicExecutor (InWorldPacketTest installs it and keeps no pointer) */
	static runtime::DeterministicExecutor& executor() {
		return dynamic_cast<runtime::DeterministicExecutor&>(*utils::ThreadPoolManager::installedBackend());
	}

	/** What GeneralInstanceHandler.spawn does (protected there): a single-time spawn in the instance, as an npc */
	static runtime::Ptr<model::gameobjects::Npc> spawnNpc(world::WorldMapInstance& instance, int32_t npcId, float x, float y, float z, int8_t h) {
		runtime::Ref<model::templates::spawns::SpawnTemplate> spawn =
			spawnengine::SpawnEngine::newSingleTimeSpawn(instance.getMapId(), npcId, x, y, z, h);
		return runtime::as<model::gameobjects::Npc>(spawnengine::SpawnEngine::spawnObject(*spawn, instance.getInstanceId()));
	}

	/** The player hits the npc: they know each other (the aggro list's awareness), then the damage goes on the npc's aggro list */
	void hit(model::gameobjects::Npc& npc, int32_t damage = 100) {
		ZoneKnownListPairing::pair(npc, player());
		npc.getAggroList().addDamage(player(), damage, true, std::nullopt);
	}

	/** spawnNpc with a static id (GeneralInstanceHandler.spawn(npcId, x, y, z, h, staticId)) */
	static runtime::Ptr<model::gameobjects::Npc> spawnStaticNpc(world::WorldMapInstance& instance, int32_t npcId, float x, float y, float z,
	                                                            int32_t staticId) {
		runtime::Ref<model::templates::spawns::SpawnTemplate> spawn =
			spawnengine::SpawnEngine::newSingleTimeSpawn(instance.getMapId(), npcId, x, y, z, int8_t{0});
		spawn->setStaticId(staticId);
		return runtime::as<model::gameobjects::Npc>(spawnengine::SpawnEngine::spawnObject(*spawn, instance.getInstanceId()));
	}

	/** The npcs of the instance, in the instance's order */
	static std::vector<runtime::Ptr<model::gameobjects::Npc>> npcs(world::WorldMapInstance& instance) {
		std::vector<runtime::Ptr<model::gameobjects::Npc>> result;
		for (runtime::Ptr<model::gameobjects::VisibleObject> object : instance) {
			if (runtime::Ptr<model::gameobjects::Npc> npc = runtime::as<model::gameobjects::Npc>(object))
				result.push_back(npc);
		}
		return result;
	}

	static std::vector<runtime::Ptr<model::gameobjects::Npc>> npcs(world::WorldMapInstance& instance, int32_t npcId) {
		std::vector<runtime::Ptr<model::gameobjects::Npc>> result;
		for (runtime::Ptr<model::gameobjects::Npc>& npc : npcs(instance)) {
			if (npc->getNpcId() == npcId)
				result.push_back(npc);
		}
		return result;
	}

	xml::LoadContext context;
	network::aion::serverpackets::detail::PacketLookupsForTests lookups{};
	std::string missingAiHandlers;
	cptest::PlayerFixture actor;
	std::unique_ptr<cptest::TestClient> client;
	std::vector<runtime::Ref<world::WorldMapInstance>> instances;
};

} // namespace aion::gameserver::handlers::ai::worlds::test
