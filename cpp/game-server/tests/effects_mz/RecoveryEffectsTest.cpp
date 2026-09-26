// P5-04, M5e stage 1, E-02 (m5e-plan.md §2.4, §15.4): the effect classes of the lane that give back or protect a resource - MPHealInstantEffect
// (249 MP Recovery, every advanced class at level 10), SkillAtkDrainInstantEffect (the Gladiator's 769 Absorbing Fury and 758 Roiling Hack
// at 10, the Ranger's 1075 Seizure Arrow and 865 Manaleech Shot), MPShieldEffect (the Rider's 2440 Kinetic Battery at 16) and ResurrectEffect (the Cleric's and the
// Chanter's 1699 Light of Resurrection).
//
// Each case drives a real Effect through calculate -> applyEffect -> startEffect -> endEffect (EffectsMzTestSupport.h) on the skill templates
// of skill_templates.xml, whose <effects> are copied verbatim below (without their <properties>, conditions and motions), and asserts what
// the Java bodies do: MPHealInstantEffect.java:15-35 over AbstractHealEffect.java:27-87, SkillAtkDrainInstantEffect.java:21-41,
// MPShieldEffect.java:18-40 over AttackShieldObserver.java:62-116 and ResurrectEffect.java:18-36.
//
// The golden values are derived by hand from the Java arithmetic. The magical drain's reserve takes the inputs of DamageEffectsTest's Flame
// Bolt golden: a level 1 MAGE (KNOWLEDGE 115, no magic boost) against the level 1 test monster (mdef 100, no elemental defense), whose magical
// path draws no random number: (int) (141 * 1.15f) = 162 through BOOST_SPELL_ATTACK, minus 100 / 10f = 152. The drain then gives back
// `reserve * percent / 100` in int arithmetic.

#include "EffectsMzTestSupport.h"

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RESURRECT.h"
#include "aion/gameserver/skillengine/effect/MPHealInstantEffect.h"
#include "aion/gameserver/skillengine/effect/MPShieldEffect.h"
#include "aion/gameserver/skillengine/effect/ResurrectEffect.h"
#include "aion/gameserver/skillengine/effect/SkillAtkDrainInstantEffect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/HitType.h"

