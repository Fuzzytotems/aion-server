// Leak investigation (hypothesis B, 2026-09-24 session: LeakCensus kept Npc 25582 alive with refcount 1 and no pinning task): does an Effect
// ON a monster (the Mage's 1328 Root with its ATTACKED observer, a periodic damage effect, the critical proc's stumble) or an Effect BY a
// monster on a player (a debuff whose effector is the npc, an instant skill attack like 16419 Brandish and the hate it broadcasts) keep the
// monster alive once it died and left the world?
//
// Every case kills the monster through the real effect path (1282 Flame Bolt's SpellAttackInstantEffect -> CreatureController.onAttack ->
// CreatureLifeStats.reduceHp -> NpcController.onDie). The monster of EffectsMzTest has no ai attribute, so it gets AIEngine's DummyAI, whose
// ask() answers false (AIEngine.cpp:59): no respawn, no reward, no decay task - NpcController.onDie deletes the corpse at once
// (NpcController.java: `else delete()`), i.e. World.removeObject -> World.despawn (NpcController.onDespawn, the known lists) ->
// CreatureController.onDelete (LogoutBreakers::onDelete). That is the same effect cleanup the 2 s decay task runs in the server
// (CreatureController.onDie's removeAllEffects, NpcController.onDespawn's removeAllEffects and ObserveController.clear, the delete breakers).
//
// After the deletion the case drops its own Effect references, drains the Reclaimer and asserts that the only reference left to the monster
// is the case's own local Ref (refCount() == 1), and that no Effect instance is left (LIVE_COUNTS_ENABLED builds).

#include "EffectsMzTestSupport.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <typeinfo>

#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/model/gameobjects/player/LogoutBreakers.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/lifetime/LiveInstanceCounters.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::skillengine::effect::mztest {
namespace {

/**
 * AIEngine's DummyNpcAI (AIEngine.cpp:52-80: every NpcAI hook back to AITemplate's empty body), except that ask(ALLOW_DECAY) answers true: the
 * corpse stays spawned and is deleted by RespawnService's DecayTask after IMMEDIATE_DECAY (2,000 ms, no drop registered), as the server's
 * aggressive kerub's corpse without loot is. No respawn, no reward, no loot.
 */
class DecayingNpcAI final : public ai::NpcAI {
public:
	explicit DecayingNpcAI(gameserver::model::gameobjects::Npc& owner) : NpcAI(owner) {}

	bool ask(ai::poll::AIQuestion question) override { return question == ai::poll::AIQuestion::ALLOW_DECAY; }

	bool isDestinationReached() override { return false; }

protected:
	void handleActivate() override {}
	void handleDeactivate() override {}
	void handleBeforeSpawned() override {}
	void handleSpawned() override {}
	void handleDespawned() override {}
	void handleDied() override {}
	void handleMoveArrived() override {}
	void handleTargetChanged(gameserver::model::gameobjects::Creature&) override {}
	void handleMoveValidate() override {}
	void handleCreatureMoved(gameserver::model::gameobjects::Creature&) override {}
};

class NpcEffectLifetimeTest : public EffectsMzTest {
protected:
	void SetUp() override {
		EffectsMzTest::SetUp();
		// the live counts are per process: a manual run of several cases in one process starts from what earlier cases left
		drain();
		effectsBefore_ = liveEffects();
		skillsBefore_ = runtime::liveCountOf(typeid(model::Skill));
	}

	void TearDown() override {
		// a failed case can leave its monster in the World singleton; it must not reach the next case of this process
		{
			EFFECT_TEST_SCOPE;
			for (const Ref<Npc>& stored : stored_)
				if (world::World::getInstance().findVisibleObject(stored->getObjectId()).get() == stored.get())
					world::World::getInstance().removeObject(*stored);
		}
		stored_.clear();
		EffectsMzTest::TearDown();
	}

	/**
	 * A monster the World singleton holds (World.storeObject, as SpawnEngine does), so NpcController.onDie's delete() removes it from the world.
	 * The fixture's own reference is dropped: the case's local Ref is the only one the test keeps.
	 */
	Ref<Npc> worldMonster(float x = 505, float y = 500) {
		Ref<Npc> npc = monster(x, y, 100);
		npcs.pop_back();
		skillengine::test::addStat(*npc, StatEnum::MAGICAL_CRITICAL_RESIST, 1000); // no magical critical, so no critical proc on the kill
		world::World::getInstance().storeObject(*npc);
		stored_.push_back(npc);
		return npc;
	}

