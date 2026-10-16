// A1 classNpc and walkers (lane A, 2026-10-16): the drakan class npcs' AIs and the walkers' AIs of chunk A1
// (game-server/data/handlers/ai/{classNpc,walkers}) on npcs of a Poeta map instance (A1LaneATestSupport.h). The servants' spawns into World,
// the skills (no skill rows) and the shouts are left to the retest.

#include "A1LaneATestSupport.h"

#include <chrono>
#include <memory>

#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/handlers/ai/classNpc/DrakanHealingServantAI.h"
#include "aion/gameserver/handlers/ai/classNpc/DrakanMedicAI.h"
#include "aion/gameserver/handlers/ai/classNpc/DrakanPriestAI.h"
#include "aion/gameserver/handlers/ai/classNpc/EnemyServantAI.h"
#include "aion/gameserver/handlers/ai/walkers/NaiaAI.h"
#include "aion/gameserver/handlers/ai/walkers/PolorSerinAI.h"
#include "aion/gameserver/handlers/ai/walkers/WalkGeneralRunnerAI.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"

namespace aion::gameserver::handlers::ai::classNpc {
::aion::gameserver::handlers::AIFactory DrakanHealingServantAI_aiFactory;
::aion::gameserver::handlers::AIFactory DrakanMedicAI_aiFactory;
::aion::gameserver::handlers::AIFactory DrakanPriestAI_aiFactory;
::aion::gameserver::handlers::AIFactory EnemyServantAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::classNpc
namespace aion::gameserver::handlers::ai::walkers {
::aion::gameserver::handlers::AIFactory NaiaAI_aiFactory;
::aion::gameserver::handlers::AIFactory PolorSerinAI_aiFactory;
::aion::gameserver::handlers::AIFactory WalkGeneralRunnerAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::walkers

namespace aion::gameserver::network::aion::clientpackets::testing::items::a1 {
namespace {

namespace classNpc = gameserver::handlers::ai::classNpc;
namespace walkers = gameserver::handlers::ai::walkers;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Npc;
using model::gameobjects::state::CreatureState;

class ClassNpcA1AiTest : public A1NpcAiTest {
protected:
	std::string rowsXml() override { return a1NpcTemplatesXml({900100, 282988, 281621}); }
};

TEST_F(ClassNpcA1AiTest, TheMarkersBuildTheirClasses) {
	Npc& npc = npcAt(900100);
	EXPECT_NE(dynamic_cast<classNpc::DrakanHealingServantAI*>(classNpc::DrakanHealingServantAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<classNpc::DrakanMedicAI*>(classNpc::DrakanMedicAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<classNpc::DrakanPriestAI*>(classNpc::DrakanPriestAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<classNpc::EnemyServantAI*>(classNpc::EnemyServantAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<walkers::NaiaAI*>(walkers::NaiaAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<walkers::PolorSerinAI*>(walkers::PolorSerinAI_aiFactory(npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<walkers::WalkGeneralRunnerAI*>(walkers::WalkGeneralRunnerAI_aiFactory(npc).get()), nullptr);
}

/** DrakanHealingServantAI.java:45-51, EnemyServantAI.java:39-45: the servants never decay, respawn or reward */
TEST_F(ClassNpcA1AiTest, TheServantsGiveNothing) {
	auto& healer = install<classNpc::DrakanHealingServantAI>(npcAt(281621));
	auto& enemy = install<classNpc::EnemyServantAI>(npcAt(282988));
	for (AIQuestion q : {AIQuestion::ALLOW_DECAY, AIQuestion::ALLOW_RESPAWN, AIQuestion::REWARD_AP_XP_DP_LOOT}) {
		EXPECT_FALSE(healer.ask(q));
		EXPECT_FALSE(enemy.ask(q));
	}
	EXPECT_EQ(healer.ask(AIQuestion::REWARD_AP), healer.NpcAI::ask(AIQuestion::REWARD_AP)) << "the rest is NpcAI's";
}

class HealerProbe final : public classNpc::DrakanHealingServantAI {
public:
	using DrakanHealingServantAI::DrakanHealingServantAI;
	using DrakanHealingServantAI::handleSpawned;
};

class EnemyServantProbe final : public classNpc::EnemyServantAI {
public:
	using EnemyServantAI::EnemyServantAI;
	using EnemyServantAI::handleSpawned;
};

/** DrakanHealingServantAI.java:24-31, EnemyServantAI.java:21-28: without a creator (or its target) a servant starts no skill task */
TEST_F(ClassNpcA1AiTest, AServantWithoutCreatorDoesNothing) {
	Npc& healer = npcAt(281621);
	install<HealerProbe>(healer).handleSpawned();
	Npc& enemy = npcAt(282988);
	install<EnemyServantProbe>(enemy).handleSpawned();
	executor->advance(std::chrono::milliseconds(2000));
	EXPECT_FALSE(healer.getController().hasTask(model::TaskId::SKILL_USE));
	EXPECT_FALSE(enemy.getController().hasTask(model::TaskId::SKILL_USE));
}

/**
 * DrakanHealingServantAI.java:24-38: two seconds after its spawn a healing servant targets its creator and heals it every 6 s (a SKILL_USE
 * task) - unless it died or was despawned in the meantime
 */
TEST_F(ClassNpcA1AiTest, AHealingServantHealsItsCreatorUnlessItIsGone) {
	world::World::getInstance().storeObject(player()); // Npc.getCreator finds the creator in World
	Npc& healer = npcAt(281621);
	healer.setCreatorId(player().getObjectId());
	install<HealerProbe>(healer).handleSpawned();
	Npc& gone = npcAt(281621, 104.0f);
	gone.setCreatorId(player().getObjectId());
	install<HealerProbe>(gone).handleSpawned();
	gone.getPosition()->setIsSpawned(false); // despawned before the 2 s
	executor->advance(std::chrono::milliseconds(2000));
	EXPECT_TRUE(healer.getController().hasTask(model::TaskId::SKILL_USE)) << "the heal task";
	EXPECT_FALSE(gone.getController().hasTask(model::TaskId::SKILL_USE)) << "!getPosition().isSpawned(): no heal";
	healer.getController().cancelAllTasks();
	world::World::getInstance().removeObject(player());
}

class RunnerProbe final : public walkers::WalkGeneralRunnerAI {
public:
	using WalkGeneralRunnerAI::WalkGeneralRunnerAI;
	using WalkGeneralRunnerAI::handleMoveArrived;
};

class PolorProbe final : public walkers::PolorSerinAI {
public:
	using PolorSerinAI::PolorSerinAI;
	using PolorSerinAI::handleMoveArrived;
};

/** WalkGeneralRunnerAI.java:18-22 and PolorSerinAI.java:21-30: a runner draws its weapon at each arrival; Polor without an adult near too */
TEST_F(ClassNpcA1AiTest, RunnersDrawTheirWeapons) {
	Npc& runner = npcAt(900100);
	install<RunnerProbe>(runner).handleMoveArrived();
	EXPECT_TRUE(runner.isInState(CreatureState::WEAPON_EQUIPPED));
	Npc& polor = npcAt(900100, 104.0f);
	install<PolorProbe>(polor).handleMoveArrived();
	EXPECT_TRUE(polor.isInState(CreatureState::WEAPON_EQUIPPED)) << "no adult in its known list: WalkGeneralRunnerAI's arrival";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items::a1