namespace aion::gameserver::skillengine::effect::mztest {
namespace {

using controllers::attack::AttackResult;
using controllers::attack::AttackStatus;
using gameserver::model::PlayerClass;
using network::aion::serverpackets::SM_ATTACK_STATUS;
using network::aion::serverpackets::SM_RESURRECT;

// ------------------------------------------------------------------------------------------------------------------------- skill templates

/**
 * The skills of the cases, their <effects> verbatim from skill_templates.xml: 249 "MP Recovery", 769 "Absorbing Fury", 1075 "Seizure Arrow",
 * 865 "Manaleech Shot" (one of the two drains of the data with an mp_percent alone), 2440 "Kinetic Battery", 1699 "Light of Resurrection" and the skill its <resurrect> names, 8296 "Soul Sickness" (the Cleric's; the
 * support's 8291 is the Chanter's).
 */
constexpr const char* RECOVERY_SKILLS_XML =
	R"(<skill_template skill_id="249" name="MP Recovery" nameId="2288153" cooldownId="1153" group="AROMATHERAPY" stack="AROMATHERAPY" lvl="1")"
	R"( skilltype="PHYSICAL" skill_category="HEAL" skillsubtype="HEAL" tslot="BUFF" activation="ACTIVE" cooldown="160" duration="4000")"
	R"( cancel_rate="100000" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<mphealinstant value="206" e="1" noresist="true" element="WATER" hoptype="SKILLLV" hopb="248" />)"
	R"(<mpheal checktime="3000" value="31" duration2="10000" effectid="118232" e="2" basiclvl="20" noresist="true" preeffect="1" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="769" name="Absorbing Fury" nameId="2287717" cooldownId="335" group="FI_SEISMICDRAIN" stack="FI_SEISMICDRAIN" lvl="1")"
	R"( skilltype="PHYSICAL" skill_category="CHAIN_SKILL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="100" duration="0")"
	R"( cancel_rate="10" chain_skill_prob="100" hostile_type="DIRECT"><effects>)"
	R"(<skillatkdraininstant hp_percent="10" value="103" e="1" hoptype="DAMAGE" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="1075" name="Seizure Arrow" nameId="2287876" cooldownId="627" group="RA_APPROACHSHOT" stack="RA_APPROACHSHOT")"
	R"( lvl="1" skilltype="PHYSICAL" skill_category="CHAIN_SKILL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="450")"
	R"( duration="0" ammospeed="35" cancel_rate="20" counter_skill="DODGE" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true"><effects>)"
	R"(<skillatkdraininstant hp_percent="50" mp_percent="50" value="538" e="1" hoptype="DAMAGE" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="865" name="Manaleech Shot" nameId="2287817" cooldownId="106" group="RA_DRAINSHOT" stack="RA_DRAINSHOT" lvl="1")"
	R"( skilltype="PHYSICAL" skill_category="CHAIN_SKILL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="150" duration="0")"
	R"( ammospeed="45" cancel_rate="10" hostile_type="DIRECT"><effects>)"
	R"(<skillatkdraininstant mp_percent="50" value="313" e="1" hoptype="DAMAGE" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="2440" name="Kinetic Battery" nameId="2286754" cooldownId="1713" group="RI_EATHIUMSHIELD" stack="RI_EATHIUMSHIELD")"
	R"( lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="30")"
	R"( activation="TOGGLE" cooldown="300" toggle_timer="90000" duration="0" cancel_rate="20" hostile_type="INDIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<mpshield mp_value="50" percent="true" hitvalue="30" value="20000" effectid="154" e="1" noresist="true" hittype="EVERYHIT")"
	R"( hoptype="SKILLLV" hopb="714" />)"
	R"(<statup effectid="936452" e="2" noresist="true" preeffect="1" hoptype="SKILLLV">)"
	R"(<change stat="STUN_RESISTANCE" func="ADD" value="100" /><change stat="STUMBLE_RESISTANCE" func="ADD" value="100" />)"
	R"(<change stat="STAGGER_RESISTANCE" func="ADD" value="100" /><change stat="SPIN_RESISTANCE" func="ADD" value="100" />)"
	R"(<change stat="OPENAERIAL_RESISTANCE" func="ADD" value="100" /><change stat="PARRY" func="ADD" value="149" /></statup>)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="1699" name="Light of Resurrection" nameId="2286403" cooldownId="1524" group="CL_REVIVE" stack="CL_REVIVE" lvl="1")"
	R"( skilltype="MAGICAL" skill_category="REBIRTH" skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="0" duration="6000")"
	R"( cancel_rate="30" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")"
	R"( apply_casting_time_bonus="true"><effects>)"
	R"(<resurrect skill_id="8296" e="1" noresist="true" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="8296" name="Soul Sickness" nameId="282699" stack="CL_RESURRECTDEBUFF" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="NONE" tslot="SPEC2" activation="PROVOKED" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true"><effects>)"
	R"(<statdown duration2="40000" duration1="20000" e="1" noresist="true" element="FIRE"><change stat="MAXHP" func="PERCENT" value="-30" />)"
	R"(</statdown>)"
	R"(<statdown duration2="40000" duration1="20000" e="2" noresist="true" element="FIRE" preeffect="1">)"
	R"(<change stat="MAXMP" func="PERCENT" value="-30" /></statdown>)"
	R"(<statdown duration2="40000" duration1="20000" e="3" noresist="true" element="FIRE" preeffect="1">)"
	R"(<change stat="SPEED" func="PERCENT" value="-50" /><change stat="FLY_SPEED" func="PERCENT" value="-50" /></statdown>)"
	R"(</effects></skill_template>)"
	// 64201: a magical drain of 30 % HP and 50 % MP whose reserve is Flame Bolt's golden 152 (1282's value 141 and element FIRE, noresist, no
	// magical critical roll)
	R"(<skill_template skill_id="64201" name="mz magical drain" nameId="1" stack="MZ_DRAIN" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK")"
	R"( tslot="NONE" activation="ACTIVE" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="false"><effects>)"
	R"(<skillatkdraininstant hp_percent="30" mp_percent="50" value="141" e="1" noresist="true" element="FIRE" hoptype="DAMAGE" />)"
	R"(</effects></skill_template>)"
	// 64202: a flat MP shield (mp_value 10) with a per-level hit: hit 20 + 5 per level, total 500 + 100 per level
	R"(<skill_template skill_id="64202" name="mz mp shield" nameId="1" stack="MZ_MPSHIELD" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
	R"( tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
	R"(<mpshield mp_value="10" hitvalue="20" hitdelta="5" value="500" delta="100" duration2="60000" effectid="154" e="1" noresist="true")"
	R"( hittype="EVERYHIT" /></effects></skill_template>)";

