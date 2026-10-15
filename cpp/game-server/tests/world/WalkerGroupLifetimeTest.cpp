// World chunk (P4-10), spawnengine: what the walker formations keep of a walker npc that World.removeObject took out of the world. Leak
// investigation of the 2026-09-24 client session (docs/design/m5b3-client-session.md, S-1): the leak census warned that Npc 25582 was still
// alive 10 minutes after its removal from the world (refcount 1, no pinning task), and it was still alive at shutdown with no player online. It
// left the world at about 20:05:13 while the Mage hunted striped kerubs and juvenile sparkies around x 1007-1226, y 962-1222 of Poeta, where
// spawns/Npcs/210010000_Poeta.xml starts three walker routes:
// - F912DD8B (npc_walker.xml:69177, pool 2, SQUARE rows 1,1): juvenile sparkie 210115 (respawn 20 s) and striped kerub 210133 (respawn 15 s),
//   both at 1147.76/997.14 - a WalkerGroup;
// - 6C27EAF1: the same pair at 1210.93/1133.86 - a WalkerGroup like F912DD8B;
// - 3805A343 (npc_walker.xml:30806, pool 2, SQUARE rows 1,1): ONE juvenile sparkie at 1134.95/1012.56, the route's only spot - no group forms.
//
// The walker formations are static: WalkerFormationsCache -> WorldWalkerFormations -> InstanceWalkerFormations, whose walkFormations holds each
// WalkerGroup (members: ClusteredNpc objects, each with a Field<Ref<Npc>>) and whose groupedSpawnObjects holds every ClusteredNpc that
// WalkerFormator.processClusteredNpc ever cached - the group members and the lone walkers alike. Java clears the two maps only in
// onInstanceDestroy, and nothing destroys a world map's instance. A member npc is replaced only by WalkerGroup.respawn, when a respawn of the
// same npc id finds the member dead (WalkerGroup.java:236-251); a lone walker's ClusteredNpc is never replaced.
//
// What Java reads decides what the C++ may let go (docs/deviations/P4-10.md):
// - A group reads every member it holds (targetReached reads isDead() and the AI's sub state, respawn, setStep, getClusterData), so a removed
//   member stays referenced in Java and in C++ until a respawn replaces it - for good when none does: (b), (c), (d) are coverage of that
//   retention, not leaks the port may fix. The leak census reports such an npc, and WorldLeakProbe names the group (checked here).
// - groupedSpawnObjects is read only by organizeAndSpawn, once per instance. Java keeps the lone walker's ClusteredNpc there for good and never
//   reads it again, so its first npc - the corpse after its first kill - stayed referenced in C++ until shutdown: (e), the likely cause of S-1.
//   InstanceWalkerFormations.organizeAndSpawn now drops the candidate lists when it returns; (e) is the regression test.
// Each case spawns the three spots the server's way (SpawnEngine.spawnObject per spot: processClusteredNpc caches the walker; then
// WalkerFormator.organizeAndSpawn forms F912DD8B and spawns it and the lone sparkie), acts on one walker, lets RespawnService run (the corpse
// has no drop: the DecayTask deletes it 2 s after the death, RespawnService.IMMEDIATE_DECAY; the RespawnTask spawns the successor after the
// spawn group's respawn time) and asks what the leak census asks: is the removed npc's reference count down to the test's own?
//
// This executable links the empty AI registry, so every npc starts with AIEngine's DummyNpcAI, whose ask() answers false to every question (no
// respawn, no decay). The walker a case kills or removes gets PollAnsweringNpcAI: the answers of NpcAI.ask, which both npcs' real AI
// ("aggressive") uses unchanged.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <deque>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

#include "WorldTestSupport.h"

