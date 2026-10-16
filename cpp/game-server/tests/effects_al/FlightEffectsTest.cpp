// The flight effects of P5-03 / P5-04 (M5b-2's "flight" group, ported 2026-10-02 on the owner's request): FP heals (FPHealEffect,
// FPHealInstantEffect, ProcFPHealInstantEffect), FP attacks (FpAttackEffect, DelayedFpAtkInstantEffect), the flight states (NoFlyEffect,
// InvulnerableWingEffect, OpenAerialEffect, CloseAerialEffect) and FlyoffEffect (an empty Java body). Each case binds the data template
// that uses the effect (skill_templates.xml, cut to its effects) and runs it on the DaevaEffectTest fixture: a case that reaches an
// AION_UNPORTED site fails in TearDown, so a green case ran through ported code only.

#include "DaevaEffectsTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/FlyState.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORCED_MOVE.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::effecttest {
namespace {

using effect::AbnormalState;
using gameserver::model::PlayerClass;
using gameserver::model::gameobjects::state::CreatureState;
using gameserver::model::gameobjects::state::FlyState;
using gameserver::model::stats::container::StatEnum;
using model::Effect;
using network::aion::serverpackets::SM_EMOTION;

constexpr int32_t TYPE_FP_DAMAGE = 26; // SM_ATTACK_STATUS.TYPE.FP_DAMAGE
constexpr int32_t LOG_FPATTACK = 137;

// ---- the skills of the data (skill_templates.xml), cut to their effects -----------------------------------------------------------------------

/** 260 Wings of Aether (:3259-3271): an instant FP heal of 7 (FPHealInstantEffect) */
const std::string WINGS_OF_AETHER_XML = templateXml(
	R"(skill_id="260" name="Wings of Aether" nameId="288149" stack="Q_SPEEDUP" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF")"
	R"( activation="ACTIVE" cooldown="10" duration="0")",
	R"(<fphealinstant value="7" e="1" noresist="true" element="WIND" />)");

/** 8242 Aura: Gliding I (:80228-80244): the chant's FP heal of 1 (ProcFPHealInstantEffect) */
const std::string AURA_GLIDING_XML = templateXml(
	R"(skill_id="8242" name="Aura: Gliding I" nameId="282697" stack="CH_FLY_LIGHTBODYAURAEFFECT" lvl="1" skilltype="MAGICAL" skillsubtype="NONE")"
	R"( tslot="CHANT" activation="PROVOKED" cooldown="0" duration="0")",
	R"(<procfphealinstant value="1" e="1" noresist="true" element="FIRE" />)");

/** 4006 Splendor of Flight (:68164-68173): an FP heal of 2 every 3 s for 15 s (FPHealEffect) */
const std::string SPLENDOR_OF_FLIGHT_XML = templateXml(
	R"(skill_id="4006" name="Splendor of Flight" nameId="2286245" stack="PR_CIRCLEOFWINGS" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
	R"( tslot="BUFF" activation="ACTIVE" cooldown="1800" duration="0")",
	R"(<fpheal checktime="3000" value="2" duration2="15000" effectid="939401" e="1" basiclvl="10" noresist="true" />)");

/** 2004 Aerial Fury (:30886-30910), its FP drain only: 2 FP every 3 s for 15 s (FpAttackEffect) */
const std::string AERIAL_FURY_XML = templateXml(
	R"(skill_id="2004" name="Aerial Fury" nameId="2286556" stack="GU_WINGBLASTER_1" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK" tslot="DEBUFF")"
	R"( activation="ACTIVE" cooldown="0" duration="0" hostile_type="DIRECT")",
	R"(<fpatk checktime="3000" value="2" duration2="15000" effectid="106576" e="1" noresist="true" element="WIND" />)");

/** 17088 Wing Petrification (:128353-128366), a monster's: 10 s later 90 % of the flight time (DelayedFpAtkInstantEffect) */
const std::string WING_PETRIFICATION_XML = templateXml(
	R"(skill_id="17088" name="Wing Petrification" nameId="289313" stack="NAR_DELAYEDFPATK_NR" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK")"
	R"( tslot="NONE" activation="ACTIVE" cooldown="0" duration="500" hostile_type="DIRECT")",
	R"(<delayedfpatk_instant delay="10000" percent="true" value="90" e="1" noresist="true" element="EARTH" />)");

