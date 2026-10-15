// P6-I2 Stonespear Reach's AIs (game-server/data/handlers/ai/instance/stonespearReach) on npcs of a real Stonespear Reach instance, each AI
// installed on its npc and driven through its protected events. The clown's walk (AethericFieldBlaststoneAI for 856305: no walker route in
// the test data) is left to a gate.

#include "I2TestSupport.h"

#include <chrono>
#include <memory>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/handlers/ai/instance/stonespearReach/AethericFieldBlaststoneAI.h"
#include "aion/gameserver/handlers/ai/instance/stonespearReach/KebbitAI.h"
#include "aion/gameserver/handlers/ai/instance/stonespearReach/StonespearAggressiveNpcAI.h"
#include "aion/gameserver/handlers/ai/instance/stonespearReach/StonespearGuardianStoneAI.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::ai::instance::stonespearReach {
::aion::gameserver::handlers::AIFactory AethericFieldBlaststoneAI_aiFactory;
::aion::gameserver::handlers::AIFactory KebbitAI_aiFactory;
::aion::gameserver::handlers::AIFactory StonespearAggressiveNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory StonespearGuardianStoneAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::instance::stonespearReach

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ais = ::aion::gameserver::handlers::ai::instance::stonespearReach;
using model::gameobjects::Npc;
using gameserver::ai::poll::AIQuestion;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

struct BlaststoneProbe : ais::AethericFieldBlaststoneAI {
	using AethericFieldBlaststoneAI::AethericFieldBlaststoneAI;
	using AethericFieldBlaststoneAI::handleSpawned;
	using AethericFieldBlaststoneAI::handleDied;
};
struct KebbitProbe : ais::KebbitAI {
	using KebbitAI::KebbitAI;
	using KebbitAI::handleSpawned;
	using KebbitAI::handleDied;
};
struct AttackerProbe : ais::StonespearAggressiveNpcAI {
	using StonespearAggressiveNpcAI::StonespearAggressiveNpcAI;
	using StonespearAggressiveNpcAI::handleSpawned;
};
struct StoneProbe : ais::StonespearGuardianStoneAI {
	using StonespearGuardianStoneAI::StonespearGuardianStoneAI;
	using StonespearGuardianStoneAI::handleSpawned;
	using StonespearGuardianStoneAI::handleDied;
	using StonespearGuardianStoneAI::handleDespawned;
};

class I2AiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(STONESPEAR_REACH, StonespearReachInstance_instanceFactory);
		enter(*instance, 231.0f, 264.0f, 96.5f);
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId) { return spawnNpc(*instance, npcId, 235.0f, 264.0f, 96.5f, int8_t{0}); }

	static void noRewards(gameserver::ai::AI& ai, bool loot) {
		EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
		EXPECT_FALSE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
		EXPECT_EQ(ai.ask(AIQuestion::REWARD_LOOT), false) << "every AI of the chunk refuses the loot (the attackers through AggressiveNoLootNpcAI)";
		if (!loot)
			EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY));
	}

	/** the attackers and the stones in each other's known list, their GENERAL tribe hostile to itself (the real tribes are) */
	void stoneAware(Npc& attacker, Npc& stone) {
		InstanceKnownListPairing::pair(attacker, stone);
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(xml::bindString<dataholders::TribeRelationsData>(context,
			R"(<tribe_relations><tribe name="GENERAL"><hostile>PC PC_DARK GENERAL</hostile></tribe><tribe name="PC"><none>GENERAL</none></tribe>)"
			R"(<tribe name="PC_DARK"><none>GENERAL</none></tribe></tribe_relations>)"));
	}

	world::WorldMapInstance* instance = nullptr;
};

