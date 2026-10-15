// P6I-8c (docs/design/p6-instances-plan.md §4): the PvP arenas of chunk I3 without HarmonyArenaScore - PvPArenaInstance and
// DisciplineTrainingGroundsInstance, ArenaOfDisciplineInstance, ChaosTrainingGroundsInstance, ArenaOfChaosInstance, ArenaOfGloryInstance
// (game-server/data/handlers/instance/pvparenas/*.java) - on real instances of their maps, two client-less players inside, the fixture's
// DeterministicExecutor driving the rounds. The item rewards (no item rows), the bonus skills and the morale boost's effect (no skill rows),
// the fly rings' passes (no rings in the test world) and the exit teleport (HousingService loads from the database) are left to a gate.

#include "I3TestSupport.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/InstanceBuffData.bind.h"
#include "aion/gameserver/dataholders/InstanceBuffData.h"
#include "aion/gameserver/handlers/instance/OphidanBridgeInstance.h"
#include "aion/gameserver/handlers/instance/pvparenas/ArenaOfChaosInstance.h"
#include "aion/gameserver/handlers/instance/pvparenas/ArenaOfDisciplineInstance.h"
#include "aion/gameserver/handlers/instance/pvparenas/ArenaOfGloryInstance.h"
#include "aion/gameserver/handlers/instance/pvparenas/ChaosTrainingGroundsInstance.h"
#include "aion/gameserver/handlers/instance/pvparenas/DisciplineTrainingGroundsInstance.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/instance/InstanceProgressionType.h"
#include "aion/gameserver/model/instance/instancescore/PvPArenaScore.h"
#include "aion/gameserver/model/instance/playerreward/PvPArenaPlayerReward.h"
#include "aion/gameserver/model/templates/rewards/ArenaRewardItem.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using model::instance::InstanceProgressionType;
using model::instance::instancescore::PvPArenaScore;
using model::instance::playerreward::PvPArenaPlayerReward;

class ArenaTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		// the rewards' morale buffs (instance_bonusattr.xml shape, ArenaScoreTest's rows; ids 7 and 8 of the arenas)
		dataholders::DataManager::INSTANCE_BUFF_DATA.publish(xml::bindString<dataholders::InstanceBuffData>(context, R"(<instance_bonusattrs>)"
			R"(<instance_bonusattr buff_id="7"><penalty_attr stat="SPEED" func="PERCENT" value="50"/></instance_bonusattr>)"
			R"(<instance_bonusattr buff_id="8"><penalty_attr stat="SPEED" func="PERCENT" value="50"/></instance_bonusattr>)"
			R"(</instance_bonusattrs>)"));
		enter(create(OPHIDAN_BRIDGE, OphidanBridgeInstance_instanceFactory), 300.0f, 300.0f, 300.0f);
	}

	void TearDown() override {
		world::World& world = world::World::getInstance();
		for (cptest::PlayerFixture& f : inside) { // out before the instances are destroyed (their exit needs the exit rows)
			f.player->getController().cancelAllTasks();
			if (f.player->isSpawned())
				world.despawn(*f.player);
			world.removeObject(*f.player);
		}
		inside.clear();
		InstanceHandlerTest::TearDown();
		dataholders::DataManager::INSTANCE_BUFF_DATA.resetForTests();
	}

	static PvPArenaScore& scoreOf(world::WorldMapInstance& instance) {
		return dynamic_cast<PvPArenaScore&>(*instance.getInstanceHandler()->getInstanceScore());
	}

	static InstanceProgressionType phase(world::WorldMapInstance& instance) { return scoreOf(instance).getInstanceProgressionType(); }

	/** a client-less player of the race in the instance, entered through the handler */
	Player& join(world::WorldMapInstance& instance, int32_t objectId) {
		inside.push_back(cptest::makePlayer(objectId, objectId + 5000, "Fighter" + std::to_string(objectId)));
		Player& p = *inside.back().player;
		p.setMotions(std::make_unique<model::gameobjects::player::motion::MotionList>(p)); // the teleport packets read it
		world::World& world = world::World::getInstance();
		world.storeObject(p);
		EXPECT_TRUE(world.setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(p), instance.getMapId(), instance.getInstanceId(), 300.0f,
			300.0f, 300.0f, int8_t{0}));
		world.spawn(runtime::Ptr<model::gameobjects::VisibleObject>(p));
		instance.getInstanceHandler()->onEnterInstance(p);
		return p;
	}

	/** the arena with two players inside, started */
	world::WorldMapInstance& started(int32_t mapId, ::aion::gameserver::handlers::InstanceFactory& factory) {
		world::WorldMapInstance& instance = create(mapId, factory);
		join(instance, 430011);
		join(instance, 430012);
		executor().advance(std::chrono::milliseconds(120000));
		return instance;
	}

	static int32_t pointsOf(world::WorldMapInstance& instance, Player& player) {
		return scoreOf(instance).getPlayerReward(player.getObjectId())->getPoints();
	}

	static std::string javaSource(const std::string& javaClass) {
		std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance/pvparenas" / (javaClass + ".java"));
		std::stringstream text;
		text << in.rdbuf();
		return text.str();
	}

	std::vector<cptest::PlayerFixture> inside;
};

