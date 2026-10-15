// Leak investigation, hypothesis A (2026-09-24 client session, LeakCensus: Npc 25582 alive 10 min after its removal, refcount 1, 0 pinning
// tasks): an npc that dies - or is despawned - while it casts, or while a skill is in flight, must not stay referenced once its corpse decayed
// and World.removeObject took it out of the world. Java collects such an npc with the garbage collector; here every Ref to it must be gone.
//
// Each case drives the real striped kerub (210133, the shipped npc_skills row: 16419 Brandish, a 2,500 ms cast) with its real
// AggressiveNpcAI through the path the server takes - SkillAttackManager.performAttack -> skillAction -> Skill.useSkill -> the endCast task -
// kills it mid-cast through CreatureController.onAttack (a plain hit) or through a skill effect (SkillEngine.applyEffectDirectly of 1282 Flame
// Bolt's effect, the Mage's bolt), lets NpcController.onDie schedule the decay (RespawnService.scheduleDecayTask: 2 s without a drop), lets the
// DecayTask find the corpse in World.allObjects and delete it (World.removeObject: despawn + onDelete), runs every task that is still due and
// then asks the one thing the leak census asks: is the kerub's reference count down to the references this test holds itself?

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/manager/SkillAttackManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "../ai/NpcSkillTestSupport.h"

namespace aion::gameserver::ai::testing {
namespace {

namespace Rnd = commons::utils::Rnd;
namespace roots = gameserver::handlers::ai;

inline constexpr int32_t MAGE_FLAME_BOLT = 1282;
inline constexpr int32_t MAGE_ROOT = 1328;

/** skills/skill_templates.xml, verbatim: the session's Mage skills 1282 Flame Bolt (:18564-18582) and 1328 Root (:19302-19311) */
inline const char* const MAGE_SKILL_TEMPLATES_XML =
	R"(<skill_template skill_id="1282" name="Flame Bolt" nameId="2287999" cooldownId="271" group="MA_FLAMEBOLT" stack="MA_FLAMEBOLT" lvl="1")"
	R"( skilltype="MAGICAL" skill_category="CHAIN_SKILL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="2000")"
	R"( ammospeed="30" cancel_rate="20" chain_skill_prob="100" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true" apply_casting_time_bonus="true">)"
	R"(<properties first_target="TARGET" first_target_range="25" target_relation="ENEMY" target_type="ONLYONE" revision_distance="12" />)"
	R"(<startconditions><chain category="M_CHAINA_1TH_1" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<endconditions><mp value="19" delta="0" /><chargeweapon value="53" /><chargearmor value="36" /><polishchargeweapon value="422" />)"
	R"(</endconditions>)"
	R"(<effects><spellatkinstant value="141" e="1" element="FIRE" hoptype="DAMAGE" /></effects>)"
	R"(<motion name="pointfire" /></skill_template>)"
	R"(<skill_template skill_id="1328" name="Root" nameId="2287465" cooldownId="277" group="MA_ROOT" stack="MA_ROOT" lvl="1" skilltype="MAGICAL")"
	R"( skill_category="PHYSICAL_DEBUFF" skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="ALL" req_dispel_level="1" req_dispel_count="10")"
	R"( activation="ACTIVE" cooldown="600" duration="0" cancel_rate="20" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true" apply_casting_time_bonus="true">)"
	R"(<properties first_target="TARGET" first_target_range="25" target_relation="ENEMY" target_type="ONLYONE" />)"
	R"(<endconditions><mp value="38" delta="0" /></endconditions>)"
	R"(<effects><root resistchance="10" duration2="20000" effectid="20003" e="1" accmod2="500" element="WATER" hoptype="SKILLLV" hopb="1239" />)"
	R"(</effects><motion name="debuff" speed="50" instant_skill="true" /></skill_template>)";

using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;
using runtime::Ptr;
using runtime::Ref;

