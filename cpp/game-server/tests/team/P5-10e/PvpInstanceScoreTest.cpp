// M5j stage 4 CP3 (J13, m5j-plan.md §18.5): InstanceScore (InstanceScore.java) with the InstanceProgressionType companion, and
// PvpInstanceScore (PvpInstanceScore.java), the base of the PvP arena and instance scores.

#include <gtest/gtest.h>

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/instance/InstanceProgressionTypeInfo.h"
#include "aion/gameserver/model/instance/instanceposition/ChaosInstancePosition.h"
#include "aion/gameserver/model/instance/instanceposition/DisciplineInstancePosition.h"
#include "aion/gameserver/model/instance/instanceposition/GloryInstancePosition.h"
#include "aion/gameserver/model/instance/instanceposition/HarmonyInstancePosition.h"
#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/model/instance/instancescore/PvpInstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/CruciblePlayerReward.h"
#include "aion/gameserver/model/instance/playerreward/InstancePlayerReward.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::model::instance::instancescore {
namespace {

/** PvpInstanceScore.java:16-32: the AP rewards of the constructor */
TEST(PvpInstanceScoreTest, Rewards) {
	runtime::Ref<PvpInstanceScore> score = PvpInstanceScore::create(3000, 1500, 2000);
	EXPECT_EQ(score->getWinnerApReward(), 3000);
	EXPECT_EQ(score->getLoserApReward(), 1500);
	EXPECT_EQ(score->getDrawApReward(), 2000);
	EXPECT_EQ(score->getElyosPoints(), 0);
	EXPECT_EQ(score->getAsmodiansPoints(), 0);
}

/** PvpInstanceScore.java:42-63: points never fall below 0; the leader; other races change nothing */
TEST(PvpInstanceScoreTest, Points) {
	runtime::Ref<PvpInstanceScore> score = PvpInstanceScore::create(0, 0, 0);
	EXPECT_EQ(score->getRaceWithHighestPoints(), Race::NONE) << "a tie";
	score->addPointsByRace(Race::ELYOS, 100);
	score->addPointsByRace(Race::ASMODIANS, 40);
	EXPECT_EQ(score->getPointsByRace(Race::ELYOS), 100);
	EXPECT_EQ(score->getPointsByRace(Race::ASMODIANS), 40);
	EXPECT_EQ(score->getPointsByRace(Race::NPC), 40) << "anything but ELYOS reads the Asmodians' points";
	EXPECT_EQ(score->getRaceWithHighestPoints(), Race::ELYOS);
	score->addPointsByRace(Race::ELYOS, -150);
	EXPECT_EQ(score->getElyosPoints(), 0) << "Math.max(0, ...)";
	EXPECT_EQ(score->getRaceWithHighestPoints(), Race::ASMODIANS);
	score->addPointsByRace(Race::NPC, 500);
	EXPECT_EQ(score->getElyosPoints(), 0);
	EXPECT_EQ(score->getAsmodiansPoints(), 40);
	score->addPointsByRace(Race::ELYOS, 40);
	EXPECT_EQ(score->getRaceWithHighestPoints(), Race::NONE);
}

/** PvpInstanceScore.java:65-70 */
TEST(PvpInstanceScoreTest, Kills) {
	runtime::Ref<PvpInstanceScore> score = PvpInstanceScore::create(0, 0, 0);
	score->incrementKillsByRace(Race::ELYOS);
	score->incrementKillsByRace(Race::ELYOS);
	score->incrementKillsByRace(Race::ASMODIANS);
	score->incrementKillsByRace(Race::PC_ALL);
	EXPECT_EQ(score->getElyosKills(), 2);
	EXPECT_EQ(score->getAsmodiansKills(), 1);
}

/** InstanceProgressionType.java: the ids and the four predicates */
TEST(InstanceScoreTest, ProgressionTypeCompanion) {
	EXPECT_EQ(getId(InstanceProgressionType::REINFORCE_MEMBER), 12582912);
	EXPECT_EQ(getId(InstanceProgressionType::PREPARING), 1048576);
	EXPECT_EQ(getId(InstanceProgressionType::START_PROGRESS), 2097152);
	EXPECT_EQ(getId(InstanceProgressionType::END_PROGRESS), 3145728);
	EXPECT_TRUE(isReinforcing(InstanceProgressionType::REINFORCE_MEMBER));
	EXPECT_FALSE(isReinforcing(InstanceProgressionType::PREPARING));
	EXPECT_TRUE(isPreparing(InstanceProgressionType::PREPARING));
	EXPECT_TRUE(isStartProgress(InstanceProgressionType::START_PROGRESS));
	EXPECT_TRUE(isEndProgress(InstanceProgressionType::END_PROGRESS));
	EXPECT_FALSE(isEndProgress(InstanceProgressionType::START_PROGRESS));
}

/** InstanceScore.java:18-64: the player rewards by owner id, the progression predicates, clear */
TEST(InstanceScoreTest, RewardsAndProgression) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST)); // the map's reads load shared pointers
	runtime::Ref<InstanceScore> score = InstanceScore::create();
	EXPECT_EQ(score->getInstanceProgressionType(), InstanceProgressionType::START_PROGRESS) << "the field's initializer";
	EXPECT_TRUE(score->isStartProgress());
	EXPECT_FALSE(score->isRewarded());
	runtime::Ref<playerreward::InstancePlayerReward> first = playerreward::InstancePlayerReward::create(101);
	runtime::Ref<playerreward::InstancePlayerReward> second = playerreward::InstancePlayerReward::create(102);
	score->addPlayerReward(*first);
	score->addPlayerReward(*second);
	EXPECT_TRUE(score->containsPlayer(101));
	EXPECT_FALSE(score->containsPlayer(103));
	EXPECT_EQ(score->getPlayerReward(102).get(), second.get());
	EXPECT_EQ(score->getPlayerReward(103), nullptr);
	EXPECT_EQ(score->getPlayerRewards().size(), 2u);
	runtime::Ref<playerreward::InstancePlayerReward> replacement = playerreward::InstancePlayerReward::create(101);
	score->addPlayerReward(*replacement);
	EXPECT_EQ(score->getPlayerReward(101).get(), replacement.get()) << "put replaces by owner id";
	score->removePlayerReward(*first);
	EXPECT_FALSE(score->containsPlayer(101)) << "remove by the owner id, whichever reward holds it";
	score->setInstanceProgressionType(InstanceProgressionType::END_PROGRESS);
	EXPECT_TRUE(score->isRewarded());
	EXPECT_FALSE(score->isStartProgress());
	score->setInstanceProgressionType(InstanceProgressionType::REINFORCE_MEMBER);
	EXPECT_TRUE(score->isReinforcing());
	score->setInstanceProgressionType(InstanceProgressionType::PREPARING);
	EXPECT_TRUE(score->isPreparing());
	score->clear();
	EXPECT_TRUE(score->getPlayerRewards().empty());
}

