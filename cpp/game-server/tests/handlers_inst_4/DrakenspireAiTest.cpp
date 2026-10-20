// P6I-12 (docs/design/p6-instances-plan.md, I4 split): the AIs of drakenspire (game-server/data/handlers/ai/instance) on npcs of a real
// Drakenspire Depths instance, each AI installed with replaceAi. Left to a gate: the casts and effects (the waves' 21874, the seals, the
// buffs, the pulse waves), the walks and their ends (no walker rows), the portal's teleport and Beritra's transformation.

#include "I4TestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/templates/item/ItemAttackType.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/BeritraMinionAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/BeritraPortalAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/DimensionalWaveAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/DimensionalWaveSmallAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/DragonBeritraAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/GhastlyProtectorAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/Lv1HumanBeritraAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/Lv2HumanBeritraAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/Lv3HumanBeritraAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/OrissanAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/OrissanDoorDestroyerAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/OrissanSummonHelperAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/OrissansSummonAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/SealBreakerAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/SealGuardianAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/TwinDoorDestroyerAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/TwinProtectorAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/WaveAttackerAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/WaveDefenderAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/WaveEntrySensorAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/WaveEventBomberAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/WaveEventExecutorAI.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/WavePortalAI.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {
::aion::gameserver::handlers::AIFactory BeritraMinionAI_aiFactory;
::aion::gameserver::handlers::AIFactory BeritraPortalAI_aiFactory;
::aion::gameserver::handlers::AIFactory DimensionalWaveAI_aiFactory;
::aion::gameserver::handlers::AIFactory DimensionalWaveSmallAI_aiFactory;
::aion::gameserver::handlers::AIFactory DragonBeritraAI_aiFactory;
::aion::gameserver::handlers::AIFactory GhastlyProtectorAI_aiFactory;
::aion::gameserver::handlers::AIFactory Lv1HumanBeritraAI_aiFactory;
::aion::gameserver::handlers::AIFactory Lv2HumanBeritraAI_aiFactory;
::aion::gameserver::handlers::AIFactory Lv3HumanBeritraAI_aiFactory;
::aion::gameserver::handlers::AIFactory OrissanAI_aiFactory;
::aion::gameserver::handlers::AIFactory OrissanDoorDestroyerAI_aiFactory;
::aion::gameserver::handlers::AIFactory OrissanSummonHelperAI_aiFactory;
::aion::gameserver::handlers::AIFactory OrissansSummonAI_aiFactory;
::aion::gameserver::handlers::AIFactory SealBreakerAI_aiFactory;
::aion::gameserver::handlers::AIFactory SealGuardianAI_aiFactory;
::aion::gameserver::handlers::AIFactory TwinDoorDestroyerAI_aiFactory;
::aion::gameserver::handlers::AIFactory TwinProtectorAI_aiFactory;
::aion::gameserver::handlers::AIFactory WaveAttackerAI_aiFactory;
::aion::gameserver::handlers::AIFactory WaveDefenderAI_aiFactory;
::aion::gameserver::handlers::AIFactory WaveEntrySensorAI_aiFactory;
::aion::gameserver::handlers::AIFactory WaveEventBomberAI_aiFactory;
::aion::gameserver::handlers::AIFactory WaveEventExecutorAI_aiFactory;
::aion::gameserver::handlers::AIFactory WavePortalAI_aiFactory;
} // namespace aion::gameserver::handlers::ai::instance::drakenspire

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace ais = ::aion::gameserver::handlers::ai::instance::drakenspire;
using gameserver::ai::poll::AIQuestion;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