class NpcCastDeathLifetimeTest : public NpcSkillWorldTest {
protected:
	void SetUp() override {
		NpcSkillWorldTest::SetUp();
		AI_TEST_SCOPE;
		// the fixture's skill templates plus the Mage's two (the npc skill lists were built from the same rows, so nothing else changes)
		std::string skills = NPC_SKILL_TEMPLATES_XML;
		skills.insert(skills.rfind("</skill_data>"), MAGE_SKILL_TEMPLATES_XML);
		dataholders::DataManager::SKILL_DATA.resetForTests();
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(contexts.emplace_back(), skills));
		regionActivator = activateRegionAt(500, 500, 100);
		executor->runReady(); // MapRegion::activate posts the ACTIVATE notification of its creatures
	}

	void TearDown() override {
		regionActivator = nullptr;
		NpcSkillWorldTest::TearDown();
	}

	template <class AI>
	AI& installRootAi(Npc& npc) {
		auto handlerAi = std::make_unique<AI>(npc);
		AI& result = *handlerAi;
		npc.replaceAi(std::move(handlerAi));
		return result;
	}

	/** The kerub in FIGHT against `target` (hating and targeting it), stored in World.allObjects like VisibleObjectSpawner stores a spawn */
	roots::AggressiveNpcAI& fightingKerub(Npc& kerub, Creature& target) {
		know(kerub, target);
		roots::AggressiveNpcAI& ai = installRootAi<roots::AggressiveNpcAI>(kerub);
		ai.setStateIfNot(AIState::FIGHT);
		kerub.getAggroList().addHate(target, 10);
		kerub.setTarget(Ptr<VisibleObject>(target));
		world::World::getInstance().storeObject(kerub);
		return ai;
	}

	/** GeneralNpcAI.chooseSkillAttack stored Brandish, AttackManager.chooseAttack asked for the skill attack: the kerub starts its 2.5 s cast */
	void startBrandish(roots::AggressiveNpcAI& ai, Npc& kerub, int32_t delay = 0) {
		kerub.getGameStats()->setLastSkill(entry(kerub, 0));
		manager::SkillAttackManager::performAttack(ai, delay);
	}

	/** A plain hit that kills: CreatureController.onAttack -> AggroList.addDamage -> reduceHp -> NpcController.onDie */
	static void killWithAHit(Npc& kerub, Creature& attacker) {
		kerub.getController().onAttack(attacker, kerub.getLifeStats()->getMaxHp() * 10, controllers::attack::AttackStatus::NORMALHIT);
	}

	/** Drops the fixture's own reference, so the test's local Ref is the only one that should remain */
	void forgetNpc(Npc& npc) {
		std::erase_if(npcs, [&npc](const Ref<Npc>& held) { return held.get() == &npc; });
	}

	/** Every structure of the test world that could still reference the kerub, for the failure message */
	std::string holders(Npc& kerub) {
		std::ostringstream out;
		out << "refCount " << kerub.refCount();
		if (kerub.isCasting())
			out << "; kerub.castingSkill is set (skill " << kerub.getCastingSkillId() << ", Skill.effector -> kerub)";
		if (kerub.getObserveController()->hasObservers())
			out << "; the kerub's own ObserveController still has observers";
		if (!kerub.getEffectController()->getAllEffects().empty())
			out << "; the kerub's EffectController still has " << kerub.getEffectController()->getAllEffects().size() << " effect(s)";
		if (kerub.getTarget())
			out << "; kerub.target is set";
		if (world::World::getInstance().findVisibleObject(kerub.getObjectId()))
			out << "; World.allObjects still holds it";
		if (kerub.getPosition() && kerub.getPosition()->getMapRegion() &&
			kerub.getPosition()->getMapRegion()->getObjects().get(kerub.getObjectId()))
			out << "; its MapRegion.objects still holds it";
		std::vector<Ptr<Creature>> others;
		for (const Ref<Npc>& npc : npcs)
			if (npc.get() != &kerub)
				others.emplace_back(Ptr<Creature>(*npc));
		for (const Ref<Player>& player : players)
			others.emplace_back(Ptr<Creature>(*player));
		for (const Ptr<Creature>& other : others) {
			std::string who = other->getName() + "#" + std::to_string(other->getObjectId());
			if (other->getKnownList().getObject(kerub.getObjectId()))
				out << "; " << who << ".knownList knows it";
			for (const Ptr<controllers::attack::AggroInfo>& info : other->getAggroList().stream())
				if (info->getAttacker().get() == &kerub)
					out << "; " << who << ".aggroList has an AggroInfo for it (hate " << info->getHate() << ")";
			if (other->getTarget().get() == &kerub)
				out << "; " << who << ".target is it";
			if (other->getObserveController()->hasObservers())
				out << "; " << who << ".observeController has observers (e.g. a Skill.firstTargetDieObserver pinning a Skill of the kerub)";
			if (Ptr<controllers::effect::EffectController> effectController = other->getEffectController())
				for (const Ptr<skillengine::model::Effect>& effect : effectController->getAllEffects())
					if (effect->getEffector().get() == &kerub)
						out << "; " << who << ".effectController holds an effect of the kerub";
		}
		return out.str();
	}

	/**
	 * Runs everything that is due in the next `millis` ms, closes the test's task scope so the Reclaimer can retire what the tasks released,
	 * and checks that the only reference left is the test's own `kerub`.
	 */
	void expectReleased(Ref<Npc>& kerub, const char* what) {
		forgetNpc(*kerub);
		runtime::Reclaimer::getInstance().drain();
		AI_TEST_SCOPE;
		std::cout << "[leak-a] " << what << ": " << holders(*kerub) << std::endl;
		EXPECT_EQ(kerub->refCount(), 1u) << what << ": the kerub is still referenced after its removal from the world - " << holders(*kerub);
	}

	runtime::Ref<Player> regionActivator;
};

