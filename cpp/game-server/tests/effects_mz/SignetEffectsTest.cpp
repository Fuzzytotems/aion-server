// P5-04, M5e stage 1, E-02 (m5e-plan.md §2.4, §15.4): the Assassin's runes - SignetEffect (8303-8307 Rune Carve I-V, which every
// <carvesignet> launches: 3385 / 3386 Rune Carve at 13 / 18, 3417 Fang Strike at 16, the stigma 3396 Sigil Strike) and SignetBurstEffect (3374
// Pain Rune at 13, 3375 at 18: the burst of the carved rune, with 8383 Stun Effect as its <subeffect addeffect="true">).
//
// Each case drives a real Effect through calculate -> applyEffect -> startEffect -> endEffect (EffectsMzTestSupport.h) on the skill templates
// of skill_templates.xml, whose <effects> are copied verbatim below (without their <properties>, conditions and motions), and asserts what
// the Java bodies do: SignetEffect.java:15-27 and SignetBurstEffect.java:33-64 with signet_data_templates.xml's SIGNET1 / SIGNET2 rows.
//
// The golden damage is derived by hand from the Java arithmetic of the magical path, with DamageEffectsTest's inputs: a level 1 MAGE (KNOWLEDGE
// 115, no magic boost) against the level 1 test monster (mdef 100, no elemental defense, 1,000 MAGICAL_CRITICAL_RESIST so that no critical is
// rolled), whose magical path draws no random number for a player effector. SignetBurstEffect answers shouldUseBoostSpellAttackEffects and
// shouldUseOneTimeBoostSkillAttack false, so the damage after the knowledge factor is not truncated to an int by BOOST_SPELL_ATTACK:
// damage = (int) (581 * dmg_multi) * 1.15f - 100 / 10f, truncated once at the end.

#include "EffectsMzTestSupport.h"

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/dataholders/SignetDataTemplates.bind.h"
#include "aion/gameserver/dataholders/SignetDataTemplates.h"
#include "aion/gameserver/skillengine/effect/SignetBurstEffect.h"
#include "aion/gameserver/skillengine/effect/SignetEffect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"

