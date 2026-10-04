// M5e stage 3, M-04 (m5e-plan.md §2.5, §5; P5-05 / aion_gs_handlers_ai_core): the AIs of the summoned npcs - ServantNpcAI ("servant"),
// TrapNpcAI ("trap"), HomingNpcAI ("homing") and SkillAreaNpcAI ("skillarea", which M-04's list missed: Ice Sheet's npcs carry it).
//
// The cases build real Servants and Traps (the objects VisibleObjectSpawner.spawnServant / spawnTrap create) over the shipped rows of
// ../ai/NpcSkillTestSupport.h and install the real AI on them, as RootAiHandlersTest does (this executable links the empty registry table):
// - the striped kerub's template (210133, one skill: 16419 Brandish, which has a cast time, so a cast shows as isCasting) for the servants,
//   created by a kerub whose target is a tamed pagati (282949, its enemy); a copy of it under the Battle Banner's id 833078, whose
//   ServantNpcAI timing differs;
// - test traps (see TRAP_NPCS_XML) created by a kerub, with a pagati as the enemy that walks in.
// The expectations follow ServantNpcAI.java:21-95, TrapNpcAI.java:23-104, HomingNpcAI.java:12-38 and SkillAreaNpcAI.java:8-14.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AttackIntention.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/TrapController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/handlers/ai/HomingNpcAI.h"
#include "aion/gameserver/handlers/ai/ServantNpcAI.h"
#include "aion/gameserver/handlers/ai/SkillAreaNpcAI.h"
#include "aion/gameserver/handlers/ai/TrapNpcAI.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/NpcObjectType.h"
#include "aion/gameserver/model/gameobjects/Servant.h"
#include "aion/gameserver/model/gameobjects/Trap.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/world/World.h"

#include "../ai/NpcSkillTestSupport.h"

// The factory functions the AION_AI markers define, declared exactly as Registry.ai.gen.cpp declares them (handlers::AIFactory).
namespace aion::gameserver::handlers::ai {
::aion::gameserver::handlers::AIFactory ServantNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory TrapNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory HomingNpcAI_aiFactory;
::aion::gameserver::handlers::AIFactory SkillAreaNpcAI_aiFactory;
} // namespace aion::gameserver::handlers::ai

