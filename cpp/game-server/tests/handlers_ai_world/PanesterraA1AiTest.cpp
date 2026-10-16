// A1, lane B, the Panesterra corridors (docs/design/a1-laneb-plan.md cp5): AdvanceCorridorAI, GovernorCorridorAI, Rank100CorridorAI,
// PanesterraEntryCorridorAI (game-server/data/handlers/ai/worlds/panesterra/*.java) on npcs of the AI tests' Poeta map.
//
// Not covered here (no player connection, no Panesterra teams, no level 65 character): the question window, joining a team, the entry's
// teleport. Ahserion's Flight cannot run in a gate yet, so nothing here has gate coverage.

#include "../ai/AiWorldTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/AdvanceCorridorAI.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/GovernorCorridorAI.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/PanesterraEntryCorridorAI.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/Rank100CorridorAI.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra {
::aion::gameserver::handlers::AIFactory AdvanceCorridorAI_aiFactory;
::aion::gameserver::handlers::AIFactory GovernorCorridorAI_aiFactory;
::aion::gameserver::handlers::AIFactory PanesterraEntryCorridorAI_aiFactory;
::aion::gameserver::handlers::AIFactory Rank100CorridorAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::worlds::panesterra

namespace aion::gameserver::ai::testing {
namespace {

namespace corridors = gameserver::handlers::ai::worlds::panesterra;

using runtime::Ref;

constexpr int32_t PLAIN_NPC_ID = 210663; // a monster of the AI tests' npc_templates (AiWorldTestSupport.h)
constexpr int32_t ASK_PASS_BY_SVS_DIRECT_PORTAL = 905067;

struct AdvanceProbe : corridors::AdvanceCorridorAI {
	using AdvanceCorridorAI::AdvanceCorridorAI;
	using AdvanceCorridorAI::handleSpawned;
};
struct GovernorProbe : corridors::GovernorCorridorAI {
	using GovernorCorridorAI::GovernorCorridorAI;
	using GovernorCorridorAI::handleSpawned;
};
struct Rank100Probe : corridors::Rank100CorridorAI {
	using Rank100CorridorAI::Rank100CorridorAI;
	using Rank100CorridorAI::handleSpawned;
};

class PanesterraA1AiTest : public AiWorldTest {
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

	/** the corridor stays the minutes Java gives it, then deleteIfAliveOrCancelRespawn takes it out of the World */
	template <class Probe>
	void expectDespawnAfter(int32_t minutes) {
		Ref<model::gameobjects::Npc> corridor = makeWorldNpc(PLAIN_NPC_ID, 501, 500, 100);
		Probe& ai = install<Probe>(*corridor);
		world::World::getInstance().storeObject(*corridor);
		ai.handleSpawned();
		EXPECT_TRUE(corridor->getController().hasTask(model::TaskId::DESPAWN));
		executor->advance(std::chrono::minutes(minutes) - std::chrono::milliseconds(1));
		EXPECT_TRUE(world::World::getInstance().findVisibleObject(corridor->getObjectId())) << minutes << " min";
		executor->advance(std::chrono::milliseconds(1));
		EXPECT_FALSE(world::World::getInstance().findVisibleObject(corridor->getObjectId())) << minutes << " min";
	}

	Ref<model::gameobjects::player::Player> regionActivator;
};

/** The four AION_AI factories build their classes; the governors' and the top 100's corridors are advance corridors */
TEST_F(PanesterraA1AiTest, TheFactoriesBuildTheirClasses) {
	AI_TEST_SCOPE;
	Ref<model::gameobjects::Npc> npc = makeWorldNpc(PLAIN_NPC_ID, 501, 500, 100);
	EXPECT_TRUE(dynamic_cast<corridors::AdvanceCorridorAI*>(corridors::AdvanceCorridorAI_aiFactory(*npc).get()));
	std::unique_ptr<AbstractAI> governor = corridors::GovernorCorridorAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<corridors::GovernorCorridorAI*>(governor.get()));
	EXPECT_TRUE(dynamic_cast<corridors::AdvanceCorridorAI*>(governor.get()));
	std::unique_ptr<AbstractAI> rank100 = corridors::Rank100CorridorAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<corridors::Rank100CorridorAI*>(rank100.get()));
	EXPECT_TRUE(dynamic_cast<corridors::AdvanceCorridorAI*>(rank100.get()));
	std::unique_ptr<AbstractAI> entry = corridors::PanesterraEntryCorridorAI_aiFactory(*npc);
	EXPECT_TRUE(dynamic_cast<corridors::PanesterraEntryCorridorAI*>(entry.get()));
	EXPECT_TRUE(dynamic_cast<gameserver::handlers::ai::GeneralNpcAI*>(entry.get()));
}