#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/WalkerData.bind.h"
#include "aion/gameserver/dataholders/WalkerData.h"
#include "aion/gameserver/dataholders/WalkerVersionsData.bind.h"
#include "aion/gameserver/dataholders/WalkerVersionsData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/services/RespawnService.h"
#include "aion/gameserver/services/RiftService.h"
#include "aion/gameserver/spawnengine/ClusteredNpc.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/spawnengine/WalkerFormator.h"
#include "aion/gameserver/spawnengine/WalkerGroup.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldLeakProbe.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::spawnengine::test {
namespace {

using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using model::templates::spawns::SpawnGroup;
using model::templates::spawns::SpawnTemplate;
using runtime::Ptr;
using runtime::Ref;
using services::RespawnService;
using world::World;
using world::test::POETA;

constexpr int32_t JUVENILE_SPARKIE_NPC_ID = 210115;
constexpr int32_t STRIPED_KERUB_NPC_ID = 210133;
constexpr std::string_view GROUP_ROUTE = "F912DD8BEF6BA40288166F8A9136151966300044";
constexpr std::string_view LONE_ROUTE = "3805A3432D75C378A149FF2D9B9B25C6AD9ED350";
/** SpawnEngine.spawnAll spawns a world map's instances; the main one is instance 1 */
constexpr int32_t INSTANCE = 1;

/**
 * world_maps.xml:11, the real Poeta row (3072 m: the routes lie beyond the 1024 m test Poeta of WorldTestSupport.h) without twin_count and
 * beginner_twin_count, so World creates one instance of it
 */
const char* const POETA_WORLD_MAPS_XML =
	R"(<world_maps>)"
	R"(<map id="210010000" cName="LF1" name="Poeta" name_id="400234" max_user="200" water_level="100" death_level="0" world_type="ELYSEA")"
	R"( world_size="3072" drop_type="ELYSEA" flags="BIND RECALL GLIDE PVP DUEL_SAME_RACE" pve_attack_ratio="150" pve_defend_ratio="50"/>)"
	R"(</world_maps>)";

/** npcs/npc_templates.xml:54427-54432 and :54505-54510, verbatim */
const char* const NPC_TEMPLATES_XML =
	R"(<npc_templates>)"
	R"(<npc_template npc_id="210115" level="1" name="juvenile sparkie" name_id="300097" height="1.02" group_drop="SPAKY" rank="DISCIPLINED" rating="NORMAL" race="BEAST" tribe="MONSTER" ai="aggressive" srange="8" sangle="240" arange="2" attack_speed="2142" hpgauge="3" floatcorpse="true">)"
	R"(<stats maxHp="143">)"
	R"(<speeds walk="2" group_walk="2" run="7" run_fight="5" group_run_fight="7" />)"
	R"(</stats>)"
	R"(<bound_radius front="0.55" side="0.56" upper="2.82" />)"
	R"(</npc_template>)"
	R"(<npc_template npc_id="210133" level="1" name="striped kerub" name_id="300110" height="1.372" group_drop="CHERUBIM" rank="DISCIPLINED" rating="NORMAL" race="MAGICALMONSTER" tribe="MONSTER" type="MONSTER" ai="aggressive" srange="7" sangle="240" arange="2" attack_speed="2100" hpgauge="3">)"
	R"(<stats maxHp="143">)"
	R"(<speeds walk="0.6" group_walk="0.6" run="7" run_fight="5.5" group_run_fight="7" />)"
	R"(</stats>)"
	R"(<bound_radius front="0.525" side="0.275" upper="1.372" />)"
	R"(</npc_template>)"
	R"(</npc_templates>)";

/** npc_walker/npc_walker.xml:30806-30849 (3805A343) and :69177-69194 (F912DD8B), verbatim */
const char* const WALKER_TEMPLATES_XML =
	R"(<npc_walker>)"
	R"(<walker_template route_id="3805A3432D75C378A149FF2D9B9B25C6AD9ED350" pool="2" formation="SQUARE" rows="1,1">)"
	R"(<routestep x="1134.95" y="1012.56" z="130.13249"/>)"
	R"(<routestep x="1130.24" y="1013.74" z="128.95137"/>)"
	R"(<routestep x="1126.06" y="1016.19" z="128.01422"/>)"
	R"(<routestep x="1122.05" y="1019.56" z="126.79314"/>)"
	R"(<routestep x="1118.88" y="1021.54" z="126.11"/>)"
	R"(<routestep x="1116.16" y="1020.56" z="125.975006"/>)"
	R"(<routestep x="1112.39" y="1015.62" z="126.1725"/>)"
	R"(<routestep x="1107.53" y="1012.55" z="126.09563"/>)"
	R"(<routestep x="1102.71" y="1012.27" z="126.17813"/>)"
	R"(<routestep x="1098.45" y="1014.15" z="127.07813"/>)"
	R"(<routestep x="1095.96" y="1018.54" z="127.348755"/>)"
	R"(<routestep x="1095.72" y="1021.70" z="127.21376"/>)"
	R"(<routestep x="1094.75" y="1026.69" z="127.859375"/>)"
	R"(<routestep x="1095.77" y="1030.61" z="127.438126"/>)"
	R"(<routestep x="1098.61" y="1032.19" z="126.55002"/>)"
	R"(<routestep x="1101.95" y="1031.85" z="126.25938"/>)"
	R"(<routestep x="1103.60" y="1028.56" z="126.275"/>)"
	R"(<routestep x="1103.20" y="1025.33" z="126.44189"/>)"
	R"(<routestep x="1100.93" y="1023.23" z="126.702354"/>)"
	R"(<routestep x="1097.93" y="1023.35" z="126.965965"/>)"
	R"(<routestep x="1095.77" y="1025.50" z="127.31504"/>)"
	R"(<routestep x="1095.43" y="1028.34" z="127.43676"/>)"
	R"(<routestep x="1096.85" y="1031.23" z="127.27814"/>)"
	R"(<routestep x="1099.17" y="1032.08" z="126.4375"/>)"
	R"(<routestep x="1102.01" y="1031.34" z="126.29063"/>)"
	R"(<routestep x="1103.65" y="1028.73" z="126.27187"/>)"
	R"(<routestep x="1103.14" y="1025.39" z="126.44937"/>)"
	R"(<routestep x="1100.92" y="1023.50" z="126.9425"/>)"
	R"(<routestep x="1097.80" y="1023.64" z="126.975975"/>)"
	R"(<routestep x="1095.76" y="1025.56" z="127.56001"/>)"
	R"(<routestep x="1095.24" y="1028.33" z="127.47427"/>)"
	R"(<routestep x="1096.38" y="1030.95" z="127.119934"/>)"
	R"(<routestep x="1099.98" y="1032.29" z="126.23312"/>)"
	R"(<routestep x="1102.00" y="1031.32" z="126.2925"/>)"
	R"(<routestep x="1103.58" y="1029.82" z="126.27625"/>)"
	R"(<routestep x="1104.61" y="1027.49" z="126.211876"/>)"
	R"(<routestep x="1106.27" y="1022.31" z="126.09125"/>)"
	R"(<routestep x="1112.52" y="1020.30" z="125.98125"/>)"
	R"(<routestep x="1118.15" y="1018.60" z="126.15313"/>)"
	R"(<routestep x="1125.77" y="1017.25" z="128.1"/>)"
	R"(<routestep x="1131.60" y="1015.38" z="129.1525"/>)"
	R"(<routestep x="1134.99" y="1012.63" z="130.12936"/>)"
	R"(</walker_template>)"
	R"(<walker_template route_id="F912DD8BEF6BA40288166F8A9136151966300044" pool="2" formation="SQUARE" rows="1,1">)"
	R"(<routestep x="1147.76" y="997.14" z="135.315"/>)"
	R"(<routestep x="1141.19" y="999.97" z="133.86107"/>)"
	R"(<routestep x="1135.26" y="1004.43" z="130.35251"/>)"
	R"(<routestep x="1124.38" y="1008.05" z="128.69626"/>)"
	R"(<routestep x="1114.44" y="1001.72" z="127.24249"/>)"
	R"(<routestep x="1107.75" y="997.93" z="126.50557"/>)"
	R"(<routestep x="1101.68" y="1000.85" z="126.20736"/>)"
	R"(<routestep x="1100.91" y="1006.83" z="126.200554"/>)"
	R"(<routestep x="1109.60" y="1010.66" z="125.969246"/>)"
	R"(<routestep x="1115.90" y="1009.90" z="127.100006"/>)"
	R"(<routestep x="1124.60" y="1009.35" z="128.73749"/>)"
	R"(<routestep x="1136.24" y="1008.67" z="131.2825"/>)"
	R"(<routestep x="1145.32" y="1010.02" z="132.53374"/>)"
	R"(<routestep x="1149.10" y="1008.36" z="133.4225"/>)"
	R"(<routestep x="1151.46" y="999.69" z="135.64243"/>)"
	R"(<routestep x="1147.79" y="997.21" z="135.32251"/>)"
	R"(</walker_template>)"
	R"(</npc_walker>)";

/**
 * Publishes this file's world data once per process: the real-size Poeta, no zones, shields or materials. World reads its maps once, so the
 * file shares a process only with tests that accept a 3072 m Poeta: publishedData becomes REAL, which makes publishTestStaticData() answer
 * false (its tests skip, "run the test on its own") and WorldRealDataTest skip - and WorldRealDataTest's real maps serve this file as well.
 *
 * @return false if the process published WorldTestSupport.h's 1024 m test maps
 */
bool publishWalkerWorldData() {
	std::scoped_lock lock(world::test::publishedDataMutex);
	if (world::test::publishedData == world::test::PublishedData::TEST)
		return false;
	if (world::test::publishedData == world::test::PublishedData::NONE) {
		configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
		configs::main::WorldConfig::WORLD_MAX_TWINS_USUAL.store(0);
		configs::main::WorldConfig::WORLD_MAX_TWINS_BEGINNER.store(0);
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		static std::deque<xml::LoadContext> contexts;
		dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(contexts.emplace_back(), POETA_WORLD_MAPS_XML));
		dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(contexts.emplace_back(), "<zones/>"));
		dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(contexts.emplace_back(), "<shields/>"));
		dataholders::DataManager::MATERIAL_DATA.publish(
			xml::bindString<dataholders::MaterialData>(contexts.emplace_back(), "<material_templates/>"));
		world::test::publishedData = world::test::PublishedData::REAL;
	}
	return true;
}

