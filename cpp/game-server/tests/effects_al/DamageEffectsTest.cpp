// P5-03, M5b-2 stage 1 part 3, items F-02 and F-05 (m5b2-plan.md §5): the damage classes of the A-L half - DamageEffect (through the data-only
// SpellAttackInstantEffect, `<spellatkinstant>`, which overrides nothing), BleedEffect and their periodic base AbstractOverTimeEffect - on real
// Effects of templates bound through the real binder, with the golden values of their arithmetic derived by hand from the Java expressions.
//
// The effected creature is an npc of EffectTestSupport.h's template (level 4, maxHp 2522, mdef 70, no elemental defence) seen by a player whose
// connection records the npc's broadcasts. Where a golden value runs through AttackUtil (calculateSkillResult, calculateMagicalOverTimeSkillResult:
// P5-01, item F-06), the effector is a second npc, because a player's damage is scaled by its own stat functions (the npc level difference and the
// PvE ratios of PlayerStatFunctions) while an npc's knowledge is the StatsTemplate's 100 and its PvE ratios are 0. For such an npc the Java
// arithmetic of a FIRE skill is, step by step (AttackUtil.java:222-351, StatFunctions.java:381-415, 472-520, AttackUtil.java:427-452):
//   damage = base * (0 / 1000f + 100 / 100f)             knowledge 100, no magic boost (the skill does not apply_magical_skill_boost_bonus)
//   damage = BOOST_SPELL_ATTACK of (int) damage           no function: the int itself (DamageEffect only; the over-time path skips it)
//   damage = damage * (1 - 0 / 1300f) - 70 / 10f          no FIRE defence; mdef 70 takes 7
//   damage += Rnd.get(-rnd, rnd), rnd = (int) (damage * 0.08f)   0 below 12.5: the golden values stay under it
//   damage *= 1 + (0 - 0) / 1000f                         the PvE ratios of two npcs
// so a base of 17 deals 10.

#include "EffectClassTestSupport.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/AttackCalcObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/effect/BleedEffect.h"
#include "aion/gameserver/skillengine/effect/SpellAttackInstantEffect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::effecttest {
namespace {

using effect::AbnormalState;
using model::Effect;
using model::EffectReserved;
using network::aion::serverpackets::SM_ABNORMAL_EFFECT;

constexpr int32_t NPC_MAX_HP = 2522; // EffectTestSupport.h's npc template

/** Counts the ATTACK, ATTACKED and DOT_ATTACKED notifications a creature's ObserveController hands its observers */
struct RecordingObserver final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	static Ref<RecordingObserver> create(controllers::observer::ObserverType type) { return runtime::makeRef<RecordingObserver>(type); }

	void attack(Creature& creature, int32_t skillId) override {
		attacks.push_back(skillId);
		lastCreature = creature.getObjectId();
	}

	void attacked(Creature& creature, int32_t skillId) override {
		attackeds.push_back(skillId);
		lastCreature = creature.getObjectId();
	}

	void dotattacked(Creature& creature, Effect& dotEffect) override {
		dotAttacks.push_back(dotEffect.getSkillId());
		lastCreature = creature.getObjectId();
	}

	std::vector<int32_t> attacks;
	std::vector<int32_t> attackeds;
	std::vector<int32_t> dotAttacks;
	int32_t lastCreature = 0;

protected:
	explicit RecordingObserver(controllers::observer::ObserverType type) : ActionObserver(type) {}
	~RecordingObserver() override = default;
};

/** An AttackCalcObserver of the effected standing in for a shield: it lets 3 of the damage through (as a ShieldEffect observer writes it) */
struct CappingShield final : controllers::observer::AttackCalcObserver {
	AION_MAKE_REF_FRIEND

	static Ref<CappingShield> create() { return runtime::makeRef<CappingShield>(); }

	void checkShield(const std::vector<Ptr<controllers::attack::AttackResult>>& attackList, Ptr<Effect> /*effect*/,
		Creature& /*attacker*/) override {
		++checks;
		attackList.at(0)->setDamage(3);
	}

	int32_t checks = 0;

protected:
	CappingShield() = default;
	~CappingShield() override = default;
};

/** A seed whose first Rnd.nextInt(1000) is below 500: a magical critical roll of critprobmod2 100000 passes (see the critical case below) */
uint64_t criticalSeed() {
	uint64_t seed = 1;
	for (;; ++seed) {
		Rnd::seedCurrentThreadForTests(seed);
		if (Rnd::nextInt(1000) < 500)
			return seed;
	}
}