/** The reference: a kerub that casts Brandish to its end and is then killed and decays is released (no cast in flight at death). */
TEST_F(NpcCastDeathLifetimeTest, AKerubKilledAfterItsCastEndedIsReleasedWhenItsCorpseDecays) {
	Ref<Npc> kerub;
	{
		AI_TEST_SCOPE;
		kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
		Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501.5f, 500, 100);
		roots::AggressiveNpcAI& ai = fightingKerub(*kerub, *pagati);
		Rnd::seedCurrentThreadForTests(20260924);
		startBrandish(ai, *kerub);
		ASSERT_TRUE(kerub->isCasting()) << "precondition: the kerub casts Brandish at the pagati";
		executor->advance(std::chrono::milliseconds(2600));
		ASSERT_FALSE(kerub->isCasting()) << "Skill.endCast ran";
		killWithAHit(*kerub, *pagati);
		ASSERT_TRUE(kerub->isDead());
		executor->advance(std::chrono::milliseconds(2100)); // RespawnService.IMMEDIATE_DECAY: the DecayTask deletes the corpse
		ASSERT_FALSE(world::World::getInstance().findVisibleObject(kerub->getObjectId())) << "World.removeObject took the corpse out";
		ASSERT_FALSE(kerub->isSpawned());
		executor->advance(std::chrono::milliseconds(10000));
	}
	expectReleased(kerub, "killed after the cast");
}

/** Hypothesis A, a plain hit: the kerub dies 1 s into Brandish; the endCast task is still pending at death and at the decay. */
TEST_F(NpcCastDeathLifetimeTest, AKerubKilledByAHitMidCastIsReleasedWhenItsCorpseDecays) {
	Ref<Npc> kerub;
	{
		AI_TEST_SCOPE;
		kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
		Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501.5f, 500, 100);
		roots::AggressiveNpcAI& ai = fightingKerub(*kerub, *pagati);
		Rnd::seedCurrentThreadForTests(20260924);
		startBrandish(ai, *kerub);
		ASSERT_TRUE(kerub->isCasting());
		ASSERT_TRUE(pagati->getObserveController()->hasObservers()) << "Brandish's first target carries the cast's DeathObserver (Skill.startCast)";
		executor->advance(std::chrono::milliseconds(1000));
		killWithAHit(*kerub, *pagati);
		ASSERT_TRUE(kerub->isDead());
		EXPECT_FALSE(kerub->isCasting()) << "CreatureController.onDie: setCasting(null)";
		executor->advance(std::chrono::milliseconds(2100));
		ASSERT_FALSE(world::World::getInstance().findVisibleObject(kerub->getObjectId()));
		executor->advance(std::chrono::milliseconds(10000));
		EXPECT_FALSE(pagati->getObserveController()->hasObservers()) << "the endCast task removed the DeathObserver from the first target";
	}
	expectReleased(kerub, "killed by a hit mid-cast");
}