	/**
	 * Flame Bolt (152 damage against this monster, DamageEffectsTest) on a monster left with 100 HP: it dies and, DummyAI, is deleted at once;
	 * with `decays` (DecayingNpcAI) the corpse stays spawned until the DecayTask 2,000 ms later
	 */
	void killWithFlameBolt(Npc& npc, Player& mage, bool decays = false) {
		npc.getLifeStats()->setCurrentHp(100);
		Ref<Effect> bolt = calculated(1282, mage, npc);
		ASSERT_TRUE(bolt->isInSuccessEffects(1)) << "Flame Bolt landed";
		bolt->applyEffect();
		ASSERT_TRUE(npc.isDead()) << "the Flame Bolt killed the monster";
		if (decays) {
			ASSERT_TRUE(world::World::getInstance().findVisibleObject(npc.getObjectId())) << "the corpse waits for its decay";
			ASSERT_TRUE(npc.isSpawned());
			return;
		}
		ASSERT_FALSE(world::World::getInstance().findVisibleObject(npc.getObjectId())) << "NpcController.onDie deleted the corpse (DummyAI: no decay)";
		std::erase_if(stored_, [&npc](const Ref<Npc>& stored) { return stored.get() == &npc; });
	}

	/** Advances past RespawnService.IMMEDIATE_DECAY: the DecayTask deletes the corpse (World.removeObject) */
	void decay(Npc& npc) {
		advance(2000);
		ASSERT_FALSE(world::World::getInstance().findVisibleObject(npc.getObjectId())) << "the DecayTask deleted the corpse";
		ASSERT_FALSE(npc.isSpawned());
		std::erase_if(stored_, [&npc](const Ref<Npc>& stored) { return stored.get() == &npc; });
	}

	/** Frees what the retired objects hold (Effects, observers, packets): only the Reclaimer runs their destructors */
	static void drain() { runtime::Reclaimer::getInstance().drain(); }

	static int64_t liveEffects() { return runtime::liveCountOf(typeid(Effect)); }

	/** What the parts this case can see still hold of the monster (printed when a count is off) */
	static std::string holdersOf(Npc& npc, Player* player) {
		EFFECT_TEST_SCOPE;
		std::string text = "refCount " + std::to_string(npc.refCount());
		text += ", npc effects " + std::to_string(npc.getEffectController()->getAllEffects().size());
		text += ", npc observers " + std::string(npc.getObserveController()->hasObservers() ? "yes" : "no");
		if (player != nullptr) {
			text += ", player effects " + std::to_string(player->getEffectController()->getAllEffects().size());
			text += ", player hates the npc " + std::string(player->getAggroList().getHate(npc) != 0 || player->getAggroList().isHating(npc) ? "yes" : "no");
			int32_t entries = 0;
			for (const Ptr<controllers::attack::AggroInfo>& info : player->getAggroList().stream())
				if (info->getAttacker().get() == &npc)
					entries++;
			text += ", player aggro entries of the npc " + std::to_string(entries);
		}
		return text;
	}

