// P6-I2 Crucible Challenge's AIs (game-server/data/handlers/ai/instance/crucibleChallenge) on npcs of a real Crucible Challenge instance,
// each AI installed on its npc and driven through its protected events. The test world has no zones (the administrator's chests stay
// unspawned) and the teleports of the arbiters and record keepers are left out: they move through the teleport service.

#include "I2TestSupport.h"
#include "CrucibleTestSupport.h"

#include <chrono>
#include <memory>

#include "aion/gameserver/handlers/ai/instance/crucibleChallenge/AdministratorArminosAI.h"
#include "aion/gameserver/handlers/ai/instance/crucibleChallenge/ArbiterAI.h"
#include "aion/gameserver/handlers/ai/instance/crucibleChallenge/BarrelAI.h"
#include "aion/gameserver/handlers/ai/instance/crucibleChallenge/CrucibleRiftAI.h"
#include "aion/gameserver/handlers/ai/instance/crucibleChallenge/PoppyOnTheRunAI.h"
#include "aion/gameserver/handlers/ai/instance/crucibleChallenge/RecordkeeperAI.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/CruciblePlayerReward.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemService.h"

namespace aion::gameserver::handlers::ai::instance::crucibleChallenge {
::aion::gameserver::handlers::AIFactory AdministratorArminosAI_aiFactory;
::aion::gameserver::handlers::AIFactory ArbiterAI_aiFactory;
::aion::gameserver::handlers::AIFactory BarrelAI_aiFactory;
::aion::gameserver::handlers::AIFactory CrucibleRiftAI_aiFactory;
::aion::gameserver::handlers::AIFactory PoppyOnTheRunAI_aiFactory;
::aion::gameserver::handlers::AIFactory RecordkeeperAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::instance::crucibleChallenge

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ais = ::aion::gameserver::handlers::ai::instance::crucibleChallenge;
using model::gameobjects::Npc;
using model::instance::StageType;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t SETPRO1 = model::DialogAction::SETPRO1;

struct AdministratorProbe : ais::AdministratorArminosAI {
	using AdministratorArminosAI::AdministratorArminosAI;
	using AdministratorArminosAI::handleDialogStart;
};
struct ArbiterProbe : ais::ArbiterAI {
	using ArbiterAI::ArbiterAI;
	using ArbiterAI::handleDialogStart;
};
struct BarrelProbe : ais::BarrelAI {
	using BarrelAI::BarrelAI;
	using BarrelAI::handleDied;
};
struct RiftProbe : ais::CrucibleRiftAI {
	using CrucibleRiftAI::CrucibleRiftAI;
	using CrucibleRiftAI::handleSpawned;
	using CrucibleRiftAI::handleUseItemFinish;
};
struct PoppyProbe : ais::PoppyOnTheRunAI {
	using PoppyOnTheRunAI::PoppyOnTheRunAI;
	using PoppyOnTheRunAI::handleSpawned;
};
struct RecordkeeperProbe : ais::RecordkeeperAI {
	using RecordkeeperAI::RecordkeeperAI;
	using RecordkeeperAI::handleDialogStart;
};

class CrucibleChallengeAiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		publishCrucibleData(context);
		instance = &create(CRUCIBLE_CHALLENGE, CrucibleChallengeInstance_instanceFactory);
		enter(*instance, 380.0f, 1663.0f, 97.6f);
		instance->getInstanceHandler()->onEnterInstance(player());
		executor().advance(std::chrono::milliseconds(1)); // the creation's record keeper and arbiter
		(*client)->clearSent();
	}

	void TearDown() override {
		InstanceHandlerTest::TearDown();
		resetCrucibleData();
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId) {
		runtime::Ptr<Npc> npc = spawnNpc(*instance, npcId, 381.0f, 1663.0f, 97.6f, int8_t{0});
		InstanceKnownListPairing::pair(*npc, player());
		return npc;
	}

	size_t count(int32_t npcId) { return npcs(*instance, npcId).size(); }

	int64_t dialogs(Npc& npc, int32_t page) { return countSent(serialized(SM_DIALOG_WINDOW(npc.getObjectId(), page))); }

	world::WorldMapInstance* instance = nullptr;
};