/** SM_ATTACK_STATUS as SM_ATTACK_STATUS.java:120-160 writes it (the fields the cases read) */
struct StatusFields {
	int32_t objectId = 0;
	int32_t value = 0;
	int32_t type = 0;
	int32_t skillId = 0;
	int32_t log = 0;
};

StatusFields decodeStatus(const std::vector<uint8_t>& bytes) {
	network::test::PacketReader reader(cp::bodyOf(bytes));
	StatusFields f;
	f.objectId = reader.D();
	f.value = reader.D();
	f.type = reader.C();
	reader.C(); // the hp or mp percentage
	f.skillId = static_cast<uint16_t>(reader.H());
	f.log = reader.C();
	reader.C(); // the critical marker
	EXPECT_EQ(reader.remaining(), 0u) << "SM_ATTACK_STATUS consumed exactly";
	return f;
}

// SM_ATTACK_STATUS.TYPE and .LOG values (SM_ATTACK_STATUS.java:22-89)
constexpr int32_t TYPE_ABSORBED_HP = 6;
constexpr int32_t TYPE_HEAL_MP = 19;
constexpr int32_t TYPE_MP = 21;
constexpr int32_t TYPE_NATURAL_HP = 3;
constexpr int32_t TYPE_NATURAL_MP = 22;
constexpr int32_t LOG_SKILLLATKDRAININSTANT = 23;
constexpr int32_t LOG_REGULAR = 191;

/** Java ShieldType.MPSHIELD.getId() (ShieldType.java:12) */
constexpr int32_t MPSHIELD_ID = 1 << 4;

class RecoveryEffectsTest : public EffectsMzTest {
protected:
	void SetUp() override {
		EffectsMzTest::SetUp();
		EFFECT_TEST_SCOPE;
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the base published the lane's templates; the holder is immortal, only forgotten
		publishSkillData(effectsMzSkills() + RECOVERY_SKILLS_XML);
	}

	/** The SM_ATTACK_STATUS packets the player was sent about `objectId`, without the natural regeneration a drop of HP or MP starts */
	std::vector<StatusFields> statusesOf(Player& to, int32_t objectId) {
		std::vector<StatusFields> result;
		for (const std::vector<uint8_t>& bytes : sentTo<SM_ATTACK_STATUS>(to)) {
			StatusFields f = decodeStatus(bytes);
			if (f.objectId == objectId && f.type != TYPE_NATURAL_HP && f.type != TYPE_NATURAL_MP)
				result.push_back(f);
		}
		return result;
	}

	static int32_t reserved(Effect& effect, int32_t position = 1) { return effect.getReserveds(position)->getValue(); }

	/** One hit of `damage` on the effected, through its shield observers (as AttackUtil does after a hit is calculated) */
	static Ref<AttackResult> hit(Creature& effected, Creature& attacker, int32_t damage) {
		Ref<AttackResult> result = AttackResult::create(static_cast<float>(damage), AttackStatus::NORMALHIT, model::HitType::PHHIT);
		effected.getObserveController()->checkShieldStatus(std::vector<Ptr<AttackResult>>{Ptr<AttackResult>(result)}, nullptr, attacker);
		return result;
	}
};

// ---- MPHealInstantEffect (MPHealInstantEffect.java:13-36) -----------------------------------------------------------------------------------

/**
 * 249 MP Recovery on the caster: position 1 (<mphealinstant value="206">) reserves value + delta * level = 206 of MP (ResourceType.MP; no
 * percent), capped at the missing MP - getMaxStatValue is the MAXMP stat, getCurrentStatValue the current MP - and applyEffect restores it as a
 * skill heal: increaseMp(TYPE.HEAL_MP, 206, 0, LOG.REGULAR), SM_ATTACK_STATUS type 19 (an item's ProcMPHealInstantEffect sends TYPE.MP 21).
 * The mage's MAXMP is raised by 1,000 so that 300 can be missing.
 */