/**
 * SpellAttackInstantEffect with DamageEffect's protected resolveMagicalCritical and its fields public, so a case can call the hook alone (a
 * data-only subclass: every body is DamageEffect's)
 */
class CriticalProbe final : public effect::SpellAttackInstantEffect {
public:
	using DamageEffect::resolveMagicalCritical;
	using EffectTemplate::critProbMod2;
	using EffectTemplate::element;
	using EffectTemplate::position;
};

/** A skill template without effects (the flags DamageEffect reads from it) */
std::string plainSkill(int32_t skillId, std::string_view extra) {
	return R"(<skill_template skill_id=")" + std::to_string(skillId) + R"(" name="plain" nameId="1" stack="EFFECTS_AL_PLAIN_)"
		+ std::to_string(skillId) + R"(" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK" activation="ACTIVE" duration="0" )" + std::string(extra)
		+ "/>";
}

/** A one-effect enemy skill; `activation` ACTIVE or PROVOKED */
std::string damageSkill(int32_t skillId, std::string_view activation, std::string_view effectXml) {
	return R"(<skill_template skill_id=")" + std::to_string(skillId) + R"(" name="damage" nameId="1" stack="EFFECTS_AL_DAMAGE_)"
		+ std::to_string(skillId) + R"(" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK" tslot="NONE" activation=")" + std::string(activation)
		+ R"(" duration="0"><properties first_target="TARGET" target_relation="ENEMY" target_type="ONLYONE"/><effects>)" + std::string(effectXml)
		+ R"(</effects></skill_template>)";
}

/** A one-effect debuff (tslot DEBUFF, so the effect has an icon slot) */
std::string debuffSkill(int32_t skillId, std::string_view effectXml) {
	return R"(<skill_template skill_id=")" + std::to_string(skillId) + R"(" name="debuff" nameId="1" stack="EFFECTS_AL_DEBUFF_)"
		+ std::to_string(skillId) + R"(" lvl="1" skilltype="PHYSICAL" skillsubtype="DEBUFF" tslot="DEBUFF" activation="ACTIVE" duration="0">)"
		+ R"(<properties first_target="TARGET" target_relation="ENEMY" target_type="ONLYONE"/><effects>)" + std::string(effectXml)
		+ R"(</effects></skill_template>)";
}

class DamageEffectsTest : public EffectClassTest {
protected:
	void TearDown() override {
		EffectClassTest::TearDown();
		if (npcDataPublished)
			dataholders::DataManager::NPC_DATA.resetForTests();
	}

	/**
	 * Publishes NPC_DATA with EffectTestSupport.h's template for an effector npc: a critical asks AttackUtil.getWeaponGroup, which reads the
	 * effector's NpcTemplate.getEquipment() from NPC_DATA (AttackUtil.java:539-544). The template has no equipment, so the group is null. NpcData's
	 * load (NpcStatCalculation) needs a rank, which the npc's own template does not.
	 */
	void publishNpcTemplate(int32_t npcId) {
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(npcDataContext,
			R"(<npc_templates><npc_template name_id="1" npc_id=")" + std::to_string(npcId) + R"(" level="4" name="Effect test")"
				R"( attack_speed="2000" arange="2" rank="NOVICE" rating="NORMAL" tribe="GENERAL"><stats maxHp="2522" maxMp="100" pdef="130")"
				R"( mdef="70" attack="16" evasion="45" parry="30" block="25" accuracy="200" macc="60" pcrit="10" mcrit="20"><speeds walk="0.8")"
				R"( run="2.0" run_fight="3.0" group_walk="0.5" group_run_fight="2.5" fly="4.0"/></stats></npc_template></npc_templates>)"));
		npcDataPublished = true;
	}

	xml::LoadContext npcDataContext;
	bool npcDataPublished = false;
};

// ---- DamageEffect -----------------------------------------------------------------------------------------------------------------------------

/**
 * DamageEffect.applyEffect (DamageEffect.java:28-40): the reserved damage of the template's position hits the effected through
 * CreatureController.onAttack with notifyAttack true. An ACTIVE skill is a TYPE.REGULAR/LOG.REGULAR hit and notifies the effector's ATTACK
 * observers; a PROVOKED one (a proc) is TYPE.DAMAGE/LOG.PROCATKINSTANT and notifies nobody. The template's hoptype reaches AggroList.addDamage:
 * DAMAGE adds calculateHate(effector, damage * 10) = damage * 10 * (1000 + BOOST_HATE 100) / 1000; a template without one adds the damage only.
 */