namespace aion::gameserver::skillengine::effect::mztest {
namespace {

using gameserver::model::PlayerClass;
using network::aion::serverpackets::SM_ABNORMAL_EFFECT;

// ------------------------------------------------------------------------------------------------------------------------- skill templates

/**
 * The skills of the cases, their <effects> verbatim from skill_templates.xml: 8303 / 8304 / 8305 "Rune Carve I / II / III" (stack SIGNET1,
 * levels 1-3), 3374 "Pain Rune" (no lvl attribute in the data) and its sub effect 8383 "Stun Effect".
 */
constexpr const char* SIGNET_SKILLS_XML =
	R"(<skill_template skill_id="8303" name="Rune Carve I" nameId="284072" stack="SIGNET1" lvl="1" skilltype="MAGICAL" skillsubtype="NONE")"
	R"( tslot="DEBUFF" activation="PROVOKED" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<effects><signet duration2="24000" e="1" noresist="true" element="FIRE" hoptype="SKILLLV" /></effects></skill_template>)"
	R"(<skill_template skill_id="8304" name="Rune Carve II" nameId="284074" stack="SIGNET1" lvl="2" skilltype="MAGICAL" skillsubtype="NONE")"
	R"( tslot="DEBUFF" activation="PROVOKED" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<effects><signet duration2="24000" e="1" noresist="true" element="FIRE" hoptype="SKILLLV" /></effects></skill_template>)"
	R"(<skill_template skill_id="8305" name="Rune Carve III" nameId="284076" stack="SIGNET1" lvl="3" skilltype="MAGICAL" skillsubtype="NONE")"
	R"( tslot="DEBUFF" activation="PROVOKED" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<effects><signet duration2="24000" e="1" noresist="true" element="FIRE" hoptype="SKILLLV" /></effects></skill_template>)"
	R"(<skill_template skill_id="3374" name="Pain Rune" nameId="2287186" cooldownId="671" group="AS_SIGNETBURST" stack="AS_SIGNETBUR")"
	R"( skilltype="MAGICAL" skill_category="CHAIN_SKILL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="180" duration="0")"
	R"( cancel_rate="20" chain_skill_prob="100" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<effects><signetburst signetlvl="5" signet="SIGNET1" add_effect_prob_multi="2" value="581" e="1" accmod2="500" element="FIRE")"
	R"( hoptype="DAMAGE"><subeffect skill_id="8383" addeffect="true" /></signetburst></effects></skill_template>)"
	R"(<skill_template skill_id="8383" name="Stun Effect" nameId="287366" stack="AS_SIGNETBURST_ADDEFFE" skilltype="MAGICAL")"
	R"( skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="STUN" activation="PROVOKED" cooldown="0" duration="0")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<stun duration2="3000" effectid="20000" e="1" accmod2="500" element="FIRE" /></effects></skill_template>)"
	// The test templates, each 3374's <signetburst> but for one attribute: 64601 signetlvl 2 (the cap), 64602 preeffect 2 without a position 2
	// (EffectTemplate.calculate's validatePreEffects refuses it), 64603 signet SIGNET2 (another stack and another data row)
	R"(<skill_template skill_id="64601" name="mz capped burst" nameId="1" stack="MZ_BURST_1" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK")"
	R"( tslot="NONE" activation="ACTIVE" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<signetburst signetlvl="2" signet="SIGNET1" add_effect_prob_multi="2" value="581" e="1" accmod2="500" element="FIRE" hoptype="DAMAGE">)"
	R"(<subeffect skill_id="8383" addeffect="true" /></signetburst></effects></skill_template>)"
	R"(<skill_template skill_id="64602" name="mz refused burst" nameId="1" stack="MZ_BURST_2" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK")"
	R"( tslot="NONE" activation="ACTIVE" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<signetburst signetlvl="5" signet="SIGNET1" add_effect_prob_multi="2" value="581" e="1" accmod2="500" element="FIRE" hoptype="DAMAGE")"
	R"( preeffect="2"><subeffect skill_id="8383" addeffect="true" /></signetburst></effects></skill_template>)"
	R"(<skill_template skill_id="64603" name="mz other burst" nameId="1" stack="MZ_BURST_3" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK")"
	R"( tslot="NONE" activation="ACTIVE" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<signetburst signetlvl="5" signet="SIGNET2" add_effect_prob_multi="2" value="581" e="1" accmod2="500" element="FIRE" hoptype="DAMAGE">)"
	R"(<subeffect skill_id="8383" addeffect="true" /></signetburst></effects></skill_template>)"
	// 64604: 8303's <signet> without noresist and with preeffect 2 and no position 2 - SignetEffect.calculate asks neither
	R"(<skill_template skill_id="64604" name="mz bare signet" nameId="1" stack="MZ_SIGNET" lvl="1" skilltype="MAGICAL" skillsubtype="NONE")"
	R"( tslot="DEBUFF" activation="PROVOKED" cooldown="0" duration="0"><effects>)"
	R"(<signet duration2="24000" e="1" element="FIRE" preeffect="2" /></effects></skill_template>)";

/** signet_data_templates.xml, the SIGNET1 and SIGNET2 rows as the data has them */
constexpr std::string_view SIGNET_DATA_XML =
	R"(<signet_data_templates><signet_data_template signet_skill="SIGNET1">)"
	R"(<signet_data lvl="0" add_effect_prob="0" dmg_multi="0.1"/><signet_data lvl="1" add_effect_prob="0" dmg_multi="0.2"/>)"
	R"(<signet_data lvl="2" add_effect_prob="20" dmg_multi="0.5"/><signet_data lvl="3" add_effect_prob="50" dmg_multi="1.0"/>)"
	R"(<signet_data lvl="4" add_effect_prob="70" dmg_multi="1.2"/><signet_data lvl="5" add_effect_prob="100" dmg_multi="1.5"/>)"
	R"(</signet_data_template><signet_data_template signet_skill="SIGNET2">)"
	R"(<signet_data lvl="0" dmg_multi="0.12"/><signet_data lvl="1" dmg_multi="0.3"/><signet_data lvl="2" dmg_multi="0.5"/>)"
	R"(<signet_data lvl="3" dmg_multi="0.7"/><signet_data lvl="4" dmg_multi="0.9"/><signet_data lvl="5" dmg_multi="1.1"/>)"
	R"(</signet_data_template></signet_data_templates>)";

/** The rune's stack, the key of EffectController.getAbnormalEffect */
constexpr std::string_view SIGNET1 = "SIGNET1";