/** The six AION_AI factories build their classes */
TEST_F(CrucibleChallengeAiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(205668);
	EXPECT_NE(dynamic_cast<ais::AdministratorArminosAI*>(ais::AdministratorArminosAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::ArbiterAI*>(ais::ArbiterAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::BarrelAI*>(ais::BarrelAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::CrucibleRiftAI*>(ais::CrucibleRiftAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::PoppyOnTheRunAI*>(ais::PoppyOnTheRunAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::RecordkeeperAI*>(ais::RecordkeeperAI_aiFactory(*npc).get()), nullptr);
}

/** AdministratorArminosAI.java:24-44: the dialog; outside both stadiums SETPRO1 spawns no chest but deletes the administrator */
TEST_F(CrucibleChallengeAiTest, TheAdministrator) {
	runtime::Ptr<Npc> arminos = place(205667);
	AdministratorProbe& ai = install<AdministratorProbe>(*arminos);
	ai.handleDialogStart(player());
	EXPECT_EQ(dialogs(*arminos, 1011), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), SETPRO1 + 1, 0, 0));
	EXPECT_TRUE(arminos->isSpawned()) << "another action: nothing";
	EXPECT_TRUE(ai.onDialogSelect(player(), SETPRO1, 0, 0));
	EXPECT_FALSE(arminos->isSpawned());
	EXPECT_EQ(count(217827), 0u) << "no zones here: no chests";
}

/** ArbiterAI.java:23-62: without a worthiness ticket, the empty dialog and no teleport */
TEST_F(CrucibleChallengeAiTest, TheArbiterWithoutATicket) {
	runtime::Ptr<Npc> arbiter = place(205682);
	ArbiterProbe& ai = install<ArbiterProbe>(*arbiter);
	ai.handleDialogStart(player());
	EXPECT_EQ(dialogs(*arbiter, 0), 1);
	EXPECT_EQ(dialogs(*arbiter, 1011), 0);
	EXPECT_TRUE(ai.onDialogSelect(player(), SETPRO1, 0, 0));
	EXPECT_EQ(dialogs(*arbiter, 0), 2) << "the dialog closes";
	EXPECT_TRUE(arbiter->isSpawned());
}

/** ArbiterAI.java:23-30: with a ticket the stage dialog */
TEST_F(CrucibleChallengeAiTest, TheArbiterWithATicket) {
	services::item::ItemService::addItem(player(), 186000134, 1);
	runtime::Ptr<Npc> arbiter = place(205682);
	ArbiterProbe& ai = install<ArbiterProbe>(*arbiter);
	(*client)->clearSent();
	ai.handleDialogStart(player());
	EXPECT_EQ(dialogs(*arbiter, 1011), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), SETPRO1 + 1, 0, 0));
	EXPECT_EQ(player().getInventory().getItemCountByItemId(186000134), 1) << "another action keeps the ticket";
}

/** BarrelAI.java:18-26: a destroyed barrel puts its replacement within 4 m and goes */
TEST_F(CrucibleChallengeAiTest, ABarrel) {
	runtime::Ptr<Npc> barrel = place(218560);
	BarrelProbe& ai = install<BarrelProbe>(*barrel);
	ai.handleDied();
	EXPECT_FALSE(barrel->isSpawned());
	ASSERT_EQ(count(218561), 1u);
	EXPECT_LE(npcs(*instance, 218561)[0]->getPosition()->getX(), 381.0f + 4.0f);
	runtime::Ptr<Npc> other = place(217840);
	install<BarrelProbe>(*other).handleDied();
	EXPECT_EQ(count(217841), 1u);
	runtime::Ptr<Npc> third = place(217848);
	install<BarrelProbe>(*third).handleDied();
	EXPECT_FALSE(third->isSpawned());
	EXPECT_EQ(count(218561) + count(217841), 2u) << "another npc: no replacement";
}

/** CrucibleRiftAI.java:25-62: the rift 730459 announces itself, opens its dialog and leads on */
TEST_F(CrucibleChallengeAiTest, TheHiddenRift) {
	runtime::Ptr<Npc> rift = place(730459);
	RiftProbe& ai = install<RiftProbe>(*rift);
	ai.handleSpawned();
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE(model::ChatType::NORMAL, runtime::Ptr<model::gameobjects::VisibleObject>(player()), 1111482,
		std::vector<std::string>{}))), 1);
	ai.handleUseItemFinish(player());
	EXPECT_EQ(dialogs(*rift, 1011), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), SETPRO1 + 1, 0, 0));
	EXPECT_TRUE(rift->isSpawned());
	EXPECT_EQ(count(218185), 0u);
}

