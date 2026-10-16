// P6I-9 (docs/design/p6-instances-plan.md §4): Eternal Bastion (game-server/data/handlers/instance/EternalBastionInstance.java) and its AIs
// (data/handlers/ai/instance/eternalBastion) on real instances of the map, the fixture's DeterministicExecutor driving the start, the
// waves and the time-out. The player stands in another instance (an Ophidan Bridge) and joins through onEnterInstance: the end's rewards (no
// item rows), the revive teleport (HousingService loads from the database), the walks, the skills (no skill rows) and the cannons' and
// tank's mounting are left to the retest.

#include "I3TestSupport.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionAggressiveNpcAI.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionAssaultMachineAI.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionAssaulterNpcAI.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionBomberAI.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionCommanderAI.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionCommanderPashidAI.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionConstructAI.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionDragonAI.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionMountableAI.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionShatterMineAI.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionSiegeMineAI.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionSiegeRamAI.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionSnareTurretAI.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionSummonerAI.h"
#include "aion/gameserver/handlers/instance/EternalBastionInstance.h"
#include "aion/gameserver/handlers/instance/OphidanBridgeInstance.h"
#include "aion/gameserver/model/instance/InstanceProgressionType.h"
#include "aion/gameserver/model/instance/instancescore/NormalScore.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {
::aion::gameserver::handlers::AIFactory EternalBastionAggressiveNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory EternalBastionAssaultMachineAI_aiFactory;
::aion::gameserver::handlers::AIFactory EternalBastionAssaulterNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory EternalBastionBomberAI_aiFactory;
::aion::gameserver::handlers::AIFactory EternalBastionCommanderAI_aiFactory;
::aion::gameserver::handlers::AIFactory EternalBastionCommanderPashidAI_aiFactory;
::aion::gameserver::handlers::AIFactory EternalBastionConstructAI_aiFactory;
::aion::gameserver::handlers::AIFactory EternalBastionDragonAI_aiFactory;
::aion::gameserver::handlers::AIFactory EternalBastionMountableAI_aiFactory;
::aion::gameserver::handlers::AIFactory EternalBastionShatterMineAI_aiFactory;
::aion::gameserver::handlers::AIFactory EternalBastionSiegeMineAI_aiFactory;
::aion::gameserver::handlers::AIFactory EternalBastionSiegeRamAI_aiFactory;
::aion::gameserver::handlers::AIFactory EternalBastionSnareTurretAI_aiFactory;
::aion::gameserver::handlers::AIFactory EternalBastionSummonerAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::instance::eternalBastion

namespace aion::gameserver::handlers::instance {
/** The score of a run (EternalBastionInstance's friend: Java's handler exposes no getInstanceScore) */
struct EternalBastionInstanceTestAccess {
	static model::instance::instancescore::NormalScore& scoreOf(world::WorldMapInstance& instance) {
		return *dynamic_cast<EternalBastionInstance&>(*instance.getInstanceHandler()).instanceReward.get();
	}
};
} // namespace aion::gameserver::handlers::instance

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace eb = ::aion::gameserver::handlers::ai::instance::eternalBastion;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Npc;
using model::instance::InstanceProgressionType;
using model::instance::instancescore::NormalScore;

class EternalBastionTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		enter(create(OPHIDAN_BRIDGE, OphidanBridgeInstance_instanceFactory), 300.0f, 300.0f, 300.0f);
	}

	static NormalScore& scoreOf(world::WorldMapInstance& instance) { return EternalBastionInstanceTestAccess::scoreOf(instance); }

	world::WorldMapInstance& bastion() { return create(ETERNAL_BASTION, EternalBastionInstance_instanceFactory); }

	/** the bastion after the three minutes of preparation, the player joined from outside */
	world::WorldMapInstance& started() {
		world::WorldMapInstance& instance = bastion();
		instance.getInstanceHandler()->onEnterInstance(player());
		executor().advance(std::chrono::milliseconds(180000));
		return instance;
	}

	runtime::Ptr<Npc> killed(world::WorldMapInstance& instance, int32_t npcId) {
		runtime::Ptr<Npc> npc = spawnNpc(instance, npcId, 700.0f, 300.0f, 230.0f, int8_t{0});
		instance.getInstanceHandler()->onDie(*npc);
		return npc;
	}

	static size_t count(world::WorldMapInstance& instance, std::initializer_list<int32_t> ids) {
		size_t n = 0;
		for (int32_t id : ids)
			n += npcs(instance, id).size();
		return n;
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}
};