	std::vector<Ref<Npc>> stored_;
	int64_t effectsBefore_ = 0;
	int64_t skillsBefore_ = 0;
};

// ---- effects ON the monster --------------------------------------------------------------------------------------------------------------

/**
 * The session's Mage: 1328 Root on a monster (the Effect in the monster's abnormal map, RootEffect's ATTACKED observer in its ObserveController,
 * the removal task in the effect's observerRemoveTasks, the end task pinning the effect), then Flame Bolt kills it while rooted.
 * CreatureController.onDie's removeAllEffects ends the root (endEffect -> stopTasks, removeObservers, clearEffect), the corpse is deleted, and
 * nothing of the root keeps the monster.
 */
TEST_F(NpcEffectLifetimeTest, ARootedMonsterKilledByFlameBoltIsReleasedWhenItsCorpseIsDeleted) {
	Ref<Npc> npc;
	Ref<Player> mage;
	{
		EFFECT_TEST_SCOPE;
		mage = player(9101);
		npc = worldMonster();
		pair(*npc, *mage);
		Ref<Effect> root = applied(1328, *mage, *npc);
		ASSERT_TRUE(npc->getEffectController()->hasAbnormalEffect(1328)) << "rooted";
		ASSERT_TRUE(npc->getObserveController()->hasObservers()) << "RootEffect's ATTACKED observer";

		ASSERT_NO_FATAL_FAILURE(killWithFlameBolt(*npc, *mage));
		EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(1328)) << "removeAllEffects on death ended the root";
		EXPECT_FALSE(npc->getObserveController()->hasObservers());
		EXPECT_TRUE(mage->getEffectController()->getAllEffects().empty());
	}
	drain();
	EXPECT_EQ(npc->refCount(), 1u) << "only the case's own Ref may be left: " << holdersOf(*npc, mage.get());
	if (runtime::LIVE_COUNTS_ENABLED)
		EXPECT_EQ(liveEffects(), effectsBefore_) << "an Effect outlived its creatures";
}

/**
 * The server's order (DecayingNpcAI): the rooted monster dies, its corpse stays spawned for IMMEDIATE_DECAY while a second Flame Bolt of the Mage
 * lands on it (Effect.applyEffect -> shouldApplyFurtherEffects: dead, nothing applied) and while the corpse's own root still holds the Mage, and
 * the DecayTask deletes it 2,000 ms later. The root on the Mage ends by time afterwards; then nothing may hold the monster.
 */
TEST_F(NpcEffectLifetimeTest, ARootedMonsterWhoseCorpseDecaysIsReleased) {
	Ref<Npc> npc;
	Ref<Player> mage;
	{
		EFFECT_TEST_SCOPE;
		mage = player(9105);
		npc = worldMonster();
		npc->replaceAi(std::make_unique<DecayingNpcAI>(*npc));
		pair(*npc, *mage);
		Ref<Effect> rootOnNpc = applied(1328, *mage, *npc);
		ASSERT_TRUE(npc->getEffectController()->hasAbnormalEffect(1328));
		Ref<Effect> rootOnMage = applied(1328, *npc, *mage);
		ASSERT_TRUE(rootOnMage->isInSuccessEffects(1)) << "the monster's root landed on the Mage";
		rootOnMage = nullptr;
		Ref<Effect> lateBolt = calculated(1282, *mage, *npc); // cast before the death, lands after it

		ASSERT_NO_FATAL_FAILURE(killWithFlameBolt(*npc, *mage, true));
		EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(1328)) << "removeAllEffects on death ended the root";
		lateBolt->applyEffect();
		lateBolt = nullptr;
		advance(500);
		ASSERT_NO_FATAL_FAILURE(decay(*npc));
	}
	drain();
	EXPECT_EQ(npc->refCount(), 2u) << "the corpse's root on the Mage is the one foreign reference while it lasts: " << holdersOf(*npc, mage.get());
	{
		EFFECT_TEST_SCOPE;
		advance(20000);
		EXPECT_FALSE(mage->getEffectController()->hasAbnormalEffect(1328));
	}
	drain();
	EXPECT_EQ(npc->refCount(), 1u) << "only the case's own Ref may be left: " << holdersOf(*npc, mage.get());
	if (runtime::LIVE_COUNTS_ENABLED)
		EXPECT_EQ(liveEffects(), effectsBefore_) << "an Effect outlived its creatures";
}

/**
 * The same corpse, but the Mage logs out (removeAllEffects(true), LogoutBreakers::run) while the decaying corpse's root is still on it, and the
 * corpse decays after the logout.
 */
TEST_F(NpcEffectLifetimeTest, ACorpseWhoseRootOutlivesTheMagesLogoutIsReleasedAtItsDecay) {
	Ref<Npc> npc;
	Ref<Player> mage;
	{
		EFFECT_TEST_SCOPE;
		mage = player(9106);
		npc = worldMonster();
		npc->replaceAi(std::make_unique<DecayingNpcAI>(*npc));
		pair(*npc, *mage);
		Ref<Effect> rootOnMage = applied(1328, *npc, *mage);
		ASSERT_TRUE(rootOnMage->isInSuccessEffects(1));
		rootOnMage = nullptr;
		ASSERT_NO_FATAL_FAILURE(killWithFlameBolt(*npc, *mage, true));
		mage->getEffectController()->removeAllEffects(true);
		gameserver::model::gameobjects::player::LogoutBreakers::run(*mage);
		ASSERT_NO_FATAL_FAILURE(decay(*npc));
	}
	drain();
	EXPECT_EQ(npc->refCount(), 1u) << holdersOf(*npc, mage.get());
	if (runtime::LIVE_COUNTS_ENABLED)
		EXPECT_EQ(liveEffects(), effectsBefore_);
}

