#pragma once

// The fixture of chunk I1's instance handlers (docs/design/p6-instances-plan.md §3): the five instance maps of world_maps.xml in this process's
// World (published once, before the first World::getInstance(); geo data off, no zones, shields or materials), npc templates for the npc ids
// the handlers spawn, an instance created as InstanceService.getNextAvailableInstance creates one with a handler supplier (the marker's
// factory, then onInstanceCreate; the event spawns of the supplier path are left out: no event runs here), and a connected player standing
// in it. No database: nothing on these paths reads one.

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

namespace aion::gameserver::handlers::instance {

// the marker factories of the handlers under test (AION_INSTANCE_HANDLER defines them in the handler library)
::aion::gameserver::handlers::InstanceFactory AdmaStrongholdInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory DraupnirCaveInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory FireTempleInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory HaramelInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory LowerUdasTempleInstance_instanceFactory;

} // namespace aion::gameserver::handlers::instance

namespace aion::gameserver::handlers::instance::dredgion {

::aion::gameserver::handlers::InstanceFactory BaranathDredgionInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory ChantraDredgionInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory TerathDredgionInstance_instanceFactory;

} // namespace aion::gameserver::handlers::instance::dredgion

namespace aion::gameserver::handlers::instance::abyss {

::aion::gameserver::handlers::InstanceFactory KrotanBarracks_instanceFactory;
::aion::gameserver::handlers::InstanceFactory KysisBarracks_instanceFactory;
::aion::gameserver::handlers::InstanceFactory MirenBarracks_instanceFactory;
::aion::gameserver::handlers::InstanceFactory KrotanChamber_instanceFactory;
::aion::gameserver::handlers::InstanceFactory KysisChamber_instanceFactory;
::aion::gameserver::handlers::InstanceFactory MirenChamber_instanceFactory;
::aion::gameserver::handlers::InstanceFactory LegionsKrotanBarracks_instanceFactory;
::aion::gameserver::handlers::InstanceFactory LegionsKysisBarracks_instanceFactory;
::aion::gameserver::handlers::InstanceFactory LegionsMirenBarracks_instanceFactory;

::aion::gameserver::handlers::InstanceFactory AsteriaInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory RoahInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory RightWingChamberInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory LeftWingChamberInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory SulfurTreeNestInstance_instanceFactory;
::aion::gameserver::handlers::InstanceFactory EsoterraceInstance_instanceFactory;

} // namespace aion::gameserver::handlers::instance::abyss

