// P6I-4f (docs/design/p6-instances-plan.md §4): TheHexwayInstance (game-server/data/handlers/instance/abyss/TheHexwayInstance.java) on a real
// instance of its map: the rings, the bonus chest timer, the bosses' timers and chests, the second bridge's twin. The doors are not asserted
// (no static doors here); the revive and the exit teleport reach HousingService (database): left to a gate.

#include "InstanceHandlerTestSupport.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/instance/abyss/TheHexwayInstance.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/flyring/FlyRing.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using network::aion::serverpackets::SM_MESSAGE;
using network::aion::serverpackets::SM_QUEST_ACTION;

constexpr int32_t HEXWAY = 300700000;
const std::vector<int32_t> BOSSES{219611, 286933, 219612, 219613, 219610, 219614};

class HexwayTest : public InstanceHandlerTest {
protected:
	world::WorldMapInstance& hexway() { return create(HEXWAY, abyss::TheHexwayInstance_instanceFactory); }

	std::vector<uint8_t> yellow(std::string_view text) { return serialized(SM_MESSAGE(0, "", text, model::ChatType::BRIGHT_YELLOW_CENTER)); }

	runtime::Ptr<Npc> at(world::WorldMapInstance& instance, int32_t npcId) { return spawnNpc(instance, npcId, 300.0f, 500.0f, 365.0f, int8_t{0}); }
};

TEST_F(HexwayTest, TheJavaClassOfItsMapWithItsSevenRings) {
	std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance/abyss/TheHexwayInstance.java");
	std::stringstream text;
	text << in.rdbuf();
	std::string source = text.str();
	std::smatch match;
	ASSERT_TRUE(std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))")));
	EXPECT_EQ(std::stoi(match[1].str()), HEXWAY);
	world::WorldMapInstance& instance = hexway();
	std::set<std::string> rings;
	for (runtime::Ptr<model::gameobjects::VisibleObject> object : instance)
		if (runtime::Ptr<model::flyring::FlyRing> ring = runtime::as<model::flyring::FlyRing>(object))
			rings.insert(ring->getName());
	EXPECT_EQ(rings, (std::set<std::string>{"HEXWAY_BONUSCHEST", "HEXWAY_BOSS_0", "HEXWAY_BOSS_1", "HEXWAY_BOSS_2", "HEXWAY_BOSS_3",
		"HEXWAY_BOSS_4", "HEXWAY_BOSS_5"}));
}

/** :74-90, 323-340: the bonus chest ring starts the 22 minutes once, with the notices, and the end */
TEST_F(HexwayTest, TheBonusChestTimer) {
	world::WorldMapInstance& instance = hexway();
	enter(instance, 576.0f, 585.0f, 354.0f);
	EXPECT_FALSE(instance.getInstanceHandler()->onPassFlyingRing(player(), "HEXWAY_BONUSCHEST"));
	instance.getInstanceHandler()->onPassFlyingRing(player(), "HEXWAY_BONUSCHEST");
	EXPECT_EQ(countSent(yellow("You have 22 minutes to kill all boss mobs located at the end of each bridge to unlock a bonus chest!")), 1);
	executor().advance(std::chrono::seconds(1320 - 900));
	EXPECT_EQ(countSent(yellow("You have 15m left to unlock the bonus chest.")), 1);
	executor().advance(std::chrono::seconds(900 - 120));
	EXPECT_EQ(countSent(yellow("You have 2m left to unlock the bonus chest.")), 1);
	executor().advance(std::chrono::seconds(120 - 30));
	EXPECT_EQ(countSent(yellow("You have 30s left to unlock the bonus chest.")), 1);
	executor().advance(std::chrono::seconds(29));
	EXPECT_EQ(countSent(yellow("Your time to unlock a bonus chest has expired!")), 0) << "1320 s, not less";
	executor().advance(std::chrono::seconds(1));
	EXPECT_EQ(countSent(yellow("Your time to unlock a bonus chest has expired!")), 1);
	EXPECT_EQ(countSent(yellow("You have 1s left to unlock the bonus chest.")), 1);
}

