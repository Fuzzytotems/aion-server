// P6-I2 the Illuminary Obelisks (game-server/data/handlers/instance/IlluminaryObeliskInstance.java and InfernalIlluminaryObelisk.java) on
// real instances of their maps. Left out:
// - the teleports (the use of the shield exits, the revive, leaveInstance);
// - the bomb-carrier effect 21511 and its re-spawned bomb 702009 (no effect data);
// - the walks of the wave npcs (no walker routes in the test data: the walker ids are set and their walk is attempted);
// - the door 129 (no doors in the test world).

#include "I2TestSupport.h"
#include "CrucibleTestSupport.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "aion/gameserver/handlers/instance/IlluminaryObeliskInstance.h"
#include "aion/gameserver/handlers/instance/InfernalIlluminaryObelisk.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

class IlluminaryObeliskTestBase : public InstanceHandlerTest {
protected:
	void open(int32_t mapId, ::aion::gameserver::handlers::InstanceFactory& factory) {
		publishCrucibleData(context); // the walker holder the wave spawns read
		instance = &create(mapId, factory);
		enter(*instance, 315.0f, 307.0f, 405.5f);
		handler().onEnterInstance(player());
		(*client)->clearSent();
	}

	void TearDown() override {
		InstanceHandlerTest::TearDown();
		resetCrucibleData();
	}

	::aion::gameserver::instance::handlers::InstanceHandler& handler() { return *instance->getInstanceHandler(); }

	IlluminaryObeliskInstance& obelisk() { return dynamic_cast<IlluminaryObeliskInstance&>(handler()); }

	size_t count(int32_t npcId) { return npcs(*instance, npcId).size(); }

	size_t countAll(std::initializer_list<int32_t> ids) {
		size_t n = 0;
		for (int32_t id : ids)
			n += count(id);
		return n;
	}

	runtime::Ptr<Npc> place(int32_t npcId) { return spawnNpc(*instance, npcId, 255.0f, 254.0f, 300.0f, int8_t{0}); }

	void after(int64_t millis) { executor().advance(std::chrono::milliseconds(millis)); }

	int64_t said(SM_SYSTEM_MESSAGE&& msg) { return countSent(serialized(std::move(msg))); }

	world::WorldMapInstance* instance = nullptr;
};

class IlluminaryObeliskTest : public IlluminaryObeliskTestBase {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		open(ILLUMINARY_OBELISK, IlluminaryObeliskInstance_instanceFactory);
	}
};

class InfernalIlluminaryObeliskTest : public IlluminaryObeliskTestBase {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		open(INFERNAL_ILLUMINARY_OBELISK, InfernalIlluminaryObelisk_instanceFactory);
	}
};

int32_t javaInstanceId(const char* file) {
	std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / file);
	std::stringstream text;
	text << in.rdbuf();
	const std::string source = text.str();
	std::smatch match;
	return std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))")) ? std::stoi(match[1].str()) : 0;
}

TEST_F(IlluminaryObeliskTest, TheHandlerIsTheJavaClassOfItsMap) {
	EXPECT_EQ(javaInstanceId("data/handlers/instance/IlluminaryObeliskInstance.java"), ILLUMINARY_OBELISK);
	EXPECT_NE(dynamic_cast<IlluminaryObeliskInstance*>(&handler()), nullptr);
	EXPECT_EQ(dynamic_cast<InfernalIlluminaryObelisk*>(&handler()), nullptr);
}

TEST_F(InfernalIlluminaryObeliskTest, TheHandlerIsTheJavaClassOfItsMap) {
	EXPECT_EQ(javaInstanceId("data/handlers/instance/InfernalIlluminaryObelisk.java"), INFERNAL_ILLUMINARY_OBELISK);
	EXPECT_NE(dynamic_cast<InfernalIlluminaryObelisk*>(&handler()), nullptr);
}