namespace aion::gameserver::handlers::instance::test {

namespace cptest = ::aion::gameserver::network::aion::clientpackets::testing;

inline constexpr int32_t BARANATH_DREDGION = 300110000;
inline constexpr int32_t CHANTRA_DREDGION = 300210000;
inline constexpr int32_t TERATH_DREDGION = 300440000;
inline constexpr int32_t LOWER_UDAS_TEMPLE = 300160000;
inline constexpr int32_t HARAMEL = 300200000;
inline constexpr int32_t DRAUPNIR_CAVE = 320080000;
inline constexpr int32_t FIRE_TEMPLE = 320100000;
inline constexpr int32_t ADMA_STRONGHOLD = 320130000;

/** world_maps.xml's rows of the I1 maps under test (the Dredgions' child elements left out) */
inline constexpr const char* INSTANCE_MAPS_XML = R"x(<world_maps>
	<map id="300250000" cName="IDF4Re_Drana" name="Esoterrace" name_id="401750" instance="true" water_level="40" death_level="0" world_size="1536" drop_type="BALAUREA_INSTANCE" flags="RECALL GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300050000" cName="IDAbRe_Up_Asteria" name="Asteria Chamber" name_id="401201" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300060000" cName="IDAbRe_Low_Divine" name="Sulfur Tree Nest" name_id="401198" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300070000" cName="IDAbRe_Up_Rhoo" name="Chamber of Roah" name_id="401202" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300080000" cName="IDAbRe_Low_Wciel" name="Left Wing Chamber" name_id="401200" instance="true" water_level="70" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300090000" cName="IDAbRe_Low_Eciel" name="Right Wing Chamber" name_id="401199" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="512" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="301300000" cName="IDAbRe_Up3_Crotan_02_N" name="Krotan Barracks" name_id="404100" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301280000" cName="IDAbRe_Up3_Dkisas_02_N" name="Kysis Barracks" name_id="404101" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301290000" cName="IDAbRe_Up3_Lamiren_02_N" name="Miren Barracks" name_id="404102" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="300140000" cName="IDAbRe_Up3_Crotan" name="Krotan Chamber" name_id="401203" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300120000" cName="IDAbRe_Up3_Dkisas" name="Kysis Chamber" name_id="401204" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300130000" cName="IDAbRe_Up3_Lamiren" name="Miren Chamber" name_id="401205" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="301260000" cName="IDAbRe_Up3_Crotan_02" name="Legion's Krotan Barracks" name_id="404061" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301240000" cName="IDAbRe_Up3_Dkisas_02" name="Legion's Kysis Barracks" name_id="404062" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="301250000" cName="IDAbRe_Up3_Lamiren_02" name="Legion's Miren Barracks" name_id="404063" instance="true" water_level="16" death_level="0" world_type="ABYSS" world_size="1024" drop_type="ABYSS_INSTANCE" flags="RECALL PVP DUEL_SAME_RACE NO_RETURN_BATTLE"/>
	<map id="300110000" cName="IDAb1_Dreadgion" name="Dredgion" name_id="401193" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="DREDGION_INSTANCE" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300210000" cName="IDDreadgion_02" name="Chantra Dredgion" name_id="401675" instance="true" water_level="16" death_level="0" world_type="BALAUREA" world_size="1024" drop_type="DREDGION_INSTANCE" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300440000" cName="IDDreadgion_03" name="Terath Dredgion" name_id="401836" instance="true" water_level="16" death_level="0" world_type="BALAUREA" world_size="1024" drop_type="DREDGION_INSTANCE" except_buff="true" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300160000" cName="IDTemple_Low" name="Lower Udas Temple" name_id="401325" instance="true" water_level="16" death_level="0" world_size="2048" drop_type="BALAUREA_INSTANCE" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="300200000" cName="IDNovice" name="Haramel" name_id="401728" instance="true" water_level="16" death_level="0" world_size="512" drop_type="OTHER_INSTANCES" flags="GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="320080000" cName="IDDF3_Dragon" name="Draupnir Cave" name_id="401022" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="OTHER_INSTANCES" flags="RECALL GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="320100000" cName="IDDF2_Dflame" name="Fire Temple" name_id="400836" instance="true" water_level="16" death_level="0" world_size="512" drop_type="OTHER_INSTANCES" flags="RECALL GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
	<map id="320130000" cName="IDDf2a_Adma" name="Adma Stronghold" name_id="401152" instance="true" water_level="16" death_level="0" world_size="1024" drop_type="OTHER_INSTANCES" flags="RECALL GLIDE PVP DUEL_SAME_RACE NO_RETURN_BATTLE" pve_attack_ratio="150" pve_defend_ratio="50"/>
</world_maps>)x";

/** The npc ids the five handlers spawn or kill */
inline const std::vector<int32_t>& handlerNpcIds() {
	static const std::vector<int32_t> ids{205224, 212839, 212790, 212842, 212799, 212840, 212794, 212843, 212803, 212841, 212845, 214094, 214621, 212846,
		216922, 216920, 216921, 217067, 700950, 700829, 700830, 700831, 700832, 700852, 213587, 213588, 213771, 213773, 213776, 213778, 213779,
		213802, 213780, 216150, 216645, 216531, 216530,
		// the Dredgions
		215093, 214823, 700485, 700486, 700487, 700488, 700489, 700490, 700491, 700492, 700493, 700495, 700496, 700497, 700498, 700508, 700502,
		700506, 730214, 216886, 216889, 216890, 730349, 730314, 701441, 701442, 701455, 233377, 233371,
		// the I1 AIs' npcs: Icaronix the Betrayer, the duke's summons, an illusion gate (test id 900100) and the ids it adds, two plain npcs
		214599, 284978, 284979, 284980, 284981, 900100, 900101, 900102, 900103, 900200, 900300,
		// Krotan Barracks: the key masters, the bosses, the chests (and the legion version's), the timer npc, the doors, a door guard, the
		// artifact, the statues
		233627, 233628, 233629, 233630, 233632, 233633, 702288, 702289, 702290, 702291, 206095, 700545, 233599, 233600, 215413, 235536,
		// the five chambers with a start ring: the treasures the timer takes, the gold chests, Treasurer Nabatma, the exit
		700475, 700476, 700477, 701483, 701488, 700472, 700473, 700474, 701489, 701484, 700471, 701482, 701487, 700466, 701481, 701486, 214804,
		700463, 700462, 700464, 701485, 701480, 215424, 700455,
		// Esoterrace
		799580, 217649, 282295, 282291, 217204, 217205, 217289, 217281, 217195, 701027, 217185, 701023, 205437, 217206, 701025, 217282, 217283,
		217284};
	return ids;
}