/**
 * The root breaks on the killing blow first (the ATTACKED observer, Rnd.chance() >= resistchance 10), then the monster dies: the same end
 * through the observer path (EffectController.removeEffect from inside the notification).
 */
TEST_F(NpcEffectLifetimeTest, ARootBrokenByTheKillingBlowLeavesNothingBehind) {
	Ref<Npc> npc;
	Ref<Player> mage;
	{
		EFFECT_TEST_SCOPE;
		mage = player(9111);
		npc = worldMonster();
		pair(*npc, *mage);
		Ref<Effect> root = applied(1328, *mage, *npc);
		ASSERT_TRUE(npc->getEffectController()->hasAbnormalEffect(1328));
		// every chance breaks the root: seed for a first draw of at least 10
		for (uint64_t seed = 1;; ++seed) {
			Rnd::seedCurrentThreadForTests(seed);
			if (Rnd::chance() >= 10.0f) {
				Rnd::seedCurrentThreadForTests(seed);
				break;
			}
		}
		npc->getObserveController()->notifyAttackedObservers(*mage, 1282);
		ASSERT_FALSE(npc->getEffectController()->hasAbnormalEffect(1328)) << "the attack broke the root";
		ASSERT_NO_FATAL_FAILURE(killWithFlameBolt(*npc, *mage));
	}
	drain();
	EXPECT_EQ(npc->refCount(), 1u) << holdersOf(*npc, mage.get());
	if (runtime::LIVE_COUNTS_ENABLED)
		EXPECT_EQ(liveEffects(), effectsBefore_);
}

/**
 * A damage over time on the monster (64015, 1447 Erosion's <spellatk checktime="3000">: the periodic task in Effect.periodicTasks pins the
 * effect), ticking when Flame Bolt kills the monster: removeAllEffects -> endEffect -> stopTasks cancels the tick task, whose Pin goes with it.
 */
TEST_F(NpcEffectLifetimeTest, AMonsterKilledDuringADamageOverTimeIsReleased) {
	Ref<Npc> npc;
	Ref<Player> mage;
	{
		EFFECT_TEST_SCOPE;
		mage = player(9121);
		npc = worldMonster();
		pair(*npc, *mage);
		Ref<Effect> dot = applied(64015, *mage, *npc);
		ASSERT_TRUE(dot->isInSuccessEffects(1));
		ASSERT_TRUE(dot->isPeriodic()) << "the tick task is scheduled";
		advance(3300); // the first tick
		ASSERT_NO_FATAL_FAILURE(killWithFlameBolt(*npc, *mage));
		EXPECT_FALSE(dot->isPeriodic()) << "stopTasks cancelled the tick task";
		advance(20000);
	}
	drain();
	EXPECT_EQ(npc->refCount(), 1u) << holdersOf(*npc, mage.get());
	if (runtime::LIVE_COUNTS_ENABLED)
		EXPECT_EQ(liveEffects(), effectsBefore_);
}

/**
 * The Warrior's critical proc: 8218 Stumble on the monster (StumbleEffect moves it and adds the effect to its controller for 2,000 ms), killed
 * while it stumbles.
 */
TEST_F(NpcEffectLifetimeTest, AStumblingMonsterKilledIsReleased) {
	Ref<Npc> npc;
	Ref<Player> warrior;
	Ref<Player> mage;
	{
		EFFECT_TEST_SCOPE;
		warrior = player(9131, gameserver::model::PlayerClass::WARRIOR, 1, 500, 500, 100);
		mage = player(9132, gameserver::model::PlayerClass::MAGE, 1, 500, 502, 100);
		npc = worldMonster();
		pair(*npc, *warrior);
		pair(*npc, *mage);
		Ref<Effect> stumble = applied(8218, *warrior, *npc);
		ASSERT_TRUE(npc->getEffectController()->hasAbnormalEffect(8218)) << "stumbling";
		ASSERT_NO_FATAL_FAILURE(killWithFlameBolt(*npc, *mage));
		advance(5000);
	}
	drain();
	EXPECT_EQ(npc->refCount(), 1u) << holdersOf(*npc, mage.get());
	if (runtime::LIVE_COUNTS_ENABLED)
		EXPECT_EQ(liveEffects(), effectsBefore_);
}