namespace aion::gameserver::ai::testing {
namespace {

namespace Rnd = commons::utils::Rnd;
namespace roots = gameserver::handlers::ai;

using event::AIEventType;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::NpcObjectType;
using model::gameobjects::Servant;
using model::gameobjects::Trap;
using model::gameobjects::state::CreatureVisualState;
using poll::AIQuestion;
using runtime::Ptr;
using runtime::Ref;

constexpr int32_t BATTLE_BANNER_NPC_ID = 833078;
constexpr int32_t TOTEM_NPC_ID = 833990;
constexpr int32_t TRAP_NPC_ID = 749990;
constexpr int32_t SCRAPPED_MECHANISMS_NPC_ID = 749991;
constexpr int32_t SHOCK_TRAP_NPC_ID = 749992;

/**
 * The striped kerub's template and Brandish row under the Battle Banner's id (ServantNpcAI times it differently), the kerub's template under a
 * test totem id with 20556 Protective Shield (a friendly skill with a 1 s cast: a totem targets itself), and three test traps: an
 * ordinary one (attack range 3), "Scrapped Mechanisms" (which TrapNpcAI does not hide) and "Shock Trap" (which goes off as it spawns).
 * The names are compared in lower case, so the test spells them in mixed case.
 */
constexpr const char* SUMMON_AI_NPCS_XML =
	R"(<npc_template npc_id="833078" level="1" name="striped kerub banner" name_id="300110" height="1.372" rank="DISCIPLINED")"
	R"( rating="NORMAL" race="MAGICALMONSTER" tribe="MONSTER" type="MONSTER" ai="servant" srange="7" arange="2" attack_speed="2100")"
	R"( hpgauge="3"><stats maxHp="143"><speeds walk="0.6" run="7" run_fight="5.5" /></stats>)"
	R"(<bound_radius front="0.525" side="0.275" upper="1.372" /></npc_template>)"
	R"(<npc_template npc_id="833990" level="1" name="striped kerub totem" name_id="300110" height="1.372" rank="DISCIPLINED")"
	R"( rating="NORMAL" race="MAGICALMONSTER" tribe="MONSTER" type="MONSTER" ai="servant" srange="7" arange="2" attack_speed="2100")"
	R"( hpgauge="3"><stats maxHp="143"><speeds walk="0.6" run="7" run_fight="5.5" /></stats>)"
	R"(<bound_radius front="0.525" side="0.275" upper="1.372" /></npc_template>)"
	R"(<npc_template npc_id="749990" level="1" name="Test Trap" name_id="1" height="3" rank="NOVICE" rating="JUNK" tribe="MONSTER")"
	R"( type="ABYSS_GUARD" ai="trap" srange="4" arange="3" attack_speed="2000" hpgauge="1"><stats maxHp="1" /></npc_template>)"
	R"(<npc_template npc_id="749991" level="1" name="Scrapped Mechanisms" name_id="1" height="3" rank="NOVICE" rating="JUNK" tribe="MONSTER")"
	R"( type="ABYSS_GUARD" ai="trap" srange="4" arange="3" attack_speed="2000" hpgauge="1"><stats maxHp="1" /></npc_template>)"
	R"(<npc_template npc_id="749992" level="1" name="Shock Trap" name_id="1" height="3" rank="NOVICE" rating="JUNK" tribe="MONSTER")"
	R"( type="ABYSS_GUARD" ai="trap" srange="4" arange="3" attack_speed="2000" hpgauge="1"><stats maxHp="1" /></npc_template>)";

constexpr const char* SUMMON_AI_SKILL_ROWS_XML = R"(<npc_skills npc_ids="833078"><npc_skill id="16419" lv="1" prob="25" /></npc_skills>)"
												 R"(<npc_skills npc_ids="833990"><npc_skill id="20556" lv="1" prob="100" /></npc_skills>)";

class SummonAiHandlersTest : public NpcSkillWorldTest {
protected:
	void SetUp() override {
		NpcSkillWorldTest::SetUp();
		AI_TEST_SCOPE;
		std::string npcTemplates = aiNpcTemplatesXml();
		npcTemplates.insert(npcTemplates.rfind("</npc_templates>"), std::string(NPC_SKILL_NPC_TEMPLATES_XML) + SUMMON_AI_NPCS_XML);
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), npcTemplates));
		std::string skillRows = NPC_SKILL_ROWS_XML;
		skillRows.insert(skillRows.rfind("</npc_skill_templates>"), SUMMON_AI_SKILL_ROWS_XML);
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.publish(xml::bindString<dataholders::NpcSkillData>(contexts.emplace_back(), skillRows));
		regionActivator = activateRegionAt(500, 500, 100);
		executor->runReady(); // MapRegion::activate posts the ACTIVATE notification of its creatures
		creator = makeWorldNpc(STRIPED_KERUB_NPC_ID, 499, 500, 100);
		pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501.5f, 500, 100);
		creator->setTarget(Ptr<model::gameobjects::VisibleObject>(*pagati));
	}

	void TearDown() override {
		{
			AI_TEST_SCOPE;
			for (const Ref<Npc>& summoned : summonedNpcs)
				world::World::getInstance().removeObject(*summoned);
		}
		summonedNpcs.clear();
		creator = nullptr;
		pagati = nullptr;
		regionActivator = nullptr;
		NpcSkillWorldTest::TearDown();
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto handlerAi = std::make_unique<AI>(npc);
		AI& result = *handlerAi;
		npc.replaceAi(std::move(handlerAi));
		return result;
	}

	/** VisibleObjectSpawner.spawnServant's object (a Servant of the creator, the object type set after creation), stored in the World */
	Ref<Servant> servant(int32_t npcId, NpcObjectType objectType = NpcObjectType::SERVANT) {
		Ref<Servant> result = model::gameobjects::VisibleObject::create<Servant>(std::make_unique<controllers::NpcController>(),
			makeSpawn(npcId, 500, 500, 100), int8_t{1}, *creator);
		result->setNpcObjectType(objectType);
		finish(*result);
		return result;
	}

	/** VisibleObjectSpawner.spawnTrap's object, stored in the World */
	Ref<Trap> trap(int32_t npcId) {
		Ref<Trap> result = model::gameobjects::VisibleObject::create<Trap>(std::make_unique<controllers::TrapController>(),
			makeSpawn(npcId, 500, 500, 100), *creator);
		finish(*result);
		return result;
	}

	void finish(Npc& npc) {
		npc.setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(npc));
		npc.setEffectController(std::make_unique<controllers::effect::EffectController>(npc));
		place(npc, 500, 500, 100);
		world::World::getInstance().storeObject(npc);
		summonedNpcs.push_back(Ref<Npc>(npc));
	}

	static bool inWorld(Npc& npc) { return static_cast<bool>(world::World::getInstance().findVisibleObject(npc.getObjectId())); }

	void advance(int64_t millis) { executor->advance(std::chrono::milliseconds(millis)); }

	Ref<model::gameobjects::player::Player> regionActivator;
	Ref<Npc> creator;
	Ref<Npc> pagati;
	std::vector<Ref<Npc>> summonedNpcs;
};