/** 242 Drakan Transformation (:3036-3045), its no-flight state only (NoFlyEffect) */
const std::string DRAKAN_NOFLY_XML = templateXml(
	R"(skill_id="242" name="Drakan Transformation" nameId="295747" stack="Q_POLYMORPH_DRAKAN_02" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
	R"( tslot="BUFF" activation="ACTIVE" cooldown="600" duration="0")",
	R"(<nofly duration2="60000" effectid="20109" e="1" noresist="true" element="WIND" />)");

/** 3128 Prayer of Freedom (:52826-52843), its invulnerable wings only (InvulnerableWingEffect) */
const std::string PRAYER_OF_FREEDOM_XML = templateXml(
	R"(skill_id="3128" name="Prayer of Freedom" nameId="2287031" stack="KN_PURIFYWING" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF")"
	R"( activation="ACTIVE" cooldown="3000" duration="0")",
	R"(<invulnerablewing duration2="120000" effectid="105372" e="1" noresist="true" />)");

/** 332 Tethering Strike (:4161-4177), its knock-up only (OpenAerialEffect) */
const std::string TETHERING_STRIKE_XML = templateXml(
	R"(skill_id="332" name="Tethering Strike" nameId="2285611" stack="IDSWEEP_FI4_AERIAL" lvl="1" skilltype="PHYSICAL" skillsubtype="ATTACK")"
	R"( tslot="DEBUFF" activation="ACTIVE" cooldown="80" duration="0" hostile_type="INDIRECT")",
	R"(<openaerial duration2="4000" effectid="20003" e="1" noresist="true" element="EARTH" />)");

/** 508 Crashing Blow (:5390-5409), its close-aerial only (CloseAerialEffect: removes the effect of skill 8224) */
const std::string CRASHING_BLOW_XML = templateXml(
	R"(skill_id="508" name="Crashing Blow" nameId="2287770" stack="FI_DROPIMPACT" lvl="1" skilltype="PHYSICAL" skillsubtype="ATTACK" tslot="NONE")"
	R"( activation="ACTIVE" cooldown="600" duration="0" hostile_type="DIRECT")",
	R"(<closeaerial e="1" noresist="true" element="FIRE" />)");

/** 8224, the skill whose effect CloseAerialEffect removes; a stand-in buff for this case (its data row is not part of the test) */
const std::string SKILL_8224_XML = templateXml(
	R"(skill_id="8224" name="aerial" nameId="1" stack="TEST_AERIAL_8224" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF")"
	R"( activation="ACTIVE" cooldown="0" duration="0")",
	R"(<invulnerablewing duration2="60000" e="1" noresist="true" />)");

class FlightEffectsTest : public DaevaEffectTest {
protected:
	/** A Daeva with 60 seconds of flight time (gameserver.base.flytime is unbound in this binary: a FLY_TIME bonus) and `fp` of it left */
	Ref<Player> flyer(int32_t objectId, int32_t fp) {
		Ref<Player> player = daeva(objectId, PlayerClass::SORCERER);
		addStat(*player, StatEnum::FLY_TIME, 60);
		player->getLifeStats()->setCurrentFp(fp);
		return player;
	}

	static void fly(Player& player) {
		player.setFlyState(FlyState::FLYING);
		player.setState(CreatureState::FLYING);
	}
};

// ---- FP heals ---------------------------------------------------------------------------------------------------------------------------------

TEST_F(FlightEffectsTest, TheInstantFpHealsOfASkillAndOfAChantAddTheirValue) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* wings = bindSkill(WINGS_OF_AETHER_XML);
	const model::SkillTemplate* aura = bindSkill(AURA_GLIDING_XML);
	ASSERT_EQ(effectOf(*wings, 0).javaClassName(), "FPHealInstantEffect");
	ASSERT_EQ(effectOf(*aura, 0).javaClassName(), "ProcFPHealInstantEffect");
	Ref<Player> player = flyer(8601, 10);
	cp::RecordingAionConnection& client = connectLast(*player);
	client.clearSent();

	cast(*player, *player, wings, 1); // AbstractHealEffect.applyEffect's FP arm: increaseFp(TYPE.FP_RINGS, value)
	EXPECT_EQ(player->getLifeStats()->getCurrentFp(), 17);
	cast(*player, *player, aura, 1);
	EXPECT_EQ(player->getLifeStats()->getCurrentFp(), 18);
	std::vector<AttackStatusFields> statuses = attackStatusesOf(client, player->getObjectId());
	ASSERT_EQ(statuses.size(), 2u);
	EXPECT_EQ(statuses[0].value, 7);
	EXPECT_EQ(statuses[1].value, 1);
}

