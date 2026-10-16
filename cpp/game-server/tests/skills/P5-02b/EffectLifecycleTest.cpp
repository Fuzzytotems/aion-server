// P5-02b, M5b-2 stage 1 part 2, work items K-01 and K-07 (m5b2-plan.md §5): the Effect engine - Effect.initialize's status table, the duration
// arithmetic of startEffect, the end task, endEffect and the lifetime of an effect (Effect.java:126-1212).
//
// Every case drives a real Effect between real, spawned creatures of a real map instance (EffectTestSupport.h) whose skill carries ProbeEffect
// templates, so what is asserted is Effect's own statement order: which templates it asks in which order, which random draws it makes and in
// which order, and what it leaves behind. Golden values are derived by hand from the Java expressions cited per case.

#include "EffectTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <typeinfo>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/LiveInstanceCounters.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/skillengine/effect/AbstractAbsoluteStatEffect.h"
#include "aion/gameserver/skillengine/effect/SubEffect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/EffectResult.h"
#include "aion/gameserver/skillengine/model/HopType.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SpellStatus.h"
#include "aion/gameserver/skillengine/model/SubEffectType.h"

namespace aion::gameserver::skillengine::effecttest {
namespace {

using controllers::attack::AttackStatus;
using gameserver::model::SkillElement;
using gameserver::model::stats::container::StatEnum;
using model::Effect;
using model::EffectReserved;
using model::EffectResult;
using model::SpellStatus;

constexpr uint64_t SEED = 20260923;

class EffectLifecycleTest : public EffectWorldTest {};

/** A skill of one probe at position 1 (the given element and calculate mode) */
std::vector<std::unique_ptr<effect::EffectTemplate>> oneProbe(Journal* journal, ProbeEffect::Calculate mode, SkillElement element) {
	auto p = probe("e1", journal, 1, mode);
	p->element = element;
	return probeList(std::move(p));
}

// ---- Effect.initialize (Effect.java:512-583) -------------------------------------------------------------------------------------------------

/**
 * Without a successful first position the effect is a DODGE when the main effect template (position 1) has no element - a physical skill - and a
 * RESIST otherwise; a CRITICAL attack status turns into CRITICAL_DODGE / CRITICAL_RESIST (Effect.java:545-563). The spell status of the
 * sm_castspell_end packet then follows the base attack status (:566-582): DODGE for either dodge, RESIST for either resist.
 */
TEST_F(EffectLifecycleTest, InitializeWithoutSuccessIsADodgeForAPhysicalAndAResistForAMagicalMainEffect) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3001);
	Ref<Npc> target = makeNpc(700101);
	Journal journal;
	const model::SkillTemplate* physical = keep(skillWithProbes(skillXml(9001, "PHYS", R"(tslot="BUFF")"),
		oneProbe(&journal, ProbeEffect::Calculate::FAIL, SkillElement::NONE)));
	const model::SkillTemplate* magical = keep(skillWithProbes(skillXml(9002, "MAG", R"(tslot="BUFF")"),
		oneProbe(&journal, ProbeEffect::Calculate::FAIL, SkillElement::FIRE)));

	struct Row {
		const model::SkillTemplate* skill;
		AttackStatus before;
		AttackStatus attackStatus;
		SpellStatus spellStatus;
		EffectResult result;
	};
	const Row rows[] = {
		{physical, AttackStatus::NORMALHIT, AttackStatus::DODGE, SpellStatus::DODGE, EffectResult::DODGE},
		{physical, AttackStatus::CRITICAL, AttackStatus::CRITICAL_DODGE, SpellStatus::DODGE, EffectResult::DODGE},
		{magical, AttackStatus::NORMALHIT, AttackStatus::RESIST, SpellStatus::RESIST, EffectResult::RESIST},
		{magical, AttackStatus::CRITICAL, AttackStatus::CRITICAL_RESIST, SpellStatus::RESIST, EffectResult::RESIST},
	};
	for (const Row& row : rows) {
		Ref<Effect> effect = Effect::create(*caster, target, row.skill, 1);
		effect->setAttackStatus(row.before);
		effect->initialize();
		const std::string what = std::to_string(row.skill->getSkillId()) + " from " + std::to_string(static_cast<int>(row.before));
		EXPECT_EQ(effect->getAttackStatus(), row.attackStatus) << what;
		EXPECT_EQ(effect->getSpellStatus(), row.spellStatus) << what;
		EXPECT_EQ(effect->getEffectResult(), row.result) << what;
		EXPECT_TRUE(effect->getSuccessEffects().isEmpty()) << what;
		EXPECT_EQ(effect->getSuccessfulEffectsAsByte(), row.result == EffectResult::DODGE ? 0 : 1) << "Effect.java:966-969; " << what;
	}
	EXPECT_EQ(journal, (Journal{"e1.calculate", "e1.calculate", "e1.calculate", "e1.calculate"})) << "the one template is asked once per effect";
}

/**
 * Position 1 gates everything: when it failed, the success effects of the other positions are cleared (`if (!isInSuccessEffects(1))
 * successEffects.clear()`, Effect.java:523-524) and the effect resists; when it succeeded, the others stay and nothing is dodged or resisted.
 */
TEST_F(EffectLifecycleTest, AFailedFirstPositionClearsTheSuccessOfEveryOtherPosition) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3011);
	Ref<Npc> target = makeNpc(700111);
	Journal journal;
	std::vector<std::unique_ptr<effect::EffectTemplate>> failingFirst;
	failingFirst.push_back(probe("e1", &journal, 1, ProbeEffect::Calculate::FAIL));
	failingFirst.push_back(probe("e2", &journal, 2, ProbeEffect::Calculate::SUCCEED));
	static_cast<ProbeEffect&>(*failingFirst[0]).element = SkillElement::WATER;
	const model::SkillTemplate* first = keep(skillWithProbes(skillXml(9011, "FIRST", R"(tslot="BUFF")"), std::move(failingFirst)));

	Ref<Effect> effect = Effect::create(*caster, target, first, 1);
	effect->initialize();
	EXPECT_EQ(journal, (Journal{"e1.calculate", "e2.calculate"})) << "every template is calculated, in the template list's order";
	EXPECT_FALSE(effect->isInSuccessEffects(2)) << "position 2 succeeded and was cleared with the failed position 1";
	EXPECT_TRUE(effect->getSuccessEffects().isEmpty());
	EXPECT_EQ(effect->getEffectResult(), EffectResult::RESIST);
	EXPECT_EQ(effect->getAttackStatus(), AttackStatus::RESIST);

	journal.clear();
	std::vector<std::unique_ptr<effect::EffectTemplate>> failingSecond;
	failingSecond.push_back(probe("e1", &journal, 1, ProbeEffect::Calculate::SUCCEED));
	failingSecond.push_back(probe("e2", &journal, 2, ProbeEffect::Calculate::FAIL));
	const model::SkillTemplate* second = keep(skillWithProbes(skillXml(9012, "SECOND", R"(tslot="BUFF")"), std::move(failingSecond)));
	Ref<Effect> kept = Effect::create(*caster, target, second, 1);
	kept->initialize();
	EXPECT_TRUE(kept->isInSuccessEffects(1));
	EXPECT_FALSE(kept->isInSuccessEffects(2));
	EXPECT_EQ(kept->getEffectResult(), EffectResult::NORMAL);
	EXPECT_EQ(kept->getAttackStatus(), AttackStatus::NORMALHIT);
	EXPECT_EQ(kept->getSpellStatus(), SpellStatus::NONE);
	EXPECT_EQ(kept->getSuccessfulEffectsAsByte(), 1 << 4) << "e1 = 0001 0000 (Effect.java:971-976)";
}

/**
 * The effect hate is the sum of every successful template's calculateHate, raised by the effector's BOOST_HATE through StatFunctions.calculateHate
 * (Effect.java:585-591): hopb 100 + hopa 10 * level 2 = 120 and hopb 30 give 150, and a level-1 player's BOOST_HATE base of 100 makes
 * (int) (150L * (1000 + 100) / 1000) = 165. Only computed when position 1 succeeded.
 */
TEST_F(EffectLifecycleTest, TheEffectHateIsTheBoostedSumOfTheSuccessfulTemplatesHate) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3021);
	Ref<Npc> target = makeNpc(700121);
	Journal journal;
	std::vector<std::unique_ptr<effect::EffectTemplate>> probes;
	probes.push_back(probe("e1", &journal, 1));
	probes.push_back(probe("e2", &journal, 2));
	probes.push_back(probe("e3", &journal, 3, ProbeEffect::Calculate::FAIL));
	auto& e1 = static_cast<ProbeEffect&>(*probes[0]);
	auto& e2 = static_cast<ProbeEffect&>(*probes[1]);
	auto& e3 = static_cast<ProbeEffect&>(*probes[2]);
	e1.hopType = model::HopType::SKILLLV;
	e1.hopB = 100;
	e1.hopA = 10;
	e2.hopType = model::HopType::SKILLLV;
	e2.hopB = 30;
	e3.hopType = model::HopType::SKILLLV;
	e3.hopB = 5000; // not successful: not counted
	const model::SkillTemplate* skill = keep(skillWithProbes(skillXml(9021, "HATE", R"(tslot="BUFF")"), std::move(probes)));

	Ref<Effect> effect = Effect::create(*caster, target, skill, 2);
	effect->initialize();
	EXPECT_EQ(effect->getEffectHate(), 165);
}