TEST_F(RecoveryEffectsTest, MpRecoveryRestoresTwoHundredSixMpAsASkillHeal) {
	EFFECT_TEST_SCOPE;
	Ref<Player> mage = player(7101);
	skillengine::test::addStat(*mage, StatEnum::MAXMP, 1000);
	const int32_t maxMp = mage->getGameStats()->getMaxMp()->getCurrent();
	ASSERT_EQ(maxMp, mage->getLifeStats()->getMaxMp());
	ASSERT_GT(maxMp, 1000);
	mage->getLifeStats()->setCurrentMp(maxMp - 300);
	const int32_t hp = mage->getLifeStats()->getCurrentHp();
	clearSent(*mage);

	Ref<Effect> effect = calculated(249, *mage, *mage);
	ASSERT_NE(dynamic_cast<const MPHealInstantEffect*>(effect->getEffectTemplates()[0]), nullptr);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(reserved(*effect), 206);
	EXPECT_EQ(effect->getReserveds(1)->getType(), model::EffectReserved::ResourceType::MP) << "ResourceType.of(HealType.MP)";

	effect->applyEffect();
	EXPECT_EQ(mage->getLifeStats()->getCurrentMp(), maxMp - 94);
	EXPECT_EQ(mage->getLifeStats()->getCurrentHp(), hp) << "an MP heal leaves the HP alone";
	std::vector<StatusFields> statuses = statusesOf(*mage, mage->getObjectId());
	ASSERT_EQ(statuses.size(), 1u);
	EXPECT_EQ(statuses[0].value, 206);
	EXPECT_EQ(statuses[0].type, TYPE_HEAL_MP) << "a skill heal: TYPE.HEAL_MP (AbstractHealEffect.java:46-47)";
	EXPECT_EQ(statuses[0].log, LOG_REGULAR);
	EXPECT_EQ(statuses[0].skillId, 0);
}

/**
 * The reserve is min(maxMp - currentMp, 206) (AbstractHealEffect.calculateHealValue): 50 missing reserve 50, none missing 0 - on a monster too
 * (MAXMP 100 from its template: 40 left, 60 missing), whose current MP is its life stats' and not a player's.
 */
TEST_F(RecoveryEffectsTest, MpRecoveryIsCappedAtTheMissingMp) {
	EFFECT_TEST_SCOPE;
	Ref<Player> mage = player(7111);
	skillengine::test::addStat(*mage, StatEnum::MAXMP, 1000);
	const int32_t maxMp = mage->getLifeStats()->getMaxMp();
	mage->getLifeStats()->setCurrentMp(maxMp - 50);
	EXPECT_EQ(reserved(*calculated(249, *mage, *mage)), 50);
	mage->getLifeStats()->setCurrentMp(maxMp);
	EXPECT_EQ(reserved(*calculated(249, *mage, *mage)), 0);

	Ref<Npc> npc = monster();
	ASSERT_EQ(npc->getGameStats()->getMaxMp()->getCurrent(), 100);
	npc->getLifeStats()->setCurrentMp(40);
	Ref<Effect> onNpc = calculated(249, *mage, *npc);
	EXPECT_EQ(reserved(*onNpc), 60);
	onNpc->applyEffect();
	EXPECT_EQ(npc->getLifeStats()->getCurrentMp(), 100);
}

// ---- SkillAtkDrainInstantEffect (SkillAtkDrainInstantEffect.java:19-42) ---------------------------------------------------------------------

/**
 * 64201 on the monster: the <skillatkdraininstant> reserves Flame Bolt's golden 152 (see the file comment) and applyEffect deals it at once
 * (DamageEffect.applyEffect: 1000 - 152 = 848). One second later (the task of SkillAtkDrainInstantEffect@L28:44) the caster gets
 * 152 * 30 / 100 = 45 HP back as TYPE.ABSORBED_HP and 152 * 50 / 100 = 76 MP as TYPE.MP, both with the skill and LOG.SKILLLATKDRAININSTANT;
 * nothing comes back before the second is over.
 */