TEST_F(EternalBastionTest, TheHandlerIsTheJavaClassOfItsMap) {
	std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance/EternalBastionInstance.java");
	std::stringstream text;
	text << in.rdbuf();
	std::string source = text.str();
	std::smatch match;
	ASSERT_TRUE(std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))")));
	EXPECT_EQ(std::stoi(match[1].str()), ETERNAL_BASTION);
	EXPECT_NE(dynamic_cast<EternalBastionInstance*>(bastion().getInstanceHandler().get()), nullptr);
}

/** EternalBastionInstance.java:353-386: 20,000 points; after three minutes the start, the scout commanders, the outer water gate and two
 * assault pods (6 and 9 s later) */
TEST_F(EternalBastionTest, TheStart) {
	world::WorldMapInstance& instance = bastion();
	EXPECT_EQ(scoreOf(instance).getPoints(), 20000);
	EXPECT_EQ(scoreOf(instance).getInstanceProgressionType(), InstanceProgressionType::PREPARING);
	executor().advance(std::chrono::milliseconds(179999));
	EXPECT_EQ(scoreOf(instance).getInstanceProgressionType(), InstanceProgressionType::PREPARING);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(scoreOf(instance).getInstanceProgressionType(), InstanceProgressionType::START_PROGRESS);
	EXPECT_EQ(count(instance, {231168, 231169, 231170}), 3u);
	ASSERT_EQ(count(instance, {831334}), 1u);
	EXPECT_EQ(npcs(instance, 831334).front()->getSpawn()->getStaticId(), 271);
	EXPECT_EQ(count(instance, {233313}), 6u);
	EXPECT_EQ(count(instance, {231162, 231167}), 0u);
	executor().advance(std::chrono::milliseconds(6000));
	EXPECT_EQ(count(instance, {231162}), 1u);
	executor().advance(std::chrono::milliseconds(3000));
	EXPECT_EQ(count(instance, {231167}), 1u);
}

/** :312-350: the first entry sets the race: an Elyos commander, flag, cannon, 16 guards and 12 cannons; Tariksha at one of two places */
TEST_F(EternalBastionTest, TheFirstEntrySpawnsTheRacesDefenders) {
	world::WorldMapInstance& instance = bastion();
	instance.getInstanceHandler()->onEnterInstance(player());
	EXPECT_EQ(count(instance, {209516}), 1u);
	EXPECT_EQ(count(instance, {209517}), 0u);
	EXPECT_EQ(count(instance, {701923}), 1u);
	EXPECT_EQ(count(instance, {701625}), 1u);
	EXPECT_EQ(count(instance, {209555}), 16u);
	EXPECT_EQ(count(instance, {701596, 701597, 701598, 701599, 701600, 701601, 701602, 701603, 701604, 701605, 701606, 701607}), 12u);
	ASSERT_EQ(count(instance, {231177}), 1u);
	EXPECT_TRUE(npcs(instance, 231177).front()->getX() == 821.146f || npcs(instance, 231177).front()->getX() == 551.146f);
	EXPECT_EQ(count(instance, {230746, 231149, 231181}), 3u);
	instance.getInstanceHandler()->onEnterInstance(player());
	EXPECT_EQ(count(instance, {209516}), 1u) << "once";
}

