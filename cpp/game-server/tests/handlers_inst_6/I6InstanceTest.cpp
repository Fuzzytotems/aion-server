// I6-1 (docs/design/p6-i6-plan.md §3): SteelRakeInstance and SteelRakeCabineInstance (game-server/data/handlers/instance/rakes) on real instances
// of their maps, Rnd seeded and driven over enough instances to see every branch.

#include "I6TestSupport.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/handlers/instance/TheobomosLabInstance.h"
#include "aion/gameserver/handlers/instance/rakes/SteelRakeCabineInstance.h"
#include "aion/gameserver/handlers/instance/rakes/SteelRakeInstance.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;

class I6InstanceTest : public InstanceHandlerTest {
protected:
	static std::string javaSource(const char* javaPath) {
		std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance" / (std::string(javaPath) + ".java"));
		std::stringstream text;
		text << in.rdbuf();
		return text.str();
	}
};

TEST_F(I6InstanceTest, EachHandlerIsTheJavaClassOfItsMap) {
	for (const auto& [javaPath, mapId] : std::map<std::string, int32_t>{{"rakes/SteelRakeInstance", STEEL_RAKE}, {"rakes/SteelRakeCabineInstance", STEEL_RAKE_CABIN},
		     {"TheobomosLabInstance", THEOBOMOS_LAB}}) {
		std::string source = javaSource(javaPath.c_str());
		std::smatch match;
		ASSERT_TRUE(std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))"))) << javaPath;
		EXPECT_EQ(std::stoi(match[1].str()), mapId) << javaPath;
	}
	world::WorldMapInstance& rake = create(STEEL_RAKE, rakes::SteelRakeInstance_instanceFactory);
	EXPECT_NE(dynamic_cast<rakes::SteelRakeInstance*>(rake.getInstanceHandler().get()), nullptr);
	world::WorldMapInstance& cabin = create(STEEL_RAKE_CABIN, rakes::SteelRakeCabineInstance_instanceFactory);
	EXPECT_NE(dynamic_cast<rakes::SteelRakeCabineInstance*>(cabin.getInstanceHandler().get()), nullptr);
	world::WorldMapInstance& lab = create(THEOBOMOS_LAB, TheobomosLabInstance_instanceFactory);
	EXPECT_NE(dynamic_cast<TheobomosLabInstance*>(lab.getInstanceHandler().get()), nullptr);
}

/** SteelRakeInstance.java:18-27: Pegureronerk three times in four, else the Kaneron Agent, at one place; one of the six special deliveries */
TEST_F(I6InstanceTest, TheSteelRakeHasItsAgentAndADelivery) {
	commons::utils::Rnd::seedCurrentThreadForTests(16);
	int32_t pegureronerk = 0;
	std::set<int32_t> deliveries;
	const int32_t rounds = 40;
	for (int32_t round = 0; round < rounds; round++) {
		world::WorldMapInstance& rake = create(STEEL_RAKE, rakes::SteelRakeInstance_instanceFactory);
		ASSERT_EQ(npcs(rake).size(), 2u);
		size_t agents = npcs(rake, 798376).size() + npcs(rake, 215041).size();
		ASSERT_EQ(agents, 1u);
		runtime::Ptr<Npc> agent = npcs(rake, 798376).empty() ? npcs(rake, 215041)[0] : npcs(rake, 798376)[0];
		EXPECT_NEAR(agent->getX(), 516.198f, 0.001f);
		EXPECT_EQ(agent->getHeading(), 60);
		pegureronerk += agent->getNpcId() == 798376 ? 1 : 0;
		runtime::Ptr<Npc> delivery = npcs(rake)[0] == agent ? npcs(rake)[1] : npcs(rake)[0];
		deliveries.insert(delivery->getNpcId());
		EXPECT_FLOAT_EQ(delivery->getY(), 510.545654f);
		EXPECT_EQ(delivery->getHeading(), 90);
	}
	EXPECT_GT(pegureronerk, rounds / 2) << "three in four";
	EXPECT_LT(pegureronerk, rounds) << "the Kaneron Agent sometimes";
	EXPECT_EQ(deliveries, (std::set<int32_t>{215074, 215075, 215076, 215077, 215054, 215055}));
}

/** SteelRakeCabineInstance.java:18-29: Sweeper Nunukin at one of two places; Madame Bovariki and the Shaman on their two places, either way */
TEST_F(I6InstanceTest, TheCabinShufflesItsBosses) {
	commons::utils::Rnd::seedCurrentThreadForTests(16);
	std::set<float> nunukinPlaces;
	std::set<float> bovarikiPlaces;
	for (int32_t round = 0; round < 20; round++) {
		world::WorldMapInstance& cabin = create(STEEL_RAKE_CABIN, rakes::SteelRakeCabineInstance_instanceFactory);
		ASSERT_EQ(npcs(cabin).size(), 3u);
		ASSERT_EQ(npcs(cabin, 219026).size(), 1u);
		ASSERT_EQ(npcs(cabin, 219032).size(), 1u);
		ASSERT_EQ(npcs(cabin, 219003).size(), 1u);
		nunukinPlaces.insert(npcs(cabin, 219026)[0]->getY());
		float bovariki = npcs(cabin, 219032)[0]->getX();
		float shaman = npcs(cabin, 219003)[0]->getX();
		EXPECT_EQ(std::set<float>({bovariki, shaman}), std::set<float>({463.124f, 502.859f}));
		bovarikiPlaces.insert(bovariki);
	}
	EXPECT_EQ(nunukinPlaces, std::set<float>({491.557f, 536.17004f}));
	EXPECT_EQ(bovarikiPlaces.size(), 2u);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