TEST_F(RecoveryEffectsTest, TheDrainGivesBackItsShareOfTheReservedDamageOneSecondLater) {
	EFFECT_TEST_SCOPE;
	Ref<Player> mage = player(7201);
	Ref<Npc> npc = monster();
	pair(*npc, *mage);
	ASSERT_EQ(mage->getGameStats()->getKnowledge()->getCurrent(), 115);
	ASSERT_EQ(mage->getGameStats()->getMBoost()->getCurrent(), 0);
	ASSERT_EQ(npc->getGameStats()->getMDef()->getCurrent(), 100);
	const int32_t maxHp = mage->getLifeStats()->getMaxHp();
	const int32_t maxMp = mage->getLifeStats()->getMaxMp();
	ASSERT_GT(maxHp, 100);
	ASSERT_GT(maxMp, 100);
	mage->getLifeStats()->setCurrentHp(maxHp - 100);
	mage->getLifeStats()->setCurrentMp(maxMp - 100);
	clearSent(*mage);

	Ref<Effect> effect = calculated(64201, *mage, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	ASSERT_NE(dynamic_cast<const SkillAtkDrainInstantEffect*>(effect->getEffectTemplates()[0]), nullptr);
	EXPECT_EQ(reserved(*effect), 152);
	effect->applyEffect();
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 848) << "DamageEffect.applyEffect deals the reserve";
	EXPECT_TRUE(statusesOf(*mage, mage->getObjectId()).empty());

	advance(999);
	EXPECT_EQ(mage->getLifeStats()->getCurrentHp(), maxHp - 100) << "the drain comes a second later";
	EXPECT_EQ(mage->getLifeStats()->getCurrentMp(), maxMp - 100);
	advance(1);
	EXPECT_EQ(mage->getLifeStats()->getCurrentHp(), maxHp - 100 + 45) << "152 * 30 / 100";
	EXPECT_EQ(mage->getLifeStats()->getCurrentMp(), maxMp - 100 + 76) << "152 * 50 / 100";
	std::vector<StatusFields> statuses = statusesOf(*mage, mage->getObjectId());
	ASSERT_EQ(statuses.size(), 2u);
	EXPECT_EQ(statuses[0].type, TYPE_ABSORBED_HP);
	EXPECT_EQ(statuses[0].value, 45);
	EXPECT_EQ(statuses[0].skillId, 64201);
	EXPECT_EQ(statuses[0].log, LOG_SKILLLATKDRAININSTANT);
	EXPECT_EQ(statuses[1].type, TYPE_MP);
	EXPECT_EQ(statuses[1].value, 76);
	EXPECT_EQ(statuses[1].skillId, 64201);
	EXPECT_EQ(statuses[1].log, LOG_SKILLLATKDRAININSTANT);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 848) << "the drain takes nothing more from the monster";
}

/**
 * 769 Absorbing Fury (the Gladiator's level 10 chain opener, hp_percent 10, no mp_percent): a physical hit, so its rolls draw from the seeded
 * generator and the first seed whose hit reserves damage is taken. A second later the warrior gets reserve * 10 / 100 HP back and no MP (the
 * mpPercent != 0 guard: no TYPE.MP status at all).
 */
TEST_F(RecoveryEffectsTest, AbsorbingFuryGivesBackATenthOfItsHitAndNoMp) {
	EFFECT_TEST_SCOPE;
	Ref<Player> warrior = player(7211, PlayerClass::WARRIOR, 1, 503, 500, 100);
	const int32_t maxHp = warrior->getLifeStats()->getMaxHp();
	for (uint64_t seed = 1; seed < 100; ++seed) {
		Rnd::seedCurrentThreadForTests(seed);
		Ref<Npc> npc = monster(505, 500, 100);
		warrior->getLifeStats()->setCurrentHp(maxHp / 2);
		const int32_t mp = warrior->getLifeStats()->getCurrentMp();
		Ref<Effect> effect = calculated(769, *warrior, *npc);
		if (!effect->isInSuccessEffects(1) || reserved(*effect) < 10)
			continue;
		const int32_t reserve = reserved(*effect);
		clearSent(*warrior);
		effect->applyEffect();
		EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 1000 - reserve);
		advance(1000);
		EXPECT_EQ(warrior->getLifeStats()->getCurrentHp(), maxHp / 2 + reserve * 10 / 100);
		EXPECT_EQ(warrior->getLifeStats()->getCurrentMp(), mp);
		std::vector<StatusFields> statuses = statusesOf(*warrior, warrior->getObjectId());
		ASSERT_EQ(statuses.size(), 1u) << "no MP status: mp_percent is 0";
		EXPECT_EQ(statuses[0].type, TYPE_ABSORBED_HP);
		EXPECT_EQ(statuses[0].value, reserve * 10 / 100);
		EXPECT_EQ(statuses[0].skillId, 769);
		return;
	}
	FAIL() << "no seed landed the hit";
}

/**
 * 1075 Seizure Arrow drains half of its hit as HP and half as MP. (The ranger's MAXHP and MAXMP are raised by 2,000, so that half of each is
 * more than the drain: increaseHp / increaseMp cap at the maximum.)
 */