/**
 * The answers NpcAI.ask (NpcAI.java:141-165) gives both walkers' real AggressiveNpcAI (neither it nor GeneralNpcAI overrides ask):
 * ALLOW_RESPAWN is SiegeService.isRespawnAllowed, true for an Npc that is not a SiegeNpc; ALLOW_DECAY is true. The reward questions are answered
 * false, so the corpse gets no drop and RespawnService.scheduleDecayTask takes IMMEDIATE_DECAY (2 s) - the kill S-1 describes. `allowRespawn`
 * false stands for a respawn the npc's AI refuses. The hooks are AIEngine's DummyNpcAI's (empty): no walking, no aggro.
 */
class PollAnsweringNpcAI final : public ai::NpcAI {
public:
	PollAnsweringNpcAI(Npc& owner, bool allowRespawn) : NpcAI(owner), allowRespawn(allowRespawn) {}

	bool ask(ai::poll::AIQuestion question) override {
		switch (question) {
			case ai::poll::AIQuestion::ALLOW_RESPAWN:
				return allowRespawn;
			case ai::poll::AIQuestion::ALLOW_DECAY:
				return true;
			default:
				return false;
		}
	}

	bool isDestinationReached() override { return false; }

protected:
	void handleActivate() override {}

	void handleDeactivate() override {}

