// P5-03, M5b-2 stage 1 part 3, items F-02 and F-05 (m5b2-plan.md §5): the heal classes of the A-L half - the default methods of the interface
// HealEffectTemplate, AbstractHealEffect and its HealInstantEffect (<healinstant>, 245 Bandage Heal, on every character's bar), and
// HealOverTimeEffect with its HealEffect (<heal>, the post-spawn heals of D7 and 8751 Light of Repose's second position) - on real Effects, with
// the golden values of the heal arithmetic derived by hand from HealEffectTemplate.java:24-48, AbstractHealEffect.java:27-87 and
// HealOverTimeEffect.java:24-56.
//
// The effected player's MAXHP is raised by 1000 so that the missing HP never caps a value a row is about; the rows that are about the cap set
// the HP themselves.

#include "EffectClassTestSupport.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::effecttest {
namespace {

using effect::AbnormalState;
using gameserver::model::stats::container::StatEnum;
using model::Effect;
using network::aion::serverpackets::SM_ABNORMAL_EFFECT;
using network::aion::serverpackets::SM_ABNORMAL_STATE;

constexpr int32_t NPC_MAX_HP = 2522; // EffectTestSupport.h's npc template

/** 245 Bandage Heal, skill_templates.xml:3072-3085 verbatim: <healinstant value="187"> */
constexpr const char* BANDAGE_HEAL_XML =
	R"(<skill_template skill_id="245" name="Bandage Heal" nameId="2288150" cooldownId="1153" group="MENDING_L" stack="MENDING_L" lvl="1")"
	R"( skilltype="PHYSICAL" skill_category="HEAL" skillsubtype="HEAL" tslot="NONE" activation="ACTIVE" cooldown="160" duration="4000")"
	R"( cancel_rate="100000" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="ME" first_target_range="1" target_relation="FRIEND" target_type="ONLYONE" />)"
	R"(<useconditions><move_casting allow="false" /></useconditions><effects>)"
	R"(<healinstant value="187" e="1" noresist="true" element="WATER" hoptype="SKILLLV" hopb="390" />)"
	R"(</effects><actions><itemuse itemid="169300002" count="1" /></actions><motion name="mending" /></skill_template>)";

/** A heal skill with one effect; `extra` holds skill attributes such as apply_heal_boost_bonus */
std::string healSkill(int32_t skillId, std::string_view extra, std::string_view effectXml) {
	return R"(<skill_template skill_id=")" + std::to_string(skillId) + R"(" name="heal" nameId="1" stack="EFFECTS_AL_HEAL_)" + std::to_string(skillId)
		+ R"(" lvl="1" skilltype="MAGICAL" skillsubtype="HEAL" tslot="BUFF" activation="ACTIVE" duration="0" )" + std::string(extra)
		+ R"(><properties first_target="TARGETORME" target_relation="FRIEND" target_type="ONLYONE"/><effects>)" + std::string(effectXml)
		+ R"(</effects></skill_template>)";
}

class HealEffectsTest : public EffectClassTest {
protected:
	/**
	 * The reserved heal of position 1 after Effect.initialize (AbstractHealEffect.calculate: `setReserveds(new EffectReserved(position,
	 * calculateHealValue(effect, healType), ...))`) of a fresh caster on a fresh player with MAXHP + 1000 and 10 HP, after `setup(caster, target)`
	 */
	int32_t reservedHeal(const model::SkillTemplate* skill, int32_t level, const std::function<void(Player&, Player&)>& setup = {}) {
		Ref<Player> caster = makePlayer(nextId++);
		Ref<Player> target = makePlayer(nextId++);
		addStat(*target, StatEnum::MAXHP, 1000);
		target->getLifeStats()->setCurrentHp(10);
		if (setup)
			setup(*caster, *target);
		Ref<Effect> effect = Effect::create(*caster, Ptr<Creature>(*target), skill, level);
		effect->initialize();
		EXPECT_TRUE(effect->isInSuccessEffects(1));
		return effect->getReserveds(1)->getValue();
	}

	int32_t nextId = 7101;
};

// ---- HealInstantEffect -----------------------------------------------------------------------------------------------------------------------