TEST_F(DamageEffectsTest, DamageEffectHitsWithTheReservedDamageOfItsPosition) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700701);
	cp::RecordingAionConnection& observer = observe(*npc, 6001);
	Ref<Player> attacker = players.back();
	Ref<RecordingObserver> attacks = RecordingObserver::create(controllers::observer::ObserverType::ATTACK);
	attacker->getObserveController()->addObserver(*attacks);
	const model::SkillTemplate* active =
		bindSkill(damageSkill(9801, "ACTIVE", R"(<spellatkinstant value="50" e="1" noresist="true" element="FIRE" hoptype="DAMAGE"/>)"));
	const model::SkillTemplate* provoked =
		bindSkill(damageSkill(9802, "PROVOKED", R"(<spellatkinstant value="50" e="1" noresist="true" element="FIRE"/>)"));
	ASSERT_EQ(effectOf(*active, 0).javaClassName(), "SpellAttackInstantEffect");
	// the result of calculate (AttackUtil.calculateSkillResult's setReserveds) set directly, so the case needs no damage formula
	auto hit = [&](const model::SkillTemplate* skill, int32_t damage) {
		Ref<Effect> effect = Effect::create(*attacker, Ptr<Creature>(*npc), skill, 1);
		effect->addSuccessEffect(&effectOf(*skill, 0));
		effect->setReserveds(*EffectReserved::create(1, damage, EffectReserved::ResourceType::HP, true), false);
		observer.clearSent();
		effect->applyEffect();
		return effect;
	};

	hit(active, 123);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP - 123);
	std::vector<AttackStatusFields> statuses = attackStatusesOf(observer, npc->getObjectId());
	ASSERT_EQ(statuses.size(), 1u);
	EXPECT_EQ(statuses[0].value, 123) << "TYPE.REGULAR writes the value itself";
	EXPECT_EQ(statuses[0].type, TYPE_REGULAR);
	EXPECT_EQ(statuses[0].skillId, 9801);
	EXPECT_EQ(statuses[0].log, LOG_REGULAR);
	EXPECT_EQ(statuses[0].critical, 0);
	EXPECT_EQ(attacks->attacks, std::vector<int32_t>{9801}) << "the effector's attack observers: notifyAttackObservers(effected, skillId)";
	EXPECT_EQ(attacks->lastCreature, npc->getObjectId());
	EXPECT_EQ(npc->getAggroList().getHate(*attacker), 1353) << "hoptype DAMAGE: 123 * 10 * (1000 + 100) / 1000";

	hit(provoked, 77);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP - 200);
	statuses = attackStatusesOf(observer, npc->getObjectId());
	ASSERT_EQ(statuses.size(), 1u);
	EXPECT_EQ(statuses[0].value, -77) << "TYPE.DAMAGE writes -value";
	EXPECT_EQ(statuses[0].type, TYPE_DAMAGE_OR_HP);
	EXPECT_EQ(statuses[0].log, LOG_PROCATKINSTANT);
	EXPECT_EQ(attacks->attacks.size(), 1u) << "a provoked skill notifies no attack observer";
	EXPECT_EQ(npc->getAggroList().getHate(*attacker), 1353) << "no hoptype: AggroList.addDamage adds no hate";
	attacker->getObserveController()->removeObserver(*attacks);
}

/**
 * DamageEffect.resolveMagicalCritical (DamageEffect.java:42-46): only an element other than NONE of a skill with apply_magical_critical rolls,
 * through Effect.rollMagicalCritical(position, calculateCritProbMod) - StatFunctions.calculateMagicalCriticalRate's single Rnd.nextInt(1000),
 * which a critprobmod2 of 100000 makes pass whenever the draw is below the MAGICAL_CRITICAL difference limit of 500.
 */
