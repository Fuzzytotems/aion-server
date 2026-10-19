// P5-12b1 bases: what the old Base keeps of its npcs after a capture. Leak investigation of the D17 soak of 2026-10-18 (gs.scenario.m5i_soak on
// a5c4826ed): the leak census warned that the flags of six casual bases (Npc 105637 flamecrest outpost, 105524 lepharist territory, 106308,
// 105551 beling base, 105603 stonereach outpost, 105518) were still alive 10 minutes after a capture removed them from the world, refcount 1, no
// pinning task, and the leak probe found no holder (Base.flag is outside its search set).
//
// The capture came from the base itself: its assault task finds the flag's map region inactive (no player near) and, with a chance of 20
// (Base.java:126-137), calls BaseService.capture, which stops this Base (despawning every npc of the base, the flag among them) and starts a new
// one - and then the same task calls scheduleAssault() on the stopped Base. That task pins the old Base for the assault delay (7.5 to 20 minutes
// for a casual base), and the old Base holds its flag (Base.flag) and its assaulters until the task fires and returns at its isStopped check.
// Java keeps the same objects reachable for the same time, so the port keeps them too (the coordinator's decision of 2026-10-19: the delayed
// release is Java's). What the port changes is the diagnosis: with header request m5b3-leak-h03 granted for Base, WorldLeakProbe finds the
// stopped Base through the pending task that pins it and names it as the flag's holder.
//
// The case drives that path on WorldTestSupport.h's Poeta with one casual base, whose flag and boss are spawned from a base_spawn, and checks the
// three steps: after the capture the old flag's one extra reference is the stopped Base's; the probe attributes it to that Base and its pinning
// task; once the stale assault task has fired, the flag is released.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../world/WorldTestSupport.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/dataholders/BaseData.bind.h"
#include "aion/gameserver/dataholders/BaseData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/SpawnsData.bind.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/model/base/BaseLocation.h"
#include "aion/gameserver/model/base/BaseOccupier.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/services/BaseService.h"
#include "aion/gameserver/services/RespawnService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldLeakProbe.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::playerevents::test {
namespace {

using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using runtime::Ptr;
using runtime::Ref;
using world::test::POETA;

constexpr int32_t BASE_ID = 9301;
constexpr int32_t BALAUR_FLAG = 990001;
constexpr int32_t ELYOS_FLAG = 990002;
constexpr int32_t ASMODIAN_FLAG = 990003;
constexpr int32_t BOSS = 990004;

constexpr const char* BASE_LOCATIONS_XML =
	R"(<base_locations><base_location id="9301" type="CASUAL" world="210010000" default_occupier="BALAUR" /></base_locations>)";

/** a base_spawn as spawns/Bases/*.xml writes it: one flag per occupier, and the Balaur boss the assault needs */
constexpr const char* BASE_SPAWNS_XML = R"(<spawns><spawn_map map_id="210010000"><base_spawn id="9301">
	<occupier_template occupier="BALAUR">
		<spawn npc_id="990001" handler="FLAG"><spot x="500" y="500" z="100" h="0"/></spawn>
		<spawn npc_id="990004" handler="BOSS"><spot x="505" y="500" z="100" h="0"/></spawn>
	</occupier_template>
	<occupier_template occupier="ELYOS">
		<spawn npc_id="990002" handler="FLAG"><spot x="500" y="500" z="100" h="0"/></spawn>
		<spawn npc_id="990004" handler="BOSS"><spot x="505" y="500" z="100" h="0"/></spawn>
	</occupier_template>
	<occupier_template occupier="ASMODIANS">
		<spawn npc_id="990003" handler="FLAG"><spot x="500" y="500" z="100" h="0"/></spawn>
		<spawn npc_id="990004" handler="BOSS"><spot x="505" y="500" z="100" h="0"/></spawn>
	</occupier_template>
</base_spawn></spawn_map></spawns>)";

std::string npcRow(int32_t id, const char* name) {
	return "<npc_template npc_id=\"" + std::to_string(id) + "\" level=\"50\" name=\"" + name +
		"\" name_id=\"1\" rank=\"DISCIPLINED\" rating=\"NORMAL\" tribe=\"GENERAL\" ai=\"general\"><stats maxHp=\"1000\"/></npc_template>";
}

