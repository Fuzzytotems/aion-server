// A1, lane C, slice 3 (worlds/kaldor): AnohasSwordAI and BerserkAnohaAI (game-server/data/handlers/ai/worlds/kaldor) on npcs of a real Kaldor
// map instance (the sword's map icon and Anoha spawn at (791.4, 488.9)), each AI installed with replaceAi. The paths that read Anoha's
// fortress (7011: the brigade general's dialog, Anoha's spawn and her occupier's quests) need the siege service and are left to a retest.

#include "ZoneA1TestSupport.h"

#include <chrono>
#include <cstdint>

#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/ai/worlds/kaldor/AnohasSwordAI.h"
#include "aion/gameserver/handlers/ai/worlds/kaldor/BerserkAnohaAI.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPage.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

#define AION_KALDOR_AIS(X)                                                                                                                    \
	X(AnohasSwordAI)                                                                                                                          \
	X(BerserkAnohaAI)

#define AION_DECLARE_FACTORY(cls)                                                                                                             \
	namespace aion::gameserver::handlers::ai::worlds::kaldor {                                                                                \
	::aion::gameserver::handlers::AIFactory cls##_aiFactory;                                                                                  \
	}
AION_KALDOR_AIS(AION_DECLARE_FACTORY)
#undef AION_DECLARE_FACTORY

namespace aion::gameserver::handlers::ai::worlds::test {
namespace {

namespace kal = ::aion::gameserver::handlers::ai::worlds::kaldor;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

template <class AI>
struct Probe : AI {
	using AI::AI;
	using AI::handleCreatureSee;
	using AI::handleDespawned;
	using AI::handleDied;
};

class KaldorA1AiTest : public ZoneA1AiTest {
protected:
	void SetUp() override {
		ZoneA1AiTest::SetUp();
		open(KALDOR, 791.0f, 495.0f, 143.5f);
	}
};

/** The two AION_AI factories build their classes; the sword never respawns, Anoha neither decays, respawns nor gives loot */
TEST_F(KaldorA1AiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200, 791.38214f, 488.93655f, 143.47617f);
#define AION_CHECK_FACTORY(cls)                                                                                                               \
	{                                                                                                                                         \
		auto ai = kal::cls##_aiFactory(*npc);                                                                                               \
		EXPECT_NE(dynamic_cast<kal::cls*>(ai.get()), nullptr) << #cls;                                                                        \
		EXPECT_FALSE(ai->ask(AIQuestion::ALLOW_RESPAWN)) << #cls;                                                                             \
	}
	AION_KALDOR_AIS(AION_CHECK_FACTORY)
#undef AION_CHECK_FACTORY
	auto anoha = kal::BerserkAnohaAI_aiFactory(*npc);
	EXPECT_FALSE(anoha->ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(anoha->ask(AIQuestion::REWARD_LOOT));
	EXPECT_TRUE(anoha->ask(AIQuestion::REWARD_AP));
}

/** AnohasSwordAI.java:44-52: only the first choice counts; without the Anoha Sealing Stone the player gets dialog NO_RIGHT */
TEST_F(KaldorA1AiTest, TheSwordWantsTheSealingStone) {
	runtime::Ptr<Npc> sword = place(900200, 791.38214f, 488.93655f, 143.47617f);
	auto& ai = install<kal::AnohasSwordAI>(*sword);
	EXPECT_FALSE(ai.onDialogSelect(player(), model::DialogAction::SETPRO2, 0, 0));
	EXPECT_FALSE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(sword->getObjectId(), model::id(model::DialogPage::NO_RIGHT)))), 1);
	EXPECT_TRUE(sword->isSpawned());
	EXPECT_EQ(count(702618), 0u);
}

/** AnohasSwordAI.java:49-72: the stone is taken, the sword dies, its icon appears; 30 minutes later Berserk Anoha, and the sword goes */
TEST_F(KaldorA1AiTest, TheSwordCallsBerserkAnoha) {
	giveItem(185000215, 1);
	runtime::Ptr<Npc> sword = place(900200, 791.38214f, 488.93655f, 143.47617f);
	auto& ai = install<kal::AnohasSwordAI>(*sword);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(player().getInventory().getItemCountByItemId(185000215), 0);
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(sword->getObjectId(), 2375))), 1);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_LDF5_FORTRESS_NAMED_SPAWN_ITEM())), 1);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_LDF5_FORTRESS_NAMED_SPAWN())), 1) << "to every player in the world";
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_LDF5_FORTRESS_NAMED_SPAWN_SYSTEM())), 1);
	EXPECT_TRUE(sword->isDead());
	EXPECT_EQ(count(702618), 1u) << "the map icon";
	executor().advance(std::chrono::minutes(30) - std::chrono::milliseconds(1));
	EXPECT_EQ(count(855263), 0u);
	EXPECT_TRUE(sword->isSpawned()) << "its decay task replays its death instead of despawning it";
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(855263), 1u);
	EXPECT_FALSE(sword->isSpawned());
}

/** BerserkAnohaAI.java:56-74: her death takes the icon and leaves a Commander Anoha (804595 without an Asmodian occupier) for an hour */
TEST_F(KaldorA1AiTest, AnohasDeathLeavesACommander) {
	place(702618, 791.38214f, 488.93655f, 143.47617f);
	runtime::Ptr<Npc> anoha = place(900200, 791.38214f, 488.93655f, 143.47517f);
	auto& ai = install<Probe<kal::BerserkAnohaAI>>(*anoha);
	ai.handleCreatureSee(player()); // no occupier read (no spawn here): no quest
	ai.handleDied();
	EXPECT_EQ(count(702618), 0u);
	ASSERT_EQ(count(804595), 1u);
	EXPECT_EQ(count(804594), 0u);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_ANOHA_DIE())), 1);
	executor().advance(std::chrono::minutes(60) - std::chrono::milliseconds(1));
	EXPECT_EQ(count(804595), 1u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(804595), 0u);
}

/** BerserkAnohaAI.java:48-54: leaving alive she tells the world; dead she does not again */
TEST_F(KaldorA1AiTest, AnohaLeavingAliveTellsTheWorld) {
	place(702618, 791.38214f, 488.93655f, 143.47617f);
	runtime::Ptr<Npc> anoha = place(900200, 791.38214f, 488.93655f, 143.47517f);
	auto& ai = install<Probe<kal::BerserkAnohaAI>>(*anoha);
	ai.handleDespawned();
	EXPECT_EQ(count(702618), 0u);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_ANOHA_DESPAWN())), 1);
}

} // namespace
} // namespace aion::gameserver::handlers::ai::worlds::test