/**
 * Hypothesis A, a skill: the kerub dies 1 s into Brandish to the damage of 1282 Flame Bolt's effect (the Mage's bolt; applied the way the
 * bolt's hit-time task applies it, through Effect.applyEffect -> SpellAttackInstantEffect -> CreatureController.onAttack(effect)).
 */
TEST_F(NpcCastDeathLifetimeTest, AKerubKilledByASkillEffectMidCastIsReleasedWhenItsCorpseDecays) {
	Ref<Npc> kerub;
	{
		AI_TEST_SCOPE;
		kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
		Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501.5f, 500, 100);
		roots::AggressiveNpcAI& ai = fightingKerub(*kerub, *pagati);
		Rnd::seedCurrentThreadForTests(20260924);
		startBrandish(ai, *kerub);
		ASSERT_TRUE(kerub->isCasting());
		executor->advance(std::chrono::milliseconds(1000));
		kerub->getLifeStats()->setCurrentHp(1); // any hit of the bolt kills
		// 1282 Flame Bolt's <spellatkinstant>, applied by the pagati
		skillengine::SkillEngine::getInstance().applyEffectDirectly(MAGE_FLAME_BOLT, *pagati, *kerub);
		ASSERT_TRUE(kerub->isDead()) << "the bolt's damage killed the kerub";
		EXPECT_FALSE(kerub->isCasting());
		executor->advance(std::chrono::milliseconds(2100));
		ASSERT_FALSE(world::World::getInstance().findVisibleObject(kerub->getObjectId()));
		executor->advance(std::chrono::milliseconds(10000));
	}
	expectReleased(kerub, "killed by a skill effect mid-cast");
}

/** Hypothesis A: the kerub dies while its delayed skillAction (performAttack with a skill delay) is still pending. */
TEST_F(NpcCastDeathLifetimeTest, AKerubKilledBeforeItsDelayedSkillActionRanIsReleased) {
	Ref<Npc> kerub;
	{
		AI_TEST_SCOPE;
		kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
		Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501.5f, 500, 100);
		roots::AggressiveNpcAI& ai = fightingKerub(*kerub, *pagati);
		Rnd::seedCurrentThreadForTests(20260924);
		startBrandish(ai, *kerub, 750);
		ASSERT_TRUE(ai.isInSubState(AISubState::CAST));
		ASSERT_FALSE(kerub->isCasting()) << "the skillAction task is pending";
		executor->advance(std::chrono::milliseconds(300));
		killWithAHit(*kerub, *pagati);
		ASSERT_TRUE(kerub->isDead());
		executor->advance(std::chrono::milliseconds(600)); // the skillAction task runs on the corpse
		EXPECT_FALSE(kerub->isCasting()) << "the dead kerub started no cast";
		executor->advance(std::chrono::milliseconds(1600));
		ASSERT_FALSE(world::World::getInstance().findVisibleObject(kerub->getObjectId()));
		executor->advance(std::chrono::milliseconds(10000));
	}
	expectReleased(kerub, "killed before the delayed skillAction");
}

/** Hypothesis A: the kerub is despawned (World.removeObject, as AIActions.deleteOwner does) while it casts - no death at all. */
TEST_F(NpcCastDeathLifetimeTest, AKerubDeletedMidCastIsReleased) {
	Ref<Npc> kerub;
	{
		AI_TEST_SCOPE;
		kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
		Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501.5f, 500, 100);
		roots::AggressiveNpcAI& ai = fightingKerub(*kerub, *pagati);
		Rnd::seedCurrentThreadForTests(20260924);
		startBrandish(ai, *kerub);
		ASSERT_TRUE(kerub->isCasting());
		executor->advance(std::chrono::milliseconds(1000));
		ASSERT_TRUE(kerub->getController().delete_());
		EXPECT_FALSE(kerub->isCasting()) << "NpcController.onDespawn: cancelCurrentSkill(null)";
		executor->advance(std::chrono::milliseconds(10000));
	}
	expectReleased(kerub, "deleted mid-cast");
}