/** :63-73, 180-200: before the start no points; three commanders open the first phase (the outer water gate goes, eight elite guards) */
TEST_F(EternalBastionTest, ThreeCommandersOpenThePhase) {
	world::WorldMapInstance& early = bastion();
	killed(early, 231168);
	EXPECT_EQ(scoreOf(early).getPoints(), 20000) << "PREPARING: no points";

	world::WorldMapInstance& instance = started();
	killed(instance, 231168);
	EXPECT_EQ(scoreOf(instance).getPoints(), 21880);
	killed(instance, 231169);
	EXPECT_EQ(count(instance, {233314}), 0u);
	runtime::Ptr<Npc> gate = npcs(instance, 831334).front();
	killed(instance, 231170);
	EXPECT_EQ(scoreOf(instance).getPoints(), 25640);
	EXPECT_FALSE(gate->isSpawned()) << "the outer water gate";
	EXPECT_EQ(count(instance, {233314}), 4u);
	EXPECT_EQ(count(instance, {233315}), 6u + 4u) << "the start's six and the phase's four";
	EXPECT_EQ(count(instance, {231171}), 0u);
	executor().advance(std::chrono::milliseconds(90000));
	EXPECT_EQ(count(instance, {231171}), 1u) << "Matuk after 90 s";
	EXPECT_EQ(count(instance, {231173, 231172}), 2u);
}

/** :74-150: the npcs' points; the assault commanders and the wall */
TEST_F(EternalBastionTest, ThePoints) {
	world::WorldMapInstance& instance = started();
	killed(instance, 231143); // a siege tower
	EXPECT_EQ(scoreOf(instance).getPoints(), 20334);
	killed(instance, 231115);
	EXPECT_EQ(scoreOf(instance).getPoints(), 20367);
	killed(instance, 231181); // a plain barricade
	EXPECT_EQ(scoreOf(instance).getPoints(), 20633);
	runtime::Ptr<Npc> rightGate = spawnNpc(instance, 831332, 700.0f, 300.0f, 230.0f, int8_t{0});
	killed(instance, 831333); // the castle wall
	EXPECT_EQ(scoreOf(instance).getPoints(), 20483);
	EXPECT_FALSE(rightGate->isSpawned()) << "the right castle gate goes with the wall";
	killed(instance, 209555); // a disciple
	EXPECT_EQ(scoreOf(instance).getPoints(), 20433);
}

/** :363, 398-407: the first assault wave a minute after the start comes from the pods there are */
TEST_F(EternalBastionTest, TheFirstWaveComesFromThePods) {
	world::WorldMapInstance& instance = started();
	executor().advance(std::chrono::milliseconds(59999));
	EXPECT_EQ(count(instance, {231106, 231108}), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(instance, {231106}), 1u) << "the assault pod 231162 (spawned 6 s after the start): one squad";
	EXPECT_EQ(count(instance, {231108}), 2u);
}

/** :113-120, 688-765: Pashid's death ends the run: 44,000 points, a C-Rank (11,000 AP), the C chest and the exit */
TEST_F(EternalBastionTest, PashidsDeathEndsTheRun) {
	world::WorldMapInstance& instance = started();
	runtime::Ptr<Npc> bystander = spawnNpc(instance, 900200, 700.0f, 300.0f, 230.0f, int8_t{0});
	killed(instance, 231130);
	NormalScore& score = scoreOf(instance);
	EXPECT_EQ(score.getPoints(), 44000);
	EXPECT_EQ(score.getInstanceProgressionType(), InstanceProgressionType::END_PROGRESS);
	EXPECT_EQ(score.getRank(), 4);
	EXPECT_EQ(score.getFinalAp(), 11000);
	EXPECT_EQ(score.getRewardItem1(), 188052598);
	EXPECT_FALSE(bystander->isSpawned());
	EXPECT_EQ(count(instance, {701916}), 1u);
	EXPECT_EQ(count(instance, {730871}), 1u);
}