/**
 * 245 Bandage Heal on the caster itself: calculate reserves 187 (no percent, no heal boost flag, no deboost function, far from the maximum) as a
 * heal of HP, and applyEffect adds it through CreatureLifeStats.increaseHp(TYPE.REGULAR, value, effector) - one SM_ATTACK_STATUS of
 * TYPE.REGULAR/LOG.REGULAR without a skill id to the healed player. An instant heal never enters the effect controller: no icon.
 */
TEST_F(HealEffectsTest, BandageHealAddsItsReservedHealToTheHp) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(7001);
	cp::RecordingAionConnection& client = connect(*player, *accounts.back());
	addStat(*player, StatEnum::MAXHP, 1000);
	player->getLifeStats()->setCurrentHp(10);
	const model::SkillTemplate* skill = bindSkill(BANDAGE_HEAL_XML);
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "HealInstantEffect");
	client.clearSent();

	Ref<Effect> effect = cast(*player, *player, skill, 1);
	EXPECT_EQ(effect->getReserveds(1)->getValue(), 187);
	EXPECT_EQ(effect->getReserveds(1)->getType(), model::EffectReserved::ResourceType::HP) << "ResourceType.of(HealType.HP)";
	EXPECT_FALSE(effect->getReserveds(1)->isDamage());
	EXPECT_EQ(player->getLifeStats()->getCurrentHp(), 197);
	std::vector<AttackStatusFields> statuses = attackStatusesOf(client, player->getObjectId());
	ASSERT_EQ(statuses.size(), 1u);
	EXPECT_EQ(statuses[0].value, 187);
	EXPECT_EQ(statuses[0].type, TYPE_REGULAR);
	EXPECT_EQ(statuses[0].skillId, 0) << "increaseHp(type, value, effector) passes skill id 0";
	EXPECT_EQ(statuses[0].log, LOG_REGULAR);
	EXPECT_TRUE(player->getEffectController()->getAllEffects().empty()) << "an instant heal is not added to the effect controller";
	EXPECT_TRUE(packetsOf<SM_ABNORMAL_STATE>(client).empty());
}

/**
 * AbstractHealEffect.calculateHealValue over HealEffectTemplate's defaults, row by row:
 *   snapshot   = percent ? maxHp * base / 100 : base, base = value + delta * level
 *   HP boost   = only with the skill's apply_heal_boost_bonus:
 *                snapshot += (int) (snapshot * clamp(HEAL_BOOST + HEAL_SKILL_BOOST - 1000, 0, 2000) / 1000f), both of the caster
 *   deboost    = max(0, the target's HEAL_SKILL_DEBOOST of the value) (always for an instant heal)
 *   result     = min(maxHp - currentHp, value), and 0 for a DISEASEd target
 */