/**
 * A new effect that conflicts with one already on the effected (same effect id, lower basic level, EffectController.isConflicting) is marked
 * CONFLICT before any template is calculated (Effect.java:516-522); nothing succeeds, the attack status is DODGE and the spell status stays NONE,
 * because the DODGE arm of the switch skips a CONFLICT (:567-569).
 */
TEST_F(EffectLifecycleTest, AConflictingEffectCalculatesNothingAndDodgesWithoutASpellStatus) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3031);
	Ref<Player> target = makePlayer(3032);
	Journal journal;
	auto existingProbe = probe("existing", &journal, 1);
	existingProbe->effectid = 500;
	existingProbe->basicLvl = 5;
	existingProbe->duration2 = 60000;
	const model::SkillTemplate* existingSkill = keep(skillWithProbes(skillXml(9031, "EXISTING", R"(tslot="BUFF")"), probeList(std::move(existingProbe))));
	auto newProbe = probe("new", &journal, 1);
	newProbe->effectid = 500;
	newProbe->basicLvl = 3;
	const model::SkillTemplate* newSkill = keep(skillWithProbes(skillXml(9032, "NEW", R"(tslot="BUFF")"), probeList(std::move(newProbe))));

	Ref<Effect> existing = Effect::create(*caster, target, existingSkill, 1);
	existing->initialize();
	target->getEffectController()->addEffect(*existing);
	ASSERT_EQ(target->getEffectController()->getAbnormalEffect("EXISTING").get(), existing.get());
	journal.clear();

	Ref<Effect> effect = Effect::create(*caster, target, newSkill, 1);
	effect->initialize();
	EXPECT_EQ(journal, Journal{}) << "no template of a conflicting effect is calculated";
	EXPECT_EQ(effect->getEffectResult(), EffectResult::CONFLICT);
	EXPECT_EQ(effect->getAttackStatus(), AttackStatus::DODGE);
	EXPECT_EQ(effect->getSpellStatus(), SpellStatus::NONE) << "the DODGE arm keeps the spell status of a CONFLICT";
	EXPECT_TRUE(effect->getSuccessEffects().isEmpty());
	existing->endEffect();
}

/** PARRY and BLOCK only fill a spell status nothing else set (Effect.java:571-578); the base status decides (CRITICAL_PARRY is a PARRY). */
TEST_F(EffectLifecycleTest, ParryAndBlockOnlyReplaceAnUnsetSpellStatus) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> caster = makeNpc(700141); // an npc effector: no critical proc roll
	Ref<Player> target = makePlayer(3041);
	Journal journal;
	const model::SkillTemplate* skill = keep(skillWithProbes(skillXml(9041, "PARRY", R"(tslot="BUFF")"),
		oneProbe(&journal, ProbeEffect::Calculate::SUCCEED, SkillElement::NONE)));

	struct Row {
		AttackStatus attackStatus;
		SpellStatus before;
		SpellStatus after;
	};
	const Row rows[] = {
		{AttackStatus::PARRY, SpellStatus::NONE, SpellStatus::PARRY},
		{AttackStatus::CRITICAL_PARRY, SpellStatus::NONE, SpellStatus::PARRY},
		{AttackStatus::PARRY, SpellStatus::STUMBLE, SpellStatus::STUMBLE},
		{AttackStatus::BLOCK, SpellStatus::NONE, SpellStatus::BLOCK},
		{AttackStatus::OFFHAND_BLOCK, SpellStatus::NONE, SpellStatus::BLOCK},
		{AttackStatus::BLOCK, SpellStatus::OPENAERIAL, SpellStatus::OPENAERIAL},
		{AttackStatus::NORMALHIT, SpellStatus::STAGGER, SpellStatus::STAGGER},
	};
	for (const Row& row : rows) {
		Ref<Effect> effect = Effect::create(*caster, target, skill, 1);
		effect->setAttackStatus(row.attackStatus);
		effect->setSpellStatus(row.before);
		effect->initialize();
		EXPECT_EQ(effect->getSpellStatus(), row.after) << static_cast<int>(row.attackStatus) << " / " << static_cast<int>(row.before);
		EXPECT_EQ(effect->getAttackStatus(), row.attackStatus) << "a successful effect keeps its attack status";
	}
}

/**
 * Draw order of initialize: the critical proc's `Rnd.chance() < 10` is the LAST term of a left-to-right conjunction (Effect.java:533), so the
 * chance is drawn only for a player effector whose attack status is CRITICAL, without a sub effect and without a periodic task - exactly one
 * draw then, none otherwise. (The caster has no main-hand weapon, so SkillEngine.createCriticalProcEffect answers null either way.)
 */
TEST_F(EffectLifecycleTest, TheCriticalProcChanceIsDrawnOnlyForACriticalPlayerHitWithoutASubEffectOrPeriodicTask) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3051);
	Ref<Npc> npcCaster = makeNpc(700151);
	Ref<Npc> target = makeNpc(700152);
	Journal journal;
	const model::SkillTemplate* skill = keep(skillWithProbes(skillXml(9051, "PROC", R"(tslot="DEBUFF")"),
		oneProbe(&journal, ProbeEffect::Calculate::SUCCEED, SkillElement::NONE)));
	const std::vector<float> stream = chanceStream(SEED, 2);

	auto drawsOf = [&](Creature& effector, AttackStatus status, bool withSubEffect, bool periodic) {
		Ref<Effect> effect = Effect::create(effector, target, skill, 1);
		effect->setAttackStatus(status);
		if (withSubEffect)
			effect->setSubEffect(Effect::create(effector, target, skill, 1));
		if (periodic)
			effect->setPeriodicTask(nullptr, 1); // Java: the first call creates the array, which is what isPeriodic() tests
		Rnd::seedCurrentThreadForTests(SEED);
		effect->initialize();
		const float next = Rnd::chance();
		EXPECT_EQ(effect->getSubEffect() != nullptr, withSubEffect) << "no proc effect for a caster without a main-hand weapon";
		return next == stream[0] ? 0 : next == stream[1] ? 1 : -1;
	};
	EXPECT_EQ(drawsOf(*caster, AttackStatus::CRITICAL, false, false), 1) << "the one case that rolls";
	EXPECT_EQ(drawsOf(*npcCaster, AttackStatus::CRITICAL, false, false), 0) << "an npc effector: the instanceof term fails first";
	EXPECT_EQ(drawsOf(*caster, AttackStatus::NORMALHIT, false, false), 0) << "not critical";
	EXPECT_EQ(drawsOf(*caster, AttackStatus::CRITICAL, true, false), 0) << "a sub effect is already set";
	EXPECT_EQ(drawsOf(*caster, AttackStatus::CRITICAL, false, true), 0) << "a periodic effect";
}

// ---- startEffect and the duration (Effect.java:652-688, 884-910) -----------------------------------------------------------------------------

/**
 * The duration is duration2 + duration1 * skillLevel of the FIRST success effect in position order whose sum is positive (Effect.java:899-910) -
 * position order is Java's ConcurrentHashMap iteration over the Integer positions, not the template list's order. The list here is e3, e2, e1:
 * e1 sums to 0 and is skipped, e2 gives 1000 + 500 * 3 = 2500 and e3's 9000 is never looked at. The end task fires exactly 2500 ms later,
 * endEffects runs the templates in position order, and getRemainingTimeToDisplay counts down on the scheduler clock.
 */
TEST_F(EffectLifecycleTest, TheDurationIsTheFirstPositiveDurationInPositionOrderAndTheEndTaskFiresThen) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3061);
	Ref<Npc> target = makeNpc(700161);
	Journal journal;
	std::vector<std::unique_ptr<effect::EffectTemplate>> probes;
	probes.push_back(probe("e3", &journal, 3));
	probes.push_back(probe("e2", &journal, 2));
	probes.push_back(probe("e1", &journal, 1));
	static_cast<ProbeEffect&>(*probes[0]).duration2 = 9000;
	static_cast<ProbeEffect&>(*probes[1]).duration2 = 1000;
	static_cast<ProbeEffect&>(*probes[1]).duration1 = 500;
	static_cast<ProbeEffect&>(*probes[2]).duration2 = -300;
	static_cast<ProbeEffect&>(*probes[2]).duration1 = 100; // -300 + 100 * 3 = 0: not positive
	const model::SkillTemplate* skill = keep(skillWithProbes(skillXml(9061, "DURATION", R"(tslot="BUFF")"), std::move(probes)));

	Ref<Effect> effect = Effect::create(*caster, target, skill, 3);
	effect->initialize();
	journal.clear();
	target->getEffectController()->addEffect(*effect);
	EXPECT_EQ(journal, (Journal{"e1.start", "e2.start", "e3.start"})) << "startEffect runs the success effects in position order";
	EXPECT_EQ(effect->getDuration(), 2500);
	EXPECT_EQ(effect->getRemainingTimeToDisplay(), 2500);
	EXPECT_EQ(effect->getEndTime(), clock.currentTimeMillis() + 2500);

	journal.clear();
	advance(1000);
	EXPECT_EQ(effect->getRemainingTimeToDisplay(), 1500);
	advance(1499);
	EXPECT_EQ(journal, Journal{}) << "1 ms before the end nothing has ended";
	EXPECT_FALSE(effect->isEndedByTime());
	advance(1);
	EXPECT_EQ(journal, (Journal{"e1.end", "e2.end", "e3.end"})) << "the end task ran endEffect(true), which ends the templates in position order";
	EXPECT_TRUE(effect->isEndedByTime());
	EXPECT_TRUE(target->getEffectController()->isEmpty());
}