/** :109-112, 724-737: the fortress commander's death ends the run with an F (no chest) */
TEST_F(EternalBastionTest, TheCommandersDeathFailsTheRun) {
	world::WorldMapInstance& instance = started();
	killed(instance, 209516);
	EXPECT_EQ(scoreOf(instance).getInstanceProgressionType(), InstanceProgressionType::END_PROGRESS);
	EXPECT_EQ(scoreOf(instance).getRank(), 8);
	EXPECT_EQ(count(instance, {701913, 701914, 701915, 701916, 701917}), 0u);
	EXPECT_EQ(count(instance, {730871}), 1u);
}

/** :364, 668-671: after 30 minutes the run ends */
TEST_F(EternalBastionTest, TheTimeOut) {
	world::WorldMapInstance& instance = started();
	executor().advance(std::chrono::milliseconds(30 * 60 * 1000 - 1));
	EXPECT_EQ(scoreOf(instance).getInstanceProgressionType(), InstanceProgressionType::START_PROGRESS);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(scoreOf(instance).getInstanceProgressionType(), InstanceProgressionType::END_PROGRESS);
}

/** The fourteen AION_AI factories; the constructs and the no-reward npcs */
TEST_F(EternalBastionTest, TheAis) {
	world::WorldMapInstance& instance = bastion();
	runtime::Ptr<Npc> npc = spawnNpc(instance, 900200, 700.0f, 300.0f, 230.0f, int8_t{0});
	EXPECT_NE(dynamic_cast<eb::EternalBastionAggressiveNpcAI*>(eb::EternalBastionAggressiveNpcAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<eb::EternalBastionAssaultMachineAI*>(eb::EternalBastionAssaultMachineAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<eb::EternalBastionAssaulterNpcAI*>(eb::EternalBastionAssaulterNpcAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<eb::EternalBastionBomberAI*>(eb::EternalBastionBomberAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<eb::EternalBastionCommanderAI*>(eb::EternalBastionCommanderAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<eb::EternalBastionCommanderPashidAI*>(eb::EternalBastionCommanderPashidAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<eb::EternalBastionConstructAI*>(eb::EternalBastionConstructAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<eb::EternalBastionDragonAI*>(eb::EternalBastionDragonAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<eb::EternalBastionMountableAI*>(eb::EternalBastionMountableAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<eb::EternalBastionShatterMineAI*>(eb::EternalBastionShatterMineAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<eb::EternalBastionSiegeMineAI*>(eb::EternalBastionSiegeMineAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<eb::EternalBastionSiegeRamAI*>(eb::EternalBastionSiegeRamAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<eb::EternalBastionSnareTurretAI*>(eb::EternalBastionSnareTurretAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<eb::EternalBastionSummonerAI*>(eb::EternalBastionSummonerAI_aiFactory(*npc).get()), nullptr);

	auto& construct = install<eb::EternalBastionConstructAI>(*spawnNpc(instance, 831333, 700.0f, 300.0f, 230.0f, int8_t{0}));
	EXPECT_FALSE(construct.canThink());
	EXPECT_TRUE(construct.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
	EXPECT_FALSE(construct.ask(AIQuestion::REWARD_LOOT));
	EXPECT_FALSE(construct.ask(AIQuestion::REWARD_AP));
	auto& aggressive = install<eb::EternalBastionAggressiveNpcAI>(*spawnNpc(instance, 900200, 700.0f, 300.0f, 230.0f, int8_t{0}));
	EXPECT_FALSE(aggressive.ask(AIQuestion::REWARD_LOOT));
	EXPECT_FALSE(aggressive.ask(AIQuestion::REWARD_AP));
	auto& summoner = install<eb::EternalBastionSummonerAI>(*spawnNpc(instance, 900200, 700.0f, 300.0f, 230.0f, int8_t{0}));
	EXPECT_FALSE(summoner.ask(AIQuestion::REWARD_LOOT));
	auto& dragon = install<eb::EternalBastionDragonAI>(*spawnNpc(instance, 900200, 700.0f, 300.0f, 230.0f, int8_t{0}));
	EXPECT_TRUE(dragon.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