TEST_F(HealEffectsTest, AnInstantHealIsTheSnapshotBoostedByTheCasterDeboostedByTheTargetAndCappedByTheMissingHp) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* plain = bindSkill(healSkill(9901, "", R"(<healinstant value="187" e="1" noresist="true"/>)"));
	const model::SkillTemplate* boosted =
		bindSkill(healSkill(9902, R"(apply_heal_boost_bonus="true")", R"(<healinstant value="187" e="1" noresist="true"/>)"));
	const model::SkillTemplate* leveled = bindSkill(healSkill(9903, "", R"(<healinstant value="100" delta="10" e="1" noresist="true"/>)"));
	const model::SkillTemplate* percent = bindSkill(healSkill(9904, "", R"(<healinstant percent="true" value="10" e="1" noresist="true"/>)"));
	const model::SkillTemplate* boosted90 =
		bindSkill(healSkill(9905, R"(apply_heal_boost_bonus="true")", R"(<healinstant value="90" e="1" noresist="true"/>)"));

	EXPECT_EQ(reservedHeal(plain, 1), 187);
	EXPECT_EQ(reservedHeal(leveled, 3), 130) << "100 + 10 * 3";
	EXPECT_EQ(reservedHeal(plain, 1, [](Player& caster, Player&) { addStat(caster, StatEnum::HEAL_BOOST, 150); }), 187)
		<< "without apply_heal_boost_bonus the caster's boost is not read";
	EXPECT_EQ(reservedHeal(boosted, 1, [](Player& caster, Player&) { addStat(caster, StatEnum::HEAL_BOOST, 150); }), 215)
		<< "187 + (int) (187 * 150 / 1000f) = 187 + 28";
	EXPECT_EQ(reservedHeal(boosted, 1,
				  [](Player& caster, Player&) {
					  addStat(caster, StatEnum::HEAL_BOOST, 150);
					  addStat(caster, StatEnum::HEAL_SKILL_BOOST, 100);
				  }),
		233)
		<< "HEAL_SKILL_BOOST counts above its base of 1000: 187 + (int) (187 * 250 / 1000f) = 187 + 46";
	EXPECT_EQ(reservedHeal(boosted, 1,
				  [](Player& caster, Player&) {
					  addStat(caster, StatEnum::HEAL_BOOST, 1000);
					  addStat(caster, StatEnum::HEAL_SKILL_BOOST, 1500);
				  }),
		561)
		<< "1000 + 1500 is clamped to 2000: 187 + 374";
	EXPECT_EQ(reservedHeal(boosted, 1, [](Player& caster, Player&) { addStat(caster, StatEnum::HEAL_SKILL_BOOST, -3000); }), 187)
		<< "0 + (-2000 - 1000) is clamped to 0";
	EXPECT_EQ(reservedHeal(boosted90, 1,
				  [](Player& caster, Player&) {
					  addStat(caster, StatEnum::HEAL_BOOST, 1000); // StatCapUtil caps HEAL_BOOST at 1000
					  addStat(caster, StatEnum::HEAL_SKILL_BOOST, 300);
				  }),
		207)
		<< "the int product is divided: 90 + (int) (90 * 1300 / 1000f) = 90 + 117; 90 * (1300 / 1000f) would be 116.99999 and give 116";
	EXPECT_EQ(reservedHeal(plain, 1, [](Player&, Player& target) { addStat(target, StatEnum::HEAL_SKILL_DEBOOST, -50); }), 137)
		<< "the target's HEAL_SKILL_DEBOOST of 187";
	EXPECT_EQ(reservedHeal(plain, 1, [](Player&, Player& target) { addStat(target, StatEnum::HEAL_SKILL_DEBOOST, -500); }), 0)
		<< "Math.max(0, 187 - 500)";
	EXPECT_EQ(reservedHeal(plain, 1, [](Player&, Player& target) { target.getLifeStats()->setCurrentHp(target.getLifeStats()->getMaxHp() - 100); }),
		100)
		<< "Math.min(cap = maxHp - currentHp, 187)";
	EXPECT_EQ(reservedHeal(plain, 1, [](Player&, Player& target) { target.getEffectController()->setAbnormal(AbnormalState::DISEASE); }), 0)
		<< "a DISEASEd target is healed by 0";

	int32_t maxHp = 0;
	const int32_t percentHeal = reservedHeal(percent, 1, [&maxHp](Player&, Player& target) { maxHp = target.getLifeStats()->getMaxHp(); });
	EXPECT_EQ(percentHeal, maxHp * 10 / 100) << "percent: maxHp * value / 100 in int arithmetic";
}

/**
 * A heal whose EffectTemplate.calculate fails - here the effect's own <conditions> (AbnormalStateCondition: the effected must be stunned) - is no
 * success and reserves nothing: AbstractHealEffect.calculate and HealOverTimeEffect.calculate both return on `!super.calculate(effect, null,
 * null)` (AbstractHealEffect.java:27-31, HealOverTimeEffect.java:24-29). The same heals on a stunned player do succeed.
 */