TEST_F(RecoveryEffectsTest, SeizureArrowDrainsHalfAsHpAndHalfAsMp) {
	EFFECT_TEST_SCOPE;
	Ref<Player> ranger = player(7221, PlayerClass::SCOUT, 1, 503, 500, 100);
	skillengine::test::addStat(*ranger, StatEnum::MAXHP, 2000);
	skillengine::test::addStat(*ranger, StatEnum::MAXMP, 2000);
	const int32_t maxHp = ranger->getLifeStats()->getMaxHp();
	const int32_t maxMp = ranger->getLifeStats()->getMaxMp();
	for (uint64_t seed = 1; seed < 100; ++seed) {
		Rnd::seedCurrentThreadForTests(seed);
		Ref<Npc> npc = monster(505, 500, 100);
		ranger->getLifeStats()->setCurrentHp(maxHp / 2);
		ranger->getLifeStats()->setCurrentMp(maxMp / 2);
		Ref<Effect> effect = calculated(1075, *ranger, *npc);
		if (!effect->isInSuccessEffects(1) || reserved(*effect) < 2)
			continue;
		const int32_t reserve = reserved(*effect);
		ASSERT_LE(reserve * 50 / 100, maxHp / 2) << "the drain fits into the missing HP";
		ASSERT_LE(reserve * 50 / 100, maxMp / 2);
		effect->applyEffect();
		advance(1000);
		EXPECT_EQ(ranger->getLifeStats()->getCurrentHp(), maxHp / 2 + reserve * 50 / 100);
		EXPECT_EQ(ranger->getLifeStats()->getCurrentMp(), maxMp / 2 + reserve * 50 / 100);
		return;
	}
	FAIL() << "no seed landed the hit";
}

/**
 * 865 Manaleech Shot (mp_percent 50 and no hp_percent, as 866 at level 2): a second after the hit the ranger gets reserve * 50 / 100 MP back
 * (TYPE.MP) and nothing else. The hpPercent != 0 guard matters although the share would be 0: CreatureLifeStats.increaseHp sends an
 * SM_ATTACK_STATUS whenever the skill id is not 0, even for a gain of 0 (CreatureLifeStats.java:190-191), so without it the ranger would also
 * read an ABSORBED_HP 0. The ranger's HP stays below its maximum, where such a status would also be sent.
 */
TEST_F(RecoveryEffectsTest, ManaleechShotDrainsHalfAsMpAndSendsNoHpStatus) {
	EFFECT_TEST_SCOPE;
	Ref<Player> ranger = player(7231, PlayerClass::SCOUT, 1, 503, 500, 100);
	skillengine::test::addStat(*ranger, StatEnum::MAXMP, 2000);
	const int32_t maxHp = ranger->getLifeStats()->getMaxHp();
	const int32_t maxMp = ranger->getLifeStats()->getMaxMp();
	for (uint64_t seed = 1; seed < 100; ++seed) {
		Rnd::seedCurrentThreadForTests(seed);
		Ref<Npc> npc = monster(505, 500, 100);
		ranger->getLifeStats()->setCurrentHp(maxHp / 2);
		ranger->getLifeStats()->setCurrentMp(maxMp / 2);
		Ref<Effect> effect = calculated(865, *ranger, *npc);
		if (!effect->isInSuccessEffects(1) || reserved(*effect) < 2)
			continue;
		ASSERT_NE(dynamic_cast<const SkillAtkDrainInstantEffect*>(effect->getEffectTemplates()[0]), nullptr);
		const int32_t reserve = reserved(*effect);
		ASSERT_LE(reserve * 50 / 100, maxMp / 2) << "the drain fits into the missing MP";
		clearSent(*ranger);
		effect->applyEffect();
		advance(1000);
		EXPECT_EQ(ranger->getLifeStats()->getCurrentMp(), maxMp / 2 + reserve * 50 / 100);
		EXPECT_EQ(ranger->getLifeStats()->getCurrentHp(), maxHp / 2) << "no HP drained";
		std::vector<StatusFields> statuses = statusesOf(*ranger, ranger->getObjectId());
		ASSERT_EQ(statuses.size(), 1u) << "no ABSORBED_HP status: hp_percent is 0";
		EXPECT_EQ(statuses[0].type, TYPE_MP);
		EXPECT_EQ(statuses[0].value, reserve * 50 / 100);
		EXPECT_EQ(statuses[0].skillId, 865);
		EXPECT_EQ(statuses[0].log, LOG_SKILLLATKDRAININSTANT);
		return;
	}
	FAIL() << "no seed landed the hit";
}

// ---- MPShieldEffect (MPShieldEffect.java:16-41) ---------------------------------------------------------------------------------------------