TEST_F(ArenaTest, EachHandlerIsTheJavaClassOfItsMap) {
	for (const auto& [javaClass, mapId] : std::map<std::string, int32_t>{{"ArenaOfChaosInstance", ARENA_OF_CHAOS},
		     {"ArenaOfDisciplineInstance", ARENA_OF_DISCIPLINE}, {"ChaosTrainingGroundsInstance", CHAOS_TRAINING_GROUNDS},
		     {"DisciplineTrainingGroundsInstance", DISCIPLINE_TRAINING_GROUNDS}, {"ArenaOfGloryInstance", ARENA_OF_GLORY}}) {
		std::string source = javaSource(javaClass);
		std::smatch match;
		ASSERT_TRUE(std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))"))) << javaClass;
		EXPECT_EQ(std::stoi(match[1].str()), mapId) << javaClass;
	}
	namespace a = pvparenas;
	EXPECT_NE(dynamic_cast<a::ArenaOfChaosInstance*>(create(ARENA_OF_CHAOS, a::ArenaOfChaosInstance_instanceFactory).getInstanceHandler().get()),
		nullptr);
	EXPECT_NE(dynamic_cast<a::ArenaOfDisciplineInstance*>(
		          create(ARENA_OF_DISCIPLINE, a::ArenaOfDisciplineInstance_instanceFactory).getInstanceHandler().get()),
		nullptr);
	EXPECT_NE(dynamic_cast<a::ChaosTrainingGroundsInstance*>(
		          create(CHAOS_TRAINING_GROUNDS, a::ChaosTrainingGroundsInstance_instanceFactory).getInstanceHandler().get()),
		nullptr);
	EXPECT_NE(dynamic_cast<a::DisciplineTrainingGroundsInstance*>(
		          create(DISCIPLINE_TRAINING_GROUNDS, a::DisciplineTrainingGroundsInstance_instanceFactory).getInstanceHandler().get()),
		nullptr);
	EXPECT_NE(dynamic_cast<a::ArenaOfGloryInstance*>(create(ARENA_OF_GLORY, a::ArenaOfGloryInstance_instanceFactory).getInstanceHandler().get()),
		nullptr);
}

/** PvPArenaInstance.java:46-68, 89-99: alone, the arena does not start: it ends after the two minutes of preparation */
TEST_F(ArenaTest, AnArenaWithOnePlayerEndsAtTheStart) {
	world::WorldMapInstance& instance = create(CHAOS_TRAINING_GROUNDS, pvparenas::ChaosTrainingGroundsInstance_instanceFactory);
	join(instance, 430011);
	runtime::Ptr<Npc> bystander = spawnNpc(instance, 900200, 300.0f, 300.0f, 300.0f, int8_t{0});
	EXPECT_EQ(phase(instance), InstanceProgressionType::PREPARING);
	executor().advance(std::chrono::milliseconds(119999));
	EXPECT_EQ(phase(instance), InstanceProgressionType::PREPARING);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(phase(instance), InstanceProgressionType::END_PROGRESS);
	EXPECT_FALSE(bystander->isSpawned());
	EXPECT_TRUE(scoreOf(instance).getPlayerReward(430011)->isRewarded());
}

/** :70-87: two players start it; three rounds of three minutes, the third the final one, then the end */
TEST_F(ArenaTest, TheRounds) {
	world::WorldMapInstance& instance = started(CHAOS_TRAINING_GROUNDS, pvparenas::ChaosTrainingGroundsInstance_instanceFactory);
	EXPECT_EQ(phase(instance), InstanceProgressionType::START_PROGRESS);
	EXPECT_EQ(scoreOf(instance).getRound(), 1);
	executor().advance(std::chrono::milliseconds(179999));
	EXPECT_EQ(scoreOf(instance).getRound(), 1);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(scoreOf(instance).getRound(), 2);
	executor().advance(std::chrono::milliseconds(180000));
	EXPECT_EQ(scoreOf(instance).getRound(), 3);
	EXPECT_EQ(phase(instance), InstanceProgressionType::START_PROGRESS);
	executor().advance(std::chrono::milliseconds(180000));
	EXPECT_EQ(phase(instance), InstanceProgressionType::END_PROGRESS);
}

