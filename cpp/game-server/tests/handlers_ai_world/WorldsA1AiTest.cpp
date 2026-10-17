// A1, lane B, the worlds-root slice (docs/design/a1-laneb-plan.md cp4): AgrintAI, BlackenedGraveAI, DefensiveCannonAI, ForgottenGraveAI,
// LegionDominionInvasionRiftOpenAI, NamelessGraveAI, WorldBlesserAI (game-server/data/handlers/ai/worlds/*.java) on npcs of the AI tests'
// Poeta map, each AI installed on its npc.
//
// Not covered here (no World registration of the test map, no player connection, no skill or quest rows): the spawns of the graves and of
// Agrint (rndSpawnInRange), the dialog pages, the morph and blessing skills, the rift opening; the zone bosses' gates are lane C's.

#include "../ai/AiWorldTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/handlers/ai/OneDmgAI.h"
#include "aion/gameserver/handlers/ai/worlds/AgrintAI.h"
#include "aion/gameserver/handlers/ai/worlds/BlackenedGraveAI.h"
#include "aion/gameserver/handlers/ai/worlds/DefensiveCannonAI.h"
#include "aion/gameserver/handlers/ai/worlds/ForgottenGraveAI.h"
#include "aion/gameserver/handlers/ai/worlds/LegionDominionInvasionRiftOpenAI.h"
#include "aion/gameserver/handlers/ai/worlds/NamelessGraveAI.h"
#include "aion/gameserver/handlers/ai/worlds/WorldBlesserAI.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/World.h"

// The factory functions the AION_AI markers define, declared exactly as Registry.ai.gen.cpp declares them (handlers::AIFactory).
namespace aion::gameserver::handlers::ai::worlds {
::aion::gameserver::handlers::AIFactory AgrintAI_aiFactory;
::aion::gameserver::handlers::AIFactory BlackenedGraveAI_aiFactory;
::aion::gameserver::handlers::AIFactory DefensiveCannonAI_aiFactory;
::aion::gameserver::handlers::AIFactory ForgottenGraveAI_aiFactory;
::aion::gameserver::handlers::AIFactory LegionDominionInvasionRiftOpenAI_aiFactory;
::aion::gameserver::handlers::AIFactory NamelessGraveAI_aiFactory;
::aion::gameserver::handlers::AIFactory WorldBlesserAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::worlds

namespace aion::gameserver::ai::testing {
namespace {

namespace worlds = gameserver::handlers::ai::worlds;

using poll::AIQuestion;
using runtime::Ref;

constexpr int32_t PLAIN_NPC_ID = 210663; // a monster of the AI tests' npc_templates (AiWorldTestSupport.h)

// the protected handlers the cases drive directly (Java: the AI's events)
struct CannonProbe : worlds::DefensiveCannonAI {
	using DefensiveCannonAI::DefensiveCannonAI;
	using DefensiveCannonAI::handleUseItemFinish;
};
struct RiftOpenProbe : worlds::LegionDominionInvasionRiftOpenAI {
	using LegionDominionInvasionRiftOpenAI::LegionDominionInvasionRiftOpenAI;
	using LegionDominionInvasionRiftOpenAI::handleDialogStart;
};

class WorldsA1AiTest : public AiWorldTest {
protected:
	void SetUp() override {
		AiWorldTest::SetUp();
		AI_TEST_SCOPE;
		regionActivator = activateRegionAt(500, 500, 100);
		executor->runReady();
	}

	void TearDown() override {
		regionActivator = nullptr;
		AiWorldTest::TearDown();
	}

	template <class AI>
	AI& install(model::gameobjects::Npc& npc) {
		auto handlerAi = std::make_unique<AI>(npc);
		AI& result = *handlerAi;
		npc.replaceAi(std::move(handlerAi));
		return result;
	}