	void handleBeforeSpawned() override {}

	void handleSpawned() override {}

	void handleDespawned() override {}

	void handleDied() override {}

	void handleMoveArrived() override {}

	void handleTargetChanged(model::gameobjects::Creature& creature) override {}

	void handleMoveValidate() override {}

	void handleCreatureMoved(model::gameobjects::Creature& creature) override {}

private:
	const bool allowRespawn;
};

class WalkerGroupLifetimeTest : public ::testing::Test {
protected:
	void SetUp() override {
		if (!publishWalkerWorldData())
			GTEST_SKIP() << "this process published WorldTestSupport.h's 1024 m test Poeta (run the test on its own)";
		prepared = true;
		missingAiHandlers = *configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn"); // "aggressive" is not registered in this executable: DummyNpcAI instead
		riftEnabled = configs::main::CustomConfig::RIFT_ENABLED.load();
		configs::main::CustomConfig::RIFT_ENABLED.store(false);
		utils::ThreadPoolManager::installBackend(nullptr);
		executor = new runtime::DeterministicExecutor(clock, 4);
		utils::ThreadPoolManager::installBackend(std::unique_ptr<runtime::DeterministicExecutor>(executor));
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		// RespawnTask.respawn -> RiftService.updateSpawned reads the rift locations the server's startup creates (none with rifts disabled)
		if (!services::RiftService::getInstance().getRiftLocations())
			services::RiftService::getInstance().initRiftLocations();
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, NPC_TEMPLATES_XML));
		dataholders::DataManager::WALKER_DATA.publish(xml::bindString<dataholders::WalkerData>(context, WALKER_TEMPLATES_XML));
		// WalkerTemplate.getVersionId asks walker_versions.xml, which names neither route
		dataholders::DataManager::WALKER_VERSIONS_DATA.publish(
			xml::bindString<dataholders::WalkerVersionsData>(context, "<walker_versions/>"));
		WalkerFormator::onInstanceDestroy(POETA, INSTANCE); // the formations are static: forget an earlier test's

		// spawns/Npcs/210010000_Poeta.xml: <spawn npc_id="210115" respawn_time="20"> (:681) spots :683 and :684, <spawn npc_id="210133"
		// respawn_time="15"> (:1224) spot :1238 - the three spots with a walker_id in the hunting area
		sparkieGroup = SpawnGroup::create(POETA, JUVENILE_SPARKIE_NPC_ID, 20, nullptr);
		kerubGroup = SpawnGroup::create(POETA, STRIPED_KERUB_NPC_ID, 15, nullptr);
		loneSparkieSpot = SpawnTemplate::create(*sparkieGroup, 1134.95f, 1012.56f, 130.13249f, int8_t{54}, 0, LONE_ROUTE, 0);
		groupSparkieSpot = SpawnTemplate::create(*sparkieGroup, 1147.76f, 997.14f, 135.315f, int8_t{55}, 0, GROUP_ROUTE, 0);
		groupKerubSpot = SpawnTemplate::create(*kerubGroup, 1147.76f, 997.14f, 135.315f, int8_t{62}, 0, GROUP_ROUTE, 0);

		// SpawnEngine.spawnInstance: spawnObject per spot (in the file's order), then WalkerFormator.organizeAndSpawn
		loneSparkie = spawnWalker(*loneSparkieSpot);
		groupSparkie = spawnWalker(*groupSparkieSpot);
		kerub = spawnWalker(*groupKerubSpot);
		ASSERT_TRUE(loneSparkie && groupSparkie && kerub);
		ASSERT_FALSE(loneSparkie->isSpawned() || groupSparkie->isSpawned() || kerub->isSpawned())
			<< "processClusteredNpc delays the spawn of a walker whose route has a pool of 2";
		WalkerFormator::organizeAndSpawn(POETA, INSTANCE);
		ASSERT_TRUE(loneSparkie->isSpawned() && groupSparkie->isSpawned() && kerub->isSpawned());
		ASSERT_FALSE(loneSparkie->getWalkerGroup()) << "3805A343 has one spot: organizeAndSpawn spawns the lone walker without a group";
		group = Ref<WalkerGroup>(kerub->getWalkerGroup());
		ASSERT_TRUE(group) << "F912DD8B: both spots share one position, organizeAndSpawn forms the group";
		ASSERT_EQ(groupSparkie->getWalkerGroup().get(), group.get());
		ASSERT_EQ(group->getPool(), 2);
		kerubCluster = Ref<ClusteredNpc>(group->getClusterData(*kerub));
		sparkieCluster = Ref<ClusteredNpc>(group->getClusterData(*groupSparkie));
		ASSERT_TRUE(kerubCluster && sparkieCluster);
		ASSERT_EQ(kerubCluster->getNpc().get(), kerub.get());
	}

	void TearDown() override {
		if (!prepared)
			return;
		{
			runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
			RespawnService::cancelRespawns([](SpawnTemplate&) { return true; });
			World& world = World::getInstance();
			for (Ptr<VisibleObject> object : *world.getWorldMap(POETA)->getMainWorldMapInstance())
				world.removeObject(*object);
			WalkerFormator::onInstanceDestroy(POETA, INSTANCE);
		}
		kerubCluster = nullptr;
		sparkieCluster = nullptr;
		group = nullptr;
		kerub = nullptr;
		groupSparkie = nullptr;
		loneSparkie = nullptr;
		groupKerubSpot = nullptr;
		groupSparkieSpot = nullptr;
		loneSparkieSpot = nullptr;
		kerubGroup = nullptr;
		sparkieGroup = nullptr;
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		executor = nullptr;
		runtime::Reclaimer::getInstance().drain();
		dataholders::DataManager::WALKER_VERSIONS_DATA.resetForTests();
		dataholders::DataManager::WALKER_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
		configs::main::CustomConfig::RIFT_ENABLED.store(riftEnabled);
		configs::main::AIConfig::MISSING_AI_HANDLERS.set(missingAiHandlers);
	}

	/** SpawnEngine.spawnObject of one spot: VisibleObjectSpawner.spawnNpc -> WalkerFormator.processClusteredNpc */
	static Ref<Npc> spawnWalker(SpawnTemplate& spot) {
		Ptr<VisibleObject> object = SpawnEngine::spawnObject(spot, INSTANCE);
		return Ref<Npc>(runtime::as<Npc>(object));
	}

	/**
	 * Kills the walker directly through its controller (CreatureController.die: reduceHp to 0 -> NpcController.onDie, which asks the AI whether
	 * to respawn, then schedules the decay)
	 */
	static void kill(Npc& npc, bool allowRespawn = true) {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		npc.replaceAi(std::make_unique<PollAnsweringNpcAI>(npc, allowRespawn));
		ASSERT_TRUE(npc.getController().die());
	}

	/** runs every task that falls due in the next `millis` ms and destroys what they released */
	void advance(int64_t millis) {
		executor->advance(std::chrono::milliseconds(millis));
		runtime::Reclaimer::getInstance().drain();
		runtime::Reclaimer::getInstance().drain();
	}

	static bool isInWorld(Npc& npc) {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		return World::getInstance().findVisibleObject(npc.getObjectId()).get() == &npc;
	}

	static bool hasRespawnTask(Npc& npc) {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		return RespawnService::hasRespawnTask(npc);
	}

	/** the objects of Poeta's main instance whose spawn template is `spot` */
	static int32_t spawnedFrom(SpawnTemplate& spot) {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		int32_t count = 0;
		for (Ptr<VisibleObject> object : *World::getInstance().getWorldMap(POETA)->getMainWorldMapInstance())
			count += object->getSpawn().get() == &spot ? 1 : 0;
		return count;
	}

	/** every structure that could still reference the npc, for the failure message (the pattern of NpcCastDeathLifetimeTest.holders) */
	std::string holders(Npc& npc) {
		std::ostringstream out;
		out << "refCount " << npc.refCount();
		if (World::getInstance().findVisibleObject(npc.getObjectId()).get() == &npc)
			out << "; World.allObjects still holds it";
		if (npc.getPosition() && npc.getPosition()->getMapRegion() &&
			npc.getPosition()->getMapRegion()->getObjects().get(npc.getObjectId()).get() == &npc)
			out << "; its MapRegion.objects still holds it";
		const std::pair<const char*, const Ref<ClusteredNpc>*> members[] = {{"kerub", &kerubCluster}, {"sparkie", &sparkieCluster}};
		for (const auto& [name, cluster] : members) {
			if (*cluster && (*cluster)->getNpc().get() == &npc)
				out << "; ClusteredNpc.npc of the " << name << " member of WalkerGroup F912DD8B (WalkerFormationsCache -> WorldWalkerFormations -> "
					<< "InstanceWalkerFormations.walkFormations -> WalkerGroup.members)";
		}
		if (npc.getWalkerGroup())
			out << "; its own Npc.walkerGroup is set (a group <-> npc cycle)";
		if (RespawnService::hasRespawnTask(npc))
			out << "; RespawnService has a pending RespawnTask for its object id";
		for (Ptr<VisibleObject> object : *World::getInstance().getWorldMap(POETA)->getMainWorldMapInstance()) {
			if (object.get() == &npc)
				continue;
			if (object->getKnownList().getObject(npc.getObjectId()).get() == &npc)
				out << "; " << object->toString() << ".knownList knows it";
			if (object->getTarget().get() == &npc)
				out << "; the target of " << object->toString();
		}
		return out.str();
	}

	/** what WorldLeakProbe (the leak census holder probe) finds for the npc */
	static std::string probed(Npc& npc) {
		std::ostringstream out;
		for (const std::string& holder : world::WorldLeakProbe::findHolders(npc))
			out << "[" << holder << "]";
		return out.str();
	}

	/** checks, outside any task, that the only reference left to the removed npc is the test's own */
	void expectReleased(Npc& npc, const std::string& what) {
		runtime::Reclaimer::getInstance().drain();
		runtime::Reclaimer::getInstance().drain();
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		std::string found = holders(npc);
		std::cout << "[walker-leak] " << what << ": " << found << std::endl;
		EXPECT_EQ(npc.refCount(), 1u) << what << ": the npc is still referenced after its removal from the world - " << found;
		EXPECT_EQ(probed(npc), "") << what << ": the leak probe finds no holder";
	}

	/**
	 * Java's retention, outside any task: the removed npc is still the member of `cluster` in F912DD8B, which reads it as Java's group does, and
	 * that ClusteredNpc is its only holder besides the test (refcount 2); the leak probe names the group, reached through `via`.
	 */
	void expectHeldByTheGroup(Npc& npc, ClusteredNpc& cluster, const std::string& via, const std::string& what) {
		runtime::Reclaimer::getInstance().drain();
		runtime::Reclaimer::getInstance().drain();
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		std::string found = holders(npc);
		std::cout << "[walker-leak] " << what << ": " << found << std::endl;
		EXPECT_EQ(cluster.getNpc().get(), &npc) << what << ": the group still has the removed npc as its member, as Java's does";
		EXPECT_EQ(npc.refCount(), 2u) << what << ": the test's reference and ClusteredNpc.npc, nothing else - " << found;
		EXPECT_EQ(probed(npc), "[ClusteredNpc.npc of a member of WalkerGroup " + std::string(GROUP_ROUTE) + " (" + via +
								   " -> WalkerGroup.members; Java's group keeps and reads it too, until a respawn replaces a dead member)]")
			<< what;
	}

	static constexpr const char* THROUGH_THE_CACHE = "WalkerFormationsCache -> InstanceWalkerFormations.walkFormations";
	static constexpr const char* THROUGH_ITS_OWN_GROUP = "its own Npc.walkerGroup, a group <-> npc cycle";

	runtime::ManualClock clock{0};
	runtime::DeterministicExecutor* executor = nullptr; // owned by ThreadPoolManager
	xml::LoadContext context;
	bool prepared = false;
	std::string missingAiHandlers;
	bool riftEnabled = false;
	Ref<SpawnGroup> sparkieGroup;
	Ref<SpawnGroup> kerubGroup;
	Ref<SpawnTemplate> loneSparkieSpot;
	Ref<SpawnTemplate> groupSparkieSpot;
	Ref<SpawnTemplate> groupKerubSpot;
	Ref<Npc> loneSparkie;
	Ref<Npc> groupSparkie;
	Ref<Npc> kerub;
	Ref<WalkerGroup> group;
	Ref<ClusteredNpc> kerubCluster;
	Ref<ClusteredNpc> sparkieCluster;
};