TEST_F(HealEffectsTest, AHealWhoseCalculateFailsIsNoSuccessAndReservesNothing) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* instant = bindSkill(healSkill(9931, "",
		R"(<healinstant value="187" e="1" noresist="true"><conditions><abnormal value="STUN"/></conditions></healinstant>)"));
	const model::SkillTemplate* overTime = bindSkill(healSkill(9932, "",
		R"(<heal checktime="2000" value="30" duration2="6000" e="1" noresist="true"><conditions><abnormal value="STUN"/></conditions></heal>)"));
	Ref<Player> player = makePlayer(7301);
	addStat(*player, StatEnum::MAXHP, 1000);
	player->getLifeStats()->setCurrentHp(10);

	Ref<Effect> failedInstant = Effect::create(*player, Ptr<Creature>(*player), instant, 1);
	failedInstant->initialize();
	EXPECT_FALSE(failedInstant->isInSuccessEffects(1));
	EXPECT_EQ(failedInstant->getReserveds(1)->getPosition(), 0) << "no reserve of position 1: getReserveds answers its placeholder";
	EXPECT_EQ(failedInstant->getReserveds(1)->getValue(), 0);
	Ref<Effect> failedOverTime = Effect::create(*player, Ptr<Creature>(*player), overTime, 1);
	failedOverTime->initialize();
	EXPECT_FALSE(failedOverTime->isInSuccessEffects(1)) << "HealOverTimeEffect.calculate returns before its addSuccessEffect";
	failedOverTime->applyEffect();
	EXPECT_TRUE(player->getEffectController()->getAllEffects().empty());

	player->getEffectController()->setAbnormal(AbnormalState::STUN);
	Ref<Effect> stunnedInstant = Effect::create(*player, Ptr<Creature>(*player), instant, 1);
	stunnedInstant->initialize();
	ASSERT_TRUE(stunnedInstant->isInSuccessEffects(1)) << "the condition is what failed";
	ASSERT_EQ(stunnedInstant->getReserveds(1)->getValue(), 187);
	Ref<Effect> stunnedOverTime = Effect::create(*player, Ptr<Creature>(*player), overTime, 1);
	stunnedOverTime->initialize();
	ASSERT_TRUE(stunnedOverTime->isInSuccessEffects(1));
}

/**
 * Every heal reads the reserve of its own position (Effect.getReserveds(position): AbstractHealEffect.java:34, HealOverTimeEffect.java:42). An
 * instant heal at e=2 behind one at e=1 adds its own 40 to the other's 100; a heal over time at e=2 behind an instant heal of 100 ticks its own 30.
 */
TEST_F(HealEffectsTest, EachHealReadsTheReserveOfItsOwnPosition) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* twoInstants = bindSkill(
		healSkill(9933, "", R"(<healinstant value="100" e="1" noresist="true"/><healinstant value="40" e="2" noresist="true"/>)"));
	const model::SkillTemplate* instantThenOverTime = bindSkill(healSkill(9934, "",
		R"(<healinstant value="100" e="1" noresist="true"/><heal checktime="2000" value="30" duration2="6000" e="2" noresist="true"/>)"));
	Ref<Player> player = makePlayer(7311);
	addStat(*player, StatEnum::MAXHP, 1000);
	player->getLifeStats()->setCurrentHp(10);

	cast(*player, *player, twoInstants, 1);
	EXPECT_EQ(player->getLifeStats()->getCurrentHp(), 150) << "10 + 100 + 40";

	Ref<Effect> overTime = cast(*player, *player, instantThenOverTime, 1);
	ASSERT_EQ(player->getLifeStats()->getCurrentHp(), 250) << "the instant heal of e=1";
	advance(2300);
	EXPECT_EQ(player->getLifeStats()->getCurrentHp(), 280) << "the first tick of e=2: its own snapshot of 30";
	overTime->endEffect();
}

// ---- HealOverTimeEffect and HealEffect ------------------------------------------------------------------------------------------------------

/**
 * <heal> on an npc (the post-spawn heals of D7): startEffect reserves the snapshot value + delta * level = 20 + 5 * 2 = 30 once
 * (HealOverTimeEffect.java:32-35), and every tick heals min(maxHp - currentHp, deboosted snapshot) through increaseHp(TYPE.HP, value, effect,
 * LOG.HEAL) (:37-56): SM_ATTACK_STATUS of TYPE.HP/LOG.HEAL with the skill id to the players who see the npc. The ticks are those of
 * AbstractOverTimeEffect: 2300, 4300 and 6300 ms for checktime 2000 and duration2 6000 (+ 1000). Near the maximum the missing HP caps the
 * value, and a tick of 0 is still announced (increaseHp sends for a skill id even without a change).
 */
