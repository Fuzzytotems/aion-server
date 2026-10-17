// I6-4 (docs/design/p6-i6-plan.md §3): ShugoImperialTombInstance (game-server/data/handlers/instance) and the shugoImperialTomb AIs
// (game-server/data/handlers/ai/instance/shugoImperialTomb) on a real Shugo Imperial Tomb instance. A wave's walkers need their registered AI to
// walk (a test's DummyAI cannot: Java's ClassCastException, std::bad_cast here, logged by the wave's task after its first spawn), so the cases
// count a wave by its first npc. Left to a gate: the walks and routes, the transformations' effects, the teleports (HousingService), the
// bosses' casts.

#include "I6TestSupport.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombAttackerAI.h"
#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombBossAI.h"
#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombGhostAI.h"
#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombImperialObeliskAI.h"
#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombImperialShrineAI.h"
#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombModoAI.h"
#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombRoyaltyAdmirerAI.h"
#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombTransformationDeviceAI.h"
#include "aion/gameserver/handlers/instance/ShugoImperialTombInstance.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/instance/StageList.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

#define AION_TOMB_AIS(X)                                                                                                                      \
	X(ShugoTombAttackerAI)                                                                                                                    \
	X(ShugoTombBossAI)                                                                                                                        \
	X(ShugoTombGhostAI)                                                                                                                       \
	X(ShugoTombImperialObeliskAI)                                                                                                             \
	X(ShugoTombImperialShrineAI)                                                                                                              \
	X(ShugoTombModoAI)                                                                                                                        \
	X(ShugoTombRoyaltyAdmirerAI)                                                                                                              \
	X(ShugoTombTransformationDeviceAI)

#define AION_DECLARE_FACTORY(cls)                                                                                                             \
	namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb {                                                                   \
	::aion::gameserver::handlers::AIFactory cls##_aiFactory;                                                                                  \
	}
AION_TOMB_AIS(AION_DECLARE_FACTORY)
#undef AION_DECLARE_FACTORY

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace st = ::aion::gameserver::handlers::ai::instance::shugoImperialTomb;
using gameserver::ai::AIState;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::instance::StageList;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

template <class AI>
struct Probe : AI {
	using AI::AI;
	using AI::handleAttack;
	using AI::handleDialogStart;
	using AI::handleSpawned;
};
struct AttackerProbe : st::ShugoTombAttackerAI {
	using ShugoTombAttackerAI::ShugoTombAttackerAI;
	using ShugoTombAttackerAI::handleHate;
	using ShugoTombAttackerAI::handleSpawned;
};

class ShugoImperialTombTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		tomb = &create(SHUGO_IMPERIAL_TOMB, ShugoImperialTombInstance_instanceFactory);
		enter(*tomb, 178.0f, 240.0f, 536.2f);
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(AIState::IDLE);
		return typed;
	}

	runtime::Ptr<Npc> place(int32_t npcId, float x = 180.0f, float y = 240.0f, float z = 536.2f) {
		return spawnNpc(*tomb, npcId, x, y, z, int8_t{0});
	}

	size_t count(int32_t id) { return npcs(*tomb, id).size(); }

	void stageList(StageList list) { tomb->getInstanceHandler()->onChangeStageList(list); }

	void dies(int32_t npcId) { tomb->getInstanceHandler()->onDie(*place(npcId)); }

	void publishSkills(std::initializer_list<int32_t> ids) {
		dataholders::DataManager::SKILL_DATA.resetForTests();
		std::string skills = "<skill_data>";
		for (int32_t id : ids)
			skills += R"(<skill_template skill_id=")" + std::to_string(id) +
				R"(" name="tomb" nameId="1" stack="TOMB" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
				R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="FRIEND" target_type="ONLYONE" /></skill_template>)";
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	}

	xml::LoadContext skillContext;
	world::WorldMapInstance* tomb = nullptr;
};

/** The eight AION_AI factories build their classes */
TEST_F(ShugoImperialTombTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
#define AION_CHECK_FACTORY(cls)                                                                                                               \
	{                                                                                                                                         \
		auto ai = st::cls##_aiFactory(*npc);                                                                                                \
		EXPECT_NE(dynamic_cast<st::cls*>(ai.get()), nullptr) << #cls;                                                                         \
	}
	AION_TOMB_AIS(AION_CHECK_FACTORY)
#undef AION_CHECK_FACTORY
}

