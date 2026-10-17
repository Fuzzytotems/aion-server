// P6I-11 (docs/design/p6-instances-plan.md, I4 split): the AIs of beshmundirTemple (game-server/data/handlers/ai/instance) on npcs of a real
// Beshmundir Temple instance, each AI installed with replaceAi. Left to a gate: the ports (no portal rows), the teleports, the group paths,
// the skills' effects (Dorakiki's 18901, the souls' 18959), Virhana's rages and the artifacts' casts.

#include "I4TestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/AethicFieldGeneratorAI.h"
#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/BeshmundirsWalkAI.h"
#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/BigOrbAI.h"
#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/DivineArtifactAI.h"
#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/Door1AI.h"
#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/Door2AI.h"
#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/Door3AI.h"
#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/DorakikiTheBoldAI.h"
#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/IsbariyaServantsAI.h"
#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/IsbariyaTheResoluteAI.h"
#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/MagicArtifactAI.h"
#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/ManadarAI.h"
#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/MonolithicAmbusherAI.h"
#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/PlegetonAI.h"
#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/SacrificialSoulAI.h"
#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/VirhanaTheGreatAI.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {
::aion::gameserver::handlers::AIFactory AethicFieldGeneratorAI_aiFactory;
::aion::gameserver::handlers::AIFactory BeshmundirsWalkAI_aiFactory;
::aion::gameserver::handlers::AIFactory BigOrbAI_aiFactory;
::aion::gameserver::handlers::AIFactory DivineArtifactAI_aiFactory;
::aion::gameserver::handlers::AIFactory Door1AI_aiFactory;
::aion::gameserver::handlers::AIFactory Door2AI_aiFactory;
::aion::gameserver::handlers::AIFactory Door3AI_aiFactory;
::aion::gameserver::handlers::AIFactory DorakikiTheBoldAI_aiFactory;
::aion::gameserver::handlers::AIFactory IsbariyaServantsAI_aiFactory;
::aion::gameserver::handlers::AIFactory IsbariyaTheResoluteAI_aiFactory;
::aion::gameserver::handlers::AIFactory MagicArtifactAI_aiFactory;
::aion::gameserver::handlers::AIFactory ManadarAI_aiFactory;
::aion::gameserver::handlers::AIFactory MonolithicAmbusherAI_aiFactory;
::aion::gameserver::handlers::AIFactory PlegetonAI_aiFactory;
::aion::gameserver::handlers::AIFactory SacrificialSoulAI_aiFactory;
::aion::gameserver::handlers::AIFactory VirhanaTheGreatAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ais = ::aion::gameserver::handlers::ai::instance::beshmundirTemple;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using questEngine::model::QuestState;
using questEngine::model::QuestStatus;