/** The four AION_AI factories build their classes (the names are the registry scan's check) */
TEST_F(I2AiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(855765);
	EXPECT_NE(dynamic_cast<ais::AethericFieldBlaststoneAI*>(ais::AethericFieldBlaststoneAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::KebbitAI*>(ais::KebbitAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::StonespearAggressiveNpcAI*>(ais::StonespearAggressiveNpcAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::StonespearGuardianStoneAI*>(ais::StonespearGuardianStoneAI_aiFactory(*npc).get()), nullptr);
}

/** KebbitAI.java:26-50: gone 15.5 s after its spawn; no respawn, rewards, loot or decay */
TEST_F(I2AiTest, TheKebbitRunsAway) {
	runtime::Ptr<Npc> kebbit = place(856303);
	KebbitProbe& ai = install<KebbitProbe>(*kebbit);
	noRewards(ai, false);
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(15499));
	EXPECT_TRUE(kebbit->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(kebbit->isSpawned());
}

/** KebbitAI.java:32-42: a dead kebbit is deleted at once, its task cancelled */
TEST_F(I2AiTest, ADeadKebbitIsDeleted) {
	runtime::Ptr<Npc> kebbit = place(856303);
	KebbitProbe& ai = install<KebbitProbe>(*kebbit);
	ai.handleSpawned();
	ai.handleDied();
	EXPECT_FALSE(kebbit->isSpawned());
	executor().advance(std::chrono::milliseconds(15500)); // the cancelled task does not run
}

/** StonespearGuardianStoneAI.java:26-37: 55 s after its spawn the stone announces its end and goes */
TEST_F(I2AiTest, TheGuardianStoneEndsAfter55Seconds) {
	runtime::Ptr<Npc> stone = place(855763);
	InstanceKnownListPairing::pair(*stone, player());
	StoneProbe& ai = install<StoneProbe>(*stone);
	noRewards(ai, false);
	ai.handleSpawned();
	(*client)->clearSent();
	executor().advance(std::chrono::milliseconds(54999));
	EXPECT_TRUE(stone->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(stone->isSpawned());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_OBJ_End())), 1);
}

/** :39-52: dead, the stone is deleted and announces nothing */
TEST_F(I2AiTest, ADeadGuardianStoneIsDeleted) {
	runtime::Ptr<Npc> stone = place(855763);
	InstanceKnownListPairing::pair(*stone, player());
	StoneProbe& ai = install<StoneProbe>(*stone);
	ai.handleSpawned();
	ai.handleDied();
	EXPECT_FALSE(stone->isSpawned());
	(*client)->clearSent();
	EXPECT_EQ(executor().advance(std::chrono::milliseconds(55000)), 1u) << "the deletion's own task only: the end task is cancelled";
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_OBJ_End())), 0);
}

/** AethericFieldBlaststoneAI.java:39-57: a field is deleted at its death; no respawn, rewards, loot or decay */
TEST_F(I2AiTest, ADeadFieldIsDeleted) {
	runtime::Ptr<Npc> field = place(855764);
	BlaststoneProbe& ai = install<BlaststoneProbe>(*field);
	noRewards(ai, false);
	ai.handleSpawned(); // not the clown: no walk
	EXPECT_TRUE(field->isSpawned());
	ai.handleDied();
	EXPECT_FALSE(field->isSpawned());
}

/** StonespearAggressiveNpcAI.java:28-48: an attacker hates the stone of the instance */
TEST_F(I2AiTest, AnAttackerGoesForTheStone) {
	runtime::Ptr<Npc> stone = place(856467);
	runtime::Ptr<Npc> attacker = place(855842);
	stoneAware(*attacker, *stone);
	AttackerProbe& ai = install<AttackerProbe>(*attacker);
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	ai.handleSpawned();
	EXPECT_EQ(attacker->getAggroList().getHate(*stone), 3000);
}

/** :34-48 (the java-bug of the list): the Elyos stage-3 stone 855809 is not on it, so its attackers hate nothing */
TEST_F(I2AiTest, AnAttackerDoesNotFindTheThirdElyosStone) {
	runtime::Ptr<Npc> stone = place(855809);
	runtime::Ptr<Npc> attacker = place(855842);
	stoneAware(*attacker, *stone);
	AttackerProbe& ai = install<AttackerProbe>(*attacker);
	ai.handleSpawned();
	EXPECT_FALSE(attacker->getAggroList().isHating(*stone));
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
