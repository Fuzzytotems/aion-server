// P6I-4a (docs/design/p6-instances-plan.md §4): the inner upper abyss siege keeps of chunk I1 - AbstractInnerUpperAbyssInstance and the
// Krotan/Kysis/Miren Barracks and Chambers and the three legion Barracks (game-server/data/handlers/instance/abyss) - on a real instance of
// Krotan Barracks (and Legion's Krotan Barracks for the chest id), through the AION_INSTANCE_HANDLER factories.

#include "InstanceHandlerTestSupport.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/handlers/instance/abyss/KrotanBarracks.h"
#include "aion/gameserver/handlers/instance/abyss/KrotanChamber.h"
#include "aion/gameserver/handlers/instance/abyss/KysisBarracks.h"
#include "aion/gameserver/handlers/instance/abyss/KysisChamber.h"
#include "aion/gameserver/handlers/instance/abyss/LegionsKrotanBarracks.h"
#include "aion/gameserver/handlers/instance/abyss/LegionsKysisBarracks.h"
#include "aion/gameserver/handlers/instance/abyss/LegionsMirenBarracks.h"
#include "aion/gameserver/handlers/instance/abyss/MirenBarracks.h"
#include "aion/gameserver/handlers/instance/abyss/MirenChamber.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using network::aion::serverpackets::SM_QUEST_ACTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t KROTAN_BARRACKS = 301300000;
constexpr int32_t LEGIONS_KROTAN_BARRACKS = 301260000;

class AbyssKeepTest : public InstanceHandlerTest {
protected:
	world::WorldMapInstance& barracks() { return create(KROTAN_BARRACKS, abyss::KrotanBarracks_instanceFactory); }

	runtime::Ptr<Npc> dies(world::WorldMapInstance& instance, int32_t npcId) {
		runtime::Ptr<Npc> npc = spawnNpc(instance, npcId, 500.0f, 500.0f, 200.0f, int8_t{0});
		instance.getInstanceHandler()->onDie(*npc);
		return npc;
	}
};