TEST_F(DamageEffectsTest, OnlyAnElementalDamageOfAMagicalCriticalSkillRollsTheCritical) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeNpc(700711);
	Ref<Player> caster = makePlayer(6011);
	const model::SkillTemplate* flagged = bindSkill(plainSkill(9811, R"(apply_magical_critical="true")"));
	const model::SkillTemplate* unflagged = bindSkill(plainSkill(9812, ""));
	// a seed whose first Rnd.nextInt(1000) is below 500: the roll passes, so a rolled critical is told from an unrolled one
	uint64_t seed = 1;
	for (;; ++seed) {
		Rnd::seedCurrentThreadForTests(seed);
		if (Rnd::nextInt(1000) < 500)
			break;
	}
	Rnd::seedCurrentThreadForTests(seed);
	Rnd::nextInt(1000);
	const float afterOneRoll = Rnd::chance();
	Rnd::seedCurrentThreadForTests(seed);
	const float withoutRoll = Rnd::chance();

	CriticalProbe fire;
	fire.element = gameserver::model::SkillElement::FIRE;
	fire.position = 1;
	fire.critProbMod2 = 100000;
	CriticalProbe physical;
	physical.element = gameserver::model::SkillElement::NONE;
	physical.position = 1;
	physical.critProbMod2 = 100000;

	Ref<Effect> rolled = Effect::create(*caster, Ptr<Creature>(*npc), flagged, 1);
	Rnd::seedCurrentThreadForTests(seed);
	fire.resolveMagicalCritical(*rolled);
	EXPECT_TRUE(rolled->isMagicalCritical(1)) << "FIRE and apply_magical_critical: rolled, and the draw passes";
	EXPECT_EQ(Rnd::chance(), afterOneRoll) << "exactly one draw";

	Ref<Effect> noFlag = Effect::create(*caster, Ptr<Creature>(*npc), unflagged, 1);
	Rnd::seedCurrentThreadForTests(seed);
	fire.resolveMagicalCritical(*noFlag);
	EXPECT_FALSE(noFlag->isMagicalCritical(1));
	EXPECT_EQ(Rnd::chance(), withoutRoll) << "no apply_magical_critical: no draw";

	Ref<Effect> noElement = Effect::create(*caster, Ptr<Creature>(*npc), flagged, 1);
	Rnd::seedCurrentThreadForTests(seed);
	physical.resolveMagicalCritical(*noElement);
	EXPECT_FALSE(noElement->isMagicalCritical(1));
	EXPECT_EQ(Rnd::chance(), withoutRoll) << "element NONE: no draw";
}

/**
 * DamageEffect.calculateDamage (DamageEffect.java:48-52) passes value + delta * skillLevel to AttackUtil.calculateSkillResult: 13 + 2 * 2 = 17,
 * which the npc effector turns into 10 on the npc effected (the file comment), reserved at the template's position and dealt by applyEffect.
 */
TEST_F(DamageEffectsTest, DamageEffectCalculatesValuePlusDeltaTimesTheLevelIntoTheSkillResult) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700721);
	Ref<Npc> caster = makeNpc(700722, {}, 510, 500);
	cp::RecordingAionConnection& observer = observe(*npc, 6021);
	const model::SkillTemplate* skill =
		bindSkill(damageSkill(9821, "ACTIVE", R"(<spellatkinstant value="13" delta="2" e="1" noresist="true" element="FIRE" hoptype="DAMAGE"/>)"));

	Ref<Effect> effect = Effect::create(*caster, Ptr<Creature>(*npc), skill, 2);
	effect->initialize();
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getReserveds(1)->getValue(), 10) << "(17 * 1.0f - 70 / 10f), no random share below 12.5";
	effect->applyEffect();
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP - 10);
	std::vector<AttackStatusFields> statuses = attackStatusesOf(observer, npc->getObjectId());
	ASSERT_EQ(statuses.size(), 1u);
	EXPECT_EQ(statuses[0].value, 10);
}

/**
 * DamageEffect.calculateDamage passes ignoreShield false (DamageEffect.java:48-52): AttackUtil.calculateEffectResult asks the effected's
 * AttackCalcObservers (ObserveController.checkShieldStatus, AttackUtil.java:387-404) before it reserves the damage, so a shield that lets 3 of the
 * previous case's 10 through leaves a reserve of 3, and the hit deals 3.
 */
TEST_F(DamageEffectsTest, TheDamageOfADamageEffectPassesTheShieldsOfTheEffected) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700801);
	Ref<Npc> caster = makeNpc(700802, {}, 510, 500);
	Ref<CappingShield> shield = CappingShield::create();
	npc->getObserveController()->addAttackCalcObserver(*shield);
	const model::SkillTemplate* skill =
		bindSkill(damageSkill(9891, "ACTIVE", R"(<spellatkinstant value="13" delta="2" e="1" noresist="true" element="FIRE" hoptype="DAMAGE"/>)"));

	Ref<Effect> effect = Effect::create(*caster, Ptr<Creature>(*npc), skill, 2);
	effect->initialize();
	EXPECT_EQ(shield->checks, 1) << "checkShieldStatus asked the shield";
	EXPECT_EQ(effect->getReserveds(1)->getValue(), 3) << "the shielded damage is reserved";
	effect->applyEffect();
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP - 3);
	npc->getObserveController()->removeAttackCalcObserver(*shield);
}