/** :91-113, 235-243: a boss ring starts its time limit and gives the quest timer; the time up takes the boss */
TEST_F(HexwayTest, ABossRingStartsItsTimeLimit) {
	world::WorldMapInstance& instance = hexway();
	enter(instance, 300.0f, 500.0f, 365.0f);
	runtime::Ptr<Npc> boss = at(instance, 219611);
	instance.getInstanceHandler()->onPassFlyingRing(player(), "HEXWAY_BOSS_0");
	EXPECT_EQ(countSent(serialized(SM_QUEST_ACTION(0, 120))), 1);
	instance.getInstanceHandler()->onPassFlyingRing(player(), "HEXWAY_BOSS_9"); // no such boss
	executor().advance(std::chrono::seconds(119));
	EXPECT_TRUE(boss->isSpawned());
	executor().advance(std::chrono::seconds(1));
	EXPECT_FALSE(boss->isSpawned());
	EXPECT_EQ(countSent(yellow("You failed to kill a boss mob. No bonus chest will be unlocked.")), 1);
	runtime::Ptr<Npc> boss2 = at(instance, 286933);
	instance.getInstanceHandler()->onPassFlyingRing(player(), "HEXWAY_BOSS_1");
	EXPECT_EQ(countSent(serialized(SM_QUEST_ACTION(0, 210))), 1);
	executor().advance(std::chrono::seconds(210));
	EXPECT_EQ(countSent(yellow("You failed to kill a boss mob.")), 1);
}

/** :148-189, 258-290: each boss killed in time spawns its chests and clears the quest timer; all six: the bonus chest */
TEST_F(HexwayTest, SixBossesInTimeBringTheBonusChest) {
	world::WorldMapInstance& instance = hexway();
	enter(instance, 300.0f, 500.0f, 365.0f);
	instance.getInstanceHandler()->onPassFlyingRing(player(), "HEXWAY_BONUSCHEST");
	for (size_t i = 0; i < BOSSES.size(); ++i) {
		runtime::Ptr<Npc> boss = at(instance, BOSSES[i]);
		instance.getInstanceHandler()->onPassFlyingRing(player(), "HEXWAY_BOSS_" + std::to_string(i));
		(*client)->clearSent();
		instance.getInstanceHandler()->onDie(*boss);
		EXPECT_EQ(countSent(serialized(SM_QUEST_ACTION(0, 0))), 1) << i;
		EXPECT_EQ(npcs(instance, 701663).size(), i + 1);
		EXPECT_EQ(npcs(instance, 701662).size(), i + 1);
	}
	ASSERT_EQ(npcs(instance, 701664).size(), 1u);
	EXPECT_FLOAT_EQ(npcs(instance, 701664).front()->getX(), 485.59f);
	EXPECT_EQ(countSent(yellow("You have successfully completed the instance. A bonus chest was spawned.")), 1);
	(*client)->clearSent();
	executor().advance(std::chrono::seconds(1320));
	EXPECT_EQ(countSent(yellow("Your time to unlock a bonus chest has expired!")), 0) << "the expiry is cancelled";
}

/** :141-145: the manager's death opens the barricades */
TEST_F(HexwayTest, TheManagerOpensTheBarricades) {
	world::WorldMapInstance& instance = hexway();
	runtime::Ptr<Npc> barricade = at(instance, 219617);
	instance.getInstanceHandler()->onDie(*at(instance, 219609));
	EXPECT_FALSE(barricade->isSpawned());
}

/** :191-211: one twin of the second bridge dies: the boss takes 0.8 x the other twin's HP below 25 % a second later, the other twin goes */
TEST_F(HexwayTest, TheSecondBridgesTwins) {
	world::WorldMapInstance& instance = hexway();
	runtime::Ptr<Npc> boss = at(instance, 286933);
	runtime::Ptr<Npc> twin = at(instance, 219636);
	twin->getLifeStats()->setCurrentHpPercent(15);
	int32_t maxHp = boss->getLifeStats()->getMaxHp();
	instance.getInstanceHandler()->onDie(*at(instance, 219635));
	EXPECT_FALSE(twin->isSpawned());
	EXPECT_EQ(boss->getLifeStats()->getCurrentHp(), maxHp);
	executor().advance(std::chrono::seconds(1));
	EXPECT_EQ(boss->getLifeStats()->getCurrentHp(), maxHp - static_cast<int32_t>(maxHp * (10 * 0.8 / 100)));
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