// the protected handlers the cases drive directly (Java: the AI's events)
struct SealGuardianProbe : ais::SealGuardianAI {
	using SealGuardianAI::SealGuardianAI;
	using SealGuardianAI::handleCreatureAggro;
	using SealGuardianAI::handleDespawned;
	using SealGuardianAI::handleSpawned;
};
struct MinionProbe : ais::BeritraMinionAI {
	using BeritraMinionAI::BeritraMinionAI;
	using BeritraMinionAI::handleSpawned;
};
struct OrissanProbe : ais::OrissanAI {
	using OrissanAI::OrissanAI;
	using OrissanAI::handleDied;
};
struct HelperProbe : ais::OrissanSummonHelperAI {
	using OrissanSummonHelperAI::OrissanSummonHelperAI;
};
struct DefenderProbe : ais::WaveDefenderAI {
	using WaveDefenderAI::WaveDefenderAI;
};
struct PortalProbe : ais::WavePortalAI {
	using WavePortalAI::WavePortalAI;
	using WavePortalAI::handleDespawned;
	using WavePortalAI::handleSpawned;
};
struct TwinDoorProbe : ais::TwinDoorDestroyerAI {
	using TwinDoorDestroyerAI::TwinDoorDestroyerAI;
};
struct BeritraProbe : ais::Lv1HumanBeritraAI {
	using Lv1HumanBeritraAI::Lv1HumanBeritraAI;
	using Lv1HumanBeritraAI::handleBackHome;
};
struct TwinProbe : ais::TwinProtectorAI {
	using TwinProtectorAI::TwinProtectorAI;
	using TwinProtectorAI::handleBackHome;
};
struct SealBreakerProbe : ais::SealBreakerAI {
	using SealBreakerAI::SealBreakerAI;
	using SealBreakerAI::handleDied;
};

class DrakenspireAiTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(DRAKENSPIRE_DEPTHS, DrakenspireDepthsInstance_instanceFactory);
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

	size_t count(std::initializer_list<int32_t> ids) {
		size_t n = 0;
		for (int32_t id : ids)
			n += npcs(*instance, id).size();
		return n;
	}

	const skillengine::model::SkillTemplate* skill(int32_t id) { return dataholders::DataManager::SKILL_DATA->getSkillTemplate(id); }

	world::WorldMapInstance* instance = nullptr;
};