/** IlluminaryObeliskInstance.java:329-336: the first player's race picks the commander, once */
TEST_F(IlluminaryObeliskTest, TheCommanderOfTheFirstRace) {
	EXPECT_EQ(count(802048), 1u) << "an Elyos player";
	EXPECT_EQ(count(802049), 0u);
	handler().onEnterInstance(player());
	EXPECT_EQ(count(802048), 1u) << "once";
}

/** :41-93: the door after a minute, the half-hour timer's messages, then the wipe 5 s after the last */
TEST_F(IlluminaryObeliskTest, TheTimerAndTheWipe) {
	after(59999);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_DOOR_OPEN()), 0);
	after(1);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_DOOR_OPEN()), 1);
	after(3000);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_GAME_TIMER_01()), 1);
	after(300000);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_GAME_TIMER_02()), 1);
	after(300001);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_GAME_TIMER_03()), 1);
	after(300002);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_GAME_TIMER_04()), 1);
	after(300003);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_GAME_TIMER_05()), 1);
	after(300004);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_GAME_TIMER_06()), 1);
	after(239999);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_GAME_TIMER_07()), 0);
	after(1);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_GAME_TIMER_07()), 1);
	after(60000);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_GAME_TIMER_08()), 1);
	runtime::Ptr<Npc> npc = place(233720);
	EXPECT_FALSE(player().isDead());
	after(4999);
	EXPECT_TRUE(npc->isSpawned());
	after(1);
	EXPECT_FALSE(npc->isSpawned()) << "the wipe deletes the npcs";
	EXPECT_EQ(count(802048), 0u);
	EXPECT_TRUE(player().isDead()) << "and kills the players";
}

/** :95-135, 137-150: a first charge of the east generator: its message and three waves of three walkers */
TEST_F(IlluminaryObeliskTest, TheEastFirstWave) {
	place(702218);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_01()), 1);
	after(1);
	EXPECT_EQ(count(233720), 1u);
	EXPECT_EQ(count(233721), 2u);
	runtime::Ptr<Npc> walker = npcs(*instance, 233720)[0];
	EXPECT_EQ(walker->getSpawn()->getWalkerId(), std::optional<std::string>("idf5_u3_east_2"));
	after(15000);
	EXPECT_EQ(count(233722), 1u);
	EXPECT_EQ(count(233720), 3u);
	after(15000);
	EXPECT_EQ(count(233723), 1u);
	EXPECT_EQ(count(233726), 2u);
}

/** :95-135: each charge's message */
TEST_F(IlluminaryObeliskTest, TheChargeMessages) {
	const std::vector<std::pair<int32_t, std::function<SM_SYSTEM_MESSAGE()>>> charges{{702219, &SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_01},
		{702221, &SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_02}, {702222, &SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_02},
		{702224, &SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_03}, {702225, &SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_03},
		{702227, &SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_04}, {702228, &SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_CHARGE_04},
		{702223, &SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_FINAL_CHARGE_02}, {702226, &SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_FINAL_CHARGE_03},
		{702229, &SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_FINAL_CHARGE_04}};
	for (const auto& [npcId, msg] : charges) {
		(*client)->clearSent();
		place(npcId);
		EXPECT_EQ(said(msg()), 1) << npcId;
	}
}

/** :137-277: one npc of each wave, at its delay */
TEST_F(IlluminaryObeliskTest, TheWaves) {
	place(702227); // north first wave: the hidden ones at 30 s
	after(29999);
	EXPECT_EQ(countAll({233882, 233883}), 0u);
	after(1);
	EXPECT_EQ(count(233882), 2u);
	EXPECT_EQ(count(233883), 1u);
	place(702220); // east third wave
	after(1);
	EXPECT_EQ(count(233736), 1u);
	EXPECT_EQ(count(233733), 1u);
	place(702223); // west third wave
	after(1);
	EXPECT_EQ(count(233737), 1u);
	after(15000);
	EXPECT_EQ(count(233731), 1u);
	place(702226); // south third wave
	after(15000);
	EXPECT_EQ(count(233738), 1u);
	EXPECT_EQ(count(233735), 1u);
}