/** randomtime subtracts Rnd.get(0, randomTime) from the first positive duration, one draw (Effect.java:904-905); randomtime 0 draws nothing */
TEST_F(EffectLifecycleTest, RandomTimeSubtractsOneDrawFromTheDuration) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3071);
	Ref<Npc> target = makeNpc(700171);
	Journal journal;
	auto randomProbe = probe("e1", &journal, 1);
	randomProbe->duration2 = 5000;
	randomProbe->randomTime = 400;
	const model::SkillTemplate* random = keep(skillWithProbes(skillXml(9071, "RANDOM", R"(tslot="BUFF")"), probeList(std::move(randomProbe))));
	auto fixedProbe = probe("e1", &journal, 1);
	fixedProbe->duration2 = 5000;
	const model::SkillTemplate* fixed = keep(skillWithProbes(skillXml(9072, "FIXED", R"(tslot="BUFF")"), probeList(std::move(fixedProbe))));

	Rnd::seedCurrentThreadForTests(SEED);
	const int32_t expected = 5000 - Rnd::get(0, 400);
	Ref<Effect> effect = Effect::create(*caster, target, random, 1);
	effect->addAllEffectToSucess();
	Rnd::seedCurrentThreadForTests(SEED);
	effect->startEffect();
	EXPECT_EQ(effect->getDuration(), expected);
	ASSERT_NE(expected, 5000) << "the seed must draw a non-zero reduction for this case to prove anything";

	const std::vector<float> stream = chanceStream(SEED, 1);
	Ref<Effect> plain = Effect::create(*caster, target, fixed, 1);
	plain->addAllEffectToSucess();
	Rnd::seedCurrentThreadForTests(SEED);
	plain->startEffect();
	EXPECT_EQ(plain->getDuration(), 5000);
	EXPECT_EQ(Rnd::chance(), stream[0]) << "no draw without randomtime";
	effect->endEffect();
	plain->endEffect();
}

/**
 * pvp_duration scales the duration of an effect on a player by another creature: duration * pvpDuration / 100 (Effect.java:892-894). It does not
 * apply to an npc target or to the effector's own effect (`!effector.equals(effected)`).
 */
TEST_F(EffectLifecycleTest, PvpDurationScalesOnlyAnEffectOnAnotherPlayer) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3081);
	Ref<Player> other = makePlayer(3082);
	Ref<Npc> npc = makeNpc(700181);
	Journal journal;
	auto pvpProbe = probe("e1", &journal, 1);
	pvpProbe->duration2 = 2500;
	const model::SkillTemplate* skill =
		keep(skillWithProbes(skillXml(9081, "PVP", R"(tslot="BUFF" pvp_duration="50")"), probeList(std::move(pvpProbe))));

	auto durationOn = [&](Ptr<Creature> effected) {
		Ref<Effect> effect = Effect::create(*caster, effected, skill, 1);
		effect->addAllEffectToSucess();
		effect->startEffect();
		int32_t value = effect->getDuration();
		effect->endEffect();
		return value;
	};
	EXPECT_EQ(durationOn(other), 1250) << "2500 * 50 / 100";
	EXPECT_EQ(durationOn(caster), 2500) << "the caster's own effect";
	EXPECT_EQ(durationOn(npc), 2500) << "not a player";
}

/**
 * A toggle takes the template's toggle_timer instead of the effect durations (Effect.java:670-673); a duration of 0 schedules no end task and
 * returns before the instance handler is told (:678-679): the effect is permanent, shows no timer and is never saved.
 */
TEST_F(EffectLifecycleTest, AToggleTakesItsToggleTimerAndAZeroDurationIsPermanent) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3091);
	Journal journal;
	auto toggleProbe = probe("e1", &journal, 1);
	toggleProbe->duration2 = 5000;
	const model::SkillTemplate* toggle = keep(skillWithProbes(
		R"(<skill_template skill_id="9091" name="toggle" nameId="1" stack="TOGGLE" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF")"
		R"( activation="TOGGLE" toggle_timer="30000" duration="0"/>)",
		probeList(std::move(toggleProbe))));
	auto permanentProbe = probe("e1", &journal, 1);
	const model::SkillTemplate* permanent = keep(skillWithProbes(skillXml(9092, "PERMANENT", R"(tslot="BUFF")"), probeList(std::move(permanentProbe))));

	Ref<Effect> toggled = Effect::create(*caster, caster, toggle, 1);
	toggled->addAllEffectToSucess();
	toggled->startEffect();
	EXPECT_TRUE(toggled->isToggle());
	EXPECT_EQ(toggled->getDuration(), 30000) << "toggle_timer, not the template's 5000";

	Ref<Effect> forever = Effect::create(*caster, caster, permanent, 1);
	forever->addAllEffectToSucess();
	size_t pendingBefore = executor->pendingTaskCount();
	forever->startEffect();
	EXPECT_EQ(forever->getDuration(), 0);
	EXPECT_EQ(executor->pendingTaskCount(), pendingBefore) << "no end task";
	EXPECT_EQ(forever->getRemainingTimeToDisplay(), -1);
	EXPECT_FALSE(forever->canSaveOnLogout());
	EXPECT_TRUE(toggled->canSaveOnLogout()) << "a 30 s toggle is saved (Effect.java:779-787)";
	toggled->endEffect();
	forever->endEffect();
}

/** A forced duration of 24 h or more is shown as permanent on an npc and never saved (Effect.java:773-774, 784-785) */
TEST_F(EffectLifecycleTest, ADayLongEffectShowsNoTimerOnAnNpcAndIsNeverSaved) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3101);
	Ref<Npc> npc = makeNpc(700201);
	Journal journal;
	const model::SkillTemplate* skill = keep(skillWithProbes(skillXml(9101, "DAY", R"(tslot="BUFF")"), probeList(probe("e1", &journal, 1))));

	Ref<Effect> onNpc = Effect::create(*caster, npc, skill, 1, 86400000, nullptr);
	onNpc->addAllEffectToSucess();
	onNpc->startEffect();
	EXPECT_EQ(onNpc->getRemainingTimeToDisplay(), -1);
	EXPECT_FALSE(onNpc->canSaveOnLogout());

	Ref<Effect> onPlayer = Effect::create(*caster, caster, skill, 1, 86400000, nullptr);
	onPlayer->addAllEffectToSucess();
	onPlayer->startEffect();
	EXPECT_EQ(onPlayer->getRemainingTimeToDisplay(), 86400000) << "a player sees the timer";
	EXPECT_FALSE(onPlayer->canSaveOnLogout());

	Ref<Effect> justBelow = Effect::create(*caster, caster, skill, 1, 86399999, nullptr);
	justBelow->addAllEffectToSucess();
	justBelow->startEffect();
	EXPECT_TRUE(justBelow->canSaveOnLogout());
	onNpc->endEffect();
	onPlayer->endEffect();
	justBelow->endEffect();
}

// ---- endEffect and the lifetime (Effect.java:704-761, 826-839, 1037-1046) --------------------------------------------------------------------

/**
 * endEffect runs once (the hasEnded CAS), cancels the end and the periodic tasks (stopTasks), removes the cancel-on-damage observers
 * (removeObservers), ends the templates and removes the stat functions the effect owns (endEffects -> CreatureGameStats.endEffect, because the
 * template has a <change>), and takes the effect out of the effected's controller (clearEffect).
 */