// the protected handlers the cases drive directly (Java: the AI's events)
struct BigOrbProbe : ais::BigOrbAI {
	using BigOrbAI::BigOrbAI;
	using BigOrbAI::handleDialogStart;
};
struct Door1Probe : ais::Door1AI {
	using Door1AI::Door1AI;
	using Door1AI::handleDialogStart;
};
struct Door2Probe : ais::Door2AI {
	using Door2AI::Door2AI;
	using Door2AI::handleDialogStart;
};
struct Door3Probe : ais::Door3AI {
	using Door3AI::Door3AI;
	using Door3AI::handleDialogStart;
	using Door3AI::handleUseItemFinish;
};
struct ServantsProbe : ais::IsbariyaServantsAI {
	using IsbariyaServantsAI::IsbariyaServantsAI;
	using IsbariyaServantsAI::handleSpawned;
};
struct ManadarProbe : ais::ManadarAI {
	using ManadarAI::ManadarAI;
	using ManadarAI::handleAttack;
	using ManadarAI::handleBackHome;
};
struct GeneratorProbe : ais::AethicFieldGeneratorAI {
	using AethicFieldGeneratorAI::AethicFieldGeneratorAI;
	using AethicFieldGeneratorAI::handleAttack;
	using AethicFieldGeneratorAI::handleBackHome;
	using AethicFieldGeneratorAI::handleSpawned;
};
struct SoulProbe : ais::SacrificialSoulAI {
	using SacrificialSoulAI::SacrificialSoulAI;
	using SacrificialSoulAI::handleAttack;
};
struct PlegetonProbe : ais::PlegetonAI {
	using PlegetonAI::PlegetonAI;
	using PlegetonAI::handleDialogStart;
};
struct WalkProbe : ais::BeshmundirsWalkAI {
	using BeshmundirsWalkAI::BeshmundirsWalkAI;
	using BeshmundirsWalkAI::handleUseItemFinish;
};
struct IsbariyaProbe : ais::IsbariyaTheResoluteAI {
	using IsbariyaTheResoluteAI::IsbariyaTheResoluteAI;
	using IsbariyaTheResoluteAI::handleAttack;
	using IsbariyaTheResoluteAI::handleBackHome;
	using IsbariyaTheResoluteAI::handleSpawned;
};
/** A 216215 that counts the aggro events it gets */
struct AmbusherProbe : ais::MonolithicAmbusherAI {
	using MonolithicAmbusherAI::MonolithicAmbusherAI;
	using MonolithicAmbusherAI::handleCreatureAggro;
};
struct CountingAggroAI : ::aion::gameserver::handlers::ai::AggressiveNpcAI {
	using AggressiveNpcAI::AggressiveNpcAI;
	int32_t aggros = 0;
	void handleCreatureAggro(model::gameobjects::Creature&) override { ++aggros; }
};

class BeshmundirAiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(BESHMUNDIR_TEMPLE, BeshmundirInstance_instanceFactory);
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

	runtime::Ptr<Npc> place(int32_t npcId, float x = 302.0f, float y = 300.0f) { return spawnNpc(*instance, npcId, x, y, 300.0f, int8_t{0}); }

	world::WorldMapInstance* instance = nullptr;
};