class SignetEffectsTest : public EffectsMzTest {
protected:
	void SetUp() override {
		EffectsMzTest::SetUp();
		EFFECT_TEST_SCOPE;
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the base published the lane's templates; the holder is immortal, only forgotten
		publishSkillData(effectsMzSkills() + SIGNET_SKILLS_XML);
		dataholders::DataManager::SIGNET_DATA_TEMPLATES.publish(
			xml::bindString<dataholders::SignetDataTemplates>(signetDataContext, std::string(SIGNET_DATA_XML)));
		mage = player(9101);
		npc = monster();
		pair(*npc, *mage);
		skillengine::test::addStat(*npc, StatEnum::MAGICAL_CRITICAL_RESIST, 1000); // no magical critical can be rolled against it
	}

	void TearDown() override {
		mage = nullptr;
		npc = nullptr;
		dataholders::DataManager::SIGNET_DATA_TEMPLATES.resetForTests();
		EffectsMzTest::TearDown();
	}

	/** The inputs of the golden damage (see the file comment) */
	void assertInputs() {
		ASSERT_EQ(mage->getGameStats()->getKnowledge()->getCurrent(), 115);
		ASSERT_EQ(mage->getGameStats()->getMBoost()->getCurrent(), 0);
		ASSERT_EQ(npc->getGameStats()->getMDef()->getCurrent(), 100);
		ASSERT_EQ(npc->getGameStats()->getElementalDefenseFor(gameserver::model::SkillElement::FIRE), 0);
		ASSERT_EQ(npc->getLifeStats()->getCurrentHp(), 1000);
	}

	/**
	 * Carves the rune `signetSkillId` on the monster the way CarveSignetEffect.applyEffect does: SkillEngine.applyEffect(signetId + level - 1,
	 * effector, effected), whose effect has the template's lvl as its skill level (SkillEngine.java:169-172)
	 */
	Ref<Effect> carve(int32_t signetSkillId) {
		Ref<Effect> signet = applied(signetSkillId, *mage, *npc, skillTemplate(signetSkillId)->getLvl());
		EXPECT_TRUE(npc->getEffectController()->getAbnormalEffect(SIGNET1)) << signetSkillId;
		return signet;
	}

	static int32_t reserved(Effect& effect) { return effect.getReserveds(1)->getValue(); }

	xml::LoadContext signetDataContext;
	Ref<Player> mage;
	Ref<Npc> npc;
};

// ---- SignetEffect (SignetEffect.java:13-28) -------------------------------------------------------------------------------------------------

/**
 * 8303 Rune Carve I: calculate only adds the success (no resist roll, no condition) and applyEffect puts the rune on the monster under its stack
 * SIGNET1 in the DEBUFF slot for duration2 24,000 ms, shown to the players who see it; the end task takes it off again.
 */
TEST_F(SignetEffectsTest, RuneCarveMarksTheMonsterForTwentyFourSeconds) {
	EFFECT_TEST_SCOPE;
	clearSent(*mage);
	Ref<Effect> rune = calculated(8303, *mage, *npc);
	ASSERT_NE(dynamic_cast<const SignetEffect*>(rune->getEffectTemplates()[0]), nullptr);
	ASSERT_TRUE(rune->isInSuccessEffects(1));
	EXPECT_FALSE(npc->getEffectController()->getAbnormalEffect(SIGNET1)) << "calculate carves nothing";
	rune->applyEffect();
	EXPECT_EQ(npc->getEffectController()->getAbnormalEffect(SIGNET1).get(), rune.get()) << "effect.addToEffectedController()";
	EXPECT_EQ(rune->getDuration(), 24000);
	EXPECT_EQ(rune->getTargetSlot(), model::SkillTargetSlot::DEBUFF);
	std::vector<std::vector<uint8_t>> announced = sentTo<SM_ABNORMAL_EFFECT>(*mage);
	ASSERT_EQ(announced.size(), 1u);
	AbnormalEffectFields shown = decodeAbnormalEffect(announced[0]);
	ASSERT_EQ(shown.effects.size(), 1u);
	EXPECT_EQ(shown.effects[0].skillId, 8303);
	EXPECT_EQ(shown.effects[0].remainingTime, 24000);
	advance(24000);
	EXPECT_FALSE(npc->getEffectController()->getAbnormalEffect(SIGNET1));
}