TEST_F(EffectLifecycleTest, EndEffectRunsOnceAndUndoesEverythingStartEffectDid) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3111);
	Ref<Player> target = makePlayer(3112);
	Journal journal;
	auto ticks = std::make_shared<int>(0);
	auto lifeProbe = probe("e1", &journal, 1);
	lifeProbe->duration2 = 20000;
	lifeProbe->change.emplace_back(); // a <change>: endEffects asks CreatureGameStats.endEffect
	lifeProbe->onStart = [ticks](Effect& effect) {
		effect.setPeriodicTask(utils::ThreadPoolManager::getInstance().scheduleAtFixedRate(runtime::Pin{&effect}, [ticks] { ++*ticks; }, 1000, 1000), 1);
		effect.getEffected()->getGameStats()->addEffect(Ptr<gameserver::model::stats::calc::StatOwner>(effect),
			{Ptr<gameserver::model::stats::calc::functions::IStatFunction>(
				gameserver::model::stats::calc::functions::RcStatFunction<gameserver::model::stats::calc::functions::StatAddFunction>::create(
					StatEnum::MAXHP, 1000, true))});
	};
	const model::SkillTemplate* skill = keep(skillWithProbes(skillXml(9111, "LIFE", R"(tslot="BUFF")"), probeList(std::move(lifeProbe))));
	const int32_t baseMaxHp = target->getGameStats()->getMaxHp()->getCurrent();
	ASSERT_FALSE(target->getObserveController()->hasObservers());

	Ref<Effect> effect = Effect::create(*caster, target, skill, 1);
	effect->initialize();
	effect->setCancelOnDmg(true);
	target->getEffectController()->addEffect(*effect);
	EXPECT_TRUE(effect->isPeriodic());
	EXPECT_TRUE(target->getObserveController()->hasObservers()) << "the two cancel-on-damage observers";
	EXPECT_EQ(target->getGameStats()->getMaxHp()->getCurrent(), baseMaxHp + 1000);
	advance(3000);
	EXPECT_EQ(*ticks, 3);

	effect->endEffect();
	EXPECT_EQ(journal, (Journal{"e1.calculate", "e1.start", "e1.end"}));
	EXPECT_FALSE(effect->isPeriodic()) << "stopTasks drops the periodic task array";
	EXPECT_FALSE(target->getObserveController()->hasObservers()) << "removeObservers ran the removal tasks";
	EXPECT_EQ(target->getGameStats()->getMaxHp()->getCurrent(), baseMaxHp) << "the effect's stat function is gone";
	EXPECT_TRUE(target->getEffectController()->isEmpty());
	advance(30000);
	EXPECT_EQ(*ticks, 3) << "the periodic task was cancelled";
	EXPECT_FALSE(effect->isEndedByTime()) << "the end task was cancelled, not run";

	effect->endEffect();
	EXPECT_EQ(journal, (Journal{"e1.calculate", "e1.start", "e1.end"})) << "a second endEffect does nothing";
}

/**
 * The lifetime the free-threaded runtime cannot give for free (m5b2-plan.md §8 items 1-4): an effect that ended - by its end task or by
 * endEffect - is released by its end task, its periodic task, its observers and the controller map, so once the test lets go and the Reclaimer
 * drains, no Effect is alive and neither creature is retained by it.
 */
TEST_F(EffectLifecycleTest, AnEndedEffectReleasesItselfAndBothCreatures) {
	if (!runtime::LIVE_COUNTS_ENABLED)
		GTEST_SKIP() << "live instance counters exist in checked builds only";
	runtime::Reclaimer::getInstance().drain();
	const int64_t effectsBefore = runtime::liveCountOf(typeid(Effect));
	uint32_t casterRefsBefore = 0;
	uint32_t targetRefsBefore = 0;
	Ref<Player> caster;
	Ref<Player> target;
	{
		EFFECT_TEST_SCOPE;
		caster = makePlayer(3121);
		target = makePlayer(3122);
		casterRefsBefore = caster->refCount();
		targetRefsBefore = target->refCount();
		Journal journal;
		auto lifeProbe = probe("e1", &journal, 1);
		lifeProbe->duration2 = 20000;
		lifeProbe->onStart = [](Effect& effect) {
			effect.setPeriodicTask(utils::ThreadPoolManager::getInstance().scheduleAtFixedRate(runtime::Pin{&effect}, [] {}, 1000, 1000), 1);
		};
		const model::SkillTemplate* skill = keep(skillWithProbes(skillXml(9121, "LIFETIME", R"(tslot="BUFF")"), probeList(std::move(lifeProbe))));
		auto shortProbe = probe("e1", &journal, 1);
		shortProbe->duration2 = 2000;
		const model::SkillTemplate* shortSkill = keep(skillWithProbes(skillXml(9122, "SHORT", R"(tslot="BUFF")"), probeList(std::move(shortProbe))));

		Ref<Effect> ended = Effect::create(*caster, target, skill, 1);
		ended->initialize();
		ended->setCancelOnDmg(true);
		target->getEffectController()->addEffect(*ended);
		Ref<Effect> timed = Effect::create(*caster, target, shortSkill, 1);
		timed->initialize();
		target->getEffectController()->addEffect(*timed);
		EXPECT_EQ(runtime::liveCountOf(typeid(Effect)), effectsBefore + 2);
		EXPECT_GT(target->refCount(), targetRefsBefore) << "the effects hold the target";
		advance(2500); // the timed effect ends by its end task
		ended->endEffect();
		EXPECT_TRUE(timed->isEndedByTime());
	}
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(runtime::liveCountOf(typeid(Effect)), effectsBefore) << "both effects were reclaimed";
	EXPECT_EQ(caster->refCount(), casterRefsBefore) << "no effect, task or observer still holds the caster";
	EXPECT_EQ(target->refCount(), targetRefsBefore) << "no effect, task or observer still holds the target";
}

/**
 * <periodicactions> (Effect.java:659, 864-882): startEffect schedules one fixed-rate task whose initial delay and period are both the checktime
 * (:871-874); every tick runs each action on the effect - MpUsePeriodicAction.act takes `value` MP from the effected, or ends the effect when the
 * MP does not cover it (MpUsePeriodicAction.java:21-30). stopTasks cancels the task (stopPeriodicActions, :760; the java-hook of cycles.toml
 * "Effect@L871:77#this"), so the drain stops with the effect - whether endEffect ends it or the drain itself does - and the ended effects give
 * back every task and, once the test lets go, both creatures. The creature counts hold in every build, the live Effect count where it exists.
 */
TEST_F(EffectLifecycleTest, PeriodicActionsDrainEveryChecktimeAndStopWithTheEffect) {
	runtime::Reclaimer::getInstance().drain();
	const int64_t effectsBefore = runtime::LIVE_COUNTS_ENABLED ? runtime::liveCountOf(typeid(Effect)) : 0;
	uint32_t casterRefsBefore = 0;
	uint32_t targetRefsBefore = 0;
	Ref<Player> caster;
	Ref<Npc> target;
	{
		EFFECT_TEST_SCOPE;
		caster = makePlayer(3171);
		target = makeNpc(700271); // an npc: its MP changes start no restore task (NpcLifeStats has no onMpChanged)
		casterRefsBefore = caster->refCount();
		targetRefsBefore = target->refCount();
		Journal journal;
		auto drainProbe = probe("e1", &journal, 1);
		drainProbe->duration2 = 60000;
		const model::SkillTemplate* drain = keep(skillWithProbes(
			R"(<skill_template skill_id="9171" name="drain" nameId="1" stack="DRAIN" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF")"
			R"( activation="ACTIVE" duration="0"><periodicactions checktime="1000"><mpuse value="10"/></periodicactions></skill_template>)",
			probeList(std::move(drainProbe))));
		auto mp = [&target] { return target->getLifeStats()->getCurrentMp(); };
		ASSERT_EQ(mp(), 100) << "the npc template's maxMp";
		const size_t pendingBefore = executor->pendingTaskCount();

		Ref<Effect> ended = Effect::create(*caster, target, drain, 1);
		ended->initialize();
		ended->addToEffectedController();
		EXPECT_EQ(executor->pendingTaskCount(), pendingBefore + 2) << "the end task and the periodic actions task";
		advance(999);
		EXPECT_EQ(mp(), 100) << "the first tick waits one checktime";
		advance(1);
		EXPECT_EQ(mp(), 90);
		advance(2000);
		EXPECT_EQ(mp(), 70) << "one tick per checktime";
		ended->endEffect();
		EXPECT_EQ(executor->pendingTaskCount(), pendingBefore) << "stopTasks cancelled the end task and the periodic actions task";
		advance(5000);
		EXPECT_EQ(mp(), 70) << "the drain stopped with the effect";

		// the drain ends its own effect: 25 -> 15 -> 5, and the third tick finds 5 < 10
		target->getLifeStats()->setCurrentMp(25);
		journal.clear();
		Ref<Effect> drained = Effect::create(*caster, target, drain, 1);
		drained->initialize();
		drained->addToEffectedController();
		advance(2000);
		EXPECT_EQ(mp(), 5);
		EXPECT_EQ(journal, (Journal{"e1.calculate", "e1.start"}));
		advance(1000);
		EXPECT_EQ(journal, (Journal{"e1.calculate", "e1.start", "e1.end"})) << "the third tick ended the effect";
		EXPECT_TRUE(target->getEffectController()->isEmpty());
		EXPECT_EQ(executor->pendingTaskCount(), pendingBefore) << "the task that ended its effect was cancelled by it";
		advance(5000);
		EXPECT_EQ(mp(), 5);
	}
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(caster->refCount(), casterRefsBefore) << "no effect or task still holds the caster";
	EXPECT_EQ(target->refCount(), targetRefsBefore) << "no effect or task still holds the target";
	if (runtime::LIVE_COUNTS_ENABLED)
		EXPECT_EQ(runtime::liveCountOf(typeid(Effect)), effectsBefore) << "both effects were reclaimed";
}

/**
 * The order of the start and end hooks (m5b2-plan.md D9), watched from inside a probe's own hooks: startEffect adds the cancel-on-damage
 * observers only after every template started (Effect.java:661-664); endEffect stops the tasks and removes the observers before it ends the
 * templates (:716-719); and an effect that has ended does not start at all (the hasEnded check under the lock, :654) - no template, no duration,
 * no end task.
 */