// ---- the AION_AI markers ----------------------------------------------------------------------------------------------------------------------

TEST_F(SummonAiHandlersTest, EachMarkerDefinesAFactoryThatBuildsItsAiForAnNpcOwner) {
	AI_TEST_SCOPE;
	Ref<Npc> npc = makeWorldNpc(STRIPED_KERUB_NPC_ID, 505, 500, 100);

	std::unique_ptr<AbstractAI> servantAi = roots::ServantNpcAI_aiFactory(*npc);
	ASSERT_TRUE(dynamic_cast<roots::ServantNpcAI*>(servantAi.get()));
	EXPECT_TRUE(dynamic_cast<roots::GeneralNpcAI*>(servantAi.get())) << "ServantNpcAI extends GeneralNpcAI";
	std::unique_ptr<AbstractAI> homingAi = roots::HomingNpcAI_aiFactory(*npc);
	ASSERT_TRUE(dynamic_cast<roots::HomingNpcAI*>(homingAi.get()));
	EXPECT_TRUE(dynamic_cast<roots::GeneralNpcAI*>(homingAi.get())) << "HomingNpcAI extends GeneralNpcAI";
	std::unique_ptr<AbstractAI> trapAi = roots::TrapNpcAI_aiFactory(*npc);
	ASSERT_TRUE(dynamic_cast<roots::TrapNpcAI*>(trapAi.get()));
	EXPECT_FALSE(dynamic_cast<roots::GeneralNpcAI*>(trapAi.get())) << "TrapNpcAI extends NpcAI directly";
	std::unique_ptr<AbstractAI> skillAreaAi = roots::SkillAreaNpcAI_aiFactory(*npc);
	ASSERT_TRUE(dynamic_cast<roots::SkillAreaNpcAI*>(skillAreaAi.get()));
	EXPECT_FALSE(dynamic_cast<roots::GeneralNpcAI*>(skillAreaAi.get()));

	EXPECT_EQ(roots::ServantNpcAI_aiFactory(*regionActivator), nullptr) << "a Player is no Npc";
}

TEST_F(SummonAiHandlersTest, TheSummonedNpcsAreNoLootNoDecayNoRespawn) {
	AI_TEST_SCOPE;
	Ref<Servant> mirror = servant(STRIPED_KERUB_NPC_ID);
	Ref<Trap> laid = trap(TRAP_NPC_ID);
	Ref<Npc> homing = makeWorldNpc(TAMED_PAGATI_NPC_ID, 505, 500, 100);
	AbstractAI& servantAi = install<roots::ServantNpcAI>(*mirror);
	AbstractAI& trapAi = install<roots::TrapNpcAI>(*laid);
	AbstractAI& homingAi = install<roots::HomingNpcAI>(*homing);
	for (AbstractAI* ai : {&servantAi, &trapAi, &homingAi}) {
		EXPECT_FALSE(ai->ask(AIQuestion::ALLOW_DECAY));
		EXPECT_FALSE(ai->ask(AIQuestion::ALLOW_RESPAWN));
		EXPECT_FALSE(ai->ask(AIQuestion::REWARD_AP_XP_DP_LOOT));
		EXPECT_TRUE(ai->ask(AIQuestion::REWARD_LOOT)) << "the other questions are NpcAI's";
	}
	auto& servantNpcAi = static_cast<roots::ServantNpcAI&>(servantAi);
	EXPECT_FALSE(servantNpcAi.canThink());
	EXPECT_FALSE(servantNpcAi.isMoveSupported());
	EXPECT_FALSE(static_cast<roots::TrapNpcAI&>(trapAi).isMoveSupported());
}

// ---- ServantNpcAI -----------------------------------------------------------------------------------------------------------------------------