TEST_F(FlightEffectsTest, AnFpHealOverTimeTicksWithItsSkillId) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* splendor = bindSkill(SPLENDOR_OF_FLIGHT_XML);
	ASSERT_EQ(effectOf(*splendor, 0).javaClassName(), "FPHealEffect");
	Ref<Player> player = flyer(8602, 10);
	cp::RecordingAionConnection& client = connectLast(*player);

	Ref<Effect> heal = cast(*player, *player, splendor, 1);
	client.clearSent();
	advance(3300); // HealOverTimeEffect.onPeriodicAction's FP arm: increaseFp(TYPE.FP, value, skillId, LOG.FPHEAL)
	EXPECT_EQ(player->getLifeStats()->getCurrentFp(), 12);
	std::vector<AttackStatusFields> ticks = attackStatusesOf(client, player->getObjectId());
	ASSERT_EQ(ticks.size(), 1u);
	EXPECT_EQ(ticks[0].value, 2);
	EXPECT_EQ(ticks[0].skillId, 4006);
	heal->endEffect();
}

TEST_F(FlightEffectsTest, AnFpHealOnAMonsterDoesNothing) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* wings = bindSkill(WINGS_OF_AETHER_XML);
	Ref<Npc> npc = makeMonster(701601);
	Ref<Effect> effect = forced(*npc, *npc, wings);
	EXPECT_EQ(npc->getLifeStats()->getCurrentFp(), 0) << "AbstractHealEffect: only a player has FP";
	(void)effect;
}

// ---- FP attacks -------------------------------------------------------------------------------------------------------------------------------

TEST_F(FlightEffectsTest, AerialFuryDrainsFpEveryThreeSeconds) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* fury = bindSkill(AERIAL_FURY_XML);
	ASSERT_EQ(effectOf(*fury, 0).javaClassName(), "FpAttackEffect");
	Ref<Npc> gunner = makeMonster(701602, 510, 500);
	Ref<Player> player = flyer(8603, 60);
	cp::RecordingAionConnection& client = connectLast(*player);

	Ref<Effect> drain = forced(*gunner, *player, fury);
	ASSERT_TRUE(drain->isInSuccessEffects(1));
	client.clearSent();
	advance(3300); // FpAttackEffect.onPeriodicAction: reduceFp(TYPE.FP_DAMAGE, value, skillId, LOG.FPATTACK)
	EXPECT_EQ(player->getLifeStats()->getCurrentFp(), 58);
	std::vector<AttackStatusFields> statuses = attackStatusesOf(client, player->getObjectId());
	const auto fp = std::find_if(statuses.begin(), statuses.end(), [](const AttackStatusFields& s) { return s.log == LOG_FPATTACK; });
	ASSERT_NE(fp, statuses.end());
	EXPECT_EQ(fp->type, TYPE_FP_DAMAGE);
	EXPECT_EQ(fp->skillId, 2004);
	drain->endEffect();

	Ref<Npc> other = makeMonster(701603);
	EXPECT_FALSE(forced(*gunner, *other, fury)->isInSuccessEffects(1)) << "FpAttackEffect.calculate: only players have FP";
}

TEST_F(FlightEffectsTest, WingPetrificationTakesNinetyPercentOfTheFlightTimeTenSecondsLater) {
	EFFECT_TEST_SCOPE;
	publishTribeRelationsWithNeutral();
	const model::SkillTemplate* petrification = bindSkill(WING_PETRIFICATION_XML);
	ASSERT_EQ(effectOf(*petrification, 0).javaClassName(), "DelayedFpAtkInstantEffect");
	Ref<Npc> monster = makeMonster(701604, 510, 500);
	Ref<Player> player = flyer(8604, 60);
	ASSERT_TRUE(monster->isEnemy(*player));

	Ref<Effect> effect = forced(*monster, *player, petrification);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	advance(9900);
	EXPECT_EQ(player->getLifeStats()->getCurrentFp(), 60) << "the hit waits for its delay";
	advance(200); // percent: (maxFP * value) / 100 = 60 * 90 / 100 = 54
	EXPECT_EQ(player->getLifeStats()->getCurrentFp(), 6);
}

// ---- flight states ----------------------------------------------------------------------------------------------------------------------------