TEST_F(EffectLifecycleTest, TemplatesStartBeforeTheObserversAndEndAfterTheTasksAndObserversAreGone) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3181);
	Ref<Player> target = makePlayer(3182);
	Journal journal;
	struct Seen {
		bool observersAtStart = true;
		bool periodicAtEnd = true;
		bool observersAtEnd = true;
	};
	auto seen = std::make_shared<Seen>();
	auto watcher = probe("e1", &journal, 1);
	watcher->duration2 = 20000;
	watcher->onStart = [seen](Effect& effect) {
		seen->observersAtStart = effect.getEffected()->getObserveController()->hasObservers();
		effect.setPeriodicTask(utils::ThreadPoolManager::getInstance().scheduleAtFixedRate(runtime::Pin{&effect}, [] {}, 1000, 1000), 1);
	};
	watcher->onEnd = [seen](Effect& effect) {
		seen->periodicAtEnd = effect.isPeriodic();
		seen->observersAtEnd = effect.getEffected()->getObserveController()->hasObservers();
	};
	const model::SkillTemplate* watched = keep(skillWithProbes(skillXml(9181, "WATCHED", R"(tslot="BUFF")"), probeList(std::move(watcher))));
	auto lateProbe = probe("late", &journal, 1);
	lateProbe->duration2 = 20000;
	const model::SkillTemplate* late = keep(skillWithProbes(skillXml(9182, "LATE", R"(tslot="BUFF")"), probeList(std::move(lateProbe))));
	ASSERT_FALSE(target->getObserveController()->hasObservers());

	Ref<Effect> effect = Effect::create(*caster, target, watched, 1);
	effect->initialize();
	effect->setCancelOnDmg(true);
	target->getEffectController()->addEffect(*effect);
	EXPECT_FALSE(seen->observersAtStart) << "the templates start before addCancelOnDmgObserver (Effect.java:662-664)";
	EXPECT_TRUE(target->getObserveController()->hasObservers()) << "the observers were added after them";
	effect->endEffect();
	EXPECT_FALSE(seen->periodicAtEnd) << "stopTasks ran before endEffects (Effect.java:716, 719)";
	EXPECT_FALSE(seen->observersAtEnd) << "removeObservers ran before endEffects (Effect.java:717, 719)";

	Ref<Effect> ended = Effect::create(*caster, target, late, 1);
	ended->addAllEffectToSucess();
	ended->endEffect(); // a conflicting cast ended it before it started (Effect.java:654)
	journal.clear();
	const size_t pendingBefore = executor->pendingTaskCount();
	ended->startEffect();
	EXPECT_EQ(journal, Journal{}) << "a start after the end starts no template";
	EXPECT_EQ(ended->getDuration(), 0) << "and computes no duration";
	EXPECT_EQ(executor->pendingTaskCount(), pendingBefore) << "and schedules no end task";
}

/**
 * The cancel-on-damage observers (Effect.java:1009-1027): the effected being attacked (ObserverType.ATTACKED -> attacked) or hit by damage over
 * time (DOT_ATTACKED -> dotattacked) ends the effect, which removes both of its observers; an effect without cancel on damage stays.
 */
TEST_F(EffectLifecycleTest, AnAttackOrADamageOverTimeHitEndsACancelOnDamageEffect) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3191);
	Ref<Player> target = makePlayer(3192);
	Ref<Npc> attacker = makeNpc(700291);
	Journal journal;
	auto longProbe = [&journal](std::string name) {
		auto p = probe(std::move(name), &journal, 1);
		p->duration2 = 60000;
		return probeList(std::move(p));
	};
	const model::SkillTemplate* byHitSkill = keep(skillWithProbes(skillXml(9191, "CANCEL_HIT", R"(tslot="BUFF")"), longProbe("hit")));
	const model::SkillTemplate* byDotSkill = keep(skillWithProbes(skillXml(9192, "CANCEL_DOT", R"(tslot="BUFF")"), longProbe("dot")));
	const model::SkillTemplate* staysSkill = keep(skillWithProbes(skillXml(9193, "STAYS", R"(tslot="BUFF")"), longProbe("stays")));
	auto start = [&](const model::SkillTemplate* skill, bool cancelOnDmg) {
		Ref<Effect> effect = Effect::create(*caster, target, skill, 1);
		effect->initialize();
		effect->setCancelOnDmg(cancelOnDmg);
		target->getEffectController()->addEffect(*effect);
		return effect;
	};
	controllers::ObserveController& observers = *target->getObserveController();

	Ref<Effect> stays = start(staysSkill, false);
	Ref<Effect> byHit = start(byHitSkill, true);
	journal.clear();
	observers.notifyAttackedObservers(*attacker, 0);
	EXPECT_EQ(journal, Journal{"hit.end"}) << "ATTACKED ended the effect";
	EXPECT_FALSE(observers.hasObservers()) << "and both of its observers are gone";

	Ref<Effect> byDot = start(byDotSkill, true);
	journal.clear();
	observers.notifyDotAttackedObservers(*attacker, *stays);
	EXPECT_EQ(journal, Journal{"dot.end"}) << "DOT_ATTACKED ended the effect";
	EXPECT_FALSE(observers.hasObservers());
	observers.notifyAttackedObservers(*attacker, 0);
	EXPECT_EQ(journal, Journal{"dot.end"}) << "nothing left to cancel";
	EXPECT_TRUE(target->getEffectController()->hasAbnormalEffect(9193)) << "an effect without cancel on damage stays";
	stays->endEffect();
}

// ---- applyEffect and the hate (Effect.java:585-646, 804-812) ---------------------------------------------------------------------------------

/**
 * broadcastHate (Effect.java:621-636): an effect hate other than 0 goes to the effected's aggro list unless the taunt hate is negative (`>= 0`: a
 * taunt hate of 0 does not block it), and is reset to 0 so a second broadcast adds nothing. applyEffect broadcasts the hate of an effect without
 * success effects (:597-600) - here the hate a Skill hands its effects (the constructor with a skill, :126-128, as a pet order sets it), which
 * initialize neither replaces (the `effectHate == 0` guard, :526) nor, for a failed position 1, computes; startEffect broadcasts the hate of a
 * successful effect (:667): hopb 100 raised by the level-1 player's BOOST_HATE of 100 is (int) (100L * 1100 / 1000) = 110.
 */
TEST_F(EffectLifecycleTest, TheEffectHateReachesTheAggroListOnceUnlessTheTauntHateIsNegative) {
	EFFECT_TEST_SCOPE;
	publishTribeRelations();
	Ref<Player> caster = makePlayer(3201);
	Ref<Npc> target = makeNpc(700301, {}, 505, 500, 100, "MONSTER");
	ASSERT_TRUE(KnownListPairing::pair(*target, *caster)) << "AggroList.isAware reads the known list";
	Journal journal;
	auto hateProbe = [&journal](ProbeEffect::Calculate mode) {
		auto p = probe("e1", &journal, 1, mode);
		p->hopType = model::HopType::SKILLLV;
		p->hopB = 100;
		p->duration2 = 60000;
		p->element = SkillElement::FIRE;
		return probeList(std::move(p));
	};
	const model::SkillTemplate* hated = keep(skillWithProbes(skillXml(9201, "HATED", R"(tslot="DEBUFF")"), hateProbe(ProbeEffect::Calculate::SUCCEED)));
	const model::SkillTemplate* resisted = keep(skillWithProbes(skillXml(9202, "RESISTED", R"(tslot="DEBUFF")"), hateProbe(ProbeEffect::Calculate::FAIL)));
	controllers::attack::AggroList& aggro = target->getAggroList();

	Ref<model::Skill> resistedCast = model::Skill::create(resisted, *caster, target, 1);
	resistedCast->setHate(200);
	Ref<Effect> resist = Effect::create(*resistedCast, target);
	resist->initialize();
	ASSERT_TRUE(resist->getSuccessEffects().isEmpty());
	EXPECT_EQ(resist->getEffectHate(), 200) << "the skill's hate";
	resist->applyEffect();
	EXPECT_EQ(aggro.getHate(*caster), 200) << "applyEffect broadcast the hate of the resisted effect";
	EXPECT_EQ(resist->getEffectHate(), 0) << "reset after the broadcast";
	resist->broadcastHate();
	EXPECT_EQ(aggro.getHate(*caster), 200) << "a second broadcast adds nothing";

	Ref<model::Skill> hatedCast = model::Skill::create(hated, *caster, target, 1);
	hatedCast->setHate(30);
	Ref<Effect> kept = Effect::create(*hatedCast, target);
	kept->initialize();
	EXPECT_EQ(kept->getEffectHate(), 30) << "a skill's hate is not replaced by the templates' (Effect.java:526)";

	Ref<Effect> hit = Effect::create(*caster, target, hated, 1);
	hit->initialize();
	EXPECT_EQ(hit->getEffectHate(), 110);
	hit->addToEffectedController();
	EXPECT_EQ(aggro.getHate(*caster), 310) << "startEffect broadcast the hate; a taunt hate of 0 does not block it";
	EXPECT_EQ(hit->getEffectHate(), 0);
	hit->endEffect();

	Ref<Effect> taunted = Effect::create(*caster, target, hated, 1);
	taunted->initialize();
	taunted->setTauntHate(-1);
	taunted->broadcastHate();
	EXPECT_EQ(aggro.getHate(*caster), 310) << "a negative taunt hate adds no hate";
	EXPECT_EQ(taunted->getEffectHate(), 110) << "and keeps it on the effect";
}