TEST_F(SummonAiHandlersTest, AServantTargetsItsCreatorsTargetAndCastsAtItEveryFiveSeconds) {
	AI_TEST_SCOPE;
	Ref<Servant> mirror = servant(STRIPED_KERUB_NPC_ID);
	install<roots::ServantNpcAI>(*mirror).onGeneralEvent(AIEventType::SPAWNED);

	advance(199);
	EXPECT_FALSE(mirror->getTarget());
	advance(1);
	EXPECT_EQ(mirror->getTarget(), Ptr<model::gameobjects::VisibleObject>(*pagati)) << "200 ms after the spawn";
	EXPECT_EQ(mirror->getGameStats()->getLastSkill()->getSkillId(), BRANDISH) << "healOrAttack picks a random skill of the list";
	EXPECT_TRUE(mirror->getController().hasTask(model::TaskId::SKILL_USE));
	EXPECT_FALSE(mirror->isCasting());

	advance(999);
	EXPECT_FALSE(mirror->isCasting());
	advance(1); // startDelay 1000 ms
	EXPECT_TRUE(mirror->isCasting()) << "the first cast of Brandish";
	advance(4999);
	EXPECT_FALSE(mirror->isCasting()) << "the cast ended before the next tick";
	advance(1); // a SERVANT casts every 5000 ms
	EXPECT_TRUE(mirror->isCasting());
}

TEST_F(SummonAiHandlersTest, ATotemTargetsItselfAndCastsEveryThreeSeconds) {
	AI_TEST_SCOPE;
	Ref<Servant> totem = servant(TOTEM_NPC_ID, NpcObjectType::TOTEM);
	install<roots::ServantNpcAI>(*totem).onGeneralEvent(AIEventType::SPAWNED);

	advance(200);
	EXPECT_EQ(totem->getTarget(), Ptr<model::gameobjects::VisibleObject>(*totem));
	advance(1000);
	EXPECT_TRUE(totem->isCasting()) << "Protective Shield on itself";
	advance(2999);
	EXPECT_FALSE(totem->isCasting());
	advance(1);
	EXPECT_TRUE(totem->isCasting()) << "a TOTEM casts every 3000 ms";
}

TEST_F(SummonAiHandlersTest, TheBattleBannerStartsAfterATenthOfASecondAndCastsEveryThreeSeconds) {
	AI_TEST_SCOPE;
	Ref<Servant> banner = servant(BATTLE_BANNER_NPC_ID);
	install<roots::ServantNpcAI>(*banner).onGeneralEvent(AIEventType::SPAWNED);

	advance(200);
	advance(99);
	EXPECT_FALSE(banner->isCasting());
	advance(1);
	EXPECT_TRUE(banner->isCasting()) << "startDelay 100 ms";
	advance(2999);
	EXPECT_FALSE(banner->isCasting());
	advance(1);
	EXPECT_TRUE(banner->isCasting()) << "every 3000 ms, though a SERVANT";
}

TEST_F(SummonAiHandlersTest, AServantWhoseTargetDiesDeletesItself) {
	AI_TEST_SCOPE;
	Ref<Servant> mirror = servant(STRIPED_KERUB_NPC_ID);
	install<roots::ServantNpcAI>(*mirror).onGeneralEvent(AIEventType::SPAWNED);
	advance(200);
	ASSERT_TRUE(inWorld(*mirror));

	pagati->getLifeStats()->reduceHp(std::nullopt, pagati->getLifeStats()->getCurrentHp(), 0, std::nullopt, *creator);
	ASSERT_TRUE(pagati->isDead());
	advance(1000);

	EXPECT_FALSE(inWorld(*mirror)) << "AIActions.deleteOwner";
	EXPECT_FALSE(mirror->isCasting());
}

TEST_F(SummonAiHandlersTest, AServantWithoutATargetDeletesItselfAtItsFirstTick) {
	AI_TEST_SCOPE;
	creator->setTarget(nullptr);
	Ref<Servant> mirror = servant(STRIPED_KERUB_NPC_ID);
	install<roots::ServantNpcAI>(*mirror).onGeneralEvent(AIEventType::SPAWNED);

	advance(200);
	EXPECT_FALSE(mirror->getTarget()) << "the creator's null target, set as it is";
	EXPECT_TRUE(inWorld(*mirror));
	advance(1000);
	EXPECT_FALSE(inWorld(*mirror));
}

TEST_F(SummonAiHandlersTest, AServantWithoutSkillsOnlyTakesItsTarget) {
	AI_TEST_SCOPE;
	Ref<Servant> mirror = servant(TAMED_PAGATI_NPC_ID); // the pagati's template has no npc_skills row
	install<roots::ServantNpcAI>(*mirror).onGeneralEvent(AIEventType::SPAWNED);

	advance(200);
	EXPECT_EQ(mirror->getTarget(), Ptr<model::gameobjects::VisibleObject>(*pagati));
	EXPECT_FALSE(mirror->getController().hasTask(model::TaskId::SKILL_USE));
}