TEST_F(FlightEffectsTest, NoFlyLandsTheFlyerUntilItEnds) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* nofly = bindSkill(DRAKAN_NOFLY_XML);
	ASSERT_EQ(effectOf(*nofly, 0).javaClassName(), "NoFlyEffect");
	Ref<Player> player = flyer(8605, 60);
	fly(*player);

	Ref<Effect> effect = cast(*player, *player, nofly, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_FALSE(player->isInFlyState(FlyState::FLYING)) << "NoFlyEffect.startEffect: FlyController.endFly(true)";
	EXPECT_TRUE(player->getEffectController()->isAbnormalSet(AbnormalState::NOFLY));
	effect->endEffect();
	EXPECT_FALSE(player->getEffectController()->isAbnormalSet(AbnormalState::NOFLY));
}

TEST_F(FlightEffectsTest, InvulnerableWingsProtectFromNoFlyUntilTheyEnd) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* prayer = bindSkill(PRAYER_OF_FREEDOM_XML);
	const model::SkillTemplate* nofly = bindSkill(DRAKAN_NOFLY_XML);
	ASSERT_EQ(effectOf(*prayer, 0).javaClassName(), "InvulnerableWingEffect");
	Ref<Player> player = flyer(8606, 60);

	Ref<Effect> wings = cast(*player, *player, prayer, 1);
	ASSERT_TRUE(wings->isInSuccessEffects(1));
	EXPECT_TRUE(player->getEffectController()->isAbnormalSet(AbnormalState::INVULNERABLE_WING));

	fly(*player);
	Ref<Effect> grounded = cast(*player, *player, nofly, 1);
	EXPECT_FALSE(grounded->isInSuccessEffects(1)) << "NoFlyEffect.isDodgedOrResisted: INVULNERABLE_WING, although noresist";
	EXPECT_TRUE(player->isInFlyState(FlyState::FLYING));

	wings->endEffect();
	EXPECT_FALSE(player->getEffectController()->isAbnormalSet(AbnormalState::INVULNERABLE_WING));

	Ref<Npc> npc = makeMonster(701606);
	EXPECT_FALSE(forced(*npc, *npc, prayer)->isInSuccessEffects(1)) << "InvulnerableWingEffect.calculate: only for players";
}

TEST_F(FlightEffectsTest, TetheringStrikeKnocksTheMonsterUpOnceAndCrashingBlowClosesTheAerialSkill) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* tether = bindSkill(TETHERING_STRIKE_XML);
	const model::SkillTemplate* crash = bindSkill(CRASHING_BLOW_XML);
	const model::SkillTemplate* aerial = bindSkill(SKILL_8224_XML);
	ASSERT_EQ(effectOf(*tether, 0).javaClassName(), "OpenAerialEffect");
	ASSERT_EQ(effectOf(*crash, 0).javaClassName(), "CloseAerialEffect");
	Ref<Player> fighter = flyer(8607, 60);
	Ref<Npc> npc = makeMonster(701607, 510, 500);

	Ref<Effect> up = cast(*fighter, *npc, tether, 1);
	ASSERT_TRUE(up->isInSuccessEffects(1));
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::OPENAERIAL)) << "OpenAerialEffect.startEffect";
	Ref<Effect> again = cast(*fighter, *npc, tether, 1);
	EXPECT_FALSE(again->isInSuccessEffects(1)) << "OpenAerialEffect.calculate: already OPENAERIAL";
	up->endEffect();
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::OPENAERIAL));

	// CloseAerialEffect.applyEffect: removeEffect(8224) on the effected
	cast(*fighter, *fighter, aerial, 1);
	ASSERT_NE(fighter->getEffectController()->findBySkillId(8224), nullptr);
	Ref<Effect> close = forced(*fighter, *fighter, crash);
	ASSERT_TRUE(close->isInSuccessEffects(1));
	EXPECT_EQ(fighter->getEffectController()->findBySkillId(8224), nullptr);
}

TEST_F(FlightEffectsTest, AKnockedUpPlayerIsForceMovedAndLosesItsCast) {
	EFFECT_TEST_SCOPE;
	const model::SkillTemplate* tether = bindSkill(TETHERING_STRIKE_XML);
	Ref<Npc> npc = makeMonster(701608, 510, 500);
	Ref<Player> player = flyer(8608, 60);
	cp::RecordingAionConnection& client = connectLast(*player);
	client.clearSent();

	Ref<Effect> up = forced(*npc, *player, tether);
	ASSERT_TRUE(up->isInSuccessEffects(1));
	EXPECT_TRUE(player->getEffectController()->isAbnormalSet(AbnormalState::OPENAERIAL));
	EXPECT_EQ(packetsOf<network::aion::serverpackets::SM_FORCED_MOVE>(client).size(), 1u)
		<< "OpenAerialEffect.startEffect: SM_FORCED_MOVE to the player and who sees it";
	up->endEffect();
}

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
