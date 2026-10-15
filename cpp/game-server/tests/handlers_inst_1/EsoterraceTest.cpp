// P6I-4c (docs/design/p6-instances-plan.md §4): EsoterraceInstance (game-server/data/handlers/instance/abyss/EsoterraceInstance.java) on a
// real instance of its map. The door arms are not asserted (the fixture has no static doors: setDoorState only warns), nor the revive
// (its teleport reaches HousingService, which reads the database) or the zone message (no zones here): left to a gate.

#include "InstanceHandlerTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/instance/abyss/EsoterraceInstance.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t ESOTERRACE = 300250000;

class EsoterraceTest : public InstanceHandlerTest {
protected:
	world::WorldMapInstance& esoterrace() { return create(ESOTERRACE, abyss::EsoterraceInstance_instanceFactory); }

	runtime::Ptr<Npc> dies(world::WorldMapInstance& instance, int32_t npcId) {
		runtime::Ptr<Npc> npc = spawnNpc(instance, npcId, 1000.0f, 1000.0f, 300.0f, int8_t{0});
		instance.getInstanceHandler()->onDie(*npc);
		return npc;
	}

	static std::vector<int32_t> staticIds(world::WorldMapInstance& instance, int32_t npcId) {
		std::vector<int32_t> ids;
		for (runtime::Ptr<Npc>& npc : npcs(instance, npcId))
			ids.push_back(npc->getSpawn()->getStaticId());
		std::sort(ids.begin(), ids.end());
		return ids;
	}
};

TEST_F(EsoterraceTest, TheJavaClassOfItsMap) {
	std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance/abyss/EsoterraceInstance.java");
	std::stringstream text;
	text << in.rdbuf();
	std::string source = text.str();
	std::smatch match;
	ASSERT_TRUE(std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))")));
	EXPECT_EQ(std::stoi(match[1].str()), ESOTERRACE);
	EXPECT_NE(dynamic_cast<abyss::EsoterraceInstance*>(esoterrace().getInstanceHandler().get()), nullptr);
}

/** :30-36: Keening Sirokin and his treasure guard at 21 %, together */
TEST_F(EsoterraceTest, KeeningSirokinAtTwentyOnePercent) {
	int with = 0;
	for (uint64_t seed = 1; seed <= 40; ++seed) {
		commons::utils::Rnd::seedCurrentThreadForTests(seed);
		world::WorldMapInstance& instance = esoterrace();
		size_t sirokin = npcs(instance, 799580).size();
		ASSERT_EQ(sirokin, npcs(instance, 217649).size()) << "seed " << seed;
		ASSERT_EQ(npcs(instance).size(), 2 * sirokin);
		if (sirokin == 1) {
			++with;
			EXPECT_FLOAT_EQ(npcs(instance, 799580).front()->getX(), 1034.11f);
			EXPECT_EQ(npcs(instance, 217649).front()->getHeading(), 35);
		}
	}
	EXPECT_GT(with, 0);
	EXPECT_LT(with, 20);
}

/** :39-95: the arms that spawn or speak */
TEST_F(EsoterraceTest, TheBossesDeathsSpawnAndSpeak) {
	world::WorldMapInstance& instance = esoterrace();
	enter(instance, 1000.0f, 1000.0f, 300.0f);
	runtime::Ptr<Npc> kexkra = spawnNpc(instance, 217204, 1310.0f, 1170.0f, 52.0f, int8_t{0});
	dies(instance, 282291); // the Surkana Feeder: hard mode
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDF4Re_Drana_08())), 1);
	EXPECT_FALSE(kexkra->isSpawned());
	ASSERT_EQ(npcs(instance, 217205).size(), 1u);
	EXPECT_FLOAT_EQ(npcs(instance, 217205).front()->getX(), 1315.43f);

	dies(instance, 217289);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDF4Re_Drana_07())), 1);
	dies(instance, 217281);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDF4Re_Drana_04())), 1);
	dies(instance, 217195);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDF4Re_Drana_05())), 1);
	EXPECT_EQ(staticIds(instance, 701027), (std::vector<int32_t>{41, 77}));
	dies(instance, 217185);
	EXPECT_EQ(staticIds(instance, 701023), (std::vector<int32_t>{112}));
	dies(instance, 217204);
	EXPECT_EQ(npcs(instance, 205437).size(), 1u);
	EXPECT_EQ(staticIds(instance, 701027), (std::vector<int32_t>{41, 77, 727}));
	dies(instance, 217206);
	EXPECT_EQ(npcs(instance, 205437).size(), 2u);
	EXPECT_EQ(staticIds(instance, 701027), (std::vector<int32_t>{41, 77, 726, 727, 727}));
}

/** :76-82: the guard's death brings the treasure chest (static id 725) and Sirokin goes; without Sirokin, Java's NullPointerException */
TEST_F(EsoterraceTest, TheGuardsDeathBringsTheTreasure) {
	world::WorldMapInstance& instance = esoterrace();
	runtime::Ptr<Npc> sirokin = spawnNpc(instance, 799580, 1034.11f, 985.01f, 327.35095f, int8_t{105});
	dies(instance, 217649);
	EXPECT_EQ(staticIds(instance, 701025), (std::vector<int32_t>{725}));
	EXPECT_FALSE(sirokin->isSpawned());
	EXPECT_THROW(dies(instance, 217649), runtime::NullPointerException);
}

/** :84-90: the guards' deaths speak when all three are dead (NpcController.onDie calls the handler) */
TEST_F(EsoterraceTest, TheThreeGuardsSpeakWhenAllAreDead) {
	world::WorldMapInstance& instance = esoterrace();
	enter(instance, 1000.0f, 1000.0f, 300.0f);
	std::vector<runtime::Ptr<Npc>> guards;
	for (int32_t id : {217282, 217283, 217284})
		guards.push_back(spawnNpc(instance, id, 1000.0f, 1000.0f, 300.0f, int8_t{0}));
	guards[0]->getController().die(); // the controller calls the handler's onDie
	guards[1]->getController().die(); // the controller calls the handler's onDie
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDF4Re_Drana_03())), 0);
	guards[2]->getController().die(); // the controller calls the handler's onDie
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDF4Re_Drana_03())), 1);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