/** One plain monster row per npc id (npc_templates.xml's shape, the attributes a spawn reads; no ai: a DummyAI) */
inline std::string handlerNpcTemplatesXml() {
	std::string xml = "<npc_templates>";
	for (int32_t id : handlerNpcIds())
		xml += R"(<npc_template npc_id=")" + std::to_string(id) +
			R"(" level="30" name="instance npc" name_id="1" height="1" rank="SEASONED" rating="NORMAL" race="DRAKAN" tribe="GENERAL">)"
			R"(<stats maxHp="1000"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="4.2" group_run_fight="4.2"/></stats>)"
			R"(<bound_radius front="0.5" side="0.5" upper="1"/></npc_template>)";
	return xml + "</npc_templates>";
}

/** Publishes the instance maps once per process, before the World is created from them */
inline void publishInstanceWorldOnce() {
	static const bool published = [] {
		configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
		configs::main::GeoDataConfig::CANSEE_ENABLE.store(false);
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		static std::deque<xml::LoadContext> contexts;
		dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(contexts.emplace_back(), INSTANCE_MAPS_XML));
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
inline runtime::Ptr<model::house::House> noHouse(model::gameobjects::player::Player&) {
	return nullptr;
}

inline runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo> noCpInfo(model::gameobjects::player::Player&) {
	return nullptr;
}

/** KnownList's protected pairing (tests/ai/AiWorldTestSupport.h's AiKnownListPairing): the unit tests run no known-list updates */
struct InstanceKnownListPairing : world::knownlist::KnownList {
	static bool pair(model::gameobjects::VisibleObject& a, model::gameobjects::VisibleObject& b) { return addPair(a, b); }
};

class InstanceHandlerTest : public cptest::InWorldPacketTest {
protected:
	void SetUp() override {
		cptest::InWorldPacketTest::SetUp();
		publishInstanceWorldOnce();
		missingAiHandlers = *configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn"); // no AI is registered in the test executables: a DummyAI instead
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, handlerNpcTemplatesXml()));
		// the aggro list asks the tribes' relations (AggroList.isAware): the npcs' GENERAL tribe is hostile to both player tribes
		lookups.activeHouseOfPlayer = &noHouse;
		lookups.cpInfoForCurrentMap = &noCpInfo;
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
		for (runtime::Ref<world::WorldMapInstance>& instance : instances)
			services::instance::InstanceService::destroyInstance(*instance);
		instances.clear();
	}

	/** InstanceService.getNextAvailableInstance(mapId, 0, 0, factory, 0, false) without the event spawns: the instance, then onInstanceCreate */
	world::WorldMapInstance& create(int32_t mapId, ::aion::gameserver::handlers::InstanceFactory& factory) {
		runtime::Ptr<world::WorldMap> map = world::World::getInstance().getWorldMap(mapId);
		runtime::Ref<world::WorldMapInstance> instance = world::WorldMapInstanceFactory::createWorldMapInstance(*map, 0,
			[&factory](world::WorldMapInstance& created) { return factory(created); }, 0);
		instance->getInstanceHandler()->onInstanceCreate();
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
		InstanceKnownListPairing::pair(npc, player());
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

} // namespace aion::gameserver::handlers::instance::test