// ---- TrapNpcAI --------------------------------------------------------------------------------------------------------------------------------

TEST_F(SummonAiHandlersTest, ATrapHidesWhenItIsLaid) {
	AI_TEST_SCOPE;
	Ref<Trap> hidden = trap(TRAP_NPC_ID);
	install<roots::TrapNpcAI>(*hidden).onGeneralEvent(AIEventType::SPAWNED);
	EXPECT_TRUE(hidden->isInVisualState(CreatureVisualState::HIDE1));

	Ref<Trap> scrapped = trap(SCRAPPED_MECHANISMS_NPC_ID);
	install<roots::TrapNpcAI>(*scrapped).onGeneralEvent(AIEventType::SPAWNED);
	EXPECT_FALSE(scrapped->isInVisualState(CreatureVisualState::HIDE1)) << "scrapped mechanisms stay visible";
}

TEST_F(SummonAiHandlersTest, AnEnemyInRangeSetsTheTrapOffAndItVanishesFiveSecondsLater) {
	AI_TEST_SCOPE;
	Ref<Trap> laid = trap(TRAP_NPC_ID);
	roots::TrapNpcAI& ai = install<roots::TrapNpcAI>(*laid);
	ai.onGeneralEvent(AIEventType::SPAWNED);
	know(*laid, *pagati); // the pairing's see notification reaches the trap as CREATURE_SEE

	EXPECT_TRUE(ai.isInState(AIState::FIGHT));
	EXPECT_FALSE(laid->isInVisualState(CreatureVisualState::HIDE1)) << "it shows itself";
	EXPECT_EQ(laid->getTarget(), Ptr<model::gameobjects::VisibleObject>(*pagati));
	advance(4999);
	EXPECT_TRUE(inWorld(*laid));
	advance(1);
	EXPECT_FALSE(inWorld(*laid));
}

TEST_F(SummonAiHandlersTest, ATrapIgnoresFriendsTheDeadAndWhatIsOutOfRange) {
	AI_TEST_SCOPE;
	Ref<Trap> laid = trap(TRAP_NPC_ID);
	roots::TrapNpcAI& ai = install<roots::TrapNpcAI>(*laid);
	ai.onGeneralEvent(AIEventType::SPAWNED);

	Ref<Npc> friendKerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 501, 500, 100);
	know(*laid, *friendKerub);
	ai.onCreatureEvent(AIEventType::CREATURE_SEE, *friendKerub);
	EXPECT_FALSE(ai.isInState(AIState::FIGHT)) << "not an enemy of the creator";

	Ref<Npc> farPagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 510, 500, 100);
	know(*laid, *farPagati);
	ai.onCreatureEvent(AIEventType::CREATURE_MOVED, *farPagati);
	EXPECT_FALSE(ai.isInState(AIState::FIGHT)) << "out of the trap's attack range";

	pagati->getLifeStats()->reduceHp(std::nullopt, pagati->getLifeStats()->getCurrentHp(), 0, std::nullopt, *creator);
	ASSERT_TRUE(pagati->isDead());
	know(*laid, *pagati); // the pairing's see notification reaches the trap as CREATURE_SEE
	ai.onCreatureEvent(AIEventType::CREATURE_SEE, *pagati);
	EXPECT_FALSE(ai.isInState(AIState::FIGHT)) << "dead";
	EXPECT_TRUE(laid->isInVisualState(CreatureVisualState::HIDE1));
}

TEST_F(SummonAiHandlersTest, AnEnemyWalkingInSetsTheTrapOffOnce) {
	AI_TEST_SCOPE;
	Ref<Trap> laid = trap(TRAP_NPC_ID);
	roots::TrapNpcAI& ai = install<roots::TrapNpcAI>(*laid);
	ai.onGeneralEvent(AIEventType::SPAWNED);
	know(*laid, *pagati);

	ai.onCreatureEvent(AIEventType::CREATURE_MOVED, *pagati); // TrapNpcAI.canHandleEvent takes CREATURE_MOVED in every state
	ASSERT_TRUE(ai.isInState(AIState::FIGHT));
	advance(3000);
	ai.onCreatureEvent(AIEventType::CREATURE_MOVED, *pagati); // the despawn task is set: no second explosion
	advance(2000);
	EXPECT_FALSE(inWorld(*laid)) << "deleted at the first explosion's 5 s";
}