/** The sixteen AION_AI factories build their classes (the names are the registry scan's check) */
TEST_F(BeshmundirAiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
	EXPECT_NE(dynamic_cast<ais::AethicFieldGeneratorAI*>(ais::AethicFieldGeneratorAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::BeshmundirsWalkAI*>(ais::BeshmundirsWalkAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::BigOrbAI*>(ais::BigOrbAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::DivineArtifactAI*>(ais::DivineArtifactAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::Door1AI*>(ais::Door1AI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::Door2AI*>(ais::Door2AI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::Door3AI*>(ais::Door3AI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::DorakikiTheBoldAI*>(ais::DorakikiTheBoldAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::IsbariyaServantsAI*>(ais::IsbariyaServantsAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::IsbariyaTheResoluteAI*>(ais::IsbariyaTheResoluteAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::MagicArtifactAI*>(ais::MagicArtifactAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::ManadarAI*>(ais::ManadarAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::MonolithicAmbusherAI*>(ais::MonolithicAmbusherAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::PlegetonAI*>(ais::PlegetonAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::SacrificialSoulAI*>(ais::SacrificialSoulAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::VirhanaTheGreatAI*>(ais::VirhanaTheGreatAI_aiFactory(*npc).get()), nullptr);
}

/** BigOrbAI.java:22-42: the orb offers the portal until it is out; the choice brings it */
TEST_F(BeshmundirAiTest, TheBigOrbBringsThePortalOnce) {
	runtime::Ptr<Npc> orb = place(900200);
	BigOrbProbe& ai = install<BigOrbProbe>(*orb);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(orb->getObjectId(), 1011))), 1);
	EXPECT_FALSE(ai.onDialogSelect(player(), model::DialogAction::SELECT1_1, 0, 0));
	EXPECT_TRUE(npcs(*instance, 730276).empty());
	EXPECT_FALSE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(orb->getObjectId(), 0))), 1);
	ASSERT_EQ(npcs(*instance, 730276).size(), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 730276).front()->getX(), 1604.6683f);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(orb->getObjectId(), 10))), 1);
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(orb->getObjectId(), 1011))), 1);
}

/** Door1AI.java:22-36, Door2AI.java:22-36: without the race's quest no right; with it the door is used and goes (no talk delay) */
TEST_F(BeshmundirAiTest, TheQuestDoorsWantTheQuest) {
	const bool elyos = player().getRace() == model::Race::ELYOS;
	runtime::Ptr<Npc> door1 = place(900200);
	Door1Probe& ai1 = install<Door1Probe>(*door1);
	runtime::Ptr<Npc> door2 = place(900300);
	Door2Probe& ai2 = install<Door2Probe>(*door2);
	ai1.handleDialogStart(player());
	ai2.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(door1->getObjectId(), model::id(model::DialogPage::NO_RIGHT)))), 1);
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(door2->getObjectId(), model::id(model::DialogPage::NO_RIGHT)))), 1);
	for (int32_t questId : {elyos ? 30308 : 30208, elyos ? 30311 : 30211}) // the other race's quests
		player().getQuestStateList()->addQuest(questId, *QuestState::create(questId, QuestStatus::START));
	ai1.handleDialogStart(player());
	ai2.handleDialogStart(player());
	EXPECT_TRUE(door1->isSpawned());
	EXPECT_TRUE(door2->isSpawned());
	const int32_t quest1 = elyos ? 30208 : 30308;
	player().getQuestStateList()->addQuest(quest1, *QuestState::create(quest1, QuestStatus::START));
	ai1.handleDialogStart(player());
	ai2.handleDialogStart(player());
	EXPECT_FALSE(door1->isSpawned());
	EXPECT_TRUE(door2->isSpawned());
	const int32_t quest2 = elyos ? 30211 : 30311;
	player().getQuestStateList()->addQuest(quest2, *QuestState::create(quest2, QuestStatus::START));
	ai2.handleDialogStart(player());
	EXPECT_FALSE(door2->isSpawned());
}

/** Door3AI.java:22-35: without the key no right; used, it goes */
TEST_F(BeshmundirAiTest, TheKeyDoorWantsTheKey) {
	runtime::Ptr<Npc> door = place(900200);
	Door3Probe& ai = install<Door3Probe>(*door);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(door->getObjectId(), model::id(model::DialogPage::NO_RIGHT)))), 1);
	EXPECT_TRUE(door->isSpawned());
	ai.handleUseItemFinish(player());
	EXPECT_FALSE(door->isSpawned());
}