	Ref<model::gameobjects::player::Player> regionActivator;
};

/** The seven AION_AI factories build their classes on the bases Java gives them (the names are the registry scan's check) */
TEST_F(WorldsA1AiTest, TheFactoriesBuildTheirClasses) {
	AI_TEST_SCOPE;
	Ref<model::gameobjects::Npc> npc = makeWorldNpc(PLAIN_NPC_ID, 501, 500, 100);
	std::unique_ptr<AbstractAI> agrint = worlds::AgrintAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<worlds::AgrintAI*>(agrint.get()));
	EXPECT_TRUE(dynamic_cast<gameserver::handlers::ai::OneDmgAI*>(agrint.get()));
	std::unique_ptr<AbstractAI> cannon = worlds::DefensiveCannonAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<worlds::DefensiveCannonAI*>(cannon.get()));
	EXPECT_TRUE(dynamic_cast<gameserver::handlers::ai::ActionItemNpcAI*>(cannon.get()));
	for (gameserver::handlers::AIFactory* factory : {&worlds::BlackenedGraveAI_aiFactory, &worlds::ForgottenGraveAI_aiFactory,
		     &worlds::LegionDominionInvasionRiftOpenAI_aiFactory, &worlds::NamelessGraveAI_aiFactory, &worlds::WorldBlesserAI_aiFactory}) {
		std::unique_ptr<AbstractAI> ai = (*factory)(*npc);
		EXPECT_TRUE(dynamic_cast<gameserver::handlers::ai::GeneralNpcAI*>(ai.get()));
	}
	EXPECT_TRUE(dynamic_cast<worlds::BlackenedGraveAI*>(worlds::BlackenedGraveAI_aiFactory(*npc).get()));
	EXPECT_TRUE(dynamic_cast<worlds::ForgottenGraveAI*>(worlds::ForgottenGraveAI_aiFactory(*npc).get()));
	EXPECT_TRUE(dynamic_cast<worlds::LegionDominionInvasionRiftOpenAI*>(worlds::LegionDominionInvasionRiftOpenAI_aiFactory(*npc).get()));
	EXPECT_TRUE(dynamic_cast<worlds::NamelessGraveAI*>(worlds::NamelessGraveAI_aiFactory(*npc).get()));
	EXPECT_TRUE(dynamic_cast<worlds::WorldBlesserAI*>(worlds::WorldBlesserAI_aiFactory(*npc).get()));
}

/** BlackenedGraveAI.java:20-23, ForgottenGraveAI.java:23-26, NamelessGraveAI.java:21-24: the graves do not think */
TEST_F(WorldsA1AiTest, TheGravesDoNotThink) {
	AI_TEST_SCOPE;
	Ref<model::gameobjects::Npc> blackened = makeWorldNpc(PLAIN_NPC_ID, 501, 500, 100);
	Ref<model::gameobjects::Npc> forgotten = makeWorldNpc(PLAIN_NPC_ID, 502, 500, 100);
	Ref<model::gameobjects::Npc> nameless = makeWorldNpc(PLAIN_NPC_ID, 503, 500, 100);
	EXPECT_FALSE(install<worlds::BlackenedGraveAI>(*blackened).canThink());
	EXPECT_FALSE(install<worlds::ForgottenGraveAI>(*forgotten).canThink());
	EXPECT_FALSE(install<worlds::NamelessGraveAI>(*nameless).canThink());
	Ref<model::gameobjects::Npc> plain = makeWorldNpc(PLAIN_NPC_ID, 504, 500, 100);
	EXPECT_TRUE(install<gameserver::handlers::ai::GeneralNpcAI>(*plain).canThink()) << "GeneralNpcAI's";
}

/** DefensiveCannonAI.java:25-43: the first finished use deletes the cannon (no morph for another npc id), a second does nothing; no rewards */
TEST_F(WorldsA1AiTest, ADefensiveCannonIsUsedOnce) {
	AI_TEST_SCOPE;
	Ref<model::gameobjects::Npc> cannon = makeWorldNpc(PLAIN_NPC_ID, 501, 500, 100);
	CannonProbe& ai = install<CannonProbe>(*cannon);
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	EXPECT_TRUE(ai.ask(AIQuestion::ALLOW_RESPAWN)) << "NpcAI's";
	Ref<model::gameobjects::player::Player> player = makeWorldPlayer(7301, 502, 500, 100);
	world::World::getInstance().storeObject(*cannon);
	ai.handleUseItemFinish(*player);
	EXPECT_FALSE(world::World::getInstance().findVisibleObject(cannon->getObjectId())) << "AIActions.deleteOwner";
	world::World::getInstance().storeObject(*cannon);
	ai.handleUseItemFinish(*player);
	EXPECT_TRUE(world::World::getInstance().findVisibleObject(cannon->getObjectId())) << "canUse: only the first use";
	world::World::getInstance().removeObject(*cannon);
}

/** LegionDominionInvasionRiftOpenAI.java:32-75: without a Legion Dominion talk info the opener shows nothing; SETPRO1 then fails as Java's NPE */
TEST_F(WorldsA1AiTest, ARiftOpenerNeedsItsTalkInfo) {
	AI_TEST_SCOPE;
	Ref<model::gameobjects::Npc> npc = makeWorldNpc(PLAIN_NPC_ID, 501, 500, 100);
	RiftOpenProbe& ai = install<RiftOpenProbe>(*npc);
	Ref<model::gameobjects::player::Player> player = makeWorldPlayer(7302, 502, 500, 100);
	ai.handleDialogStart(*player); // no talk info: returns
	EXPECT_TRUE(ai.onDialogSelect(*player, model::DialogAction::SELECT1_1, 0, 0)) << "another action: the closing page, true";
	EXPECT_THROW(ai.onDialogSelect(*player, model::DialogAction::SETPRO1, 0, 0), runtime::NullPointerException);
}

} // namespace
} // namespace aion::gameserver::ai::testing
