// P6I-8b (docs/design/p6-instances-plan.md §4): the AIs of the arenas (pvpArenas in game-server/data/handlers/ai/instance) and Ophidan
// Bridge's DefenseWallAI, on npcs of real instances, each AI installed with replaceAi. The arenas' handlers are not ported yet: the relics
// and flame throwers stand in other instances here. The morph and bonus skills (no skill rows) are left to a gate.

#include "I3TestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/ai/instance/ophidianBridge/DefenseWallAI.h"
#include "aion/gameserver/handlers/ai/instance/pvpArenas/AntiAirCraftGunAI.h"
#include "aion/gameserver/handlers/ai/instance/pvpArenas/HarmonyShifterAI.h"
#include "aion/gameserver/handlers/ai/instance/pvpArenas/PlazaFlameThrowerAI.h"
#include "aion/gameserver/handlers/ai/instance/pvpArenas/RelicsAI.h"
#include "aion/gameserver/handlers/instance/OphidanBridgeInstance.h"

namespace aion::gameserver::handlers::ai::instance {
namespace pvpArenas {
::aion::gameserver::handlers::AIFactory AntiAirCraftGunAI_aiFactory;
::aion::gameserver::handlers::AIFactory HarmonyShifterAI_aiFactory;
::aion::gameserver::handlers::AIFactory PlazaFlameThrowerAI_aiFactory;
::aion::gameserver::handlers::AIFactory RelicsAI_aiFactory;
} // namespace pvpArenas
namespace ophidianBridge {
::aion::gameserver::handlers::AIFactory DefenseWallAI_aiFactory;
} // namespace ophidianBridge
} // namespace aion::gameserver::handlers::ai::instance

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ais = ::aion::gameserver::handlers::ai::instance;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Npc;

// the protected handlers the cases drive directly (Java: the AI's events)
struct RelicsProbe : ais::pvpArenas::RelicsAI {
	using RelicsAI::RelicsAI;
	using RelicsAI::handleDialogStart;
	using RelicsAI::handleUseItemFinish;
};
struct FlameThrowerProbe : ais::pvpArenas::PlazaFlameThrowerAI {
	using PlazaFlameThrowerAI::PlazaFlameThrowerAI;
	using PlazaFlameThrowerAI::handleUseItemFinish;
};

class ArenaAiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(OPHIDAN_BRIDGE, OphidanBridgeInstance_instanceFactory);
		enter(*instance, 300.0f, 300.0f, 300.0f);
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(world::WorldMapInstance& in, int32_t npcId) { return spawnNpc(in, npcId, 302.0f, 300.0f, 300.0f, int8_t{0}); }

	world::WorldMapInstance* instance = nullptr;
};

/** The five AION_AI factories build their classes (the names are the registry scan's check) */
TEST_F(ArenaAiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(*instance, 900200);
	EXPECT_NE(dynamic_cast<ais::pvpArenas::AntiAirCraftGunAI*>(ais::pvpArenas::AntiAirCraftGunAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::pvpArenas::HarmonyShifterAI*>(ais::pvpArenas::HarmonyShifterAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::pvpArenas::PlazaFlameThrowerAI*>(ais::pvpArenas::PlazaFlameThrowerAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::pvpArenas::RelicsAI*>(ais::pvpArenas::RelicsAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::ophidianBridge::DefenseWallAI*>(ais::ophidianBridge::DefenseWallAI_aiFactory(*npc).get()), nullptr);
}

/** DefenseWallAI.java:18-29: never thinks; no decay, no rewards */
TEST_F(ArenaAiTest, ADefenseWallNeverThinks) {
	runtime::Ptr<Npc> wall = place(*instance, 900200);
	auto& ai = install<ais::ophidianBridge::DefenseWallAI>(*wall);
	EXPECT_FALSE(ai.canThink());
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	EXPECT_EQ(ai.ask(AIQuestion::ALLOW_RESPAWN), ai.GeneralNpcAI::ask(AIQuestion::ALLOW_RESPAWN)) << "the rest is GeneralNpcAI's";
}

/** RelicsAI.java:23-30: before the run's start the dialog does nothing; once there is no score to wait for, the use bar starts */
TEST_F(ArenaAiTest, ARelicWaitsForTheStart) {
	RelicsProbe& here = install<RelicsProbe>(*place(*instance, 900200));
	(*client)->clearSent();
	here.handleDialogStart(player());
	EXPECT_FALSE(sent().empty()) << "no score: ActionItemNpcAI's use bar";

	leave(); // the actor goes to an Iron Wall Warfront, still reinforcing
	world::WorldMapInstance& arena = create(IRON_WALL_WARFRONT, pvp::IronWallWarfrontInstance_instanceFactory);
	enter(arena, 300.0f, 300.0f, 300.0f);
	RelicsProbe& early = install<RelicsProbe>(*place(arena, 900200));
	(*client)->clearSent();
	early.handleDialogStart(player());
	EXPECT_TRUE(sent().empty()) << "REINFORCE_MEMBER";
}

/** RelicsAI.java:32-43: the first use rewards and the relic goes; the random relics 701187/701188 do not come back */
TEST_F(ArenaAiTest, ARelicGoesAfterItsUse) {
	runtime::Ptr<Npc> relic = place(*instance, 701187);
	install<RelicsProbe>(*relic).handleUseItemFinish(player());
	EXPECT_FALSE(relic->isSpawned());
	executor().advance(std::chrono::milliseconds(600000));
	EXPECT_TRUE(npcs(*instance, 701187).empty());
}

/** PlazaFlameThrowerAI.java:34-62: the use rewards once and the thrower goes */
TEST_F(ArenaAiTest, AFlameThrowerGoesAfterItsUse) {
	runtime::Ptr<Npc> thrower = place(*instance, 900200);
	install<FlameThrowerProbe>(*thrower).handleUseItemFinish(player());
	EXPECT_FALSE(thrower->isSpawned());
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