/**
 * (a) The server's everyday path: the kerub dies, its corpse decays at +2 s, the respawn at +15 s finds the member dead
 * (WalkerGroup.respawn) and replaces it with the new kerub, which re-enters the group at route step max(0, groupStep - 1). Until then the
 * group holds the corpse, as Java's does (13 s, far below the census' 10 minutes).
 */
TEST_F(WalkerGroupLifetimeTest, ARespawnReplacesTheDeadKerubInItsGroupAndReleasesTheCorpse) {
	kill(*kerub);
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(hasRespawnTask(*kerub)) << "NpcController.onDie scheduled the respawn (15 s)";
	advance(2100);
	ASSERT_FALSE(isInWorld(*kerub)) << "the DecayTask deleted the corpse (World.removeObject)";
	advance(13000); // +15.1 s: the RespawnTask ran
	Ref<Npc> successor;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		successor = Ref<Npc>(kerubCluster->getNpc());
		ASSERT_NE(successor.get(), kerub.get()) << "WalkerGroup.respawn replaced the dead member (ClusteredNpc.setNpc)";
		EXPECT_EQ(successor->getNpcId(), STRIPED_KERUB_NPC_ID);
		EXPECT_TRUE(successor->isSpawned()) << "ClusteredNpc.spawn brought the new kerub into the world";
		EXPECT_EQ(successor->getWalkerGroup().get(), group.get()) << "processClusteredNpc: npc.setWalkerGroup(wg)";
		std::cout << "[walker-leak] (a) the new kerub " << successor->toString() << " at " << successor->getX() << "/" << successor->getY()
				  << ", group step " << group->getGroupStep() << std::endl;
	}
	expectReleased(*kerub, "(a) killed, decayed and replaced by its respawn");
}