// ---- BleedEffect and AbstractOverTimeEffect -----------------------------------------------------------------------------------------------------

/** 17018 Bite's <bleed> (skill_templates.xml:127239) with a value under the random share, no critical (critprobmod2 0) and a 2000 ms check time */
constexpr const char* BLEED_XML = R"(<bleed checktime="2000" value="15" delta="1" duration2="6000" effectid="20006" e="1" noresist="true")"
								  R"( element="FIRE" critprobmod2="0" hoptype="DAMAGE" hopb="199" hopa="22"/>)";

/**
 * BleedEffect on an npc (BleedEffect.java:23-52, AbstractOverTimeEffect.java:33-67): startEffect reserves
 * calculateMagicalOverTimeSkillResult(value + delta * level = 15 + 1 * 2 = 17) = 10 (the file comment; the over-time path uses no knowledge and
 * no BOOST_SPELL_ATTACK), sets the BLEED abnormal state and starts the periodic task: the first tick 300 + checktime = 2300 ms after the start,
 * then every 2000 ms. The effect lasts duration2 + 1000 = 7000 ms (AbstractOverTimeEffect.getDuration2), so it ticks at 2300, 4300 and 6300 ms:
 * three hits of 10 as TYPE.DAMAGE/LOG.BLEED, each a DOT_ATTACKED notification of the effected's observers; then the end unsets BLEED.
 */
TEST_F(DamageEffectsTest, BleedTicksItsReservedDamageEveryCheckTimeUntilDurationTwoPlusOneSecond) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700731);
	Ref<Npc> caster = makeNpc(700732, {}, 510, 500);
	cp::RecordingAionConnection& observer = observe(*npc, 6031);
	Ref<RecordingObserver> dots = RecordingObserver::create(controllers::observer::ObserverType::DOT_ATTACKED);
	npc->getObserveController()->addObserver(*dots);
	const model::SkillTemplate* skill = bindSkill(debuffSkill(9831, BLEED_XML));
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "BleedEffect");
	EXPECT_EQ(effectOf(*skill, 0).getDuration2(), 7000) << "AbstractOverTimeEffect.getDuration2: 6000 + 1000";

	Ref<Effect> bleed = cast(*caster, *npc, skill, 2);
	ASSERT_TRUE(bleed->isInSuccessEffects(1));
	EXPECT_EQ(bleed->getDuration(), 7000);
	EXPECT_EQ(bleed->getReserveds(1)->getValue(), 10) << "(int) max(1, 17 * 1.0f - 70 / 10f)";
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::BLEED)) << "startEffect(effect, AbnormalState.BLEED)";
	EXPECT_NE(bleed->getAbnormals(), 0);
	std::vector<std::vector<uint8_t>> announced = packetsOf<SM_ABNORMAL_EFFECT>(observer);
	ASSERT_EQ(announced.size(), 1u);
	AbnormalEffectFields shown = decodeAbnormalEffect(announced[0]);
	EXPECT_EQ(shown.abnormals, npc->getEffectController()->getAbnormals());
	ASSERT_EQ(shown.effects.size(), 1u);
	EXPECT_EQ(shown.effects[0].skillId, 9831);
	EXPECT_EQ(shown.effects[0].remainingTime, 7000);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP) << "startEffect deals nothing";

	observer.clearSent();
	advance(2299);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP) << "not before 300 + checktime";
	advance(1);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP - 10) << "the first tick at 2300 ms";
	advance(1999);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP - 10);
	advance(1);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP - 20) << "every checktime after it";
	advance(2000);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP - 30) << "6300 ms";
	advance(699);
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::BLEED)) << "6999 ms: still bleeding";
	advance(1);
	EXPECT_TRUE(bleed->isEndedByTime());
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::BLEED)) << "endEffect(effect, AbnormalState.BLEED)";
	advance(10000);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP - 30) << "Effect.stopTasks cancelled the periodic task: three ticks in all";

	std::vector<AttackStatusFields> ticks = attackStatusesOf(observer, npc->getObjectId());
	ASSERT_EQ(ticks.size(), 3u);
	for (const AttackStatusFields& tick : ticks) {
		EXPECT_EQ(tick.value, -10);
		EXPECT_EQ(tick.type, TYPE_DAMAGE_OR_HP);
		EXPECT_EQ(tick.log, LOG_BLEED);
		EXPECT_EQ(tick.skillId, 9831);
		EXPECT_EQ(tick.critical, 0) << "isMagicalCritical(position) of a roll with critprobmod2 0";
	}
	EXPECT_EQ(dots->dotAttacks, (std::vector<int32_t>{9831, 9831, 9831})) << "notifyDotAttackedObservers after each hit";
	EXPECT_EQ(dots->lastCreature, caster->getObjectId()) << "the effector";
	EXPECT_EQ(packetsOf<SM_ABNORMAL_EFFECT>(observer).size(), 1u) << "the end announced the removed effect";
	npc->getObserveController()->removeObserver(*dots);
}