// ---- effects BY the monster on a player ---------------------------------------------------------------------------------------------------

/**
 * The monster roots the Mage (1328 with the npc as effector: the Effect in the Mage's abnormal map, its observer on the Mage), then the Mage
 * kills it. The effect stays on the Mage after the effector's death - Java's EffectController keeps it until it ends (CreatureController.onDie
 * ends only the dead creature's own effects), so while it lasts the corpse is reachable from the Mage in Java too. When it ends by time, the
 * monster must be free.
 */
TEST_F(NpcEffectLifetimeTest, AMonstersRootOnTheMageReleasesTheDeadMonsterWhenItEnds) {
	Ref<Npc> npc;
	Ref<Player> mage;
	{
		EFFECT_TEST_SCOPE;
		mage = player(9141);
		npc = worldMonster();
		pair(*npc, *mage);
		Ref<Effect> root = applied(1328, *npc, *mage);
		ASSERT_TRUE(root->isInSuccessEffects(1)) << "the monster's root landed on the Mage";
		ASSERT_TRUE(mage->getEffectController()->hasAbnormalEffect(1328));
		root = nullptr;
		ASSERT_NO_FATAL_FAILURE(killWithFlameBolt(*npc, *mage));
		EXPECT_TRUE(mage->getEffectController()->hasAbnormalEffect(1328)) << "Java: the effector's death does not end its effects on others";
	}
	drain();
	EXPECT_EQ(npc->refCount(), 2u) << "the Mage's root (Effect.effector) is the one foreign reference while it lasts: " << holdersOf(*npc, mage.get());
	{
		EFFECT_TEST_SCOPE;
		advance(20000); // duration2 of 1328
		EXPECT_FALSE(mage->getEffectController()->hasAbnormalEffect(1328)) << "ended by its end task";
	}
	drain();
	EXPECT_EQ(npc->refCount(), 1u) << "after the root ended: " << holdersOf(*npc, mage.get());
	if (runtime::LIVE_COUNTS_ENABLED)
		EXPECT_EQ(liveEffects(), effectsBefore_);
}

/**
 * The same, but the Mage logs out while the dead monster's root is still on it: PlayerLeaveWorldService.leaveWorld's
 * removeAllEffects(true) (PlayerLeaveWorldService.java:112; PlayerEffectsDAO.storePlayerEffects before it only reads the effects) and the
 * logout breakers (LogoutBreakers::run, the finally guard of leaveWorld).
 */
TEST_F(NpcEffectLifetimeTest, AMonstersRootOnTheMageReleasesTheDeadMonsterAtLogout) {
	Ref<Npc> npc;
	Ref<Player> mage;
	{
		EFFECT_TEST_SCOPE;
		mage = player(9151);
		npc = worldMonster();
		pair(*npc, *mage);
		Ref<Effect> root = applied(1328, *npc, *mage);
		ASSERT_TRUE(root->isInSuccessEffects(1));
		root = nullptr;
		ASSERT_NO_FATAL_FAILURE(killWithFlameBolt(*npc, *mage));
		advance(5000);
		mage->getEffectController()->removeAllEffects(true);
		gameserver::model::gameobjects::player::LogoutBreakers::run(*mage);
		EXPECT_TRUE(mage->getEffectController()->getAllEffects().empty());
	}
	drain();
	EXPECT_EQ(npc->refCount(), 1u) << holdersOf(*npc, mage.get());
	if (runtime::LIVE_COUNTS_ENABLED)
		EXPECT_EQ(liveEffects(), effectsBefore_);
}