/**
 * 2440 Kinetic Battery (a toggle of the Rider at 16): startEffect adds an AttackShieldObserver of ShieldType.MPSHIELD with hit 30 %
 * (hitvalue + hitdelta * level, percent) and total 20,000 (calculateBaseValue) and the template's mp_value 50. A hit of 100 is absorbed by
 * 100 * 30 / 100 = 30 (70 get through), the result carries the MPSHIELD bit, and the absorbed damage costs (int) (30 * 0.01f * 50) MP
 * (reduceMp, mpAbsorbed, the shield's skill id) - float arithmetic left to right: 30 * 0.01f rounds to 0.29999998f, times 50 to 14.999999f,
 * so 14, not 15. A hit of 200 loses 60: 60 * 0.01f = 0.59999996f, times 50 = 29.999998f, so 29 more. The toggle lasts toggle_timer
 * 90,000 ms; its end takes the observer away (endEffect itself does nothing).
 */
TEST_F(RecoveryEffectsTest, KineticBatteryAbsorbsThirtyPercentAndPaysHalfOfItInMp) {
	EFFECT_TEST_SCOPE;
	Ref<Player> rider = player(7301, PlayerClass::ENGINEER);
	Ref<Npc> npc = monster();
	const int32_t maxMp = rider->getLifeStats()->getMaxMp();
	ASSERT_GT(maxMp, 60);
	ASSERT_FALSE(rider->getObserveController()->hasObservers());
	Ref<Effect> effect = applied(2440, *rider, *rider);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	ASSERT_NE(dynamic_cast<const MPShieldEffect*>(effect->getEffectTemplates()[0]), nullptr);
	EXPECT_EQ(effect->getDuration(), 90000) << "a toggle lasts its toggle_timer";
	EXPECT_TRUE(rider->getObserveController()->hasObservers()) << "the shield observer";

	Ref<AttackResult> first = hit(*rider, *npc, 100);
	EXPECT_EQ(first->getDamage(), 70);
	EXPECT_EQ(first->getShieldType(), MPSHIELD_ID);
	EXPECT_EQ(first->getMpAbsorbed(), 14) << "(int) (30 * 0.01f * 50) = (int) 14.999999f";
	EXPECT_EQ(first->getMpShieldSkillId(), 2440);
	EXPECT_EQ(rider->getLifeStats()->getCurrentMp(), maxMp - 14);
	Ref<AttackResult> second = hit(*rider, *npc, 200);
	EXPECT_EQ(second->getDamage(), 140);
	EXPECT_EQ(second->getMpAbsorbed(), 29) << "(int) (60 * 0.01f * 50) = (int) 29.999998f";
	EXPECT_EQ(rider->getLifeStats()->getCurrentMp(), maxMp - 43);
	EXPECT_TRUE(rider->getEffectController()->hasAbnormalEffect(2440)) << "20,000 - 90 absorbed: the shield holds";

	effect->endEffect();
	EXPECT_FALSE(rider->getObserveController()->hasObservers()) << "Effect.endEffect -> removeObservers";
	Ref<AttackResult> third = hit(*rider, *npc, 100);
	EXPECT_EQ(third->getDamage(), 100);
	EXPECT_EQ(third->getMpAbsorbed(), 0);
}

/**
 * 64202 at skill level 2: a flat shield (not percent) with hit 20 + 5 * 2 = 30 and total 500 + 100 * 2 = 700; mp_value 10. A hit of 100 is
 * absorbed by min(100, 30) = 30 and costs (int) (30 * 0.01f * 10) = (int) 2.9999998f = 2 MP; after 23 such hits 10 of the total are left, the
 * next hit loses only those (90 get through) for (int) (10 * 0.01f * 10) = (int) 0.99999994f = 0 MP, and the spent shield ends its effect.
 */
TEST_F(RecoveryEffectsTest, AnMpShieldTakesItsHitAndTotalFromTheSkillLevel) {
	EFFECT_TEST_SCOPE;
	Ref<Player> rider = player(7311, PlayerClass::ENGINEER);
	Ref<Npc> npc = monster();
	const int32_t maxMp = rider->getLifeStats()->getMaxMp();
	Ref<Effect> effect = applied(64202, *rider, *rider, 2);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getDuration(), 60000);
	Ref<AttackResult> first = hit(*rider, *npc, 100);
	EXPECT_EQ(first->getDamage(), 70) << "min(100, 20 + 5 * 2)";
	EXPECT_EQ(first->getMpAbsorbed(), 2) << "(int) (30 * 0.01f * 10) = (int) 2.9999998f";
	EXPECT_EQ(rider->getLifeStats()->getCurrentMp(), maxMp - 2);
	for (int32_t i = 0; i < 22; ++i)
		hit(*rider, *npc, 100); // 23 hits absorb 690 of the 700
	EXPECT_TRUE(rider->getEffectController()->hasAbnormalEffect(64202));
	Ref<AttackResult> last = hit(*rider, *npc, 100);
	EXPECT_EQ(last->getDamage(), 90) << "only 10 of the total were left";
	EXPECT_EQ(last->getMpAbsorbed(), 0) << "(int) (10 * 0.01f * 10) = (int) 0.99999994f";
	EXPECT_FALSE(rider->getEffectController()->hasAbnormalEffect(64202)) << "the spent shield ends its effect";
}