TEST_F(HealEffectsTest, AHealOverTimeHealsItsSnapshotEveryCheckTimeCappedByTheMissingHp) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700901);
	cp::RecordingAionConnection& observer = observe(*npc, 7201);
	const model::SkillTemplate* skill =
		bindSkill(healSkill(9911, "", R"(<heal checktime="2000" value="20" delta="5" duration2="6000" e="1" noresist="true"/>)"));
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "HealEffect");
	npc->getLifeStats()->setCurrentHp(2400);
	observer.clearSent();

	Ref<Effect> heal = cast(*npc, *npc, skill, 2);
	EXPECT_EQ(heal->getDuration(), 7000) << "duration2 + 1000";
	EXPECT_EQ(heal->getReserveds(1)->getValue(), 30) << "the snapshot: 20 + 5 * 2";
	EXPECT_FALSE(heal->getReserveds(1)->isDamage());
	std::vector<std::vector<uint8_t>> announced = packetsOf<SM_ABNORMAL_EFFECT>(observer);
	ASSERT_EQ(announced.size(), 1u);
	ASSERT_EQ(decodeAbnormalEffect(announced[0]).effects.size(), 1u);
	EXPECT_EQ(decodeAbnormalEffect(announced[0]).effects[0].skillId, 9911);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 2400) << "startEffect heals nothing";

	observer.clearSent();
	advance(2299);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 2400);
	advance(1);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 2430);
	advance(4000);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 2490) << "4300 and 6300 ms";
	advance(700);
	EXPECT_TRUE(heal->isEndedByTime());
	std::vector<AttackStatusFields> ticks = attackStatusesOf(observer, npc->getObjectId());
	ASSERT_EQ(ticks.size(), 3u);
	for (const AttackStatusFields& tick : ticks) {
		EXPECT_EQ(tick.value, 30);
		EXPECT_EQ(tick.type, TYPE_DAMAGE_OR_HP) << "TYPE.HP";
		EXPECT_EQ(tick.log, LOG_HEAL);
		EXPECT_EQ(tick.skillId, 9911);
	}

	npc->getLifeStats()->setCurrentHp(NPC_MAX_HP - 50);
	observer.clearSent();
	cast(*npc, *npc, skill, 2);
	advance(7000);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP);
	ticks = attackStatusesOf(observer, npc->getObjectId());
	ASSERT_EQ(ticks.size(), 3u);
	EXPECT_EQ(ticks[0].value, 30);
	EXPECT_EQ(ticks[1].value, 20) << "Math.min(maxHp - currentHp = 20, 30)";
	EXPECT_EQ(ticks[2].value, 0) << "a full npc is healed by 0, and told so";
}

/**
 * A percent heal over time with the heal boost flag (8751 Light of Repose's <heal percent="true" value="1">): the snapshot is maxHp * 1 / 100 =
 * 2522 / 100 = 25, boosted by the caster's HEAL_BOOST 200 to 25 + (int) (25 * 200 / 1000f) = 30; each tick deboosts the snapshot by the
 * effected's HEAL_SKILL_DEBOOST (the flag allows it for a heal over time, HealOverTimeEffect.java:68-71): 30 - 10 = 20.
 */
TEST_F(HealEffectsTest, APercentHealOverTimeIsBoostedAtTheStartAndDeboostedAtEveryTick) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700911);
	cp::RecordingAionConnection& observer = observe(*npc, 7211);
	const model::SkillTemplate* skill = bindSkill(healSkill(9921, R"(apply_heal_boost_bonus="true")",
		R"(<heal percent="true" checktime="2000" value="1" duration2="12000" e="1" noresist="true"/>)"));
	addStat(*npc, StatEnum::HEAL_BOOST, 200);
	npc->getLifeStats()->setCurrentHp(1000);

	Ref<Effect> heal = cast(*npc, *npc, skill, 1);
	EXPECT_EQ(heal->getReserveds(1)->getValue(), 30) << "2522 * 1 / 100 = 25, + (int) (25 * 200 / 1000f)";
	addStat(*npc, StatEnum::HEAL_SKILL_DEBOOST, -10);
	observer.clearSent();
	advance(2300);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 1020) << "the deboost is read at the tick: 30 - 10";
	std::vector<AttackStatusFields> ticks = attackStatusesOf(observer, npc->getObjectId());
	ASSERT_EQ(ticks.size(), 1u);
	EXPECT_EQ(ticks[0].value, 20);
	heal->endEffect();
}