/** InstancePlayerReward.java:15-45: the owner, points, kills */
TEST(InstanceScoreTest, PlayerReward) {
	runtime::Ref<playerreward::InstancePlayerReward> reward = playerreward::InstancePlayerReward::create(4711);
	EXPECT_EQ(reward->getOwnerId(), 4711);
	reward->addPoints(30);
	reward->addPoints(-50);
	EXPECT_EQ(reward->getPoints(), -20) << "no floor here";
	reward->setPoints(7);
	EXPECT_EQ(reward->getPoints(), 7);
	reward->addPvPKill();
	reward->addPvPKill();
	reward->addMonsterKillToPlayer();
	EXPECT_EQ(reward->getPvPKills(), 2);
	EXPECT_EQ(reward->getMonsterKills(), 1);
}

/** GeneralInstancePosition.initialize (GeneralInstancePosition.java:14-18): the map and instance the ports teleport into */
struct DisciplinePositionPeer : instanceposition::DisciplineInstancePosition {
	int32_t map() const { return mapId.get(); }
	int32_t instance() const { return instanceId.get(); }
};

TEST(InstanceScoreTest, PositionInitialize) {
	runtime::Ref<DisciplinePositionPeer> position = runtime::makeRef<DisciplinePositionPeer>();
	EXPECT_EQ(position->map(), 0);
	instanceposition::InstancePositionHandler& handler = *position;
	handler.initialize(300350000, 3);
	EXPECT_EQ(position->map(), 300350000);
	EXPECT_EQ(position->instance(), 3);
}

/** the four arenas' position tables are handlers of the one interface (PvPArenaScore.java:41-62 picks one per map) */
TEST(InstanceScoreTest, PositionKinds) {
	runtime::Ref<instanceposition::GeneralInstancePosition> positions[] = {instanceposition::DisciplineInstancePosition::create(),
		instanceposition::GloryInstancePosition::create(), instanceposition::HarmonyInstancePosition::create(),
		instanceposition::ChaosInstancePosition::create()};
	for (const runtime::Ref<instanceposition::GeneralInstancePosition>& position : positions) {
		instanceposition::InstancePositionHandler& handler = *position;
		EXPECT_NO_THROW(handler.initialize(300350000, 1));
	}
}

/** CruciblePlayerReward.java: the flags and values over the base reward */
TEST(InstanceScoreTest, CruciblePlayerReward) {
	runtime::Ref<playerreward::CruciblePlayerReward> reward = playerreward::CruciblePlayerReward::create(77);
	EXPECT_EQ(reward->getOwnerId(), 77);
	EXPECT_FALSE(reward->isRewarded());
	EXPECT_FALSE(reward->isPlayerLeave());
	EXPECT_FALSE(reward->isPlayerDefeated());
	reward->setRewarded();
	reward->setPlayerLeave();
	reward->setPlayerDefeated(true);
	reward->setInsignia(12);
	reward->setSpawnPosition(3);
	EXPECT_TRUE(reward->isRewarded());
	EXPECT_TRUE(reward->isPlayerLeave());
	EXPECT_TRUE(reward->isPlayerDefeated());
	EXPECT_EQ(reward->getInsignia(), 12);
	EXPECT_EQ(reward->getSpawnPosition(), 3);
	reward->setPlayerDefeated(false);
	EXPECT_FALSE(reward->isPlayerDefeated());
}

} // namespace
} // namespace aion::gameserver::model::instance::instancescore