/** Each Java class's @InstanceID is the map of its factory's handler */
TEST_F(AbyssKeepTest, EachKeepIsTheJavaClassOfItsMap) {
	const std::map<std::string, int32_t> expected{{"KrotanBarracks", 301300000}, {"KysisBarracks", 301280000}, {"MirenBarracks", 301290000},
		{"KrotanChamber", 300140000}, {"KysisChamber", 300120000}, {"MirenChamber", 300130000}, {"LegionsKrotanBarracks", 301260000},
		{"LegionsKysisBarracks", 301240000}, {"LegionsMirenBarracks", 301250000}};
	const std::filesystem::path javaDir = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance/abyss";
	const std::regex instanceId(R"(@InstanceID\((\d+)\))");
	for (const auto& [javaClass, mapId] : expected) {
		std::ifstream in(javaDir / (javaClass + ".java"));
		ASSERT_TRUE(in) << javaClass;
		std::stringstream text;
		text << in.rdbuf();
		std::string source = text.str();
		std::smatch match;
		ASSERT_TRUE(std::regex_search(source, match, instanceId)) << javaClass;
		EXPECT_EQ(std::stoi(match[1].str()), mapId) << javaClass;
	}
	using namespace abyss;
	EXPECT_NE(dynamic_cast<KrotanBarracks*>(create(301300000, KrotanBarracks_instanceFactory).getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<KysisBarracks*>(create(301280000, KysisBarracks_instanceFactory).getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<MirenBarracks*>(create(301290000, MirenBarracks_instanceFactory).getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<KrotanChamber*>(create(300140000, KrotanChamber_instanceFactory).getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<KysisChamber*>(create(300120000, KysisChamber_instanceFactory).getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<MirenChamber*>(create(300130000, MirenChamber_instanceFactory).getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<LegionsKrotanBarracks*>(create(301260000, LegionsKrotanBarracks_instanceFactory).getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<LegionsKysisBarracks*>(create(301240000, LegionsKysisBarracks_instanceFactory).getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<LegionsMirenBarracks*>(create(301250000, LegionsMirenBarracks_instanceFactory).getInstanceHandler().get()), nullptr);
}

/** AbstractInnerUpperAbyssInstance.java:103-118: one of the four key masters at the entrance */
TEST_F(AbyssKeepTest, OneOfTheFourKeyMasters) {
	std::set<int32_t> seen;
	for (uint64_t seed = 1; seed <= 20; ++seed) {
		commons::utils::Rnd::seedCurrentThreadForTests(seed);
		std::vector<runtime::Ptr<Npc>> all = npcs(barracks());
		ASSERT_EQ(all.size(), 1u);
		EXPECT_FLOAT_EQ(all.front()->getX(), 527.64f);
		EXPECT_EQ(all.front()->getHeading(), 90);
		seen.insert(all.front()->getNpcId());
	}
	EXPECT_EQ(seen, (std::set<int32_t>{233627, 233628, 233629, 233630}));
}

/** KrotanBarracks.java onDie and AbstractInnerUpperAbyssInstance.java:143-145: a guard's death opens its door (static id), no other */
TEST_F(AbyssKeepTest, AGuardsDeathOpensItsDoor) {
	world::WorldMapInstance& instance = barracks();
	runtime::Ptr<Npc> door10 = spawnStaticNpc(instance, 700545, 400.0f, 400.0f, 200.0f, 10);
	runtime::Ptr<Npc> door5 = spawnStaticNpc(instance, 700545, 410.0f, 400.0f, 200.0f, 5);
	runtime::Ptr<Npc> other10 = spawnStaticNpc(instance, 233599, 420.0f, 400.0f, 200.0f, 10); // static id 10, but not a door
	dies(instance, 233599); // openDoor(10)
	EXPECT_FALSE(door10->isSpawned());
	EXPECT_TRUE(door5->isSpawned());
	EXPECT_TRUE(other10->isSpawned());
	dies(instance, 233600); // openDoor(5)
	EXPECT_FALSE(door5->isSpawned());
}

/** :164-180: the boss's death brings 11 chests minus the failures, and the bonus chest in hard mode; the legion version has its own chest.
 * The second boss id's death brings them again: spawnChests keeps its locations (the correction of 2026-10-19, was J-P6I4-1) */
TEST_F(AbyssKeepTest, TheBossesDeathBringsTheChests) {
	world::WorldMapInstance& instance = barracks();
	dies(instance, 233633);
	ASSERT_EQ(npcs(instance, 702288).size(), 11u);
	ASSERT_EQ(npcs(instance, 702289).size(), 1u) << "hard mode: the bonus chest";
	EXPECT_FLOAT_EQ(npcs(instance, 702289).front()->getX(), 576.463f);
	EXPECT_EQ(npcs(instance, 702289).front()->getHeading(), 56);
	EXPECT_NO_THROW(dies(instance, 233632)) << "Java: the second spawnChests found chestLocations null";
	EXPECT_EQ(npcs(instance, 702288).size(), 22u) << "the kept locations";
	EXPECT_EQ(npcs(instance, 702289).size(), 2u);

	world::WorldMapInstance& legion = create(LEGIONS_KROTAN_BARRACKS, abyss::LegionsKrotanBarracks_instanceFactory);
	dies(legion, 233632);
	EXPECT_EQ(npcs(legion, 702290).size(), 11u);
	EXPECT_EQ(npcs(legion, 702291).size(), 1u);
}

/** :150-158: the artifact switches to easy mode once - the boss becomes his weaker version (id - 1), and no bonus chest follows */
TEST_F(AbyssKeepTest, TheArtifactSwitchesToEasyMode) {
	world::WorldMapInstance& instance = barracks();
	runtime::Ptr<Npc> boss = spawnNpc(instance, 233633, 530.0f, 850.0f, 199.0f, int8_t{30});
	dies(instance, 215413);
	EXPECT_FALSE(boss->isSpawned());
	ASSERT_EQ(npcs(instance, 233632).size(), 1u);
	EXPECT_FLOAT_EQ(npcs(instance, 233632).front()->getX(), 530.0f);
	EXPECT_EQ(npcs(instance, 233632).front()->getHeading(), 30);
	dies(instance, 215413);
	EXPECT_EQ(npcs(instance, 233632).size(), 1u) << "once";
	instance.getInstanceHandler()->onDie(*npcs(instance, 233632).front());
	EXPECT_EQ(npcs(instance, 702288).size(), 11u);
	EXPECT_TRUE(npcs(instance, 702289).empty()) << "easy mode: no bonus chest";
}

/** :121-136: the timer npc seeing a player starts the run once; after 5 min and 6 x 30 s (11 failures) the boss goes, and the chests are
 * 11 - failures */
TEST_F(AbyssKeepTest, TheTimerNpcStartsTheChestReduction) {
	world::WorldMapInstance& instance = barracks();
	enter(instance, 500.0f, 500.0f, 200.0f);
	runtime::Ptr<Npc> timer = spawnNpc(instance, 206095, 500.0f, 600.0f, 200.0f, int8_t{0});
	runtime::Ptr<Npc> boss = spawnNpc(instance, 233633, 530.0f, 850.0f, 199.0f, int8_t{30});
	runtime::Ptr<Npc> stranger = spawnNpc(instance, 233599, 500.0f, 610.0f, 200.0f, int8_t{0});
	instance.getInstanceHandler()->onCreatureDetected(*stranger, player()); // not the timer npc
	instance.getInstanceHandler()->onCreatureDetected(*timer, *stranger);  // not a player
	EXPECT_TRUE(timer->isSpawned());
	instance.getInstanceHandler()->onCreatureDetected(*timer, player());
	EXPECT_FALSE(timer->isSpawned());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_START_IDABRE())), 1);
	EXPECT_EQ(countSent(serialized(SM_QUEST_ACTION(0, 600))), 1);
	executor().advance(std::chrono::milliseconds(300000 + 2 * 30000)); // 3 failures
	EXPECT_TRUE(boss->isSpawned());
	instance.getInstanceHandler()->onDie(*boss);
	EXPECT_EQ(npcs(instance, 702288).size(), 8u) << "11 - 3";
	executor().advance(std::chrono::milliseconds(600000));
	EXPECT_EQ(npcs(instance, 702288).size(), 8u) << "spawnChests cancelled the task";
}

TEST_F(AbyssKeepTest, ElevenFailuresTakeTheBoss) {
	world::WorldMapInstance& instance = barracks();
	enter(instance, 500.0f, 500.0f, 200.0f);
	runtime::Ptr<Npc> timer = spawnNpc(instance, 206095, 500.0f, 600.0f, 200.0f, int8_t{0});
	runtime::Ptr<Npc> boss = spawnNpc(instance, 233633, 530.0f, 850.0f, 199.0f, int8_t{30});
	instance.getInstanceHandler()->onCreatureDetected(*timer, player());
	executor().advance(std::chrono::milliseconds(300000 + 9 * 30000)); // 10 failures
	EXPECT_TRUE(boss->isSpawned());
	executor().advance(std::chrono::milliseconds(30000)); // the 11th
	EXPECT_FALSE(boss->isSpawned());
}

/** :187-190: a statue's AP is split over the players inside; isBoss is the awakened boss */
TEST_F(AbyssKeepTest, IsBossIsTheAwakenedBoss) {
	world::WorldMapInstance& instance = barracks();
	auto& handler = dynamic_cast<::aion::gameserver::instance::handlers::GeneralInstanceHandler&>(*instance.getInstanceHandler());
	EXPECT_TRUE(handler.isBoss(*spawnNpc(instance, 233633, 530.0f, 850.0f, 199.0f, int8_t{0})));
	EXPECT_FALSE(handler.isBoss(*spawnNpc(instance, 233632, 530.0f, 850.0f, 199.0f, int8_t{0})));
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