/**
 * applyEffect (Effect.java:596-619) applies the success effects in position order, each followed by its sub effect (EffectTemplate.startSubEffect
 * applies the effect's sub effect for a template with a <subeffect>, EffectTemplate.java:462-470), and asks shouldApplyFurtherEffects (:638-646)
 * before either step: an effected that died stops the loop - even between a template and its own sub effect - unless the skill resurrects, and a
 * despawned one stops it unless the skill is passive. After the loop only a critical proc's sub effect is applied (:612-613).
 * addToEffectedController adds nothing to a dead effected (:804-812).
 */
TEST_F(EffectLifecycleTest, ApplyEffectAppliesThePositionsInOrderWhileTheEffectedCanTakeThem) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3211);
	Ref<Npc> target = makeNpc(700311);
	Ref<Npc> despawned = makeNpc(700312);
	despawned->getPosition()->setIsSpawned(false);
	Journal journal;
	auto withSubEffect = [](std::unique_ptr<ProbeEffect> p) {
		p->subEffect = std::make_unique<effect::SubEffect>();
		p->subEffect->skillId = 9215;
		return p;
	};
	std::vector<std::unique_ptr<effect::EffectTemplate>> reversed;
	reversed.push_back(withSubEffect(probe("e3", &journal, 3)));
	reversed.push_back(withSubEffect(probe("e2", &journal, 2)));
	reversed.push_back(withSubEffect(probe("e1", &journal, 1)));
	const model::SkillTemplate* ordered = keep(skillWithProbes(skillXml(9211, "ORDERED", R"(tslot="DEBUFF")"), std::move(reversed)));
	std::vector<std::unique_ptr<effect::EffectTemplate>> killing;
	auto killer = withSubEffect(probe("e1", &journal, 1));
	killer->onApply = [](Effect& effect) { effect.getEffected()->getLifeStats()->setCurrentHp(0); };
	killing.push_back(std::move(killer));
	killing.push_back(withSubEffect(probe("e2", &journal, 2)));
	const model::SkillTemplate* lethal = keep(skillWithProbes(skillXml(9212, "LETHAL", R"(tslot="DEBUFF")"), std::move(killing)));
	const model::SkillTemplate* plain = keep(skillWithProbes(skillXml(9213, "PLAIN", R"(tslot="DEBUFF")"), probeList(probe("plain", &journal, 1))));
	const model::SkillTemplate* revive = keep(skillWithProbes(skillXml(9214, "REVIVE", R"(tslot="BUFF")"), probeList(probe("revive", &journal, 1)),
		{effect::EffectType::RESURRECT}));
	const model::SkillTemplate* passive = keep(skillWithProbes(R"(<skill_template skill_id="9216" name="passive" nameId="1" stack="PASSIVE_APPLY")"
															   R"( lvl="1" skilltype="PHYSICAL" skillsubtype="NONE" activation="PASSIVE" duration="0"/>)",
		probeList(probe("passive", &journal, 1))));
	// the one sub effect every <subeffect> applies: an effect on the caster, so its own shouldApplyFurtherEffects never sees the target die
	const model::SkillTemplate* subSkill = keep(skillWithProbes(skillXml(9215, "SUB", R"(tslot="BUFF")"), probeList(probe("sub", &journal, 1))));
	Ref<Effect> sub = Effect::create(*caster, caster, subSkill, 1);
	sub->addAllEffectToSucess();
	auto applied = [&](Ptr<Creature> effected, const model::SkillTemplate* skill, bool withSub) {
		journal.clear();
		Ref<Effect> effect = Effect::create(*caster, effected, skill, 1);
		effect->addAllEffectToSucess();
		if (withSub)
			effect->setSubEffect(sub);
		effect->applyEffect();
		return journal;
	};

	EXPECT_EQ(applied(target, ordered, true), (Journal{"e1.apply", "sub.apply", "e2.apply", "sub.apply", "e3.apply", "sub.apply"}))
		<< "position order, each position followed by its sub effect";
	EXPECT_EQ(applied(target, plain, true), Journal{"plain.apply"}) << "no <subeffect>, no critical proc: the sub effect is not applied";
	EXPECT_EQ(applied(despawned, plain, false), Journal{}) << "a despawned effected takes nothing";
	EXPECT_EQ(applied(despawned, passive, false), Journal{"passive.apply"}) << "but a passive skill (players get them before they spawn)";

	EXPECT_EQ(applied(target, lethal, true), Journal{"e1.apply"}) << "e1 killed the effected: neither its sub effect nor e2 follows";
	ASSERT_TRUE(target->isDead());
	EXPECT_EQ(applied(target, ordered, true), Journal{}) << "a dead effected takes nothing";
	EXPECT_EQ(applied(target, revive, false), Journal{"revive.apply"}) << "but a resurrection";

	journal.clear();
	Ref<Effect> late = Effect::create(*caster, target, plain, 1);
	late->addAllEffectToSucess();
	late->addToEffectedController();
	EXPECT_EQ(journal, Journal{}) << "a dead effected's controller does not start the effect";
	EXPECT_TRUE(target->getEffectController()->isEmpty());
}

// ---- reserveds, magical criticals, the success byte and reflection ----------------------------------------------------------------------------

/**
 * setReserveds stores the value and, for a sent HP value of a non-periodic effect, the effected's HP percentage after it (Effect.java:345-368):
 * max(1, (int) (100f * (currentHp - damage) / maxHp)), 0 and the killing blow when that is not positive. getReserveds answers the reserved of a
 * position or a new empty one; getReservedEffectsToSend keeps the sent, non-zero ones in position order and falls back to one empty value that
 * carries the attack status (:370-381).
 */
TEST_F(EffectLifecycleTest, ReservedsSetTheEffectedHpAndAreSentInPositionOrder) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3131);
	Ref<Player> target = makePlayer(3132);
	Journal journal;
	const model::SkillTemplate* skill = keep(skillWithProbes(skillXml(9131, "RESERVED", R"(tslot="DEBUFF")"), probeList(probe("e1", &journal, 1))));
	const int32_t currentHp = target->getLifeStats()->getCurrentHp();
	const int32_t maxHp = target->getLifeStats()->getMaxHp();
	ASSERT_GT(currentHp, 100);

	Ref<Effect> effect = Effect::create(*caster, target, skill, 1);
	EXPECT_EQ(effect->getEffectedHp(), -1);
	Ref<EffectReserved> damage = EffectReserved::create(2, 100, EffectReserved::ResourceType::HP, true);
	effect->setReserveds(*damage, false);
	const int32_t expectedPercent = std::max(1, static_cast<int32_t>(100.0f * static_cast<float>(currentHp - 100) / static_cast<float>(maxHp)));
	EXPECT_EQ(effect->getEffectedHp(), expectedPercent);
	Ref<EffectReserved> mana = EffectReserved::create(1, 40, EffectReserved::ResourceType::MP, false);
	effect->setReserveds(*mana, false);
	EXPECT_EQ(effect->getEffectedHp(), expectedPercent) << "an MP value does not touch the HP percentage";
	effect->setReserveds(*EffectReserved::create(3, 0, EffectReserved::ResourceType::HP, true), false);
	effect->setReserveds(*EffectReserved::create(4, 70, EffectReserved::ResourceType::HP, true, false), false);

	EXPECT_EQ(effect->getReserveds(2).get(), damage.get());
	Ref<EffectReserved> none = effect->getReserveds(9);
	EXPECT_EQ(none->getValue(), 0);
	EXPECT_FALSE(none->isSend());
	std::vector<Ref<EffectReserved>> toSend = effect->getReservedEffectsToSend();
	ASSERT_EQ(toSend.size(), 2u) << "the zero value and the unsent one are left out";
	EXPECT_EQ(toSend[0].get(), mana.get()) << "position order: 1 before 2";
	EXPECT_EQ(toSend[1].get(), damage.get());

	Ref<Effect> periodic = Effect::create(*caster, target, skill, 1);
	periodic->setReserveds(*EffectReserved::create(1, currentHp + 5, EffectReserved::ResourceType::HP, true), true);
	EXPECT_EQ(periodic->getEffectedHp(), -1) << "an over-time value leaves the HP percentage alone";
	EXPECT_FALSE(target->getLifeStats()->isAboutToDie());

	Ref<Effect> killing = Effect::create(*caster, target, skill, 1);
	killing->setAttackStatus(AttackStatus::CRITICAL);
	std::vector<Ref<EffectReserved>> fallback = killing->getReservedEffectsToSend();
	ASSERT_EQ(fallback.size(), 1u);
	EXPECT_EQ(fallback[0]->getValue(), 0);
	EXPECT_TRUE(fallback[0]->isSend());
	EXPECT_EQ(fallback[0]->getAttackStatus(), AttackStatus::CRITICAL) << "the attack status is shown without a value";
	killing->setReserveds(*EffectReserved::create(1, currentHp, EffectReserved::ResourceType::HP, true), false);
	EXPECT_EQ(killing->getEffectedHp(), 0);
	EXPECT_TRUE(target->getLifeStats()->isAboutToDie()) << "the killing blow is set on the effected";
	Ref<EffectReserved> heal = EffectReserved::create(1, 25, EffectReserved::ResourceType::HP, false);
	EXPECT_EQ(heal->getValueToSend(), -25) << "EffectReserved.getValueToSend: a heal is sent negated";
	EXPECT_EQ(damage->getValueToSend(), 100);
}

