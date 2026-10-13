// M5i I-5 (m5i-plan.md 15.3): RiftProtectorAI ("rift_protector", P5-05's root AI) and WorldRaidAI ("world_raid_aggressive", A1's
// handlers/ai/events), both under P5-12b2's lease (chunks.cmake).
//
// Java: data/handlers/ai/RiftProtectorAI.java:19-22, events/WorldRaidAI.java:28-71.
//
// LINK WORKAROUND (the one TravelAiHandlersTest.cpp describes): A1's handler library is not linked into this executable, so this file compiles
// WorldRaidAI.cpp itself; it is the only translation unit that does.
// Not covered here: the advent effect npc of a world raid boss's first attack (npc 702549 and its 10 s despawn) - the gate's (I-11).
#include "aion/gameserver/handlers/ai/events/WorldRaidAI.cpp"

#include "../ai/AiWorldTestSupport.h"

#include <memory>

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/handlers/ai/RiftProtectorAI.h"
#include "aion/gameserver/model/stats/calc/AdditionStat.h"

// The factory functions the AION_AI markers define, declared exactly as Registry.ai.gen.cpp declares them (handlers::AIFactory).
namespace aion::gameserver::handlers::ai {
::aion::gameserver::handlers::AIFactory RiftProtectorAI_aiFactory;
} // namespace aion::gameserver::handlers::ai
namespace aion::gameserver::handlers::ai::events {
::aion::gameserver::handlers::AIFactory WorldRaidAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::events

namespace aion::gameserver::ai::testing {
namespace {

namespace roots = gameserver::handlers::ai;

class RiftAndWorldRaidAiTest : public AiWorldTest {};

TEST_F(RiftAndWorldRaidAiTest, TheFactoriesBuildAggressiveAIs) {
	AI_TEST_SCOPE;
	runtime::Ref<model::gameobjects::Npc> npc = makeWorldNpc(210663);
	std::unique_ptr<AbstractAI> protector = roots::RiftProtectorAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<roots::RiftProtectorAI*>(protector.get()));
	EXPECT_TRUE(dynamic_cast<roots::AggressiveNpcAI*>(protector.get()));
	std::unique_ptr<AbstractAI> boss = roots::events::WorldRaidAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<roots::events::WorldRaidAI*>(boss.get()));
	EXPECT_TRUE(dynamic_cast<roots::AggressiveNpcAI*>(boss.get()));
}

TEST_F(RiftAndWorldRaidAiTest, ARiftProtectorHasATenthOfItsMaxHp) {
	AI_TEST_SCOPE;
	runtime::Ref<model::gameobjects::Npc> npc = makeWorldNpc(210663);
	roots::RiftProtectorAI ai(*npc);
	model::stats::calc::AdditionStat maxHp(model::stats::container::StatEnum::MAXHP, 1000, *npc);
	ai.modifyOwnerStat(maxHp);
	EXPECT_FLOAT_EQ(maxHp.getBaseRate(), 0.1f);
	model::stats::calc::AdditionStat attack(model::stats::container::StatEnum::MAIN_HAND_POWER, 100, *npc);
	ai.modifyOwnerStat(attack);
	EXPECT_FLOAT_EQ(attack.getBaseRate(), 1.0f) << "only MAXHP";
}

} // namespace
} // namespace aion::gameserver::ai::testing