/** IsbariyaServantsAI.java:22-35: the servants go 10 s after their spawn, 281659 20 s */
TEST_F(BeshmundirAiTest, TheServantsGo) {
	runtime::Ptr<Npc> servant = place(281660);
	runtime::Ptr<Npc> elder = place(281659);
	install<ServantsProbe>(*servant).handleSpawned();
	install<ServantsProbe>(*elder).handleSpawned();
	executor().advance(std::chrono::milliseconds(9999));
	EXPECT_TRUE(servant->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(servant->isSpawned());
	executor().advance(std::chrono::milliseconds(9999));
	EXPECT_TRUE(elder->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(elder->isSpawned());
}

/** ManadarAI.java:22-47: at 90 % hp five traps around it, again every 6 s; back home ends them */
TEST_F(BeshmundirAiTest, ManadarLaysTraps) {
	runtime::Ptr<Npc> manadar = place(900200);
	ManadarProbe& ai = install<ManadarProbe>(*manadar);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	auto traps = [this] { return npcs(*instance, 281545).size() + npcs(*instance, 281756).size(); };
	EXPECT_EQ(traps(), 0u) << "full hp";
	manadar->getLifeStats()->setCurrentHpPercent(90);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_EQ(traps(), 5u);
	executor().advance(std::chrono::milliseconds(5999));
	EXPECT_EQ(traps(), 5u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(traps(), 10u);
	ai.handleBackHome();
	executor().advance(std::chrono::milliseconds(12000));
	EXPECT_EQ(traps(), 10u);
}

/** AethicFieldGeneratorAI.java:43-74, 102-127: it never thinks; at 75, 50, 25 % hp 2, 3, 4 guards; back home takes them */
TEST_F(BeshmundirAiTest, TheGeneratorCallsGuards) {
	publishSkills({19127});
	runtime::Ptr<Npc> generator = place(900200);
	GeneratorProbe& ai = install<GeneratorProbe>(*generator);
	EXPECT_FALSE(ai.canThink());
	ai.handleSpawned();
	InstanceKnownListPairing::pair(*generator, player());
	const int32_t percents[] = {75, 50, 25};
	const size_t guards[] = {2, 5, 9};
	for (size_t i = 0; i < 3; ++i) {
		generator->getLifeStats()->setCurrentHpPercent(percents[i]);
		ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
		EXPECT_EQ(npcs(*instance, 216532).size(), guards[i]) << percents[i];
	}
	EXPECT_TRUE(std::ranges::any_of(npcs(*instance, 216532), [](const runtime::Ptr<Npc>& guard) { return guard->getX() == 1364.3739f; }));
	runtime::Ptr<Npc> other = place(900300);
	ai.handleBackHome();
	EXPECT_TRUE(npcs(*instance, 216532).empty());
	EXPECT_TRUE(other->isSpawned());
}

/** :76-89: every 2 s without a living player 40 m near it goes home and takes the guards */
TEST_F(BeshmundirAiTest, TheGeneratorGivesUpWithoutPlayers) {
	runtime::Ptr<Npc> generator = place(900200, 330.0f);
	GeneratorProbe& ai = install<GeneratorProbe>(*generator);
	InstanceKnownListPairing::pair(*generator, player());
	generator->getLifeStats()->setCurrentHpPercent(75);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	ASSERT_EQ(npcs(*instance, 216532).size(), 2u);
	executor().advance(std::chrono::milliseconds(4000));
	EXPECT_EQ(npcs(*instance, 216532).size(), 2u) << "30 m";
	generator->getPosition()->setXYZH(340.5f, 300.0f, 300.0f, int8_t{0});
	executor().advance(std::chrono::milliseconds(1999));
	EXPECT_EQ(npcs(*instance, 216532).size(), 2u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_TRUE(npcs(*instance, 216532).empty()) << "40.5 m";
}

/** SacrificialSoulAI.java:38-45, 53-56: it never thinks; hit by one with 18959 it goes, by another it stays */
TEST_F(BeshmundirAiTest, TheSoulNeverThinks) {
	runtime::Ptr<Npc> soul = place(281645);
	SoulProbe& ai = install<SoulProbe>(*soul);
	EXPECT_FALSE(ai.canThink());
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_TRUE(soul->isSpawned());
}

/** PlegetonAI.java:72-75: the boatman's dialog opens 1011; another choice only returns */
TEST_F(BeshmundirAiTest, TheBoatmanOpensItsDialog) {
	runtime::Ptr<Npc> boatman = place(799518);
	PlegetonProbe& ai = install<PlegetonProbe>(*boatman);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(boatman->getObjectId(), 1011))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SELECT1_1, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(boatman->getObjectId(), 0))), 0);
}

/** BeshmundirsWalkAI.java:26-53, 81-88: the use opens 10; alone the entry wants a group; a path choice asks and shows the paths */
TEST_F(BeshmundirAiTest, TheWalkWantsAGroup) {
	runtime::Ptr<Npc> walk = place(900200);
	WalkProbe& ai = install<WalkProbe>(*walk);
	ai.handleUseItemFinish(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(walk->getObjectId(), 10))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::INSTANCE_ENTRY, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_ENTER_ONLY_PARTY_DON())), 1);
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(walk->getObjectId(), 4762))), 0);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SELECT_NONE_2, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(walk->getObjectId(), 4762))), 1);
}