/**
 * SignetEffect.calculate is `effect.addSuccessEffect(this)` and nothing else: 64604 (no noresist, a pre-effect position 2 that does not
 * exist) lands where EffectTemplate.calculate would refuse it, even on a monster that resists every magic.
 */
TEST_F(SignetEffectsTest, ARuneLandsWithoutAnyCheck) {
	EFFECT_TEST_SCOPE;
	skillengine::test::addStat(*npc, StatEnum::MAGICAL_RESIST, 100000);
	Ref<Effect> rune = applied(64604, *mage, *npc);
	ASSERT_TRUE(rune->isInSuccessEffects(1));
	EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(64604));
}

// ---- SignetBurstEffect (SignetBurstEffect.java:17-65) ---------------------------------------------------------------------------------------

/**
 * 3374 Pain Rune on a rune of level 3 (8305): signetLvl = min(signetlvl 5, the rune effect's skill level 3) = 3, SIGNET1's row 3 multiplies
 * the base 581 by 1.0 (581) and gives add_effect_prob 50 * add_effect_prob_multi 2 = 100: 581 * 1.15f - 10 = 658.15 -> 658. The burst count
 * is 3, the sub effect is launched (Rnd.chance() < 100 always), and EffectTemplate.calculateSubEffect creates 8383 at the burst count as its
 * level (addeffect). The rune is consumed (signetEffect.endEffect()) during calculate. applyEffect deals the 658 and stuns.
 */
TEST_F(SignetEffectsTest, PainRuneBurstsALevelThreeRuneForItsFullDamageAndStuns) {
	EFFECT_TEST_SCOPE;
	ASSERT_NO_FATAL_FAILURE(assertInputs());
	carve(8305);
	Ref<Effect> burst = calculated(3374, *mage, *npc);
	ASSERT_NE(dynamic_cast<const SignetBurstEffect*>(burst->getEffectTemplates()[0]), nullptr);
	ASSERT_TRUE(burst->isInSuccessEffects(1));
	EXPECT_EQ(reserved(*burst), 658);
	EXPECT_EQ(burst->getSignetBurstedCount(), 3);
	EXPECT_TRUE(burst->isLaunchSubEffect());
	ASSERT_TRUE(burst->getSubEffect());
	EXPECT_EQ(burst->getSubEffect()->getSkillId(), 8383);
	EXPECT_EQ(burst->getSubEffect()->getSkillLevel(), 3) << "the burst count as the sub effect's level";
	EXPECT_TRUE(burst->getSubEffect()->isInSuccessEffects(1));
	EXPECT_FALSE(npc->getEffectController()->getAbnormalEffect(SIGNET1)) << "the rune is consumed";

	burst->applyEffect();
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 1000 - 658);
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::STUN));
}

/**
 * The launch roll `Rnd.chance() < effectProb` with effectProb = add_effect_prob * add_effect_prob_multi: 50 * 2 = 100 for a level 3 rune is
 * sure and 0 * 2 = 0 for a level 1 rune never, whatever the random draws - under 32 seeds each (the multiplier left out, 50 would miss about
 * half of them).
 */
TEST_F(SignetEffectsTest, ALevelThreeBurstAlwaysLaunchesItsStunAndALevelOneNever) {
	EFFECT_TEST_SCOPE;
	for (uint64_t seed = 1; seed <= 32; ++seed) {
		Rnd::seedCurrentThreadForTests(seed);
		Ref<Npc> third = monster(505, 505, 100);
		skillengine::test::addStat(*third, StatEnum::MAGICAL_CRITICAL_RESIST, 1000);
		applied(8305, *mage, *third, 3);
		EXPECT_TRUE(calculated(3374, *mage, *third)->isLaunchSubEffect()) << "seed " << seed;
		Ref<Npc> first = monster(505, 510, 100);
		skillengine::test::addStat(*first, StatEnum::MAGICAL_CRITICAL_RESIST, 1000);
		applied(8303, *mage, *first, 1);
		EXPECT_FALSE(calculated(3374, *mage, *first)->isLaunchSubEffect()) << "seed " << seed;
	}
}

/**
 * On a rune of level 1 (8303): row 1 of SIGNET1, (int) (581 * 0.2f) = 116, 116 * 1.15f - 10 = 123.39 -> 123; add_effect_prob 0 * 2 = 0, so no
 * roll of Rnd.chance() is below it and no stun is launched.
 */