/** The twenty-three AION_AI factories build their classes (the names are the registry scan's check) */
TEST_F(DrakenspireAiTest, TheFactoriesBuildTheirClasses) {
	runtime::Ptr<Npc> npc = place(900200);
	EXPECT_NE(dynamic_cast<ais::BeritraMinionAI*>(ais::BeritraMinionAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::BeritraPortalAI*>(ais::BeritraPortalAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::DimensionalWaveAI*>(ais::DimensionalWaveAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::DimensionalWaveSmallAI*>(ais::DimensionalWaveSmallAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::DragonBeritraAI*>(ais::DragonBeritraAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::GhastlyProtectorAI*>(ais::GhastlyProtectorAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::Lv1HumanBeritraAI*>(ais::Lv1HumanBeritraAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::Lv2HumanBeritraAI*>(ais::Lv2HumanBeritraAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::Lv3HumanBeritraAI*>(ais::Lv3HumanBeritraAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::OrissanAI*>(ais::OrissanAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::OrissanDoorDestroyerAI*>(ais::OrissanDoorDestroyerAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::OrissanSummonHelperAI*>(ais::OrissanSummonHelperAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::OrissansSummonAI*>(ais::OrissansSummonAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::SealBreakerAI*>(ais::SealBreakerAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::SealGuardianAI*>(ais::SealGuardianAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::TwinDoorDestroyerAI*>(ais::TwinDoorDestroyerAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::TwinProtectorAI*>(ais::TwinProtectorAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::WaveAttackerAI*>(ais::WaveAttackerAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::WaveDefenderAI*>(ais::WaveDefenderAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::WaveEntrySensorAI*>(ais::WaveEntrySensorAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::WaveEventBomberAI*>(ais::WaveEventBomberAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::WaveEventExecutorAI*>(ais::WaveEventExecutorAI_aiFactory(*npc).get()), nullptr);
	EXPECT_NE(dynamic_cast<ais::WavePortalAI*>(ais::WavePortalAI_aiFactory(*npc).get()), nullptr);
}

/** OrissanAI.java:132-136 with the correction of 2026-10-19 (was J-P6I12-1): his death is NpcAI's death (DiedEventHandler: the DIED state,
 * no target), not the despawn handling Java called */
TEST_F(DrakenspireAiTest, OrissansDeathIsADeath) {
	runtime::Ptr<Npc> orissan = place(900200);
	auto& ai = install<OrissanProbe>(*orissan);
	orissan->setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(player()));
	ai.handleDied();
	EXPECT_EQ(ai.getState(), gameserver::ai::AIState::DIED);
	EXPECT_EQ(orissan->getTarget(), nullptr);
}

/** SealBreakerAI.java, DragonBeritraAI.java, OrissansSummonAI.java, OrissanSummonHelperAI.java: their answers */
TEST_F(DrakenspireAiTest, TheAnswers) {
	SealBreakerProbe& breaker = install<SealBreakerProbe>(*place(900200));
	EXPECT_TRUE(breaker.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
	auto& dragon = install<ais::DragonBeritraAI>(*place(900200));
	EXPECT_TRUE(dragon.ask(AIQuestion::CONSIDER_BOUNDS_IN_CAN_SEE_CHECK_WHEN_ATTACKED));
	EXPECT_TRUE(dragon.ask(AIQuestion::CONSIDER_BOUNDS_IN_CAN_SEE_CHECK_WHEN_ATTACKING));
	auto& crystal = install<ais::OrissansSummonAI>(*place(900200));
	EXPECT_TRUE(crystal.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
	EXPECT_FALSE(crystal.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	auto& helper = install<ais::OrissanSummonHelperAI>(*place(900200));
	EXPECT_FALSE(helper.ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
	EXPECT_FALSE(helper.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
	auto& guardian = install<ais::SealGuardianAI>(*place(900200));
	EXPECT_TRUE(guardian.ask(AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES));
	EXPECT_EQ(guardian.modifyAttackType(model::templates::item::ItemAttackType::PHYSICAL), model::templates::item::ItemAttackType::MAGICAL_WIND);
	auto& ghastly = install<ais::GhastlyProtectorAI>(*place(900200));
	EXPECT_EQ(ghastly.modifyAttackType(model::templates::item::ItemAttackType::PHYSICAL), model::templates::item::ItemAttackType::MAGICAL_WIND);
}

/** OrissansSummonAI.java:30-49: another npc's hit takes 10000, the player's its damage */
TEST_F(DrakenspireAiTest, TheCrystalsTakeTheNpcsHits) {
	runtime::Ptr<Npc> crystal = place(855699);
	auto& ai = install<ais::OrissansSummonAI>(*crystal);
	EXPECT_FLOAT_EQ(ai.modifyDamage(player(), 42.0f, nullptr), 42.0f);
	EXPECT_FLOAT_EQ(ai.modifyDamage(*crystal, 42.0f, nullptr), 42.0f) << "its own";
	EXPECT_FLOAT_EQ(ai.modifyDamage(*place(900200), 42.0f, nullptr), 10000.0f);
}

/** OrissanSummonHelperAI.java:25-48: no damage; 1.5 s after a cast it becomes its crystal (855607: 855699, else 855700) */
TEST_F(DrakenspireAiTest, TheHelpersBecomeCrystals) {
	publishSkills({21647});
	runtime::Ptr<Npc> frigid = place(855607, 310.0f, 320.0f);
	HelperProbe& ai = install<HelperProbe>(*frigid);
	EXPECT_FLOAT_EQ(ai.modifyDamage(player(), 42.0f, nullptr), 0.0f);
	ai.onEndUseSkill(skill(21647), 1);
	executor().advance(std::chrono::milliseconds(1499));
	EXPECT_TRUE(frigid->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(frigid->isSpawned());
	ASSERT_EQ(count({855699}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 855699).front()->getX(), 310.0f);
	runtime::Ptr<Npc> icing = place(855608);
	install<HelperProbe>(*icing).onEndUseSkill(skill(21647), 1);
	executor().advance(std::chrono::milliseconds(1500));
	EXPECT_EQ(count({855700}), 1u);
}

/** SealGuardianAI.java:74-104, 113-125, 130-148: a second after the spawn its specter comes; idle 60 s it goes and its specter with it;
 * an aggro stops the idle timer */
TEST_F(DrakenspireAiTest, TheSealGuardiansIdleOut) {
	runtime::Ptr<Npc> guardian = place(855461);
	SealGuardianProbe& ai = install<SealGuardianProbe>(*guardian);
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(1000));
	ASSERT_EQ(count({855454}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 855454).front()->getX(), 172.045f);
	EXPECT_EQ(count({855452, 855456, 855458}), 0u);
	executor().advance(std::chrono::milliseconds(58999));
	EXPECT_TRUE(guardian->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(guardian->isSpawned());
	EXPECT_EQ(count({855454}), 0u) << "its specter goes with it (handleDespawned)";

	runtime::Ptr<Npc> busy = place(855462);
	SealGuardianProbe& busyAi = install<SealGuardianProbe>(*busy);
	busyAi.handleSpawned();
	busyAi.handleCreatureAggro(player());
	executor().advance(std::chrono::milliseconds(61000));
	EXPECT_TRUE(busy->isSpawned()) << "fighting";
}

/** SealGuardianAI.java:94-100, 141-148: a dragon phase guardian brings all four specters and takes them all */
TEST_F(DrakenspireAiTest, TheDragonPhaseGuardianBringsAllSpecters) {
	runtime::Ptr<Npc> guardian = place(855464);
	SealGuardianProbe& ai = install<SealGuardianProbe>(*guardian);
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(1000));
	EXPECT_EQ(count({855452, 855454, 855456, 855458}), 4u);
	ai.handleDespawned();
	EXPECT_EQ(count({855452, 855454, 855456, 855458}), 0u);
}

/** SealGuardianAI.java:77-81, 150-155 and Lv1HumanBeritraAI.java:69-74: the idle guardian tells Beritra (event 2), who calls a new one */
TEST_F(DrakenspireAiTest, AnIdleGuardianCallsTheNext) {
	runtime::Ptr<Npc> beritra = place(236245, 152.0f, 518.0f);
	install<BeritraProbe>(*beritra);
	runtime::Ptr<Npc> guardian = place(855463);
	install<SealGuardianProbe>(*guardian).handleSpawned();
	executor().advance(std::chrono::milliseconds(60000));
	EXPECT_FALSE(guardian->isSpawned());
	EXPECT_EQ(count({855460, 855461, 855462, 855463}), 1u) << "a new one";
}

/** Lv1HumanBeritraAI.java:69-74, 59-61: event 1 (a guardian's death) waits a minute while he fights; before the fight nothing comes */
TEST_F(DrakenspireAiTest, BeritraCallsGuardiansOnlyInTheFight) {
	runtime::Ptr<Npc> beritra = place(236245, 152.0f, 518.0f);
	install<BeritraProbe>(*beritra);
	beritra->getAi().onCustomEvent(1);
	executor().advance(std::chrono::milliseconds(61000));
	EXPECT_EQ(count({855460, 855461, 855462, 855463}), 0u);
}

/** Lv1HumanBeritraAI.java:301-308: home, he takes the helpers, pustules and guardians */
TEST_F(DrakenspireAiTest, BeritraHomeTakesHisCompany) {
	runtime::Ptr<Npc> beritra = place(236245, 152.0f, 518.0f);
	BeritraProbe& ai = install<BeritraProbe>(*beritra);
	for (int32_t id : {209734, 209802, 855446, 855460})
		place(id);
	runtime::Ptr<Npc> other = place(855447);
	ai.handleBackHome();
	EXPECT_EQ(count({209734, 209802, 855446, 855460}), 0u);
	EXPECT_TRUE(other->isSpawned());
}

/** BeritraMinionAI.java:31-41, 48-55: 20 s after the spawn it leaves its skill npc (855444: 855447) and dies half a second later */
TEST_F(DrakenspireAiTest, TheMinionsLeaveTheirSkillNpc) {
	runtime::Ptr<Npc> minion = place(855444, 310.0f, 320.0f);
	MinionProbe& ai = install<MinionProbe>(*minion);
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(19999));
	EXPECT_EQ(count({855447}), 0u);
	executor().advance(std::chrono::milliseconds(1));
	ASSERT_EQ(count({855447}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 855447).front()->getY(), 320.0f);
	EXPECT_FALSE(minion->isDead());
	executor().advance(std::chrono::milliseconds(500));
	EXPECT_TRUE(minion->isDead());
}

/** WavePortalAI.java:26-63, 76-82: from 8 s the top left portal (static 84) sends two destroyers every 22 s; gone, it stops */
TEST_F(DrakenspireAiTest, ThePortalsSendTheirWaves) {
	runtime::Ptr<Npc> portal = spawnStaticNpc(*instance, 731581, 300.0f, 300.0f, 300.0f, 84);
	PortalProbe& ai = install<PortalProbe>(*portal);
	ai.handleSpawned();
	executor().advance(std::chrono::milliseconds(7999));
	EXPECT_EQ(count({236204}), 0u);
	executor().advance(std::chrono::milliseconds(1));
	ASSERT_EQ(count({236204}), 2u);
	std::vector<std::string> walkers;
	for (const runtime::Ptr<Npc>& npc : npcs(*instance, 236204))
		walkers.push_back(npc->getSpawn()->getWalkerId().value_or(""));
	std::ranges::sort(walkers);
	EXPECT_EQ(walkers, (std::vector<std::string>{"301390000_Wave_Top_Left_01", "301390000_Wave_Top_Left_02"}));
	EXPECT_FLOAT_EQ(npcs(*instance, 236204).front()->getX(), 581.92f);
	executor().advance(std::chrono::milliseconds(22000));
	EXPECT_EQ(count({236204}), 4u);
	ai.handleDespawned();
	executor().advance(std::chrono::milliseconds(44000));
	EXPECT_EQ(count({236204}), 4u);
}

/** WavePortalAI.java:32-36: the central portal (static 405) sends three commanders every 40 s, two of them a second late */
TEST_F(DrakenspireAiTest, TheCentralPortalSendsCommanders) {
	runtime::Ptr<Npc> portal = spawnStaticNpc(*instance, 731581, 300.0f, 300.0f, 300.0f, 405);
	install<PortalProbe>(*portal).handleSpawned();
	auto commanders = [this] { return count({236216, 236217, 236218, 236219, 236220}); };
	executor().advance(std::chrono::milliseconds(8000));
	EXPECT_EQ(commanders(), 1u);
	executor().advance(std::chrono::milliseconds(1000));
	EXPECT_EQ(commanders(), 3u);
	executor().advance(std::chrono::milliseconds(38999));
	EXPECT_EQ(commanders(), 3u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(commanders(), 4u);
}

/** WaveDefenderAI.java:26-30: an arrival before its route's end (isStop: the last step of a walker route, no walker rows here) does nothing */
TEST_F(DrakenspireAiTest, TheDefendersWaitForTheirRoutesEnd) {
	runtime::Ptr<Npc> defender = place(900200, 310.0f, 320.0f);
	defender->getSpawn()->setWalkerId(std::string_view("301390000_NPCPathFunction_Npc_Path09"));
	DefenderProbe& ai = install<DefenderProbe>(*defender);
	ai.handleMoveArrived();
	executor().advance(std::chrono::milliseconds(5000));
	EXPECT_TRUE(defender->isSpawned());
	EXPECT_EQ(count({236248, 236249}), 0u);
}

/** TwinDoorDestroyerAI.java:22-43: 1.5 s after its spawn the traps go */
TEST_F(DrakenspireAiTest, TheTwinDoorDestroyerTakesTheTraps) {
	runtime::Ptr<Npc> trap = place(207128);
	runtime::Ptr<Npc> other = place(900300);
	runtime::Ptr<Npc> destroyer = place(900200);
	install<TwinDoorProbe>(*destroyer).handleSpawned();
	executor().advance(std::chrono::milliseconds(1499));
	EXPECT_TRUE(trap->isSpawned());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(trap->isSpawned());
	EXPECT_TRUE(other->isSpawned());
}

/** WaveEntrySensorAI.java:21-26: a detected player removes it, an npc does not */
TEST_F(DrakenspireAiTest, TheSensorGoesForAPlayer) {
	runtime::Ptr<Npc> sensor = place(900200);
	auto& ai = install<ais::WaveEntrySensorAI>(*sensor);
	ai.handleCreatureDetected(*place(900300));
	EXPECT_TRUE(sensor->isSpawned());
	ai.handleCreatureDetected(player());
	EXPECT_FALSE(sensor->isSpawned());
}

/** TwinProtectorAI.java:31-63: at 25 % hp one add (855621 for an odd npc id) on its target, 3 below 20 % (one target here); home takes them */
TEST_F(DrakenspireAiTest, TheTwinsCallAdds) {
	runtime::Ptr<Npc> twin = place(236227);
	TwinProbe& ai = install<TwinProbe>(*twin);
	hit(*twin);
	ai.handleHpPhase(25);
	ASSERT_EQ(count({855621}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 855621).front()->getX(), player().getX());
	EXPECT_EQ(count({855622}), 0u);
	ai.handleBackHome();
	EXPECT_EQ(count({855621}), 0u);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