/**
 * A cast rolls its magical critical once per target: the first position rolls (one Rnd.nextInt(1000) in StatFunctions.calculateMagicalCriticalRate)
 * and the next positions take it over; resetMagicalCritical makes the next position roll again (Effect.java:271-292). Saved effects restore the
 * positions through setMagicalCriticals, which tests positions.contains(i) for the array INDEX i, as Java writes it (:294-301).
 */
TEST_F(EffectLifecycleTest, TheMagicalCriticalIsRolledOncePerTargetAndRestoredByIndex) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3141);
	Ref<Npc> target = makeNpc(700241);
	Journal journal;
	const model::SkillTemplate* skill = keep(skillWithProbes(skillXml(9141, "CRIT", R"(tslot="DEBUFF")"), probeList(probe("e1", &journal, 1))));

	Rnd::seedCurrentThreadForTests(SEED);
	std::vector<int32_t> reference;
	for (int i = 0; i < 3; ++i)
		reference.push_back(Rnd::nextInt(1000));

	Ref<Effect> effect = Effect::create(*caster, target, skill, 1);
	Rnd::seedCurrentThreadForTests(SEED);
	effect->rollMagicalCritical(1, 100);
	effect->rollMagicalCritical(2, 100);
	effect->reuseMagicalCritical(3);
	EXPECT_EQ(Rnd::nextInt(1000), reference[1]) << "one draw for three positions";
	EXPECT_EQ(effect->isMagicalCritical(1), effect->isMagicalCritical(2));
	EXPECT_EQ(effect->isMagicalCritical(1), effect->isMagicalCritical(3));

	Rnd::seedCurrentThreadForTests(SEED);
	effect->resetMagicalCritical();
	effect->rollMagicalCritical(1, 100);
	EXPECT_EQ(Rnd::nextInt(1000), reference[1]) << "a reset chain rolls again";
	EXPECT_THROW(effect->isMagicalCritical(5), runtime::ArrayIndexOutOfBoundsException) << "Java: magicalCriticals[position - 1]";

	const std::unordered_set<int32_t> saved{0, 2};
	Ref<Effect> restored = Effect::create(*caster, target, skill, 1, 60000, nullptr, false, &saved);
	EXPECT_TRUE(restored->isMagicalCritical(1)) << "index 0";
	EXPECT_FALSE(restored->isMagicalCritical(2));
	EXPECT_TRUE(restored->isMagicalCritical(3)) << "index 2";
	EXPECT_FALSE(restored->isMagicalCritical(4));
	Rnd::seedCurrentThreadForTests(SEED);
	restored->rollMagicalCritical(4, 100);
	EXPECT_EQ(Rnd::nextInt(1000), reference[0]) << "restored criticals count as rolled: no draw";
	EXPECT_TRUE(restored->isMagicalCritical(4)) << "position 4 takes over the restored critical";
}

/**
 * getSuccessfulEffectsAsByte: DODGE 0, RESIST 1, otherwise the byte sum of 1 << (position + 3) over the success effects plus the sub effect
 * type's id (Effect.java:965-977): positions 1 and 4 and STUMBLE (4) give 16 + 128 + 4 = 148, which is the byte -108.
 */
TEST_F(EffectLifecycleTest, TheSuccessByteSumsThePositionBitsAndTheSubEffectType) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3151);
	Journal journal;
	std::vector<std::unique_ptr<effect::EffectTemplate>> probes;
	probes.push_back(probe("e1", &journal, 1));
	probes.push_back(probe("e2", &journal, 2));
	probes.push_back(probe("e4", &journal, 4));
	const model::SkillTemplate* skill = keep(skillWithProbes(skillXml(9151, "BYTE", R"(tslot="BUFF")"), std::move(probes)));

	const auto& templates = skill->getEffects()->getEffects(); // positions 1, 2, 4 (getEffectTemplate indexes the list, it does not search)
	Ref<Effect> effect = Effect::create(*caster, caster, skill, 1);
	effect->addSuccessEffect(templates[0].get());
	effect->addSuccessEffect(templates[2].get());
	effect->setSubEffectType(model::SubEffectType::STUMBLE);
	EXPECT_EQ(effect->getSuccessfulEffectsAsByte(), static_cast<int8_t>(-108));
	effect->addSuccessEffect(templates[1].get());
	effect->setSubEffectType(model::SubEffectType::SIMPLE_MOVE_BACK);
	EXPECT_EQ(effect->getSuccessfulEffectsAsByte(), static_cast<int8_t>(16 + 32 + 128 + 12 - 256));
	effect->setEffectResult(EffectResult::RESIST);
	EXPECT_EQ(effect->getSuccessfulEffectsAsByte(), 1);
	effect->setEffectResult(EffectResult::DODGE);
	EXPECT_EQ(effect->getSuccessfulEffectsAsByte(), 0);
}

/**
 * setShieldDefense drops the SKILL_REFLECTOR bit (32) of anything but an ATTACK or DEBUFF skill (Effect.java:395-399); a reflected effect's
 * getEffected() is the effector (:229-231).
 */
TEST_F(EffectLifecycleTest, OnlyAttackAndDebuffEffectsCanBeReflectedWhole) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3161);
	Ref<Npc> target = makeNpc(700261);
	Journal journal;
	const model::SkillTemplate* buff = keep(skillWithProbes(skillXml(9161, "REFLECT_BUFF", R"(tslot="BUFF")"), probeList(probe("e1", &journal, 1))));
	const model::SkillTemplate* attack = keep(skillWithProbes(
		R"(<skill_template skill_id="9162" name="attack" nameId="1" stack="REFLECT_ATK" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK")"
		R"( tslot="NONE" activation="ACTIVE" duration="0"/>)",
		probeList(probe("e1", &journal, 1))));

	Ref<Effect> buffEffect = Effect::create(*caster, target, buff, 1);
	buffEffect->setShieldDefense(2 | 32);
	EXPECT_EQ(buffEffect->getShieldDefense(), 2);
	EXPECT_FALSE(buffEffect->isReflected());
	EXPECT_EQ(buffEffect->getEffected().get(), target.get());

	Ref<Effect> attackEffect = Effect::create(*caster, target, attack, 1);
	attackEffect->setShieldDefense(2 | 32);
	EXPECT_EQ(attackEffect->getShieldDefense(), 34);
	EXPECT_TRUE(attackEffect->isReflected());
	EXPECT_EQ(attackEffect->getEffected().get(), caster.get()) << "a reflected effect hits its effector";
	EXPECT_EQ(attackEffect->getOriginalEffected().get(), target.get());
}

/**
 * setReserveds leaves the effected's HP percentage alone for the effect of an instant skill (a Skill without hit time is one,
 * Skill.isInstantSkill) and for an invulnerable effected (Effect.java:348-351), and floors a positive rest at 1 (:362): a rest of 1 HP is
 * (int) (100f * 1 / maxHp) = 0 for a maxHp above 100, and max(1, 0) = 1.
 */
TEST_F(EffectLifecycleTest, SetReservedsSkipsInstantSkillsAndInvulnerableEffectedsAndFloorsTheHpAtOne) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3251);
	Ref<Player> target = makePlayer(3252);
	Journal journal;
	const model::SkillTemplate* skill = keep(skillWithProbes(skillXml(9251, "RESERVED_GUARDS", R"(tslot="DEBUFF")"), probeList(probe("e1", &journal, 1))));
	const int32_t currentHp = target->getLifeStats()->getCurrentHp();
	ASSERT_GT(target->getLifeStats()->getMaxHp(), 100) << "1 HP must be less than 1 %";
	auto damage = [](int32_t value) { return EffectReserved::create(1, value, EffectReserved::ResourceType::HP, true); };

	Ref<model::Skill> instant = model::Skill::create(skill, *caster, target, 1);
	ASSERT_TRUE(instant->isInstantSkill());
	Ref<Effect> ofInstant = Effect::create(*instant, target);
	ofInstant->setReserveds(*damage(100), false);
	EXPECT_EQ(ofInstant->getEffectedHp(), -1) << "an instant skill's effect";

	target->setCustomState(gameserver::model::gameobjects::player::CustomPlayerState::INVULNERABLE);
	Ref<Effect> onInvulnerable = Effect::create(*caster, target, skill, 1);
	onInvulnerable->setReserveds(*damage(100), false);
	EXPECT_EQ(onInvulnerable->getEffectedHp(), -1) << "an invulnerable effected";
	target->unsetCustomState(gameserver::model::gameobjects::player::CustomPlayerState::INVULNERABLE);

	Ref<Effect> almost = Effect::create(*caster, target, skill, 1);
	almost->setReserveds(*damage(currentHp - 1), false);
	EXPECT_EQ(almost->getEffectedHp(), 1) << "1 HP left: 0 %, floored at 1";
	EXPECT_FALSE(target->getLifeStats()->isAboutToDie());
}