/**
 * BleedEffect.calculate asks EffectTemplate.calculate for BLEED_RESISTANCE (BleedEffect.java:28-31), so the effected's bleed resistance can
 * resist it; and resolveMagicalCritical rolls regardless of apply_magical_critical (BleedEffect.java:23-26: one draw even for a skill without the
 * flag).
 */
TEST_F(DamageEffectsTest, BleedIsResistedByBleedResistanceAndAlwaysRollsItsCritical) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeNpc(700741);
	Ref<Npc> caster = makeNpc(700742, {}, 510, 500);
	const model::SkillTemplate* resistible = bindSkill(debuffSkill(9841,
		R"(<bleed checktime="2000" value="15" duration2="6000" e="1" element="FIRE" critprobmod2="0"/>)"));
	addStat(*npc, gameserver::model::stats::container::StatEnum::BLEED_RESISTANCE, 1000);

	Ref<Effect> resisted = Effect::create(*caster, Ptr<Creature>(*npc), resistible, 1);
	effectOf(*resistible, 0).calculate(*resisted);
	EXPECT_FALSE(resisted->isInSuccessEffects(1)) << "effect power 1000 - 1000 BLEED_RESISTANCE: resisted";

	const model::SkillTemplate* unresisted = bindSkill(debuffSkill(9842, BLEED_XML));
	Rnd::seedCurrentThreadForTests(20260923);
	Rnd::nextInt(1000);
	const float afterOneRoll = Rnd::chance();
	Ref<Effect> rolled = Effect::create(*caster, Ptr<Creature>(*npc), unresisted, 1);
	Rnd::seedCurrentThreadForTests(20260923);
	effectOf(*unresisted, 0).calculate(*rolled);
	EXPECT_TRUE(rolled->isInSuccessEffects(1)) << "noresist: no resistance roll";
	EXPECT_EQ(Rnd::chance(), afterOneRoll) << "the critical roll of a skill without apply_magical_critical, and nothing else";
}

/**
 * The bleed's damage is fixed once, at startEffect, by calculateMagicalOverTimeSkillResult(effect, value + delta * level, this, false)
 * (BleedEffect.java:34-39): useMagicBoost false, so the effector's BOOST_MAGICAL_SKILL of 1000 does not count (with it, StatFunctions.java:388-394
 * would scale 17 by 1000 / 1000f + 100 / 100f). A critical that resolveMagicalCritical rolled (critprobmod2 100000, the seed's draw below the
 * MAGICAL_CRITICAL limit of 500) multiplies it by the elemental critical coefficient 1.5 (AttackUtil.java:439-444, calculateWeaponCritical with
 * no critadddmg): (int) (10 * 1.5f) = 15. Every tick sends it as a critical hit (onAttack's last argument is effect.isMagicalCritical(position)).
 * The reserve is `new EffectReserved(position, finalDamage, HP, true, false)` with overTimeEffect true: it is not sent with the cast result
 * (Effect.getReservedEffectsToSend answers only its placeholder of position 0, Effect.java:370-381) and leaves the HP snapshot alone
 * (Effect.setReserveds, :345-363: effectedHp stays -1).
 */