TEST_F(SignetEffectsTest, PainRuneOnAFirstLevelRuneDealsAFifthAndNeverStuns) {
	EFFECT_TEST_SCOPE;
	ASSERT_NO_FATAL_FAILURE(assertInputs());
	carve(8303);
	Ref<Effect> burst = calculated(3374, *mage, *npc);
	ASSERT_TRUE(burst->isInSuccessEffects(1));
	EXPECT_EQ(reserved(*burst), 123);
	EXPECT_EQ(burst->getSignetBurstedCount(), 1);
	EXPECT_FALSE(burst->isLaunchSubEffect());
	EXPECT_FALSE(burst->getSubEffect());
	EXPECT_FALSE(npc->getEffectController()->getAbnormalEffect(SIGNET1));
}

/** Without a rune: signetLvl = min(5, 0) = 0, row 0 of SIGNET1: (int) (581 * 0.1f) = 58, 58 * 1.15f - 10 = 56.69 -> 56, no stun */
TEST_F(SignetEffectsTest, PainRuneWithoutARuneDealsATenth) {
	EFFECT_TEST_SCOPE;
	ASSERT_NO_FATAL_FAILURE(assertInputs());
	Ref<Effect> burst = calculated(3374, *mage, *npc);
	ASSERT_TRUE(burst->isInSuccessEffects(1));
	EXPECT_EQ(reserved(*burst), 56);
	EXPECT_EQ(burst->getSignetBurstedCount(), 0);
	EXPECT_FALSE(burst->getSubEffect());
}

/**
 * The template's signetlvl caps the rune's level: 64601 (signetlvl 2) on a rune of level 3 bursts level 2 - (int) (581 * 0.5f) = 290,
 * 290 * 1.15f - 10 = 323.5 -> 323 - and still consumes the rune.
 */
TEST_F(SignetEffectsTest, TheTemplatesSignetLevelCapsTheBurst) {
	EFFECT_TEST_SCOPE;
	ASSERT_NO_FATAL_FAILURE(assertInputs());
	carve(8305);
	Ref<Effect> burst = calculated(64601, *mage, *npc);
	ASSERT_TRUE(burst->isInSuccessEffects(1));
	EXPECT_EQ(reserved(*burst), 323);
	EXPECT_EQ(burst->getSignetBurstedCount(), 2);
	EXPECT_FALSE(npc->getEffectController()->getAbnormalEffect(SIGNET1));
}

/**
 * A burst of another stack (64603, SIGNET2) finds no SIGNET2 rune: level 0 of SIGNET2's rows, (int) (581 * 0.12f) = 69,
 * 69 * 1.15f - 10 = 69.35 -> 69; the SIGNET1 rune is left alone.
 */
TEST_F(SignetEffectsTest, ABurstOfAnotherStackLeavesTheRune) {
	EFFECT_TEST_SCOPE;
	ASSERT_NO_FATAL_FAILURE(assertInputs());
	Ref<Effect> rune = carve(8305);
	Ref<Effect> burst = calculated(64603, *mage, *npc);
	ASSERT_TRUE(burst->isInSuccessEffects(1));
	EXPECT_EQ(reserved(*burst), 69);
	EXPECT_EQ(burst->getSignetBurstedCount(), 0);
	EXPECT_EQ(npc->getEffectController()->getAbnormalEffect(SIGNET1).get(), rune.get());
}

/**
 * A burst that EffectTemplate.calculate refuses (64602: its pre-effect position 2 is missing) consumes the rune all the same
 * (SignetBurstEffect.calculate: `if (!super.calculate(...)) signetEffect.endEffect()`), without damage or burst count.
 */
TEST_F(SignetEffectsTest, ARefusedBurstStillConsumesTheRune) {
	EFFECT_TEST_SCOPE;
	Ref<Effect> rune = carve(8305);
	Ref<Effect> burst = calculated(64602, *mage, *npc);
	EXPECT_FALSE(burst->isInSuccessEffects(1));
	EXPECT_EQ(burst->getSignetBurstedCount(), 0);
	EXPECT_FALSE(npc->getEffectController()->getAbnormalEffect(SIGNET1));
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(8305));
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 1000);
}

} // namespace
} // namespace aion::gameserver::skillengine::effect::mztest