/** :258-276: the north third wave (alone: with the other three third charges the generators' check would end the waves) */
TEST_F(IlluminaryObeliskTest, TheNorthThirdWave) {
	place(702229);
	after(1);
	EXPECT_EQ(count(233731), 1u);
	after(15000);
	EXPECT_EQ(count(233739), 1u);
}

/** :279-300: the four third charges: all npcs go, the four shield exits and Dynatoum come 30 s after the last */
TEST_F(IlluminaryObeliskTest, AllGeneratorsCharged) {
	place(702220);
	place(702223);
	place(702226);
	after(30000);
	EXPECT_EQ(count(233740), 0u) << "three of four: nothing";
	place(702229);
	after(29999);
	EXPECT_EQ(count(233740), 0u);
	(*client)->clearSent();
	after(1);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_OBJ_ALL_COMPLETE()), 1);
	EXPECT_EQ(count(702229), 0u) << "the npcs are deleted";
	EXPECT_EQ(count(802048), 0u);
	EXPECT_EQ(count(730886), 4u);
	EXPECT_EQ(count(233740), 1u) << "Dynatoum";
	after(2000000);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_GAME_TIMER_08()), 0) << "the timer is cancelled";
}

/** :355-361, 391-394: Dynatoum's death opens the exit; it is the boss */
TEST_F(IlluminaryObeliskTest, DynatoumDies) {
	runtime::Ptr<Npc> boss = place(233740);
	EXPECT_TRUE(obelisk().isBoss(*boss));
	EXPECT_FALSE(obelisk().isBoss(*place(233720)));
	boss->getLifeStats()->setCurrentHp(0);
	EXPECT_EQ(count(730905), 1u);
	place(233720)->getLifeStats()->setCurrentHp(0);
	EXPECT_EQ(count(730905), 1u);
}

/** :385-389, 315-323: a destroyed instance spawns no more waves */
TEST_F(IlluminaryObeliskTest, DestroyedSpawnsNothing) {
	place(702218);
	handler().onInstanceDestroy();
	after(30000);
	EXPECT_EQ(count(233720), 0u);
	after(2000000);
	EXPECT_EQ(said(SM_SYSTEM_MESSAGE::STR_MSG_IDF5_U3_GAME_TIMER_01()), 0);
}

/** :315-323: a charge spawned after the destruction schedules its waves, but they spawn nothing */
TEST_F(IlluminaryObeliskTest, NoWaveAfterTheDestruction) {
	handler().onInstanceDestroy();
	place(702218);
	after(30000);
	EXPECT_EQ(count(233720) + count(233721) + count(233722), 0u);
}

/** InfernalIlluminaryObelisk.java: the infernal waves and Infernal Dynatoum */
TEST_F(InfernalIlluminaryObeliskTest, TheInfernalWavesAndBoss) {
	place(702218);
	after(1);
	EXPECT_EQ(count(234720), 1u);
	EXPECT_EQ(count(233720), 0u);
	place(702220);
	place(702223);
	place(702226);
	place(702229);
	after(30000);
	EXPECT_EQ(count(234686), 1u) << "Infernal Dynatoum";
	EXPECT_EQ(count(233740), 0u);
	EXPECT_TRUE(obelisk().isBoss(*npcs(*instance, 234686)[0]));
	npcs(*instance, 234686)[0]->getLifeStats()->setCurrentHp(0);
	EXPECT_EQ(count(730905), 1u);
}

/** InfernalIlluminaryObelisk.java: a third wave's elite each */
TEST_F(InfernalIlluminaryObeliskTest, TheInfernalElites) {
	place(702220);
	place(702223);
	after(1);
	EXPECT_EQ(count(234682), 1u);
	EXPECT_EQ(count(234683), 1u);
	place(702226);
	place(702229);
	after(15000);
	EXPECT_EQ(count(234684), 1u);
	EXPECT_EQ(count(234685), 1u);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