TEST_F(DamageEffectsTest, ABleedIgnoresTheMagicBoostAndTicksItsRolledCriticalAsACriticalHitWithoutSendingItsReserve) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700771);
	Ref<Npc> caster = makeNpc(700772, {}, 510, 500);
	cp::RecordingAionConnection& observer = observe(*npc, 6071);
	addStat(*caster, gameserver::model::stats::container::StatEnum::BOOST_MAGICAL_SKILL, 1000);
	publishNpcTemplate(700772);
	const model::SkillTemplate* skill = bindSkill(debuffSkill(9871, R"(<bleed checktime="2000" value="15" delta="1" duration2="6000" e="1")"
		R"( noresist="true" element="FIRE" critprobmod2="100000" hoptype="DAMAGE"/>)"));

	Rnd::seedCurrentThreadForTests(criticalSeed());
	Ref<Effect> bleed = cast(*caster, *npc, skill, 2);
	ASSERT_TRUE(bleed->isMagicalCritical(1)) << "the seed's roll passes";
	EXPECT_EQ(bleed->getReserveds(1)->getValue(), 15) << "(int) ((17 * 1.0f - 70 / 10f) * 1.5f): no magic boost";
	std::vector<Ref<EffectReserved>> toSend = bleed->getReservedEffectsToSend();
	ASSERT_EQ(toSend.size(), 1u);
	EXPECT_EQ(toSend[0]->getPosition(), 0) << "only the placeholder: the bleed's reserve is not sent";
	EXPECT_EQ(bleed->getEffectedHp(), -1) << "an over-time reserve takes no HP snapshot";
	observer.clearSent();
	advance(2300);
	std::vector<AttackStatusFields> ticks = attackStatusesOf(observer, npc->getObjectId());
	ASSERT_EQ(ticks.size(), 1u);
	EXPECT_EQ(ticks[0].value, -15);
	EXPECT_EQ(ticks[0].critical, CRITICAL_DISPLAY_CODE) << "the tick is shown as a critical hit";
	bleed->endEffect();
}

/**
 * BleedEffect.onPeriodicAction passes notifyAttack false (BleedEffect.java:49): a tick notifies none of the effected's ATTACKED observers (the
 * cancel-on-damage observers of a hide, the concentration check of a cast) and adds no hate, although the template's hoptype is DAMAGE, which a
 * notified hit turns into calculateHate(effector, damage * 10) (AggroList.java:47-50). The effector is a player, whom the monster is aware of.
 */
TEST_F(DamageEffectsTest, ABleedTickNotifiesNoAttackAndAddsNoHate) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700781);
	cp::RecordingAionConnection& observer = observe(*npc, 6081);
	Ref<Player> attacker = players.back();
	Ref<RecordingObserver> attacked = RecordingObserver::create(controllers::observer::ObserverType::ATTACKED);
	npc->getObserveController()->addObserver(*attacked);
	const model::SkillTemplate* skill = bindSkill(debuffSkill(9881, BLEED_XML));

	Ref<Effect> bleed = cast(*attacker, *npc, skill, 2);
	const int32_t hateAtStart = npc->getAggroList().getHate(*attacker);
	observer.clearSent();
	advance(2300);
	ASSERT_EQ(attackStatusesOf(observer, npc->getObjectId()).size(), 1u) << "the first tick";
	ASSERT_LT(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP);
	EXPECT_TRUE(attacked->attackeds.empty()) << "notifyAttack false: no ATTACKED notification";
	EXPECT_EQ(npc->getAggroList().getHate(*attacker), hateAtStart) << "notifyAttack false: the tick's damage adds no hate";
	bleed->endEffect();
	npc->getObserveController()->removeObserver(*attacked);
}

/** 17018 Bite, skill_templates.xml:127225-127238 verbatim (Scar, npc 210306 on Poeta, m5b2-plan.md §2.4(b)): <skillatk> at e=1, <bleed> at e=2 */
constexpr const char* BITE_XML =
	R"(<skill_template skill_id="17018" name="Bite" nameId="282789" cooldownId="1" stack="NAS_BLOWBLEED_BITE" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE")"
	R"( cooldown="0" duration="2500" cancel_rate="35" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="2" target_relation="ENEMY" target_type="ONLYONE" target_maxcount="1" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions><effects>)"
	R"(<skillatk mode="PERCENT" value="81" e="1" accmod2="0" hoptype="DAMAGE" />)"
	R"(<bleed checktime="3000" value="24" delta="1" duration2="12100" duration1="300" effectid="20006" e="2" noresist="true" element="FIRE")"
	R"( preeffect="1" hoptype="DAMAGE" hopb="199" hopa="22" /></effects><motion name="poweratk" /></skill_template>)";

/**
 * Each position reads its own reserve (Effect.getReserveds(position), BleedEffect.java:49): Bite's bleed at e=2 ticks what its startEffect
 * reserved at position 2, not the <skillatk> damage AttackUtil.calculateSkillResult reserved at position 1. Both values come from the seeded rolls;
 * the case needs only that they differ.
 */