/**
 * 16419 Brandish's shape: an instant <skillatk> by the monster on the Mage (64017, the fixture's magical skillatk), whose DIRECT hate lands in
 * the Mage's aggro list (Effect.broadcastHate: effected.getAggroList().addHate(effector, ...)). The monster dies to the Mage while the Mage
 * knows it: World.despawn -> KnownList.clear -> the Mage's CreatureController.notKnow -> AggroList.remove (CreatureController.java:77-81).
 */
TEST_F(NpcEffectLifetimeTest, AMonstersSkillAttackOnAMageThatKnowsItLeavesNoHateEntryBehind) {
	Ref<Npc> npc;
	Ref<Player> mage;
	{
		EFFECT_TEST_SCOPE;
		mage = player(9161);
		npc = worldMonster();
		pair(*npc, *mage);
		for (uint64_t seed = 1; seed < 200; ++seed) {
			Rnd::seedCurrentThreadForTests(seed);
			Ref<Effect> hit = calculated(64017, *npc, *mage);
			if (!hit->isInSuccessEffects(1))
				continue;
			hit->applyEffect();
			break;
		}
		int32_t entries = 0;
		for (const Ptr<controllers::attack::AggroInfo>& info : mage->getAggroList().stream())
			if (info->getAttacker().get() == npc.get())
				entries++;
		ASSERT_EQ(entries, 1) << "the hit put the monster into the Mage's aggro list";
		ASSERT_NO_FATAL_FAILURE(killWithFlameBolt(*npc, *mage));
	}
	drain();
	EXPECT_EQ(npc->refCount(), 1u) << holdersOf(*npc, mage.get());
	if (runtime::LIVE_COUNTS_ENABLED)
		EXPECT_EQ(liveEffects(), effectsBefore_);
}

/**
 * The same hit on a Mage that does NOT know the monster (no known-list pair): the hate of Effect.broadcastHate never enters the Mage's aggro
 * list, because PlayerAggroList.isAware (and AggroList.isAware) require owner.getKnownList().knows(creature) - so an aggro entry exists only
 * while the pair exists, and the pair's removal (notKnow) takes it out again. After the kill nothing holds the monster.
 */
TEST_F(NpcEffectLifetimeTest, AMonstersSkillAttackOnAMageThatDoesNotKnowItAddsNoHateAndHoldsNothing) {
	Ref<Npc> npc;
	Ref<Player> mage;
	{
		EFFECT_TEST_SCOPE;
		mage = player(9171);
		npc = worldMonster();
		bool landed = false;
		for (uint64_t seed = 1; seed < 200 && !landed; ++seed) {
			Rnd::seedCurrentThreadForTests(seed);
			Ref<Effect> hit = calculated(64017, *npc, *mage);
			if (!hit->isInSuccessEffects(1))
				continue;
			hit->applyEffect();
			landed = true;
		}
		ASSERT_TRUE(landed);
		EXPECT_FALSE(mage->getAggroList().isHating(*npc)) << "isAware: the Mage does not know the monster";
		ASSERT_NO_FATAL_FAILURE(killWithFlameBolt(*npc, *mage));
	}
	drain();
	EXPECT_EQ(npc->refCount(), 1u) << holdersOf(*npc, mage.get());
	if (runtime::LIVE_COUNTS_ENABLED)
		EXPECT_EQ(liveEffects(), effectsBefore_);
}

// ---- the kerub's own cast: 16419 Brandish ------------------------------------------------------------------------------------------------

/** 16419 Brandish as skill_templates.xml:118255-118267 has it (the striped kerubs' only npc skill, npc_skills.xml:2816-2821) */
inline constexpr const char* BRANDISH_XML =
	R"(<skill_template skill_id="16419" name="Brandish" nameId="282847" cooldownId="1" stack="NFI_SMALLBLOW_BRANDISH" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="2500" cancel_rate="35" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="2" target_relation="ENEMY" target_type="ONLYONE" target_maxcount="1" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><skillatk mode="PERCENT" value="85" e="1" accmod2="0" hoptype="DAMAGE" /></effects>)"
	R"(<motion name="poweratk" /></skill_template>)";

class NpcCastLifetimeTest : public NpcEffectLifetimeTest {
protected:
	const model::SkillTemplate* brandish() {
		xml::LoadContext context;
		return keep(xml::bindString<model::SkillTemplate>(context, BRANDISH_XML));
	}