/**
 * (b) The respawn is denied: NpcController.onDie asks ALLOW_RESPAWN and the npc's AI says no, so no respawn ever replaces the member. Java keeps
 * the corpse in the group and reads it whenever the sparkie reaches a route step (WalkerGroup.targetReached: isDead()), so the C++ keeps it too.
 * Not a live path in Poeta: both walkers' AI is NpcAI.ask, whose ALLOW_RESPAWN is true for a plain npc.
 */
TEST_F(WalkerGroupLifetimeTest, AKerubWhoseRespawnIsDeniedStaysInItsGroupAfterItsCorpseDecayed) {
	kill(*kerub, false);
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_FALSE(hasRespawnTask(*kerub)) << "no respawn scheduled";
	advance(2100);
	ASSERT_FALSE(isInWorld(*kerub)) << "the DecayTask deleted the corpse";
	advance(60000);
	expectHeldByTheGroup(*kerub, *kerubCluster, THROUGH_THE_CACHE, "(b) killed with its respawn denied (ALLOW_RESPAWN false), decayed");
}

/**
 * (b) The respawn is skipped: the pending RespawnTask is cancelled after the corpse decayed (RespawnService.cancelRespawn, which
 * VisibleObjectController.deleteIfAliveOrCancelRespawn does for a dead npc - ClusteredNpc.despawn, a GM despawn).
 */
