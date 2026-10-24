// P6I-11 (docs/design/p6-instances-plan.md, I4 split): BeshmundirInstance (game-server/data/handlers/instance) on a real instance of its map.
// The doors (the fixture has no static doors: setDoorState only warns) and the Macunbellos' effects are left to a gate.

#include "I4TestSupport.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <utility>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/handlers/instance/BeshmundirInstance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using network::aion::serverpackets::SM_PLAY_MOVIE;
using network::aion::serverpackets::SM_QUEST_ACTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

class BeshmundirTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(BESHMUNDIR_TEMPLE, BeshmundirInstance_instanceFactory);
		enter(*instance, 300.0f, 300.0f, 300.0f);
	}

	runtime::Ptr<Npc> at(int32_t npcId, float x = 300.0f, float y = 300.0f) { return spawnNpc(*instance, npcId, x, y, 300.0f, int8_t{0}); }

	runtime::Ptr<Npc> dies(int32_t npcId) {
		runtime::Ptr<Npc> npc = at(npcId);
		instance->getInstanceHandler()->onDie(*npc);
		return npc;
	}

	size_t count(std::initializer_list<int32_t> ids) {
		size_t n = 0;
		for (int32_t id : ids)
			n += npcs(*instance, id).size();
		return n;
	}

	/** The Macunbellos get an effect (applyEffectDirectly asks for the skill rows and an NpcAI): a root AI on each as it spawns */
	void macunbelloRows() { publishSkills({19046, 19047, 19048}); }

	world::WorldMapInstance* instance = nullptr;
};

TEST_F(BeshmundirTest, TheHandlerIsTheJavaClassOfItsMap) {
	std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance/BeshmundirInstance.java");
	std::stringstream text;
	text << in.rdbuf();
	std::string source = text.str();
	std::smatch match;
	ASSERT_TRUE(std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))")));
	EXPECT_EQ(std::stoi(match[1].str()), BESHMUNDIR_TEMPLE);
	EXPECT_NE(dynamic_cast<BeshmundirInstance*>(instance->getInstanceHandler().get()), nullptr);
	EXPECT_FLOAT_EQ(instance->getInstanceHandler()->getExpMultiplier(), 3.0f);
}

/** BeshmundirInstance.java:49-60: each soulwatcher (both modes) brings its boatman */
TEST_F(BeshmundirTest, TheSoulwatchersBringTheBoatmen) {
	const std::pair<int32_t, int32_t> boatmen[] = {{216583, 799518}, {216587, 799518}, {216584, 799519}, {216588, 799519}, {216585, 799520},
		{216589, 799520}};
	size_t expected[3] = {0, 0, 0};
	for (const auto& [dead, boatman] : boatmen) {
		dies(dead);
		++expected[boatman - 799518];
		EXPECT_EQ(count({boatman}), expected[boatman - 799518]) << dead;
	}
	EXPECT_FLOAT_EQ(npcs(*instance, 799518).front()->getX(), 936.0029f);
	EXPECT_EQ(npcs(*instance, 799520).front()->getHeading(), 55);
}

/** :61-80, 126-137: the Lich's servants' kills weaken the next Macunbello (12, 14, 21 tell it); Temadaro's death brings him, starts over and
 * stops the timer */
TEST_F(BeshmundirTest, TheServantsKillsWeakenMacunbello) {
	macunbelloRows();
	for (int32_t i = 1; i <= 21; ++i) {
		dies(216206 + (i % 8));
		EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_NmdLich_weakness1())), i >= 12 ? 1 : 0) << i;
		EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_NmdLich_weakness2())), i >= 14 ? 1 : 0) << i;
		EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_NmdLich_weakness3())), i >= 21 ? 1 : 0) << i;
	}
	dies(216590);
	EXPECT_EQ(count({216245}), 1u) << "the weakest";
	EXPECT_EQ(count({216735, 216734, 216737}), 0u);
	EXPECT_FLOAT_EQ(npcs(*instance, 216245).front()->getX(), 981.015015f);
	EXPECT_EQ(countSent(serialized(SM_QUEST_ACTION(0, 0))), 1);
}