/** :116-144 and Chaos Training :25-30: the players who damaged an npc share its points by damage; a kill gives the killer 1000, the victim
 * loses 125 */
TEST_F(ArenaTest, ChaosPointsForNpcsAndKills) {
	world::WorldMapInstance& instance = started(CHAOS_TRAINING_GROUNDS, pvparenas::ChaosTrainingGroundsInstance_instanceFactory);
	Player& a = *inside[0].player;
	Player& b = *inside[1].player;
	int32_t a0 = pointsOf(instance, a);
	int32_t b0 = pointsOf(instance, b);
	runtime::Ptr<Npc> gatherer = spawnNpc(instance, 218682, 300.0f, 300.0f, 300.0f, int8_t{0}); // MuMu Rake Gatherer: 1250
	InstanceKnownListPairing::pair(*gatherer, a);
	InstanceKnownListPairing::pair(*gatherer, b);
	gatherer->getAggroList().addDamage(a, 300, true, std::nullopt);
	gatherer->getAggroList().addDamage(b, 100, true, std::nullopt);
	instance.getInstanceHandler()->onDie(*gatherer);
	EXPECT_EQ(pointsOf(instance, a), a0 + 937) << "1250 * 300 / 400";
	EXPECT_EQ(pointsOf(instance, b), b0 + 312);
	runtime::Ptr<Npc> plain = spawnNpc(instance, 900200, 300.0f, 300.0f, 300.0f, int8_t{0});
	plain->getAggroList().addDamage(a, 300, true, std::nullopt);
	instance.getInstanceHandler()->onDie(*plain);
	EXPECT_EQ(pointsOf(instance, a), a0 + 937) << "no points";

	instance.getInstanceHandler()->onDie(b, a);
	EXPECT_EQ(pointsOf(instance, b), b0 + 312 - 125);
	EXPECT_EQ(scoreOf(instance).getPlayerReward(a.getObjectId())->getPvPKills(), 1);
}

/** :101-114 with Discipline Training :17-28: the victim's damagers share 200 points; the victim loses 100 */
TEST_F(ArenaTest, DisciplineKillPoints) {
	world::WorldMapInstance& instance = started(DISCIPLINE_TRAINING_GROUNDS, pvparenas::DisciplineTrainingGroundsInstance_instanceFactory);
	Player& a = *inside[0].player;
	Player& b = *inside[1].player;
	int32_t a0 = pointsOf(instance, a);
	int32_t b0 = pointsOf(instance, b);
	InstanceKnownListPairing::pair(b, a);
	b.getAggroList().addDamage(a, 100, true, std::nullopt);
	instance.getInstanceHandler()->onDie(b, a);
	EXPECT_EQ(pointsOf(instance, a), a0 + 200) << "the victim's damage list";
	EXPECT_EQ(pointsOf(instance, b), b0 - 100);
	EXPECT_EQ(scoreOf(instance).getPlayerReward(a.getObjectId())->getPvPKills(), 1);
}

/** :148-152, 465-532 with Arena of Glory :38-63: an interrupted run rewards as the last rank; the AP are the base AP */
TEST_F(ArenaTest, GlorysRewardsWhenAloneAreTheLastRanks) {
	world::WorldMapInstance& instance = create(ARENA_OF_GLORY, pvparenas::ArenaOfGloryInstance_instanceFactory);
	Player& alone = join(instance, 430011);
	executor().advance(std::chrono::milliseconds(120000));
	ASSERT_EQ(phase(instance), InstanceProgressionType::END_PROGRESS);
	runtime::Ptr<PvPArenaPlayerReward> reward = scoreOf(instance).getPlayerReward(alone.getObjectId());
	EXPECT_EQ(reward->getAp()->baseCount(), 0) << "Glory has no per-player base rewards";
	EXPECT_EQ(reward->getAp()->rankingCount(), static_cast<int32_t>(770 * 0.05f)) << "1100 * 0.7, the last rank's 5 %";
	ASSERT_NE(reward->getRewardItem1(), nullptr);
	EXPECT_EQ(reward->getRewardItem1()->getId(), 162000077) << "the default rank, difficulty 0: Fine Life Serum";
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