TEST_F(WalkerGroupLifetimeTest, AKerubWhoseRespawnIsCancelledStaysInItsGroupAfterItsCorpseDecayed) {
	kill(*kerub);
	ASSERT_FALSE(HasFatalFailure());
	advance(2100);
	ASSERT_FALSE(isInWorld(*kerub));
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		RespawnService::cancelRespawn(*kerub);
	}
	ASSERT_FALSE(hasRespawnTask(*kerub));
	advance(60000);
	expectHeldByTheGroup(*kerub, *kerubCluster, THROUGH_THE_CACHE, "(b) killed, decayed, its respawn cancelled");
}

/**
 * (c) The member is removed while alive (World.removeObject without a death, e.g. AIActions.deleteOwner): no respawn is scheduled. Java's group
 * keeps the npc and waits for it (targetReached reads its AI's sub state), so the C++ keeps it too.
 */
TEST_F(WalkerGroupLifetimeTest, AKerubRemovedAliveStaysInItsGroup) {
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		ASSERT_TRUE(kerub->getController().delete_());
	}
	ASSERT_FALSE(isInWorld(*kerub));
	advance(60000);
	expectHeldByTheGroup(*kerub, *kerubCluster, THROUGH_THE_CACHE, "(c) removed alive");
}

/**
 * (c) The member is removed alive and its respawn scheduled (VisibleObjectController.deleteAndScheduleRespawn): the respawn at +15 s finds no
 * DEAD kerub in the group, so WalkerGroup.respawn neither replaces the member nor spawns the new kerub - it never enters the world.
 */
TEST_F(WalkerGroupLifetimeTest, AKerubRemovedAliveWithARespawnIsNotReplacedAndItsSuccessorNeverSpawns) {
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		kerub->getController().deleteAndScheduleRespawn();
	}
	ASSERT_FALSE(isInWorld(*kerub));
	ASSERT_TRUE(hasRespawnTask(*kerub));
	advance(15100);
	EXPECT_FALSE(hasRespawnTask(*kerub)) << "the RespawnTask ran";
	EXPECT_EQ(spawnedFrom(*groupKerubSpot), 0) << "the member is alive, so WalkerGroup.respawn matched nothing and did not spawn the new kerub";
	expectHeldByTheGroup(*kerub, *kerubCluster, THROUGH_THE_CACHE, "(c) removed alive with a respawn that could not replace it");
}