	static int64_t liveSkills() { return runtime::liveCountOf(typeid(model::Skill)); }
};

/**
 * The kerub starts casting Brandish at the Mage (2,500 ms cast: Creature.castingSkill, the endCast task pinning the Skill, the move listener
 * in the kerub's ObserveController and the first-target death observer in the Mage's) and dies to a Flame Bolt 1,000 ms into the cast:
 * CreatureController.onDie only sets castingSkill to null (CreatureController.java:156), the endCast task still runs at 2,500 ms
 * (Skill.endCast: removeObservers, then `!effector.isCasting()` returns), the corpse decays at 3,000 ms.
 */
TEST_F(NpcCastLifetimeTest, AKerubKilledWhileCastingBrandishIsReleased) {
	Ref<Npc> npc;
	Ref<Player> mage;
	{
		EFFECT_TEST_SCOPE;
		mage = player(9181, gameserver::model::PlayerClass::MAGE, 1, 504, 500, 100);
		npc = worldMonster(505, 500);
		npc->replaceAi(std::make_unique<DecayingNpcAI>(*npc));
		pair(*npc, *mage);
		npc->setTarget(mage);
		Ref<model::Skill> cast = model::Skill::create(brandish(), *npc, 1, Ptr<Creature>(mage), nullptr);
		ASSERT_TRUE(cast->useSkill()) << "the kerub started casting Brandish";
		ASSERT_TRUE(npc->isCasting());
		cast = nullptr;
		advance(1000);
		ASSERT_NO_FATAL_FAILURE(killWithFlameBolt(*npc, *mage, true));
		EXPECT_FALSE(npc->isCasting()) << "onDie: setCasting(null)";
		advance(1500); // the endCast task
		ASSERT_NO_FATAL_FAILURE(decay(*npc));
		advance(10000);
	}
	drain();
	EXPECT_EQ(npc->refCount(), 1u) << holdersOf(*npc, mage.get());
	if (runtime::LIVE_COUNTS_ENABLED) {
		EXPECT_EQ(liveSkills(), skillsBefore_) << "a Skill outlived its caster";
		EXPECT_EQ(liveEffects(), effectsBefore_);
	}
}

/**
 * The kerub's Brandish completes and hits the Mage (the Effect by the kerub on the Mage, its DIRECT hate in the Mage's aggro list, the
 * SM_CASTSPELL_RESULT), then the Mage kills the kerub and the corpse decays.
 */
TEST_F(NpcCastLifetimeTest, AKerubWhoseBrandishHitTheMageIsReleasedAfterItsDecay) {
	Ref<Npc> npc;
	Ref<Player> mage;
	{
		EFFECT_TEST_SCOPE;
		mage = player(9191, gameserver::model::PlayerClass::MAGE, 1, 504, 500, 100);
		npc = worldMonster(505, 500);
		npc->replaceAi(std::make_unique<DecayingNpcAI>(*npc));
		pair(*npc, *mage);
		npc->setTarget(mage);
		const int32_t mageHp = mage->getLifeStats()->getCurrentHp();
		Ref<model::Skill> cast = model::Skill::create(brandish(), *npc, 1, Ptr<Creature>(mage), nullptr);
		ASSERT_TRUE(cast->useSkill()) << "the kerub started casting Brandish";
		cast = nullptr;
		advance(5000); // cast, hit time, application
		EXPECT_FALSE(npc->isCasting()) << "the cast ended";
		std::cout << "Brandish: Mage HP " << mageHp << " -> " << mage->getLifeStats()->getCurrentHp() << ", Mage hates the kerub: "
				  << mage->getAggroList().getHate(*npc) << std::endl;
		ASSERT_NO_FATAL_FAILURE(killWithFlameBolt(*npc, *mage, true));
		ASSERT_NO_FATAL_FAILURE(decay(*npc));
		advance(10000);
	}
	drain();
	EXPECT_EQ(npc->refCount(), 1u) << holdersOf(*npc, mage.get());
	if (runtime::LIVE_COUNTS_ENABLED) {
		EXPECT_EQ(liveSkills(), skillsBefore_) << "a Skill outlived its caster";
		EXPECT_EQ(liveEffects(), effectsBefore_);
	}
}

} // namespace
} // namespace aion::gameserver::skillengine::effect::mztest