/** Hypothesis A with a Player as Brandish's target and as the killer (the session's Mage). */
TEST_F(NpcCastDeathLifetimeTest, AKerubCastingAtAPlayerAndKilledByItMidCastIsReleased) {
	Ref<Npc> kerub;
	{
		AI_TEST_SCOPE;
		kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
		Ref<Player> mage = makeWorldPlayer(700101, 501.5f, 500, 100);
		mage->setEffectController(std::make_unique<controllers::effect::PlayerEffectController>(*mage)); // PlayerService.getPlayer does
		ASSERT_TRUE(kerub->isEnemy(*mage)) << "precondition: Brandish's ENEMY target relation holds for a player";
		roots::AggressiveNpcAI& ai = fightingKerub(*kerub, *mage);
		Rnd::seedCurrentThreadForTests(20260924);
		startBrandish(ai, *kerub);
		ASSERT_TRUE(kerub->isCasting()) << "precondition: the kerub casts Brandish at the player";
		executor->advance(std::chrono::milliseconds(1000));
		killWithAHit(*kerub, *mage);
		ASSERT_TRUE(kerub->isDead());
		executor->advance(std::chrono::milliseconds(2100));
		ASSERT_FALSE(world::World::getInstance().findVisibleObject(kerub->getObjectId()));
		executor->advance(std::chrono::milliseconds(10000));
	}
	expectReleased(kerub, "cast at a player and killed by it");
}

/** Hypothesis A with the Player as the killer through 1282 Flame Bolt's effect, and 1328 Root on the casting kerub first (its observer). */
TEST_F(NpcCastDeathLifetimeTest, ARootedKerubCastingAtAPlayerAndKilledByItsFlameBoltMidCastIsReleased) {
	Ref<Npc> kerub;
	{
		AI_TEST_SCOPE;
		kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
		Ref<Player> mage = makeWorldPlayer(700102, 501.5f, 500, 100);
		mage->setEffectController(std::make_unique<controllers::effect::PlayerEffectController>(*mage));
		roots::AggressiveNpcAI& ai = fightingKerub(*kerub, *mage);
		Rnd::seedCurrentThreadForTests(20260924);
		startBrandish(ai, *kerub);
		ASSERT_TRUE(kerub->isCasting());
		executor->advance(std::chrono::milliseconds(500));
		skillengine::SkillEngine::getInstance().applyEffectDirectly(MAGE_ROOT, *mage, *kerub);
		std::cout << "[leak-a] root applied: kerub effects " << kerub->getEffectController()->getAllEffects().size() << ", kerub observers "
				  << kerub->getObserveController()->hasObservers() << std::endl;
		executor->advance(std::chrono::milliseconds(500));
		kerub->getLifeStats()->setCurrentHp(1);
		skillengine::SkillEngine::getInstance().applyEffectDirectly(MAGE_FLAME_BOLT, *mage, *kerub);
		ASSERT_TRUE(kerub->isDead()) << "the bolt's damage killed the kerub";
		executor->advance(std::chrono::milliseconds(2100));
		ASSERT_FALSE(world::World::getInstance().findVisibleObject(kerub->getObjectId()));
		executor->advance(std::chrono::milliseconds(25000)); // past Root's 20 s
	}
	expectReleased(kerub, "rooted, cast at a player and killed by its Flame Bolt");
}

/** Hypothesis A: Brandish's first target dies mid-cast (the cast's DeathObserver fires and cancels it), then the kerub dies and decays. */
TEST_F(NpcCastDeathLifetimeTest, AKerubWhoseTargetDiedMidCastIsReleasedWhenItsCorpseDecays) {
	Ref<Npc> kerub;
	{
		AI_TEST_SCOPE;
		kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
		Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501.5f, 500, 100);
		Ref<Npc> other = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501, 501, 100);
		know(*kerub, *other);
		roots::AggressiveNpcAI& ai = fightingKerub(*kerub, *pagati);
		Rnd::seedCurrentThreadForTests(20260924);
		startBrandish(ai, *kerub);
		ASSERT_TRUE(kerub->isCasting());
		executor->advance(std::chrono::milliseconds(1000));
		pagati->getController().die(*other);
		ASSERT_TRUE(pagati->isDead());
		EXPECT_FALSE(kerub->isCasting()) << "the DeathObserver of Skill.startCast cancelled the cast (STR_SKILL_TARGET_LOST)";
		executor->advance(std::chrono::milliseconds(500));
		killWithAHit(*kerub, *other);
		ASSERT_TRUE(kerub->isDead());
		executor->advance(std::chrono::milliseconds(2100));
		ASSERT_FALSE(world::World::getInstance().findVisibleObject(kerub->getObjectId()));
		executor->advance(std::chrono::milliseconds(10000));
	}
	expectReleased(kerub, "target died mid-cast, then the kerub");
}