/** :63-74: under 12 kills the strongest, under 14 the second, under 21 the third; the count starts over with each Temadaro */
TEST_F(BeshmundirTest, MacunbellosStrengthFollowsTheKills) {
	macunbelloRows();
	const std::pair<int32_t, int32_t> rounds[] = {{11, 216735}, {13, 216734}, {20, 216737}, {0, 216735}, {12, 216734}, {14, 216737}};
	for (const auto& [kills, macunbello] : rounds) {
		for (int32_t i = 0; i < kills; ++i)
			dies(216213);
		size_t before = count({macunbello});
		dies(216586);
		EXPECT_EQ(count({macunbello}), before + 1) << kills;
	}
	EXPECT_EQ(count({216245}), 0u);
}

/** :84-90: the guardian's death brings the next one */
TEST_F(BeshmundirTest, TheGuardiansBringTheNext) {
	dies(216157);
	dies(216238);
	EXPECT_EQ(count({216159}), 2u);
	EXPECT_FLOAT_EQ(npcs(*instance, 216159).front()->getY(), 388.6637f);
	dies(216165);
	EXPECT_EQ(count({216159}), 2u);
}

/** :91-101: nine specters tell they come, the tenth brings their master */
TEST_F(BeshmundirTest, TheTenthSpecterBringsTheMaster) {
	for (int32_t i = 1; i <= 9; ++i)
		dies(i % 2 ? 216739 : 216740);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_NmdSpecter_Spawn())), 9);
	EXPECT_EQ(count({216158}), 0u);
	dies(216740);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_NmdSpecter_Start())), 1);
	EXPECT_EQ(count({216158}), 1u);
	dies(216739);
	EXPECT_EQ(count({216158}), 1u);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_NmdSpecter_Spawn())), 9) << "nothing after the tenth";
}

/** :38-41, 106-110: the first player's race decides: an Asmodian instance gets 799342 when 700608 dies, whose death plays the movie 447 */
TEST_F(BeshmundirTest, TheAsmodiansGetTheirGuide) {
	player().getCommonData()->setRace(model::Race::ASMODIANS);
	instance->getInstanceHandler()->onEnterInstance(player());
	player().getCommonData()->setRace(model::Race::ELYOS);
	instance->getInstanceHandler()->onEnterInstance(player());
	dies(700608);
	ASSERT_EQ(count({799342}), 1u) << "the first race stays";
	EXPECT_FLOAT_EQ(npcs(*instance, 799342).front()->getX(), 1357.1f);
	instance->getInstanceHandler()->onDie(*npcs(*instance, 799342).front());
	EXPECT_EQ(countSent(serialized(SM_PLAY_MOVIE(false, 0, 0, 447, true))), 1);
}

/** :106-110: an Elyos instance gets nothing */
TEST_F(BeshmundirTest, TheElyosGetNoGuide) {
	player().getCommonData()->setRace(model::Race::ELYOS);
	instance->getInstanceHandler()->onEnterInstance(player());
	dies(700608);
	EXPECT_EQ(count({799342}), 0u);
}

/** :111-119: Isbariya's death brings the super boss and the gate (static id 426) and tells the orb */
TEST_F(BeshmundirTest, IsbariyaBringsTheSuperBoss) {
	dies(216263);
	EXPECT_EQ(count({216183}), 1u);
	ASSERT_EQ(count({730275}), 1u);
	EXPECT_EQ(npcs(*instance, 730275).front()->getSpawn()->getStaticId(), 426);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_BigOrb_Spawn())), 1);
	dies(216182);
	EXPECT_EQ(count({216183}), 2u);
}

/** :120-124: Dorakiki's death brings Lupukin */
TEST_F(BeshmundirTest, DorakikiBringsLupukin) {
	dies(216250);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_NmdShulack_Rufukin())), 1);
	ASSERT_EQ(count({216527}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 216527).front()->getX(), 1161.859985f);
	dies(216169);
	EXPECT_EQ(count({216527}), 2u);
}

/** :144-148: the movie 443's end tells the orb */
TEST_F(BeshmundirTest, TheMovie443TellsTheOrb) {
	instance->getInstanceHandler()->onPlayMovieEnd(player(), 442);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_BigOrb_Spawn())), 0);
	instance->getInstanceHandler()->onPlayMovieEnd(player(), 443);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_BigOrb_Spawn())), 1);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