// ---- ResurrectEffect (ResurrectEffect.java:16-37) -------------------------------------------------------------------------------------------

/**
 * 1699 Light of Resurrection on a dead player: calculate lands (EffectTemplate.calculate, noresist) and applyEffect offers the revive -
 * setPlayerResActivate(true), setResurrectionSkill(8296) (the soul sickness PlayerReviveService.skillRevive casts, C-02) and SM_RESURRECT with
 * the caster's name and the skill (writeS name, writeH 1699, writeD 0) to the dead player only. The dead stay dead until they accept.
 */
TEST_F(RecoveryEffectsTest, LightOfResurrectionOffersTheDeadPlayerARevive) {
	EFFECT_TEST_SCOPE;
	Ref<Player> cleric = player(7401, PlayerClass::PRIEST);
	Ref<Player> dead = player(7402, PlayerClass::WARRIOR, 1, 505, 500, 100);
	// a player at 0 HP without PlayerController.onDie's services (the cm_ak support's life stats)
	dead->setLifeStats(std::make_unique<cp::DeadPlayerLifeStats>(*dead));
	ASSERT_TRUE(dead->isDead());
	clearSent(*cleric);
	clearSent(*dead);

	Ref<Effect> effect = calculated(1699, *cleric, *dead);
	ASSERT_NE(dynamic_cast<const ResurrectEffect*>(effect->getEffectTemplates()[0]), nullptr);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_FALSE(dead->getResStatus()) << "calculate offers nothing yet";
	effect->applyEffect();
	EXPECT_TRUE(dead->getResStatus()) << "setPlayerResActivate(true)";
	EXPECT_EQ(dead->getResurrectionSkill(), 8296);
	EXPECT_TRUE(dead->isDead());
	std::vector<std::vector<uint8_t>> offers = sentTo<SM_RESURRECT>(*dead);
	ASSERT_EQ(offers.size(), 1u);
	network::test::PacketReader reader(cp::bodyOf(offers[0]));
	EXPECT_EQ(reader.S(), cleric->getName());
	EXPECT_EQ(static_cast<uint16_t>(reader.H()), 1699) << "the resurrecting skill, not the soul sickness";
	EXPECT_EQ(reader.D(), 0);
	EXPECT_EQ(reader.remaining(), 0u);
	EXPECT_TRUE(sentTo<SM_RESURRECT>(*cleric).empty());
}

/**
 * calculate asks `effected instanceof Player && effected.isDead()` (ResurrectEffect.java:35): a living player and a dead monster are not
 * resurrected (no success, so Effect.applyEffect does nothing and nobody is offered a revive).
 */
TEST_F(RecoveryEffectsTest, OnlyADeadPlayerIsResurrected) {
	EFFECT_TEST_SCOPE;
	Ref<Player> cleric = player(7411, PlayerClass::PRIEST);
	Ref<Player> living = player(7412, PlayerClass::WARRIOR, 1, 505, 500, 100);
	clearSent(*living);
	Ref<Effect> onLiving = calculated(1699, *cleric, *living);
	EXPECT_FALSE(onLiving->isInSuccessEffects(1));
	onLiving->applyEffect();
	EXPECT_FALSE(living->getResStatus());
	EXPECT_EQ(living->getResurrectionSkill(), 0);
	EXPECT_TRUE(sentTo<SM_RESURRECT>(*living).empty());

	Ref<Npc> deadMonster = monster();
	deadMonster->getLifeStats()->setCurrentHp(0);
	ASSERT_TRUE(deadMonster->isDead());
	EXPECT_FALSE(calculated(1699, *cleric, *deadMonster)->isInSuccessEffects(1));

	// applyEffect itself offers a revive only to a Player effected (ResurrectEffect.java:25-30): on the monster it does nothing
	Ref<Effect> onMonster = Effect::create(*cleric, Ptr<Creature>(deadMonster), skillTemplate(1699), 1);
	EXPECT_NO_THROW(onMonster->getEffectTemplates()[0]->applyEffect(*onMonster));
}

} // namespace
} // namespace aion::gameserver::skillengine::effect::mztest
