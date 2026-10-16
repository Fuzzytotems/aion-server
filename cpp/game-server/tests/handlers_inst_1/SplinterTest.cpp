// P6I-4e (docs/design/p6-instances-plan.md §4): AbyssalSplinterInstance and UnstableSplinterpathInstance
// (game-server/data/handlers/instance/abyss) on real instances of their maps. The effect removals on the bosses and the exit buff are not
// asserted (no skill rows here); the quest items need the item rows: left to a gate.

#include "InstanceHandlerTestSupport.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/instance/abyss/AbyssalSplinterInstance.h"
#include "aion/gameserver/handlers/instance/abyss/UnstableSplinterpathInstance.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t ABYSSAL_SPLINTER = 300220000;
constexpr int32_t UNSTABLE_SPLINTERPATH = 300600000;

class SplinterTest : public InstanceHandlerTest {
protected:
	world::WorldMapInstance& abyssal() { return create(ABYSSAL_SPLINTER, abyss::AbyssalSplinterInstance_instanceFactory); }
	world::WorldMapInstance& unstable() { return create(UNSTABLE_SPLINTERPATH, abyss::UnstableSplinterpathInstance_instanceFactory); }

	runtime::Ptr<Npc> at(world::WorldMapInstance& instance, int32_t npcId, float x = 500.0f, float y = 500.0f) {
		return spawnNpc(instance, npcId, x, y, 430.0f, int8_t{0});
	}

	runtime::Ptr<Npc> dies(world::WorldMapInstance& instance, int32_t npcId, float x = 500.0f, float y = 500.0f) {
		runtime::Ptr<Npc> npc = at(instance, npcId, x, y);
		instance.getInstanceHandler()->onDie(*npc);
		return npc;
	}

	static std::string javaSource(const char* javaClass) {
		std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance/abyss" / (std::string(javaClass) + ".java"));
		std::stringstream text;
		text << in.rdbuf();
		return text.str();
	}
};

TEST_F(SplinterTest, EachSplinterIsTheJavaClassOfItsMap) {
	for (const auto& [javaClass, mapId] : std::map<std::string, int32_t>{{"AbyssalSplinterInstance", ABYSSAL_SPLINTER},
		     {"UnstableSplinterpathInstance", UNSTABLE_SPLINTERPATH}}) {
		std::string source = javaSource(javaClass.c_str());
		std::smatch match;
		ASSERT_TRUE(std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))"))) << javaClass;
		EXPECT_EQ(std::stoi(match[1].str()), mapId) << javaClass;
	}
	EXPECT_NE(dynamic_cast<abyss::AbyssalSplinterInstance*>(abyssal().getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<abyss::UnstableSplinterpathInstance*>(unstable().getInstanceHandler().get()), nullptr);
}

/** AbyssalSplinterInstance.java:32-39: the two Yamennes wake with their messages (onSpawn) */
TEST_F(SplinterTest, TheYamennesWakeUp) {
	world::WorldMapInstance& instance = abyssal();
	enter(instance, 330.0f, 733.0f, 197.0f);
	at(instance, 216960, 330.0f, 734.0f);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdDH_Wakeup())), 1);
	at(instance, 216952, 330.0f, 734.0f);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdD_Wakeup())), 1);
}

/** :42-122, 163-207: the bosses' fragments and treasure boxes */
TEST_F(SplinterTest, TheBossesLeaveFragmentsAndBoxes) {
	world::WorldMapInstance& instance = abyssal();
	enter(instance, 600.0f, 500.0f, 430.0f);
	dies(instance, 216950); // Kaluva
	EXPECT_EQ(npcs(instance, 700955).size(), 1u);
	EXPECT_EQ(npcs(instance, 700934).size(), 3u);
	EXPECT_EQ(npcs(instance, 700935).size(), 1u);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdC_BoxSpawn())), 1);
	dies(instance, 216952); // Yamennes Blindsight
	EXPECT_EQ(npcs(instance, 700937).size(), 1u);
	EXPECT_EQ(npcs(instance, 730317).size(), 1u) << "the exit";
	EXPECT_EQ(npcs(instance, 700934).size(), 6u);
	std::map<std::string, int> pazuzuBoxes;
	for (uint64_t seed = 1; seed <= 30; ++seed) {
		commons::utils::Rnd::seedCurrentThreadForTests(seed);
		world::WorldMapInstance& other = abyssal();
		dies(other, 216951); // Pazuzu
		ASSERT_EQ(npcs(other, 700860).size(), 1u);
		++pazuzuBoxes[npcs(other, 700861).empty() ? "without" : "with"];
	}
	EXPECT_GT(pazuzuBoxes["without"], pazuzuBoxes["with"]) << "Pazuzu's box at 12 %";
	EXPECT_GT(pazuzuBoxes["with"], 0);
}

/** :54-71: Rukril dies first while Ebonsoul lives: the message, and Rukril back after a minute if Ebonsoul still lives; both dead: the
 * fragment and the boxes */
TEST_F(SplinterTest, RukrilAndEbonsoulMustDieTogether) {
	world::WorldMapInstance& instance = abyssal();
	enter(instance, 450.0f, 690.0f, 433.0f);
	runtime::Ptr<Npc> ebonsoul = at(instance, 216949, 455.0f, 700.0f);
	runtime::Ptr<Npc> rukril = dies(instance, 216948, 447.0f, 683.0f);
	EXPECT_FALSE(rukril->isSpawned());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_NmdC_Light_Die())), 1);
	executor().advance(std::chrono::milliseconds(60000));
	ASSERT_EQ(npcs(instance, 216948).size(), 1u) << "Rukril is back";
	EXPECT_FLOAT_EQ(npcs(instance, 216948).front()->getX(), 447.1937f);
	npcs(instance, 216948).front()->getController().delete_();
	instance.getInstanceHandler()->onDie(*ebonsoul); // Rukril gone: the fragment and the boxes
	EXPECT_EQ(npcs(instance, 700955).size(), 1u);
	EXPECT_EQ(npcs(instance, 700936).size(), 1u);
	EXPECT_FALSE(ebonsoul->isSpawned());
}

