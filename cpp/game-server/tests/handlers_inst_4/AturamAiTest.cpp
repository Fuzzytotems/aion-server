// P6I-10 (docs/design/p6-instances-plan.md, I4 split): the AIs of aturamSkyFortress (game-server/data/handlers/ai/instance) on npcs of a real
// Aturam Sky Fortress instance, each AI installed with replaceAi. The doors, the walks (no walker rows), the item grants (no item rows) and the
// skills' effects (the shadows' mark) are left to a gate.

#include "I4TestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/AlarmAI.h"
#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/AshunatalShadowslipAI.h"
#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/DredgionGeneratorAI.h"
#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/ExplosionShadowsAI.h"
#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/HarikenSupplyChestAI.h"
#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/PopuchinAI.h"
#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/ShulackGuidedBombAI.h"
#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/ShulackThermoBombAI.h"
#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/SteamTachysphereAI.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {
::aion::gameserver::handlers::AIFactory AlarmAI_aiFactory;
::aion::gameserver::handlers::AIFactory AshunatalShadowslipAI_aiFactory;
::aion::gameserver::handlers::AIFactory DredgionGeneratorAI_aiFactory;
::aion::gameserver::handlers::AIFactory ExplosionShadowsAI_aiFactory;
::aion::gameserver::handlers::AIFactory HarikenSupplyChestAI_aiFactory;
::aion::gameserver::handlers::AIFactory PopuchinAI_aiFactory;
::aion::gameserver::handlers::AIFactory ShulackGuidedBombAI_aiFactory;
::aion::gameserver::handlers::AIFactory ShulackThermoBombAI_aiFactory;
::aion::gameserver::handlers::AIFactory SteamTachysphereAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ais = ::aion::gameserver::handlers::ai::instance::aturamSkyFortress;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using questEngine::model::QuestState;
using questEngine::model::QuestStatus;

// the protected handlers the cases drive directly (Java: the AI's events)
struct AlarmProbe : ais::AlarmAI {
	using AlarmAI::AlarmAI;
	using AlarmAI::handleCreatureMoved;
};
struct GuidedBombProbe : ais::ShulackGuidedBombAI {
	using ShulackGuidedBombAI::ShulackGuidedBombAI;
	using ShulackGuidedBombAI::handleCreatureAggro;
	using ShulackGuidedBombAI::handleSpawned;
};
struct ThermoBombProbe : ais::ShulackThermoBombAI {
	using ShulackThermoBombAI::ShulackThermoBombAI;
	using ShulackThermoBombAI::handleSpawned;
};
struct PopuchinProbe : ais::PopuchinAI {
	using PopuchinAI::PopuchinAI;
	using PopuchinAI::handleBackHome;
};
struct ChestProbe : ais::HarikenSupplyChestAI {
	using HarikenSupplyChestAI::HarikenSupplyChestAI;
	using HarikenSupplyChestAI::handleDialogStart;
};
struct TachysphereProbe : ais::SteamTachysphereAI {
	using SteamTachysphereAI::SteamTachysphereAI;
	using SteamTachysphereAI::handleUseItemFinish;
};

class AturamAiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(ATURAM_SKY_FORTRESS, AturamSkyFortressInstance_instanceFactory);
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

	runtime::Ptr<Npc> place(int32_t npcId, float x = 302.0f, float y = 300.0f) {
		return spawnNpc(*instance, npcId, x, y, 300.0f, int8_t{0});
	}

	const skillengine::model::SkillTemplate* skill(int32_t id) { return dataholders::DataManager::SKILL_DATA->getSkillTemplate(id); }

	world::WorldMapInstance* instance = nullptr;
};