/** ShugoImperialTombInstance.java:45-50, 140-160: the three admirers wait; stage 1 starts once, with its towers and monument, and its waves */
TEST_F(ShugoImperialTombTest, TheFirstStageStartsOnce) {
	EXPECT_EQ(count(831110) + count(831111) + count(831112), 3u);
	stageList(StageList::START_STAGE_2_PHASE_1);
	EXPECT_EQ(count(831304), 0u) << "stage 2 waits for stage 1";
	stageList(StageList::START_STAGE_1_PHASE_1);
	EXPECT_EQ(count(831251), 1u);
	EXPECT_EQ(count(831250), 1u);
	EXPECT_EQ(count(831130), 1u) << "the Crown Prince's Monument";
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_IDEVENT01_S1_START())), 1);
	stageList(StageList::START_STAGE_1_PHASE_1);
	EXPECT_EQ(count(831130), 1u) << "once";
	executor().advance(std::chrono::milliseconds(1999));
	EXPECT_EQ(count(219508), 0u);
	executor().advance(std::chrono::milliseconds(1));
	ASSERT_EQ(count(219508), 1u) << "wave 1's first walker";
	EXPECT_EQ(npcs(*tomb, 219508)[0]->getSpawn()->getWalkerId(), "3005600001");
}

/** ShugoImperialTombInstance.java:91-93, 298-304: the Strong Kobold Worker's death starts phase 2: two 219505 and a 219544 for 10 s */
TEST_F(ShugoImperialTombTest, TheStrongWorkerStartsThePhase2) {
	dies(219514);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_IDEVENT01_PHASE03())), 1);
	EXPECT_EQ(count(219505), 2u);
	EXPECT_EQ(count(219544), 1u);
	executor().advance(std::chrono::milliseconds(9999));
	EXPECT_EQ(count(219544), 1u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(219544) + count(219505), 0u);
}

/** ShugoImperialTombInstance.java:94-110: the bosses' deaths bring the delighted admirers; Captain Lediar the shrine, chests and portals */
TEST_F(ShugoImperialTombTest, TheBossesRewardTheirStages) {
	dies(219515);
	EXPECT_EQ(count(831114), 1u);
	dies(219523);
	EXPECT_EQ(count(831115), 1u);
	EXPECT_EQ(count(219506), 9u) << "the second bonus room";
	place(831250);
	place(831305);
	dies(219531);
	EXPECT_EQ(count(831250) + count(831305), 0u) << "the towers go";
	EXPECT_EQ(count(831350), 1u) << "the Imperial Shrine";
	EXPECT_EQ(count(831116), 1u);
	EXPECT_GT(count(831122), 0u) << "the chests";
	EXPECT_GT(count(831123), 0u);
	EXPECT_GT(count(831124), 0u);
	EXPECT_GT(count(831373), 0u);
	EXPECT_EQ(count(831118), 20u) << "the exit portals";
	executor().advance(std::chrono::seconds(6));
	EXPECT_EQ(count(219506), 0u);
}

/** ShugoImperialTombInstance.java:121-137, 1321-1328: a destroyed monument stops the waves and takes the attackers away (but 219512 and 219513:
 * Java bug J-I6-3) */
TEST_F(ShugoImperialTombTest, ADestroyedMonumentEndsTheStage) {
	stageList(StageList::START_STAGE_1_PHASE_1);
	executor().advance(std::chrono::seconds(2));
	ASSERT_EQ(count(219508), 1u);
	runtime::Ptr<Npc> commander = place(219512);
	runtime::Ptr<Npc> looter = place(219510);
	tomb->getInstanceHandler()->onDie(*npcs(*tomb, 831130)[0]);
	EXPECT_EQ(count(831306), 1u) << "the Crown Prince's Disappointed Admirer";
	EXPECT_EQ(count(219508), 0u);
	EXPECT_FALSE(looter->isSpawned());
	EXPECT_TRUE(commander->isSpawned()) << "Java's despawn list reads 291512 for 219512";
	executor().advance(std::chrono::seconds(60));
	EXPECT_EQ(count(219508), 0u) << "the waves stopped";
	dies(831305);
	EXPECT_EQ(count(831307), 1u);
	EXPECT_EQ(count(831350), 1u);
	EXPECT_EQ(count(831118), 20u);
}

/** ShugoTombRoyaltyAdmirerAI.java:29-81: dialog 1011; the Crown Prince's Admirer starts stage 1 and goes; the delighted one's second choice */
TEST_F(ShugoImperialTombTest, TheAdmirersStartTheStages) {
	publishSkills({21095, 21104});
	runtime::Ptr<Npc> admirer = npcs(*tomb, 831110)[0];
	auto& ai = install<Probe<st::ShugoTombRoyaltyAdmirerAI>>(*admirer);
	ai.handleDialogStart(player());
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(admirer->getObjectId(), 1011))), 1);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_EQ(count(831130), 1u) << "stage 1 started";
	EXPECT_FALSE(admirer->isSpawned());
	runtime::Ptr<Npc> delighted = place(831114);
	auto& delightedAi = install<Probe<st::ShugoTombRoyaltyAdmirerAI>>(*delighted);
	EXPECT_TRUE(delightedAi.onDialogSelect(player(), model::DialogAction::SETPRO2, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(delighted->getObjectId(), 1012))), 1);
}