/** CrucibleRiftAI.java:39-45, 47-58: the way back 730460 announces nothing and its dialog does nothing */
TEST_F(CrucibleChallengeAiTest, TheWayBack) {
	runtime::Ptr<Npc> rift = place(730460);
	RiftProbe& ai = install<RiftProbe>(*rift);
	ai.handleSpawned();
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE(model::ChatType::NORMAL, runtime::Ptr<model::gameobjects::VisibleObject>(player()), 1111482,
		std::vector<std::string>{}))), 0);
	EXPECT_TRUE(ai.onDialogSelect(player(), SETPRO1, 0, 0));
	EXPECT_TRUE(rift->isSpawned());
	EXPECT_EQ(dialogs(*rift, 0), 0);
}

/** PoppyOnTheRunAI.java:24-40 */
TEST_F(CrucibleChallengeAiTest, Poppy) {
	runtime::Ptr<Npc> poppy = place(217802);
	PoppyProbe& ai = install<PoppyProbe>(*poppy);
	EXPECT_FALSE(ai.canThink());
	ai.handleSpawned();
	EXPECT_TRUE(poppy->isInState(model::gameobjects::state::CreatureState::ACTIVE));
	EXPECT_EQ(countSent(serialized(SM_EMOTION(*poppy, model::EmotionType::CHANGE_SPEED, 0, poppy->getObjectId()))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), SETPRO1, 0, 0));
	EXPECT_EQ(dialogs(*poppy, 0), 1);
}

/** RecordkeeperAI.java:27-103: the first record keeper starts stage 1 and goes */
TEST_F(CrucibleChallengeAiTest, TheRecordKeeperStartsAStage) {
	runtime::Ptr<Npc> keeper = npcs(*instance, 205668)[0];
	InstanceKnownListPairing::pair(*keeper, player());
	RecordkeeperProbe& ai = install<RecordkeeperProbe>(*keeper);
	ai.handleDialogStart(player());
	EXPECT_EQ(dialogs(*keeper, 1011), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), SETPRO1 + 1, 0, 0));
	EXPECT_EQ(instance->getInstanceHandler()->getStage(), StageType::DEFAULT);
	EXPECT_TRUE(keeper->isSpawned());
	EXPECT_EQ(dialogs(*keeper, 0), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), SETPRO1, 0, 0));
	EXPECT_EQ(instance->getInstanceHandler()->getStage(), StageType::START_STAGE_1_ROUND_1);
	EXPECT_FALSE(keeper->isSpawned());
}

/** :35-90: each starting keeper starts its stage */
TEST_F(CrucibleChallengeAiTest, EveryStartingKeeper) {
	const std::vector<std::pair<int32_t, StageType>> keepers{{205669, StageType::START_STAGE_2_ROUND_1},
		{205670, StageType::START_STAGE_3_ROUND_1}, {205666, StageType::START_ALTERNATIVE_STAGE_4_ROUND_1},
		{205671, StageType::START_STAGE_4_ROUND_1}, {205672, StageType::START_STAGE_5_ROUND_1}, {205673, StageType::START_STAGE_6_ROUND_1}};
	for (const auto& [npcId, stage] : keepers) {
		runtime::Ptr<Npc> keeper = place(npcId);
		install<RecordkeeperProbe>(*keeper).onDialogSelect(player(), SETPRO1, 0, 0);
		EXPECT_EQ(instance->getInstanceHandler()->getStage(), stage) << npcId;
		EXPECT_FALSE(keeper->isSpawned()) << npcId;
	}
}

/** :93-99: the last keeper gives the score and stays */
TEST_F(CrucibleChallengeAiTest, TheLastKeeperGivesTheScore) {
	runtime::Ptr<Npc> keeper = place(205679);
	install<RecordkeeperProbe>(*keeper).onDialogSelect(player(), SETPRO1, 0, 0);
	EXPECT_TRUE(keeper->isSpawned());
	runtime::Ptr<model::instance::playerreward::CruciblePlayerReward> reward = runtime::cast<model::instance::playerreward::CruciblePlayerReward>(
		instance->getInstanceHandler()->getInstanceScore()->getPlayerReward(player().getObjectId()));
	EXPECT_TRUE(reward->isRewarded());
	EXPECT_EQ(player().getInventory().getItemCountByItemId(186000130), 120) << "0 points: 120 insignias";
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