TEST_F(DamageEffectsTest, BitesBleedTicksTheReserveOfItsOwnPositionNotTheSkillAttacks) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700791);
	Ref<Npc> scar = makeNpc(700792, {}, 510, 500);
	cp::RecordingAionConnection& observer = observe(*npc, 6091);
	const model::SkillTemplate* bite = bindSkill(BITE_XML);
	ASSERT_EQ(effectOf(*bite, 1).javaClassName(), "BleedEffect");

	Rnd::seedCurrentThreadForTests(20260923);
	Ref<Effect> effect = cast(*scar, *npc, bite, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1) && effect->isInSuccessEffects(2)) << "the seed lets the skill attack hit, and the bleed follows it";
	const int32_t bleedDamage = effect->getReserveds(2)->getValue();
	ASSERT_NE(effect->getReserveds(1)->getValue(), bleedDamage);
	observer.clearSent();
	advance(3300);
	std::vector<AttackStatusFields> ticks = attackStatusesOf(observer, npc->getObjectId());
	ASSERT_EQ(ticks.size(), 1u) << "the first tick at 300 + checktime";
	EXPECT_EQ(ticks[0].log, LOG_BLEED);
	EXPECT_EQ(ticks[0].value, -bleedDamage) << "the reserve of position 2";
	effect->endEffect();
}

/**
 * The int arithmetic of AbstractOverTimeEffect (m5b2-p2-10): getDuration2 is `duration2 + 1000` in Java int, so 2147483000 wraps to
 * 2147484000 - 2^32 = -2147483296 (the m5b2 skills oracle's `effectiveDuration2` for skill 1017), and Effect.calculateTemplateDuration then finds no
 * positive duration: the effect has no end task. The initial delay `300 + checktime` wraps the same way before it is widened to long, so a
 * checktime of Integer.MAX_VALUE schedules the first tick at a negative delay - at once.
 */
TEST_F(DamageEffectsTest, TheOverTimeDurationAndTheFirstTickDelayWrapLikeJavasIntSums) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeNpc(700751);
	Ref<Npc> caster = makeNpc(700752, {}, 510, 500);
	const model::SkillTemplate* endless = bindSkill(debuffSkill(9851,
		R"(<bleed checktime="2000" value="15" delta="1" duration2="2147483000" e="1" noresist="true" element="FIRE" critprobmod2="0"/>)"));
	const model::SkillTemplate* largest = bindSkill(debuffSkill(9852,
		R"(<bleed checktime="2000" value="15" duration2="2147482647" e="1" noresist="true" element="FIRE" critprobmod2="0"/>)"));
	const model::SkillTemplate* immediate = bindSkill(debuffSkill(9853,
		R"(<bleed checktime="2147483647" value="15" delta="1" duration2="6000" e="1" noresist="true" element="FIRE" critprobmod2="0"/>)"));
	EXPECT_EQ(effectOf(*endless, 0).getDuration2(), -2147483296) << "2147483000 + 1000 wraps";
	EXPECT_EQ(effectOf(*largest, 0).getDuration2(), std::numeric_limits<int32_t>::max()) << "2147482647 + 1000 does not";

	Ref<Effect> bleed = cast(*caster, *npc, endless, 2);
	EXPECT_EQ(bleed->getDuration(), 0) << "a negative first duration: calculateTemplateDuration answers 0, no end task";
	advance(2300);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP - 10) << "the periodic task runs all the same";
	bleed->endEffect();

	Ref<Effect> tickAtOnce = cast(*caster, *npc, immediate, 2);
	advance(1);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP - 20) << "300 + Integer.MAX_VALUE wraps negative: the first tick is due at once";
	advance(6999);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP - 20) << "and the next one is a period of Integer.MAX_VALUE away";
	EXPECT_TRUE(tickAtOnce->isEndedByTime());
}

/** checktime 0 (AbstractOverTimeEffect.java:49-51, "TODO figure out what to do with such cases"): the abnormal state is set, nothing ticks */
TEST_F(DamageEffectsTest, AnOverTimeEffectWithoutACheckTimeSetsItsAbnormalStateButNeverTicks) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeNpc(700761);
	Ref<Npc> caster = makeNpc(700762, {}, 510, 500);
	const model::SkillTemplate* skill = bindSkill(debuffSkill(9861,
		R"(<bleed checktime="0" value="15" duration2="6000" e="1" noresist="true" element="FIRE" critprobmod2="0"/>)"));

	Ref<Effect> bleed = cast(*caster, *npc, skill, 1);
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::BLEED));
	advance(6999);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP);
	advance(1);
	EXPECT_TRUE(bleed->isEndedByTime());
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::BLEED));
}

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