/** :102-108, 214-224: the third huge aether fragment turns the easy artifact into the hard one */
TEST_F(SplinterTest, TheThirdFragmentBringsTheHardArtifact) {
	world::WorldMapInstance& instance = abyssal();
	enter(instance, 326.0f, 766.0f, 202.0f);
	runtime::Ptr<Npc> easy = at(instance, 700856, 326.0f, 766.0f);
	dies(instance, 700955);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_Artifact_Die_01())), 1);
	dies(instance, 700955);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_Artifact_Die_02())), 1);
	EXPECT_TRUE(easy->isSpawned());
	dies(instance, 700955);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDAbRe_Core_Artifact_Die_03())), 1);
	EXPECT_FALSE(easy->isSpawned());
	ASSERT_EQ(npcs(instance, 701593).size(), 1u);
	EXPECT_EQ(npcs(instance, 701593).front()->getSpawn()->getStaticId(), 79);
	EXPECT_TRUE(npcs(instance, 700955).empty()) << "each fragment goes";
}

/** :115-119, 209-212: the summons go when all three spawn gates are dead */
TEST_F(SplinterTest, TheSummonsGoWithTheLastGate) {
	world::WorldMapInstance& instance = abyssal();
	runtime::Ptr<Npc> summon = at(instance, 281903);
	std::vector<runtime::Ptr<Npc>> gates{at(instance, 282014), at(instance, 282015), at(instance, 282131)};
	gates[2]->getController().die();
	gates[1]->getController().die();
	EXPECT_TRUE(summon->isSpawned()) << "the first gate lives";
	gates[0]->getController().die();
	EXPECT_FALSE(summon->isSpawned());
}

/** :130-161: the artifacts wake a Yamennes and give the race's quest artifact (static id 79) */
TEST_F(SplinterTest, TheArtifactsWakeAYamennes) {
	world::WorldMapInstance& instance = abyssal();
	enter(instance, 326.0f, 766.0f, 202.0f);
	runtime::Ptr<Npc> hard = at(instance, 701593, 326.0f, 766.0f);
	instance.getInstanceHandler()->handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player>(player()), *hard);
	EXPECT_EQ(npcs(instance, 216960).size(), 1u);
	ASSERT_EQ(npcs(instance, 700857).size(), 1u) << "an Elyos";
	EXPECT_TRUE(hard->isDead());
	runtime::Ptr<Npc> easy = at(instance, 700856, 326.0f, 766.0f);
	instance.getInstanceHandler()->handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player>(player()), *easy);
	EXPECT_EQ(npcs(instance, 216952).size(), 1u);
	EXPECT_EQ(npcs(instance, 700857).size(), 2u);
}

/** UnstableSplinterpathInstance.java:40-116: the unstable ids; the returning boss is the other one of the pair (Java, J-P6I4-2) */
TEST_F(SplinterTest, TheUnstableBossesAndTheirTwins) {
	world::WorldMapInstance& instance = unstable();
	enter(instance, 450.0f, 690.0f, 433.0f);
	at(instance, 219552, 455.0f, 700.0f); // Ebonsoul lives
	dies(instance, 219551, 447.0f, 683.0f); // Rukril
	executor().advance(std::chrono::milliseconds(60000));
	EXPECT_EQ(npcs(instance, 219552).size(), 2u) << "Java spawns 219552 for Rukril's case";
	EXPECT_TRUE(npcs(instance, 219551).empty());
	dies(instance, 219553); // Kaluva
	EXPECT_EQ(npcs(instance, 701588).size(), 1u);
	EXPECT_EQ(npcs(instance, 701577).size(), 1u);
	dies(instance, 219555); // Durable Yamennes Blindsight
	EXPECT_EQ(npcs(instance, 701579).size(), 1u);
	EXPECT_EQ(npcs(instance, 730317).size(), 1u);
}

/** UnstableSplinterpathInstance.java:124-135: the artifacts wake a Yamennes and go */
TEST_F(SplinterTest, TheUnstableArtifactsWakeAYamennes) {
	world::WorldMapInstance& instance = unstable();
	enter(instance, 326.0f, 766.0f, 202.0f);
	runtime::Ptr<Npc> hard = at(instance, 700957, 326.0f, 766.0f);
	instance.getInstanceHandler()->handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player>(player()), *hard);
	EXPECT_EQ(npcs(instance, 219563).size(), 1u);
	EXPECT_FALSE(hard->isSpawned());
	runtime::Ptr<Npc> easy = at(instance, 701589, 326.0f, 766.0f);
	instance.getInstanceHandler()->handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player>(player()), *easy);
	EXPECT_EQ(npcs(instance, 219555).size(), 1u);
	runtime::Ptr<Npc> other = at(instance, 701588, 326.0f, 766.0f);
	instance.getInstanceHandler()->handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player>(player()), *other);
	EXPECT_TRUE(other->isSpawned());
}

/** UnstableSplinterpathInstance.java:186-196: three fragments: the hard artifact */
TEST_F(SplinterTest, TheUnstableThirdFragmentBringsTheHardArtifact) {
	world::WorldMapInstance& instance = unstable();
	runtime::Ptr<Npc> easy = at(instance, 701589);
	for (int i = 0; i < 3; ++i)
		dies(instance, 701588);
	EXPECT_FALSE(easy->isSpawned());
	EXPECT_EQ(npcs(instance, 700957).size(), 1u);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