/** MonolithicAmbusherAI.java:24-44: the first aggro calls the living 216215 of its row within 60 m, once until back home */
TEST_F(BeshmundirAiTest, TheAmbusherCallsItsRow) {
	runtime::Ptr<Npc> ambusher = place(900200, 300.0f, 300.0f);
	AmbusherProbe& ai = install<AmbusherProbe>(*ambusher);
	runtime::Ptr<Npc> row = place(216215, 350.0f, 300.5f);
	runtime::Ptr<Npc> otherRow = place(216215, 320.0f, 310.0f);
	runtime::Ptr<Npc> far = place(216215, 361.0f, 300.0f);
	CountingAggroAI& rowAi = install<CountingAggroAI>(*row);
	CountingAggroAI& otherAi = install<CountingAggroAI>(*otherRow);
	CountingAggroAI& farAi = install<CountingAggroAI>(*far);
	for (Npc* npc : {row.get(), otherRow.get(), far.get()})
		InstanceKnownListPairing::pair(*ambusher, *npc);
	ai.handleCreatureAggro(player());
	EXPECT_EQ(rowAi.aggros, 1);
	EXPECT_EQ(otherAi.aggros, 0) << "another row";
	EXPECT_EQ(farAi.aggros, 0) << "61 m";
	ai.handleCreatureAggro(player());
	EXPECT_EQ(rowAi.aggros, 1) << "once";
}

/** IsbariyaTheResoluteAI.java:30-72: attacked she starts once; at 75 % she tells the third phase and calls souls, at 50 % the second; back
 * home she starts over */
TEST_F(BeshmundirAiTest, IsbariyaTellsHerPhases) {
	publishSkills({18912, 18913, 18959, 18993});
	runtime::Ptr<Npc> isbariya = place(900200);
	IsbariyaProbe& ai = install<IsbariyaProbe>(*isbariya);
	ai.handleSpawned();
	hit(*isbariya); // the player on her aggro list: her attack's think does not send her home
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_Boss_ArchPriest_3phase())), 0);
	isbariya->getLifeStats()->setCurrentHpPercent(50);
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_Boss_ArchPriest_3phase())), 1);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_Boss_ArchPriest_2phase())), 0) << "one phase per hit";
	ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDCatacombs_Boss_ArchPriest_2phase())), 1);
	size_t souls = npcs(*instance, 281645).size();
	EXPECT_GE(souls, 3u);
	EXPECT_LE(souls, 6u);
	ai.handleBackHome();
	executor().advance(std::chrono::milliseconds(30000));
	EXPECT_TRUE(npcs(*instance, 281660).empty()) << "back home: no special";
}

/** IsbariyaTheResoluteAI.java:121-131: each first special calls 3 to 6 of the six souls (twelve Isbariyas, a seeded draw) */
TEST_F(BeshmundirAiTest, IsbariyaCallsThreeToSixSouls) {
	publishSkills({18912, 18913, 18959});
	commons::utils::Rnd::seedCurrentThreadForTests(20261016);
	size_t before = 0;
	for (int32_t round = 0; round < 12; ++round) {
		runtime::Ptr<Npc> isbariya = place(900200);
		IsbariyaProbe& ai = install<IsbariyaProbe>(*isbariya);
		ai.handleSpawned();
		hit(*isbariya);
		isbariya->getLifeStats()->setCurrentHpPercent(75);
		ai.handleAttack(runtime::Ptr<model::gameobjects::Creature>(player()));
		size_t souls = npcs(*instance, 281645).size() - before;
		EXPECT_GE(souls, 3u) << round;
		EXPECT_LE(souls, 6u) << round;
		before += souls;
		isbariya->getController().delete_();
	}
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