/**
 * initialize calculates the sub effects of the success effects only for an effect that launches them (Effect.java:528-532): a <subeffect> of
 * chance 0 draws its one Rnd.chance() in EffectTemplate.calculateSubEffect (EffectTemplate.java:415-416) and stops there, so the draw shows
 * whether it ran.
 */
TEST_F(EffectLifecycleTest, SubEffectsAreCalculatedOnlyForAnEffectThatLaunchesThem) {
	EFFECT_TEST_SCOPE;
	Ref<Player> caster = makePlayer(3261);
	Ref<Npc> target = makeNpc(700361);
	Journal journal;
	auto launcher = probe("e1", &journal, 1);
	launcher->subEffect = std::make_unique<effect::SubEffect>();
	launcher->subEffect->skillId = 9262;
	launcher->subEffect->chance = 0;
	const model::SkillTemplate* skill = keep(skillWithProbes(skillXml(9261, "LAUNCHER", R"(tslot="DEBUFF")"), probeList(std::move(launcher))));
	const std::vector<float> stream = chanceStream(SEED, 2);

	auto drawsOf = [&](bool launch) {
		Ref<Effect> effect = Effect::create(*caster, target, skill, 1);
		effect->setLaunchSubEffect(launch);
		Rnd::seedCurrentThreadForTests(SEED);
		effect->initialize();
		const float next = Rnd::chance();
		EXPECT_EQ(effect->getSubEffect(), nullptr) << "chance 0 never creates the sub effect";
		return next == stream[0] ? 0 : next == stream[1] ? 1 : -1;
	};
	EXPECT_EQ(drawsOf(true), 1) << "the sub effect's chance was drawn";
	EXPECT_EQ(drawsOf(false), 0) << "setLaunchSubEffect(false): no sub effect is calculated";
}

// ---- the smaller branches of endEffect, endEffects, canSaveOnLogout and canRemoveOnDie ---------------------------------------------------------

/**
 * endEffect's special cases (Effect.java:721-732): the effect of the skill a player's stance runs stops the stance (another effect of the player
 * leaves it), and a SPEC2 effect gives back (int) (maxHp * 0.2f) HP and (int) (maxMp * 0.2f) MP when it ends - 2522 * 0.2f = 504.4 and
 * 100 * 0.2f = 20 for the test npc.
 */
TEST_F(EffectLifecycleTest, EndEffectStopsTheStanceOfItsSkillAndRestoresAfterASpec2Effect) {
	EFFECT_TEST_SCOPE;
	publishSkillData(skillXml(9271, "STANCE", R"(tslot="BUFF")") + skillXml(9272, "NO_STANCE", R"(tslot="BUFF")"));
	Journal journal;
	injectProbes(dataholders::DataManager::SKILL_DATA->getSkillTemplate(9271), probeList(probe("stance", &journal, 1)));
	injectProbes(dataholders::DataManager::SKILL_DATA->getSkillTemplate(9272), probeList(probe("other", &journal, 1)));
	const model::SkillTemplate* spec2 = keep(skillWithProbes(skillXml(9273, "SPEC2", R"(tslot="SPEC2")"), probeList(probe("spec2", &journal, 1))));
	Ref<Player> player = makePlayer(3271);
	Ref<Npc> npc = makeNpc(700371);
	auto started = [&](Creature& effected, const model::SkillTemplate* skill) {
		Ref<Effect> effect = Effect::create(*player, Ptr<Creature>(effected), skill, 1);
		effect->addAllEffectToSucess();
		effected.getEffectController()->addEffect(*effect);
		return effect;
	};

	player->getController().startStance(9271);
	Ref<Effect> stance = started(*player, dataholders::DataManager::SKILL_DATA->getSkillTemplate(9271));
	started(*player, dataholders::DataManager::SKILL_DATA->getSkillTemplate(9272))->endEffect();
	EXPECT_EQ(player->getController().getStanceSkillId(), 9271) << "another skill's effect leaves the stance";
	stance->endEffect();
	EXPECT_EQ(player->getController().getStanceSkillId(), 0) << "the stance skill's effect stopped the stance";

	ASSERT_EQ(npc->getLifeStats()->getMaxHp(), 2522);
	ASSERT_EQ(npc->getLifeStats()->getMaxMp(), 100);
	npc->getLifeStats()->setCurrentHp(1000);
	npc->getLifeStats()->setCurrentMp(10);
	started(*npc, spec2)->endEffect();
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 1504) << "1000 + (int) (2522 * 0.2f)";
	EXPECT_EQ(npc->getLifeStats()->getCurrentMp(), 30) << "10 + (int) (100 * 0.2f)";
}

/** An AbstractAbsoluteStatEffect without a <change> that owns a MAXHP function while it runs (the absolute stat effects' functions come from a stat set) */
class ProbeAbsoluteStat final : public effect::AbstractAbsoluteStatEffect {
public:
	explicit ProbeAbsoluteStat(Journal* probeJournal) : journal(probeJournal) {}
	std::string_view javaClassName() const override { return "ProbeAbsoluteStat"; }
	void calculate(model::Effect& effect) const override { effect.addSuccessEffect(this); }
	void applyEffect(model::Effect&) const override {}
	void startEffect(model::Effect& effect) const override {
		effect.getEffected()->getGameStats()->addEffect(Ptr<gameserver::model::stats::calc::StatOwner>(effect),
			{Ptr<gameserver::model::stats::calc::functions::IStatFunction>(
				gameserver::model::stats::calc::functions::RcStatFunction<gameserver::model::stats::calc::functions::StatAddFunction>::create(
					StatEnum::MAXHP, 1000, true))});
	}
	void endEffect(model::Effect&) const override { journal->push_back("absolute.end"); }
	using AbstractAbsoluteStatEffect::position;
	Journal* const journal;
};

/** endEffects asks CreatureGameStats.endEffect for a success effect with a <change> or of an AbstractAbsoluteStatEffect (Effect.java:1037-1046) */
TEST_F(EffectLifecycleTest, AnAbsoluteStatEffectTakesItsStatFunctionsAlongWithoutAChange) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(3281);
	Journal journal;
	auto absolute = std::make_unique<ProbeAbsoluteStat>(&journal);
	absolute->position = 1;
	std::vector<std::unique_ptr<effect::EffectTemplate>> list;
	list.push_back(std::move(absolute));
	const model::SkillTemplate* skill = keep(skillWithProbes(skillXml(9281, "ABSOLUTE", R"(tslot="BUFF")"), std::move(list)));
	const int32_t baseMaxHp = player->getGameStats()->getMaxHp()->getCurrent();

	Ref<Effect> effect = Effect::create(*player, player, skill, 1);
	effect->initialize();
	player->getEffectController()->addEffect(*effect);
	ASSERT_EQ(player->getGameStats()->getMaxHp()->getCurrent(), baseMaxHp + 1000);
	effect->endEffect();
	EXPECT_EQ(journal, Journal{"absolute.end"});
	EXPECT_EQ(player->getGameStats()->getMaxHp()->getCurrent(), baseMaxHp) << "the stat function went with the effect";
}

/**
 * Two flags of the skill: no_save_on_logout keeps an effect that would be saved from being saved (Effect.java:779-781), and an XPBOOST effect
 * survives its effected's death (canRemoveOnDie, :1119-1127) where an effect without it is removed.
 */
TEST_F(EffectLifecycleTest, NoSaveOnLogoutBlocksTheSaveAndAnXpBoostSurvivesDeath) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(3291);
	Journal journal;
	auto timedProbe = [&journal](std::string name) {
		auto p = probe(std::move(name), &journal, 1);
		p->duration2 = 60000;
		return probeList(std::move(p));
	};
	const model::SkillTemplate* saved = keep(skillWithProbes(skillXml(9291, "SAVED", R"(tslot="BUFF")"), timedProbe("saved")));
	const model::SkillTemplate* unsaved = keep(skillWithProbes(skillXml(9292, "UNSAVED", R"(tslot="BUFF" no_save_on_logout="true")"), timedProbe("unsaved")));
	const model::SkillTemplate* boost =
		keep(skillWithProbes(skillXml(9293, "XP_BOOST", R"(tslot="BUFF")"), timedProbe("boost"), {effect::EffectType::XPBOOST}));
	auto started = [&](const model::SkillTemplate* skill) {
		Ref<Effect> effect = Effect::create(*player, player, skill, 1);
		effect->initialize();
		player->getEffectController()->addEffect(*effect);
		return effect;
	};

	EXPECT_TRUE(started(saved)->canSaveOnLogout());
	EXPECT_FALSE(started(unsaved)->canSaveOnLogout()) << "the same 60 s, but no_save_on_logout";
	Ref<Effect> xpBoost = started(boost);
	EXPECT_TRUE(xpBoost->canSaveOnLogout());
	EXPECT_FALSE(xpBoost->canRemoveOnDie());
	journal.clear();
	player->getEffectController()->removeAllEffects();
	EXPECT_EQ(journal, (Journal{"saved.end", "unsaved.end"})) << "death ends the other two, not the XPBOOST";
	EXPECT_TRUE(player->getEffectController()->hasAbnormalEffect(9293));
	player->getEffectController()->removeAllEffects(true);
}

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