/** The nine AION_AI factories build their classes (the names are the registry scan's check) */
TEST_F(AturamAiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
	EXPECT_NE(dynamic_cast<ais::AlarmAI*>(ais::AlarmAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::AshunatalShadowslipAI*>(ais::AshunatalShadowslipAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::DredgionGeneratorAI*>(ais::DredgionGeneratorAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::ExplosionShadowsAI*>(ais::ExplosionShadowsAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::HarikenSupplyChestAI*>(ais::HarikenSupplyChestAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::PopuchinAI*>(ais::PopuchinAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::ShulackGuidedBombAI*>(ais::ShulackGuidedBombAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::ShulackThermoBombAI*>(ais::ShulackThermoBombAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::SteamTachysphereAI*>(ais::SteamTachysphereAI_aiFactory(*npc).get()), nullptr);
}

/** AlarmAI.java:39-67: a player 23 m away or closer sets it off once: it tells the door, stops thinking and goes 3 s later */
TEST_F(AturamAiTest, TheAlarmGoesOffOnce) {
	runtime::Ptr<Npc> alarm = place(900200, 324.0f);
	AlarmProbe& ai = install<AlarmProbe>(*alarm);
	InstanceKnownListPairing::pair(*alarm, player());
	ai.handleCreatureMoved(player());
	EXPECT_TRUE(ai.canThink()) << "24 m";
	alarm->getPosition()->setXYZH(323.0f, 300.0f, 300.0f, int8_t{0});
	ai.handleCreatureMoved(player());
	EXPECT_FALSE(ai.canThink());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_Station_DoorCtrl_Evileye())), 1);
	ai.handleCreatureMoved(player());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_Station_DoorCtrl_Evileye())), 1) << "once";
	executor().advance(std::chrono::milliseconds(2999));
	EXPECT_TRUE(alarm->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(alarm->isSpawned());
}

/** AshunatalShadowslipAI.java:31-34, 59-72: her shadow skill stops her thinking; she walks 3 s later and goes 4 s after that */
TEST_F(AturamAiTest, AshunatalGoesAfterHerShadow) {
	publishSkills({19417, 19418});
	runtime::Ptr<Npc> ashunatal = place(900200);
	auto& ai = install<ais::AshunatalShadowslipAI>(*ashunatal);
	ai.onEndUseSkill(skill(19418), 1);
	EXPECT_TRUE(ai.canThink());
	ai.onEndUseSkill(skill(19417), 1);
	EXPECT_FALSE(ai.canThink());
	executor().advance(std::chrono::milliseconds(6999));
	EXPECT_TRUE(ashunatal->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(ashunatal->isSpawned());
}

/** DredgionGeneratorAI.java:19-30: it never thinks and is immune to abnormal states */
TEST_F(AturamAiTest, TheDredgionGeneratorNeverThinks) {
	runtime::Ptr<Npc> generator = place(900200);
	auto& ai = install<ais::DredgionGeneratorAI>(*generator);
	EXPECT_FALSE(ai.canThink());
	EXPECT_TRUE(ai.ask(gameserver::ai::poll::AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
	EXPECT_TRUE(ai.ask(gameserver::ai::poll::AIQuestion::ALLOW_DECAY)) << "GeneralNpcAI's";
}

/** ExplosionShadowsAI.java:40-62: 1.5 s after its self destruct the shadow goes (no player near has the mark); another skill does nothing */
TEST_F(AturamAiTest, TheShadowsGoAfterTheirSelfDestruct) {
	publishSkills({19425, 19426});
	runtime::Ptr<Npc> shadow = place(900200);
	auto& ai = install<ais::ExplosionShadowsAI>(*shadow);
	InstanceKnownListPairing::pair(*shadow, player());
	ai.onEndUseSkill(skill(19426), 1);
	executor().advance(std::chrono::milliseconds(5000));
	EXPECT_TRUE(shadow->isSpawned());
	ai.onEndUseSkill(skill(19425), 1);
	executor().advance(std::chrono::milliseconds(1499));
	EXPECT_TRUE(shadow->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(shadow->isSpawned());
	EXPECT_TRUE(npcs(*instance, 799657).empty()) << "the player has no mark";
}

/** HarikenSupplyChestAI.java:24-36: the dialog opens 1011; a choice closes it */
TEST_F(AturamAiTest, TheSupplyChestOpensItsDialog) {
	runtime::Ptr<Npc> chest = place(900200);
	ChestProbe& ai = install<ChestProbe>(*chest);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(chest->getObjectId(), 1011))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SELECT1_1, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(chest->getObjectId(), 0))), 1);
}

/** PopuchinAI.java:30-96: attacked, every 15.5 + 3 + 1.5 s two bombs at its place above half hp; back home stops it */
TEST_F(AturamAiTest, PopuchinThrowsBombs) {
	publishSkills({19412, 19413});
	runtime::Ptr<Npc> popuchin = place(900200);
	PopuchinProbe& ai = install<PopuchinProbe>(*popuchin);
	EXPECT_TRUE(ai.ask(gameserver::ai::poll::AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	executor().advance(std::chrono::milliseconds(19999));
	EXPECT_TRUE(npcs(*instance, 217374).empty());
	executor().advance(std::chrono::milliseconds(1));
	ASSERT_EQ(npcs(*instance, 217374).size(), 2u);
	EXPECT_FLOAT_EQ(npcs(*instance, 217374).front()->getX(), popuchin->getX());
	executor().advance(std::chrono::milliseconds(20000));
	EXPECT_EQ(npcs(*instance, 217374).size(), 4u) << "again";
	ai.handleBackHome();
	executor().advance(std::chrono::milliseconds(40000));
	EXPECT_EQ(npcs(*instance, 217374).size(), 4u) << "back home";
}

/** PopuchinAI.java:57-60, 98-112: at half hp or less ten bombs around it, 1.5 s later */
TEST_F(AturamAiTest, PopuchinScattersBombsAtHalfHp) {
	publishSkills({19412, 19413});
	runtime::Ptr<Npc> popuchin = place(900200);
	PopuchinProbe& ai = install<PopuchinProbe>(*popuchin);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	executor().advance(std::chrono::milliseconds(19999));
	popuchin->getLifeStats()->setCurrentHpPercent(50);
	ASSERT_EQ(popuchin->getLifeStats()->getHpPercentage(), 50);
	executor().advance(std::chrono::milliseconds(1500));
	EXPECT_TRUE(npcs(*instance, 217375).empty());
	EXPECT_TRUE(npcs(*instance, 217374).empty());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(npcs(*instance, 217375).size(), 10u);
}

/** ShulackGuidedBombAI.java:37-57, 89-96: unprovoked it goes after 10 s; no decay, respawn or rewards; immune */
TEST_F(AturamAiTest, TheGuidedBombGoesAfter10Seconds) {
	runtime::Ptr<Npc> bomb = place(900200);
	GuidedBombProbe& ai = install<GuidedBombProbe>(*bomb);
	EXPECT_FALSE(ai.ask(gameserver::ai::poll::AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(ai.ask(gameserver::ai::poll::AIQuestion::ALLOW_RESPAWN));
	EXPECT_FALSE(ai.ask(gameserver::ai::poll::AIQuestion::REWARD_AP_XP_DP_LOOT));
	EXPECT_TRUE(ai.ask(gameserver::ai::poll::AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(9999));
	EXPECT_TRUE(bomb->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(bomb->isSpawned());
}

/** ShulackGuidedBombAI.java:43-87: aggroed, it explodes within 4 m of its target at the next second and goes 3.2 s later */
TEST_F(AturamAiTest, TheGuidedBombExplodesNearItsTarget) {
	publishSkills({19415});
	runtime::Ptr<Npc> bomb = place(900200, 305.0f);
	GuidedBombProbe& ai = install<GuidedBombProbe>(*bomb);
	ai.handleCreatureAggro(player());
	executor().advance(std::chrono::milliseconds(3000));
	EXPECT_TRUE(bomb->isSpawned()) << "5 m";
	bomb->getPosition()->setXYZH(304.0f, 300.0f, 300.0f, int8_t{0});
	executor().advance(std::chrono::milliseconds(1000));
	executor().advance(std::chrono::milliseconds(3199));
	EXPECT_TRUE(bomb->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(bomb->isSpawned());
}

/** ShulackThermoBombAI.java:22-53: 2 s after the spawn it explodes and goes 4 s later */
TEST_F(AturamAiTest, TheThermoBombExplodes) {
	publishSkills({19416});
	runtime::Ptr<Npc> bomb = place(900200);
	ThermoBombProbe& ai = install<ThermoBombProbe>(*bomb);
	EXPECT_FALSE(ai.ask(gameserver::ai::poll::AIQuestion::ALLOW_RESPAWN));
	EXPECT_TRUE(ai.ask(gameserver::ai::poll::AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(5999));
	EXPECT_TRUE(bomb->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(bomb->isSpawned());
}

/** SteamTachysphereAI.java:30-54: no quest is no right, an open quest page 10, the completed quest 1011 */
TEST_F(AturamAiTest, TheTachysphereWantsTheQuest) {
	runtime::Ptr<Npc> sphere = place(900200);
	TachysphereProbe& ai = install<TachysphereProbe>(*sphere);
	ai.handleUseItemFinish(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(sphere->getObjectId(), model::id(model::DialogPage::NO_RIGHT)))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(sphere->getObjectId(), 0))), 0) << "no quest: no flight";
	const int32_t questId = player().getRace() == model::Race::ELYOS ? 18302 : 28302;
	runtime::Ref<QuestState> qs = QuestState::create(questId, QuestStatus::START);
	player().getQuestStateList()->addQuest(questId, *qs);
	ai.handleUseItemFinish(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(sphere->getObjectId(), 10))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(sphere->getObjectId(), 0))), 0) << "not complete";
	qs->setStatus(QuestStatus::COMPLETE);
	ai.handleUseItemFinish(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(sphere->getObjectId(), 1011))), 1);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