/**
 * (d) The whole group despawns (WalkerGroup.despawn, InstanceWalkerFormations.changeCluster's): the living sparkie is removed
 * (deleteIfAliveOrCancelRespawn -> delete), the dead kerub's pending respawn cancelled. Both stay members, so that WalkerGroup.spawn can
 * bring the same npcs back, and form() links each removed member to the group again after LogoutBreakers D5 cut Npc.walkerGroup. This is
 * coverage of code the server never runs: WalkerGroup.despawn's only caller is changeCluster, whose only entry, WalkerFormator.changeWalkerGroup,
 * has no caller in Java or C++ (and none of the 12 version ids of walker_versions.xml is a route of the walker data, so formationVariants and
 * walkerVariants stay empty). The group <-> member cycle it leaves after WalkerFormator.onInstanceDestroy therefore never forms in play; the
 * leak probe names it.
 */
TEST_F(WalkerGroupLifetimeTest, ADespawnedGroupKeepsItsRemovedMembers) {
	kill(*kerub);
	ASSERT_FALSE(HasFatalFailure());
	advance(2100);
	ASSERT_FALSE(isInWorld(*kerub));
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		group->despawn();
	}
	ASSERT_FALSE(isInWorld(*groupSparkie)) << "the living sparkie was removed";
	ASSERT_FALSE(hasRespawnTask(*kerub)) << "the dead kerub's respawn was cancelled";
	advance(60000);
	expectHeldByTheGroup(*kerub, *kerubCluster, THROUGH_THE_CACHE, "(d) group despawned: the dead kerub");
	expectHeldByTheGroup(*groupSparkie, *sparkieCluster, THROUGH_THE_CACHE, "(d) group despawned: the living sparkie");
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		EXPECT_EQ(kerub->getWalkerGroup().get(), group.get()) << "form() linked the removed corpse to its group again";
		EXPECT_EQ(groupSparkie->getWalkerGroup().get(), group.get()) << "form() linked the removed sparkie to its group again";
	}
	// an instance's destruction: WalkerFormator.onInstanceDestroy drops the formations, the removed members and the group keep each other
	kerubCluster = nullptr;
	sparkieCluster = nullptr;
	group = nullptr;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		WalkerFormator::onInstanceDestroy(POETA, INSTANCE);
	}
	advance(0);
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	const std::string cycle = "[ClusteredNpc.npc of a member of WalkerGroup " + std::string(GROUP_ROUTE) + " (" + THROUGH_ITS_OWN_GROUP +
		" -> WalkerGroup.members; Java's group keeps and reads it too, until a respawn replaces a dead member)]";
	EXPECT_EQ(kerub->refCount(), 2u) << "(d) after onInstanceDestroy: the test's reference and the group <-> npc cycle - " << holders(*kerub);
	EXPECT_EQ(groupSparkie->refCount(), 2u) << "(d) after onInstanceDestroy: the test's reference and the group <-> npc cycle";
	EXPECT_EQ(probed(*kerub), cycle) << "the probe finds the group through the corpse's own Npc.walkerGroup";
	EXPECT_EQ(probed(*groupSparkie), cycle) << "the probe finds the group through the sparkie's own Npc.walkerGroup";
}

/**
 * (e) The lone walker of route 3805A343 (the sparkie at 1134.95/1012.56), the likely S-1 leak: no group, so its ClusteredNpc was only in
 * InstanceWalkerFormations.groupedSpawnObjects, which Java never reads after organizeAndSpawn and which kept the first npc - the corpse after
 * its first kill - until shutdown (refcount 1 in the server, no pinning task). organizeAndSpawn now drops the candidate lists when it returns.
 * Java's own bug stays: the respawn goes through processClusteredNpc again, finds no group for the route and is cached as a new candidate
 * (cacheWalkerCandidate returns true), so the successor never enters the world - in game, the sparkie never comes back after its first kill.
 */
TEST_F(WalkerGroupLifetimeTest, TheLoneWalkerOfAPoolRouteIsReleasedAfterItsCorpseDecayedAndStillNeverRespawns) {
	kill(*loneSparkie);
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(hasRespawnTask(*loneSparkie)) << "NpcController.onDie scheduled the respawn (20 s)";
	advance(2100);
	ASSERT_FALSE(isInWorld(*loneSparkie)) << "the DecayTask deleted the corpse";
	advance(18000); // +20.1 s: the RespawnTask ran
	EXPECT_FALSE(hasRespawnTask(*loneSparkie)) << "the RespawnTask ran";
	EXPECT_EQ(spawnedFrom(*loneSparkieSpot), 0) << "processClusteredNpc cached the new sparkie as a walker candidate instead of spawning it, as Java";
	expectReleased(*loneSparkie, "(e) the lone 3805A343 sparkie, killed and decayed");
}

} // namespace
} // namespace aion::gameserver::spawnengine::test