TEST_F(SummonAiHandlersTest, ATrapTakesAMoveInAnyState) {
	AI_TEST_SCOPE;
	Ref<Trap> laid = trap(TRAP_NPC_ID);
	roots::TrapNpcAI& ai = install<roots::TrapNpcAI>(*laid);
	ai.onGeneralEvent(AIEventType::SPAWNED);
	pagati->getPosition()->setXYZH(510.0f, std::nullopt, std::nullopt, std::nullopt); // out of range while the pairing sees it
	know(*laid, *pagati);
	ASSERT_FALSE(ai.isInState(AIState::FIGHT));
	ASSERT_TRUE(ai.setStateIfNot(AIState::RETURNING)); // AbstractAI.canHandleEvent takes CREATURE_MOVED only while IDLE or WALKING
	pagati->getPosition()->setXYZH(501.5f, std::nullopt, std::nullopt, std::nullopt);

	ai.onCreatureEvent(AIEventType::CREATURE_MOVED, *pagati);

	EXPECT_TRUE(ai.isInState(AIState::FIGHT)) << "TrapNpcAI.canHandleEvent takes it in every state";
}

TEST_F(SummonAiHandlersTest, AShockTrapGoesOffAsItIsLaid) {
	AI_TEST_SCOPE;
	Ref<Trap> shock = trap(SHOCK_TRAP_NPC_ID);
	roots::TrapNpcAI& ai = install<roots::TrapNpcAI>(*shock);
	ai.onGeneralEvent(AIEventType::SPAWNED);

	EXPECT_TRUE(ai.isInState(AIState::FIGHT));
	EXPECT_EQ(shock->getTarget(), Ptr<model::gameobjects::VisibleObject>(*shock)) << "it explodes on itself";
	advance(5000);
	EXPECT_FALSE(inWorld(*shock));
}

// ---- HomingNpcAI ------------------------------------------------------------------------------------------------------------------------------

TEST_F(SummonAiHandlersTest, AHomingWithoutATargetSwingsAndOneWithAReadySkillCastsIt) {
	AI_TEST_SCOPE;
	Ref<Npc> homing = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
	roots::HomingNpcAI& ai = install<roots::HomingNpcAI>(*homing);
	Rnd::seedCurrentThreadForTests(seedWhere(chanceDraws(2100, 25), true));
	EXPECT_EQ(ai.chooseAttackIntention(), AttackIntention::SIMPLE_ATTACK) << "no target, though Brandish's draw would pass";
	EXPECT_FALSE(homing->getGameStats()->getLastSkill()) << "the skill was not even chosen";

	know(*homing, *pagati);
	homing->setTarget(Ptr<model::gameobjects::VisibleObject>(*pagati));
	Rnd::seedCurrentThreadForTests(seedWhere(chanceDraws(2100, 25), true));
	EXPECT_EQ(ai.chooseAttackIntention(), AttackIntention::SKILL_ATTACK) << "Brandish's 25 % draw passed";
	Rnd::seedCurrentThreadForTests(seedWhere(chanceDraws(2100, 25), false));
	homing->getGameStats()->setLastSkill(nullptr);
	EXPECT_EQ(ai.chooseAttackIntention(), AttackIntention::SIMPLE_ATTACK) << "Brandish's 25 % draw failed";
}

TEST_F(SummonAiHandlersTest, HomingsAndServantsDoNotThink) {
	AI_TEST_SCOPE;
	// GeneralNpcAI.think schedules the heading reset of an idle npc (RootAiHandlersTest); these two leave think empty
	Ref<Npc> homing = makeWorldNpc(STRIPED_KERUB_NPC_ID, 510, 500, 100);
	roots::HomingNpcAI& homingAi = install<roots::HomingNpcAI>(*homing);
	Ref<Servant> mirror = servant(STRIPED_KERUB_NPC_ID);
	roots::ServantNpcAI& servantAi = install<roots::ServantNpcAI>(*mirror);
	homingAi.setStateIfNot(AIState::IDLE);
	servantAi.setStateIfNot(AIState::IDLE);
	homing->getPosition()->setH(int8_t{42}); // off the spawn heading, which GeneralNpcAI.think would schedule back
	mirror->getPosition()->setH(int8_t{42});
	executor->runReady();
	const size_t pending = executor->pendingTasksCount();

	homingAi.think();
	servantAi.think();

	EXPECT_EQ(executor->pendingTasksCount(), pending);
}

} // namespace
} // namespace aion::gameserver::ai::testing