class BaseCaptureLifetimeTest : public ::testing::Test {
protected:
	void SetUp() override {
		// BaseService reads BASE_DATA once per process, and another case of this executable publishes its own table: run alone (ctest does)
		if (dataholders::DataManager::BASE_DATA || !world::test::publishTestStaticData())
			GTEST_SKIP() << "another case of this process published the base or world data (run the case on its own)";
		prepared = true;
		missingAiHandlers = *configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn"); // "general" is not registered in this executable
		utils::ThreadPoolManager::installBackend(nullptr);
		executor = new runtime::DeterministicExecutor(clock, 4);
		utils::ThreadPoolManager::installBackend(std::unique_ptr<runtime::DeterministicExecutor>(executor));
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		dataholders::DataManager::BASE_DATA.publish(xml::bindString<dataholders::BaseData>(context, BASE_LOCATIONS_XML));
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context,
			"<npc_templates>" + npcRow(BALAUR_FLAG, "balaur flag") + npcRow(ELYOS_FLAG, "elyos flag") + npcRow(ASMODIAN_FLAG, "asmodian flag") +
				npcRow(BOSS, "boss") + "</npc_templates>"));
		dataholders::DataManager::SPAWNS_DATA.publish(xml::bindString<dataholders::SpawnsData>(context, BASE_SPAWNS_XML));
		savedGenerator = std::make_unique<commons::utils::Rnd::Xoshiro256PlusPlus>(commons::utils::Rnd::generator());
	}

	void TearDown() override {
		if (!prepared)
			return;
		{
			runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
			services::BaseService::getInstance().stop(BASE_ID);
			services::RespawnService::cancelRespawns([](model::templates::spawns::SpawnTemplate&) { return true; });
			world::World& world = world::World::getInstance();
			for (Ptr<VisibleObject> object : *world.getWorldMap(POETA)->getMainWorldMapInstance())
				world.removeObject(*object);
		}
		oldFlag = nullptr;
		drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		executor = nullptr;
		drain();
		dataholders::DataManager::SPAWNS_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set(missingAiHandlers);
		commons::utils::Rnd::generator() = *savedGenerator;
	}

	static bool isSpawned(Npc& npc) {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		return npc.isSpawned();
	}

	static void drain() {
		runtime::Reclaimer::getInstance().drain();
		runtime::Reclaimer::getInstance().drain();
	}

	/** the spawned npc of the template in Poeta, null if there is none */
	static Ptr<Npc> spawned(int32_t npcId) {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		for (Ptr<VisibleObject> object : *world::World::getInstance().getWorldMap(POETA)->getMainWorldMapInstance()) {
			Ptr<Npc> npc = runtime::as<Npc>(object);
			if (npc && npc->getNpcId() == npcId && npc->isSpawned())
				return npc;
		}
		return nullptr;
	}

	runtime::ManualClock clock{0};
	runtime::DeterministicExecutor* executor = nullptr;
	xml::LoadContext context;
	std::unique_ptr<commons::utils::Rnd::Xoshiro256PlusPlus> savedGenerator;
	std::string missingAiHandlers;
	bool prepared = false;
	Ref<Npc> oldFlag;
};

TEST_F(BaseCaptureLifetimeTest, AStoppedBaseHoldsItsFlagUntilItsLastAssaultTaskFiresAndTheProbeNamesIt) {
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		services::BaseService::getInstance().start(BASE_ID);
	}
	oldFlag = Ref<Npc>(spawned(BALAUR_FLAG));
	ASSERT_TRUE(oldFlag) << "Base.handleStart spawns the occupier's flag";

	// the boss spawn (10 to 20 minutes) schedules the assault (7.5 to 20 minutes): no player is near, so each assault captures the base with a
	// chance of 20 and schedules the next one. Within 40 hours of assaults, one captures
	commons::utils::Rnd::generator() = commons::utils::Rnd::Xoshiro256PlusPlus(17);
	for (int32_t minute = 0; minute < 40 * 60 && isSpawned(*oldFlag); minute++)
		executor->advance(std::chrono::minutes(1));
	ASSERT_FALSE(isSpawned(*oldFlag)) << "a capture removed the old flag";
	ASSERT_TRUE(spawned(ELYOS_FLAG) || spawned(ASMODIAN_FLAG)) << "the new Base spawned the new occupier's flag";
	drain();

	// 1) Java's reachability: besides the test's reference, the stopped Base holds the flag (Base.flag) - the leak census' "refcount 1"
	EXPECT_EQ(oldFlag->refCount(), 2u) << "the test's reference and the stopped Base's flag";

	// 2) the probe names that holder: the stopped Base, found through the assault task that pins it (Base.java:134-136, after the capture)
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		std::vector<std::string> holders = world::WorldLeakProbe::findHolders(*oldFlag);
		ASSERT_EQ(holders.size(), 1u) << ::testing::PrintToString(holders);
		EXPECT_EQ(holders[0].rfind("Base.flag of the stopped Base 9301, pinned by the pending task(s) ", 0), 0u) << holders[0];
		EXPECT_NE(holders[0].find("Base.cpp"), std::string::npos) << "the pinning task's call site: " << holders[0];
	}

	// 3) the stale assault task fires within the assault delay (at most 20 minutes), finds isStopped and returns: the Base and its flag go
	executor->advance(std::chrono::minutes(21));
	drain();
	EXPECT_EQ(oldFlag->refCount(), 1u) << "only the test's reference is left";
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		EXPECT_TRUE(world::WorldLeakProbe::findHolders(*oldFlag).empty());
	}
}

} // namespace
} // namespace aion::gameserver::playerevents::test