/**
 * The split between the start and the tick, with the effected's HEAL_SKILL_DEBOOST -10 already there at the cast: startEffect reserves the
 * snapshot (calculateSnapshotHealValue, HealOverTimeEffect.java:32-35), which the target's deboost does not touch, and each tick deboosts it
 * (:44-45) - but only for a skill with apply_heal_boost_bonus, the flag allowHpHealSkillDeboost answers for a heal over time (:68-71; an instant
 * heal always deboosts). So the flagged heal reserves 30 and ticks 20, the plain one reserves 30 and ticks 30. The reserve is
 * `new EffectReserved(position, value, type, false, false)` with overTimeEffect true: not sent with the cast result, no HP snapshot.
 */
TEST_F(HealEffectsTest, AHealOverTimeReservesItsUndeboostedSnapshotAndDeboostsItsTicksOnlyWithTheBoostFlag) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700921);
	cp::RecordingAionConnection& observer = observe(*npc, 7221);
	const model::SkillTemplate* flagged = bindSkill(
		healSkill(9941, R"(apply_heal_boost_bonus="true")", R"(<heal checktime="2000" value="30" duration2="6000" e="1" noresist="true"/>)"));
	const model::SkillTemplate* plain = bindSkill(healSkill(9942, "", R"(<heal checktime="2000" value="30" duration2="6000" e="1" noresist="true"/>)"));
	addStat(*npc, StatEnum::HEAL_SKILL_DEBOOST, -10);
	npc->getLifeStats()->setCurrentHp(1000);

	Ref<Effect> heal = cast(*npc, *npc, flagged, 1);
	EXPECT_EQ(heal->getReserveds(1)->getValue(), 30) << "the snapshot: no deboost at the start";
	std::vector<Ref<model::EffectReserved>> toSend = heal->getReservedEffectsToSend();
	ASSERT_EQ(toSend.size(), 1u);
	EXPECT_EQ(toSend[0]->getPosition(), 0) << "only the placeholder: the heal's reserve is not sent";
	EXPECT_EQ(heal->getEffectedHp(), -1) << "an over-time reserve takes no HP snapshot";
	observer.clearSent();
	advance(2300);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 1020) << "the tick deboosts the snapshot: 30 - 10";
	heal->endEffect();

	Ref<Effect> plainHeal = cast(*npc, *npc, plain, 1);
	advance(2300);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 1050) << "no apply_heal_boost_bonus: the tick is not deboosted";
	plainHeal->endEffect();
}

/**
 * The MP arm of HealOverTimeEffect.onPeriodicAction (HealOverTimeEffect.java:49) through <mpheal> (MPHealEffect, P5-04; 8751 Light of Repose's
 * third position, which CuringZoneService casts once a second): each tick is increaseMp(TYPE.MP, value, skillId, LOG.MPHEAL) - SM_ATTACK_STATUS of
 * TYPE.MP/LOG.MPHEAL with the skill id to the healed player.
 */
TEST_F(HealEffectsTest, AnMpHealOverTimeTicksAsTypeMpWithItsSkillId) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(7321);
	cp::RecordingAionConnection& client = connect(*player, *accounts.back());
	const model::SkillTemplate* skill =
		bindSkill(healSkill(9951, "", R"(<mpheal checktime="3000" value="31" duration2="10000" e="1" noresist="true"/>)"));
	ASSERT_EQ(effectOf(*skill, 0).javaClassName(), "MPHealEffect");
	player->getLifeStats()->setCurrentMp(10);

	Ref<Effect> heal = cast(*player, *player, skill, 1);
	client.clearSent();
	advance(3300);
	EXPECT_EQ(player->getLifeStats()->getCurrentMp(), 41);
	std::vector<AttackStatusFields> ticks = attackStatusesOf(client, player->getObjectId());
	ASSERT_EQ(ticks.size(), 1u);
	EXPECT_EQ(ticks[0].value, 31);
	EXPECT_EQ(ticks[0].type, TYPE_MP);
	EXPECT_EQ(ticks[0].log, LOG_MPHEAL);
	EXPECT_EQ(ticks[0].skillId, 9951);
	heal->endEffect();
}

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