/** ShugoTombImperialShrineAI.java:57-98: without a tag each choice tells the player which tag he lacks and sends him nowhere */
TEST_F(ShugoImperialTombTest, TheShrineWantsATag) {
	runtime::Ptr<Npc> shrine = place(831350);
	auto& ai = install<st::ShugoTombImperialShrineAI>(*shrine);
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO1, 0, 0));
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO2, 0, 0));
	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SETPRO3, 0, 0));
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_IDEVENT01_GOLD_MAP())), 1);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_IDEVENT01_SILVER_MAP())), 1);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_IDEVENT01_BRONZE_MAP())), 1);
	EXPECT_EQ(countSent(serialized(SM_DIALOG_WINDOW(shrine->getObjectId(), 0))), 0);
}

/** ShugoTombAttackerAI.java:32-78, ShugoTombModoAI.java:16-20: no thinking while it walks; the towers within 30 m are hated, the nearest most;
 * without one it is deleted */
TEST_F(ShugoImperialTombTest, TheAttackersHateTheTowers) {
	// the towers' tribe is hostile to the attackers' in the real data: here GENERAL to GENERAL (the fixture's TearDown resets the holder)
	dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
	dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(xml::bindString<dataholders::TribeRelationsData>(skillContext,
		R"(<tribe_relations><tribe name="GENERAL"><hostile>GENERAL PC PC_DARK</hostile></tribe><tribe name="PC"><none>GENERAL</none></tribe>)"
		R"(<tribe name="PC_DARK"><none>GENERAL</none></tribe></tribe_relations>)"));
	runtime::Ptr<Npc> nearTower = place(831250, 185.0f, 240.0f);
	runtime::Ptr<Npc> farTower = place(831251, 200.0f, 240.0f);
	place(831130, 260.0f, 240.0f); // 80 m away
	runtime::Ptr<Npc> attacker = place(219508, 180.0f, 240.0f);
	InstanceKnownListPairing::pair(*attacker, *nearTower);
	InstanceKnownListPairing::pair(*attacker, *farTower);
	auto& ai = install<AttackerProbe>(*attacker);
	EXPECT_TRUE(ai.canThink());
	ai.handleSpawned();
	EXPECT_FALSE(ai.canThink());
	EXPECT_FALSE(ai.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
	EXPECT_FALSE(ai.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_FALSE(ai.ask(AIQuestion::REWARD_LOOT));
	ai.handleHate();
	EXPECT_EQ(attacker->getAggroList().getTarget(controllers::attack::AggroTarget::MOST_HATED), runtime::Ptr<Creature>(nearTower));
	runtime::Ptr<Npc> alone = place(219508, 120.0f, 240.0f);
	auto& aloneAi = install<AttackerProbe>(*alone);
	aloneAi.handleHate();
	EXPECT_FALSE(alone->isSpawned());
	runtime::Ptr<Npc> modo = place(219630, 182.0f, 240.0f);
	InstanceKnownListPairing::pair(*modo, *nearTower);
	auto& modoAi = install<Probe<st::ShugoTombModoAI>>(*modo);
	modoAi.handleSpawned();
	EXPECT_EQ(modo->getAggroList().getTarget(controllers::attack::AggroTarget::MOST_HATED), runtime::Ptr<Creature>(nearTower));
}

/** ShugoTombBossAI.java:57-80, ShugoTombGhostAI.java:23-44, ShugoTombImperialObeliskAI.java:24-56: the boss's 21266 calls four 219509; the
 * ghosts and the obelisks' questions */
TEST_F(ShugoImperialTombTest, TheBossGhostsAndObelisks) {
	publishSkills({21266});
	runtime::Ptr<Npc> boss = place(219531, 180.0f, 240.0f);
	auto& ai = install<Probe<st::ShugoTombBossAI>>(*boss);
	EXPECT_TRUE(ai.ask(AIQuestion::ALLOW_DECAY));
	EXPECT_TRUE(ai.ask(AIQuestion::REWARD_LOOT));
	ai.onEndUseSkill(dataholders::DataManager::SKILL_DATA->getSkillTemplate(21266), 1);
	ASSERT_EQ(count(219509), 4u);
	for (const runtime::Ptr<Npc>& add : npcs(*tomb, 219509))
		EXPECT_LE(std::hypot(add->getX() - 180.0f, add->getY() - 240.0f), 2.01f);
	runtime::Ptr<Npc> bonusGhost = place(219505);
	auto& ghostAi = install<st::ShugoTombGhostAI>(*bonusGhost);
	EXPECT_FALSE(ghostAi.canThink());
	EXPECT_FALSE(ghostAi.ask(AIQuestion::REWARD_LOOT));
	runtime::Ptr<Npc> otherGhost = place(219461);
	auto& otherGhostAi = install<st::ShugoTombGhostAI>(*otherGhost);
	EXPECT_TRUE(otherGhostAi.ask(AIQuestion::ALLOW_DECAY));
	runtime::Ptr<Npc> obelisk = place(831250);
	auto& obeliskAi = install<st::ShugoTombImperialObeliskAI>(*obelisk);
	EXPECT_FALSE(obeliskAi.canThink());
	EXPECT_FALSE(obeliskAi.ask(AIQuestion::ALLOW_RESPAWN));
	EXPECT_TRUE(obeliskAi.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