/** AdvanceCorridorAI.java:30-35, GovernorCorridorAI.java:17-20, Rank100CorridorAI.java:17-20: 10 minutes, the governors' and top 100's 5 */
TEST_F(PanesterraA1AiTest, TheCorridorsDespawn) {
	AI_TEST_SCOPE;
	expectDespawnAfter<AdvanceProbe>(10);
	expectDespawnAfter<GovernorProbe>(5);
	expectDespawnAfter<Rank100Probe>(5);
}

/** AdvanceCorridorAI.java:37-41, GovernorCorridorAI.java:22-29, Rank100CorridorAI.java:22-29: a soldier is refused, no question is asked */
TEST_F(PanesterraA1AiTest, ASoldierIsRefusedByEveryCorridor) {
	AI_TEST_SCOPE;
	Ref<model::gameobjects::player::Player> soldier = makeWorldPlayer(7401, 502, 500, 100);
	soldier->setAbyssRank(model::gameobjects::player::AbyssRank::create(0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0)); // rank 1: a soldier
	Ref<model::gameobjects::Npc> advance = makeWorldNpc(PLAIN_NPC_ID, 501, 500, 100);
	install<corridors::AdvanceCorridorAI>(*advance).handleDialogStart(*soldier);
	EXPECT_FALSE(soldier->getResponseRequester().remove(ASK_PASS_BY_SVS_DIRECT_PORTAL)) << "below a one-star officer";
	Ref<model::gameobjects::Npc> governor = makeWorldNpc(PLAIN_NPC_ID, 502, 500, 100);
	install<corridors::GovernorCorridorAI>(*governor).handleDialogStart(*soldier);
	EXPECT_FALSE(soldier->getResponseRequester().remove(ASK_PASS_BY_SVS_DIRECT_PORTAL)) << "not a supreme commander";
	Ref<model::gameobjects::Npc> rank100 = makeWorldNpc(PLAIN_NPC_ID, 503, 500, 100);
	install<corridors::Rank100CorridorAI>(*rank100).handleDialogStart(*soldier);
	EXPECT_FALSE(soldier->getResponseRequester().remove(ASK_PASS_BY_SVS_DIRECT_PORTAL)) << "below a five-star officer";
}

/** PanesterraEntryCorridorAI.java:33-49: every selection answers true; SETPRO1 below level 65 teleports nobody */
TEST_F(PanesterraA1AiTest, AnEntryCorridorAnswersEverySelection) {
	AI_TEST_SCOPE;
	Ref<model::gameobjects::Npc> npc = makeWorldNpc(PLAIN_NPC_ID, 501, 500, 100);
	corridors::PanesterraEntryCorridorAI& ai = install<corridors::PanesterraEntryCorridorAI>(*npc);
	Ref<model::gameobjects::player::Player> player = makeWorldPlayer(7402, 502, 500, 100);
	EXPECT_TRUE(ai.onDialogSelect(*player, model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(player->getWorldId(), POETA) << "level 1: STR_MSG_TELEPOTER_GAB1_USER04, no teleport";
	EXPECT_TRUE(ai.onDialogSelect(*player, model::DialogAction::SELECT1_1, 0, 0));
}

} // namespace
} // namespace aion::gameserver::ai::testing