/**
 * Hypothesis A, the narrowest window: the kerub dies 100 ms into Brandish, so the corpse is removed (decay at +2,000 ms) BEFORE the cast's
 * endCast task runs (+2,500 ms) - the endCast of an npc that is no longer in the world - and a second bolt of the Mage reaches the corpse
 * after the kill (its hit-time task, Effect.applyEffect on a dead effected: NpcController.onAttack returns, broadcastHate adds hate).
 */
TEST_F(NpcCastDeathLifetimeTest, AKerubRemovedBeforeItsEndCastTaskRanIsReleased) {
	Ref<Npc> kerub;
	{
		AI_TEST_SCOPE;
		kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
		Ref<Player> mage = makeWorldPlayer(700103, 501.5f, 500, 100);
		mage->setEffectController(std::make_unique<controllers::effect::PlayerEffectController>(*mage));
		roots::AggressiveNpcAI& ai = fightingKerub(*kerub, *mage);
		Rnd::seedCurrentThreadForTests(20260924);
		startBrandish(ai, *kerub);
		ASSERT_TRUE(kerub->isCasting());
		executor->advance(std::chrono::milliseconds(100));
		kerub->getLifeStats()->setCurrentHp(1);
		skillengine::SkillEngine::getInstance().applyEffectDirectly(MAGE_FLAME_BOLT, *mage, *kerub);
		ASSERT_TRUE(kerub->isDead());
		executor->advance(std::chrono::milliseconds(300));
		skillengine::SkillEngine::getInstance().applyEffectDirectly(MAGE_FLAME_BOLT, *mage, *kerub); // the second bolt reaches the corpse
		executor->advance(std::chrono::milliseconds(1800));
		ASSERT_FALSE(world::World::getInstance().findVisibleObject(kerub->getObjectId())) << "removed at +2,100 ms";
		executor->advance(std::chrono::milliseconds(10000)); // Brandish's endCast task runs on the removed npc at +2,500 ms
	}
	expectReleased(kerub, "removed before its endCast task ran");
}

/**
 * Control: the check sees a reference the production code would not drop. A reference to the removed kerub stored in a live npc (here the
 * pagati's target, set after the removal) must turn the count to 2 and be named by holders().
 */
TEST_F(NpcCastDeathLifetimeTest, ControlTheCheckSeesAReferenceLeftInALiveNpc) {
	Ref<Npc> kerub;
	Ref<Npc> pagati;
	{
		AI_TEST_SCOPE;
		kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
		pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501.5f, 500, 100);
		roots::AggressiveNpcAI& ai = fightingKerub(*kerub, *pagati);
		Rnd::seedCurrentThreadForTests(20260924);
		startBrandish(ai, *kerub);
		executor->advance(std::chrono::milliseconds(1000));
		killWithAHit(*kerub, *pagati);
		executor->advance(std::chrono::milliseconds(12100));
		ASSERT_FALSE(world::World::getInstance().findVisibleObject(kerub->getObjectId()));
		pagati->setTarget(Ptr<VisibleObject>(*kerub)); // the planted holder
	}
	forgetNpc(*kerub);
	runtime::Reclaimer::getInstance().drain();
	AI_TEST_SCOPE;
	std::string found = holders(*kerub);
	std::cout << "[leak-a] control: " << found << std::endl;
	EXPECT_EQ(kerub->refCount(), 2u);
	EXPECT_NE(found.find(".target is it"), std::string::npos) << found;
	pagati->setTarget(nullptr);
}

} // namespace
} // namespace aion::gameserver::ai::testing
