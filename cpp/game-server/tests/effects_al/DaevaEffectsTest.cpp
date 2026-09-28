// P5-03, M5e stage 1, item E-01 (m5e-plan.md §2.4, §15.4): the effect classes of this chunk that a Daeva of levels 10-20 reaches through its
// skills and the skills they launch - CondSkillLauncherEffect first (D7: the Gladiator's level-15 passive 563 Determination is applied at every
// enter world, W-07), then AuraEffect with its AuraTask, HostileUpEffect with its temporary hate task and DeathObserver, BindEffect,
// BoostSkillCastingTimeEffect, DeformEffect, DelayedSkillEffect, AlwaysParryEffect, DashEffect, BackDashEffect and CarveSignetEffect.
//
// Each case drives a real Effect through calculate -> applyEffect -> startEffect -> endEffect (EffectClassTestSupport.h,
// DaevaEffectsTestSupport.h) on skill templates whose attributes and <effects> are copied verbatim from skill_templates.xml (line cited per
// template; <properties>, conditions and motions left out: no Effect reads them), and asserts what the Java bodies do, with the golden values of
// their arithmetic derived by hand from the Java expressions. The fixture fails a case that reaches an AION_UNPORTED site (TearDown).

#include "DaevaEffectsTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MANTRA_EFFECT.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/DashStatus.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/Skill.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::effecttest {
namespace {

using controllers::attack::AttackStatus;
using effect::AbnormalState;
using gameserver::model::PlayerClass;
using gameserver::model::Race;
using gameserver::model::stats::container::StatEnum;
using model::Effect;
using network::aion::serverpackets::SM_ABNORMAL_EFFECT;
using network::aion::serverpackets::SM_ABNORMAL_STATE;
using network::aion::serverpackets::SM_MANTRA_EFFECT;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

/** Java AbnormalState.getId() of the states these bodies set (AbnormalState.java:9-43) */
constexpr int32_t SNARE_ID = 1 << 17;
constexpr int32_t BIND_ID = 1 << 20;
constexpr int32_t DEFORM_ID = 1 << 21;

// ---- the skills of the data (skill_templates.xml) ----------------------------------------------------------------------------------------

/** 563 "Determination" (:6481-6489), the Gladiator's level-15 passive: below 10 % HP it launches 8930 */
const std::string DETERMINATION_XML = templateXml(
	R"(skill_id="563" name="Determination" nameId="2287784" stack="P_TANACITY" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="NOSHOW")"
	R"( activation="PASSIVE" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<condskilllauncher skill_id="8930" type="HP" value="10" effectid="185" e="1" noresist="true" />)");

/** 563 with activation ACTIVE instead of PASSIVE: the launched effect then keeps its own duration (the `permanent` arm of $1.hpChanged) */
const std::string ACTIVE_DETERMINATION_XML = templateXml(
	R"(skill_id="7563" name="Determination" nameId="2287784" stack="EFFECTS_AL_DAEVA_ACTIVE_TANACITY" lvl="1" skilltype="MAGICAL" skillsubtype="NONE")"
	R"( tslot="NOSHOW" activation="ACTIVE" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<condskilllauncher skill_id="8930" type="HP" value="10" effectid="185" e="1" noresist="true" />)");

/** 8930 "Determination Effect" (:89082-89090): a provoker buff of duration2 10,000 ms */
const std::string DETERMINATION_EFFECT_XML = templateXml(
	R"(skill_id="8930" name="Determination Effect" nameId="2287788" stack="FI_N_TANACITY_PROC" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE")"
	R"( tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10" activation="PROVOKED" cooldown="0" duration="0")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<provoker skill_id="8931" provoke_target="OPPONENT" duration2="10000" effectid="183551" e="1" noresist="true" hittype="NMLATK")"
	R"( hittypeprob2="20" element="FIRE" />)");

/** 1809 "Celerity Mantra" (:27175-27184), the Chanter's level-10 toggle, with its <periodicactions> */
const std::string CELERITY_MANTRA_XML =
	R"(<skill_template skill_id="1809" name="Celerity Mantra" nameId="2286463" cooldownId="1519" group="CH_CHANT_IMPROVEDMOVE")"
	R"( stack="CH_CHANT_IMPROVEDMOVE" lvl="1" skilltype="MAGICAL" skillsubtype="CHANT" tslot="NOSHOW" activation="TOGGLE" cooldown="0" duration="0")"
	R"( cancel_rate="20" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true" apply_casting_time_bonus="true">)"
	R"(<effects><aura skill_id="8998" distance="25" distance_z="10" effectid="112141" e="1" noresist="true" /></effects>)"
	R"(<periodicactions checktime="6000"><mpuse value="0" /></periodicactions></skill_template>)";

/** 8998 "Celerity Mantra Effect" (:89906-89920): SPEED + 1000 for duration2 6,500 ms, re-applied by every tick of the mantra */
const std::string CELERITY_MANTRA_EFFECT_XML = templateXml(
	R"(skill_id="8998" name="Celerity Mantra Effect" nameId="2286465" stack="CH_N_CHANT_IMPROVEDMOVE_EFFECT" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="NONE" tslot="CHANT" activation="PROVOKED" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true")",
	R"(<statup duration2="6500" effectid="182841" e="1" noresist="true" hoptype="SKILLLV"><change stat="SPEED" func="ADD" value="1000" />)"
	R"(</statup>)");

/** 2945 "Incite Rage" (:49582-49595), the Templar's level-20 taunt, without its position 2 (a targetchange, P5-04's) */
const std::string INCITE_RAGE_XML = templateXml(
	R"(skill_id="2945" name="Incite Rage" nameId="2286923" cooldownId="402" group="KN_HIGHPROVOKE" stack="KN_HIGHPROVOKE" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="606" cooldown_delta_lv="-6" duration="0" stigma="BASIC")"
	R"( cancel_rate="10" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<hostileup value="1" e="1" noresist="true" element="FIRE" hoptype="SKILLLV" hopb="27895" temp_value="92478" temp_duration="5000" />)");

/** 2981 "Taunt" (:50118-50130), the Gladiator's and Templar's level-10 taunt, without its position 2 (a targetchange, P5-04's) */
const std::string TAUNT_XML = templateXml(
	R"(skill_id="2981" name="Taunt" nameId="2286947" cooldownId="125" group="WA_PROVOKE" stack="WA_PROVOKE" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="100" duration="0" cancel_rate="20" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<hostileup value="1" e="1" noresist="true" element="FIRE" hoptype="SKILLLV" hopb="14873" />)");

/**
 * 517 "Mocking Blast" (:5570-5586), the Gladiator's level-65 chain attack: a hit, then a hostileup of preeffect 1 - the one hostileup of the
 * data with a temp_delta (1378 per level)
 */
const std::string MOCKING_BLAST_XML = templateXml(
	R"(skill_id="517" name="Mocking Blast" nameId="2287742" cooldownId="160" group="FI_SLASHIGFORCE" stack="FI_SLASHIGFORCE" lvl="1")"
	R"( skilltype="PHYSICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="300" duration="0" ammospeed="45" cancel_rate="10")"
	R"( hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<skillatk value="14" e="1" accmod2="-100" critprobmod2="0" hoptype="DAMAGE" />)"
	R"(<hostileup value="1" e="2" noresist="true" element="FIRE" preeffect="1" hoptype="SKILLLV" hopb="41707" hopa="1378" temp_value="41707")"
	R"( temp_delta="1378" temp_duration="5000" />)");

/** 1574 "Binding Word" (:23302-23318), the Chanter's level-20 bind and snare */
const std::string BINDING_WORD_XML = templateXml(
	R"(skill_id="1574" name="Binding Word" nameId="2286341" cooldownId="1504" group="CH_SWORDBIND" stack="CH_SWORDBIND" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="20")"
	R"( activation="ACTIVE" cooldown="300" duration="0" pvp_duration="80" cancel_rate="20" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true" apply_casting_time_bonus="true")",
	R"(<bind duration2="5000" effectid="20014" e="1" accmod2="400" element="FIRE" hoptype="SKILLLV" hopb="800" />)"
	R"(<snare duration2="5000" effectid="20007" e="2" accmod2="240" element="EARTH" preeffect="1">)"
	R"(<change stat="SPEED" func="PERCENT" value="-50" /><change stat="FLY_SPEED" func="PERCENT" value="-50" /></snare>)");

/** 1350 "Boon of Quickness" (:19695-19709), the Sorcerer's level-20 self-buff: BOOST_CASTING_TIME_SKILL + 50 % */
const std::string BOON_OF_QUICKNESS_XML = templateXml(
	R"(skill_id="1350" name="Boon of Quickness" nameId="2288034" cooldownId="1588" group="WI_ICYVEINS" stack="WI_DARK_ICYVEINS" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE")"
	R"( cooldown="3000" duration="0" stigma="BASIC" cancel_rate="20" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true" apply_casting_time_bonus="true")",
	R"(<boostskillcastingtime duration2="15000" effectid="173" e="1" noresist="true" hoptype="SKILLLV" hopb="684">)"
	R"(<change stat="BOOST_CASTING_TIME_SKILL" func="PERCENT" value="50" /></boostskillcastingtime>)");

/** 873 "Dilation Arrow" (:11945-11969), the Ranger's level-20 debuff: its position 2 slows the target's casts by 100 % */
const std::string DILATION_ARROW_XML = templateXml(
	R"(skill_id="873" name="Dilation Arrow" nameId="2287836" cooldownId="115" group="RA_SEALINGARROW" stack="RA_SEALINGARROW" lvl="1")"
	R"( skilltype="MAGICAL" skill_category="PHYSICAL_DEBUFF" skillsubtype="ATTACK" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL")"
	R"( req_dispel_level="1" req_dispel_count="10" activation="ACTIVE" cooldown="300" duration="0" ammospeed="40" stigma="BASIC" cancel_rate="10")"
	R"( hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<skillatk value="113" e="1" accmod2="290" hoptype="DAMAGE" />)"
	R"(<boostskillcastingtime duration2="8000" randomtime="2000" effectid="127302" e="2" basiclvl="40" accmod2="150" element="WIND")"
	R"( preeffect="1" hoptype="SKILLLV" hopb="936"><change stat="BOOST_CASTING_TIME_SKILL" func="PERCENT" value="-100" /></boostskillcastingtime>)"
	R"(<slow duration2="6000" randomtime="2000" effectid="20008" e="3" basiclvl="80" accmod2="150" element="FIRE" preeffect="2")"
	R"( hoptype="SKILLLV"><change stat="ATTACK_SPEED" func="PERCENT" value="50" /></slow>)");

/** 1417 "Curse of Roots" (:20850-20872), the Sorcerer's level-10 deform, without its positions 2 and 3 (a sleep - P5-04's - and its statup) */
const std::string CURSE_OF_ROOTS_XML = templateXml(
	R"(skill_id="1417" name="Curse of Roots" nameId="2288107" cooldownId="1454" group="WI_CURSEDTREE" stack="WI_CURSEDTREE" lvl="1")"
	R"( skilltype="MAGICAL" skill_category="MENTAL_DEBUFF" skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="DEBUFF_MENTAL")"
	R"( req_dispel_level="1" req_dispel_count="10" activation="ACTIVE" cooldown="300" duration="1500" pvp_duration="40" cancel_rate="30")"
	R"( hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<deform model="833227" type="NONE" cantUseSkills="true" duration2="20000" effectid="175" e="1" accmod2="50" element="FIRE")"
	R"( hoptype="SKILLLV" hopb="6775" />)");

/** 4285 "Syncopated Echo" (:72722-72738), the Bard's level-10 delayed skill: 8905 after 5,000 ms */
const std::string SYNCOPATED_ECHO_XML = templateXml(
	R"(skill_id="4285" name="Syncopated Echo" nameId="2285997" cooldownId="1265" group="BA_SONGOFDELAYDAMAGE" stack="BA_SONGOFDELAYDAMAGE")"
	R"( lvl="1" skilltype="MAGICAL" skill_category="CHAIN_SKILL" skillsubtype="ATTACK" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL")"
	R"( req_dispel_level="1" req_dispel_count="10" activation="ACTIVE" cooldown="160" duration="0" ammospeed="50" cancel_rate="30")"
	R"( chain_skill_prob="100" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")"
	R"( apply_casting_time_bonus="true")",
	R"(<delayedskill skill_id="8905" value="1" duration2="5000" effectid="942171" e="1" element="FIRE" />)");

/** 8905 "Syncopated Echo Effect" (:88791-88803), with its <properties> (SkillEngine.applyEffectsDirectly validates the effected list) */
const std::string SYNCOPATED_ECHO_EFFECT_XML =
	R"(<skill_template skill_id="8905" name="Syncopated Echo Effect" nameId="2286000" stack="BA_N_SONGOFDELAYDAMAGE_SYS" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="ATTACK" tslot="NONE" activation="PROVOKED" cooldown="0" duration="0" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true">)"
	R"(<properties first_target="TARGET" first_target_range="25" target_relation="ENEMY" target_type="ONLYONE" />)"
	R"(<effects><procatk_instant value="279" e="1" accmod2="100" element="FIRE" hoptype="DAMAGE" /></effects></skill_template>)";

/** 2529 "Nullification Trigger" (:41367-41383), the Aethertech's level-20 self-buff: one resist and one parry */
const std::string NULLIFICATION_TRIGGER_XML = templateXml(
	R"(skill_id="2529" name="Nullification Trigger" nameId="2286793" cooldownId="1725" group="RI_EATHIUMCURTAIN" stack="RI_EATHIUMCURTAIN")"
	R"( lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10")"
	R"( activation="ACTIVE" cooldown="300" duration="0" cancel_rate="20" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true")",
	R"(<alwaysresist value="1" duration2="8000" effectid="204" e="1" noresist="true" hoptype="SKILLLV" hopb="584" />)"
	R"(<alwaysparry value="1" duration2="8000" effectid="143" e="2" noresist="true" preeffect="1" />)");

/** 1759 "Perfect Parry" (:26297-26309), the Chanter's level-44 parry: an alwaysparry alone */
const std::string PERFECT_PARRY_XML = templateXml(
	R"(skill_id="1759" name="Perfect Parry" nameId="2286441" cooldownId="1538" group="CH_CROSSPARRY" stack="CH_CROSSPARRY" lvl="1")"
	R"( skilltype="PHYSICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10")"
	R"( activation="ACTIVE" cooldown="300" duration="0" cancel_rate="20" hostile_type="INDIRECT")",
	R"(<alwaysparry value="1" duration2="10000" effectid="143" e="1" noresist="true" hoptype="SKILLLV" hopb="424" />)");

/** 3455 "Dash Attack" (:58392-58408), the Assassin's level-10 dash */
const std::string DASH_ATTACK_XML = templateXml(
	R"(skill_id="3455" name="Dash Attack" nameId="2287212" cooldownId="652" group="AS_DASHATTACK" stack="AS_DASHATTACK" lvl="1")"
	R"( skilltype="PHYSICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="400" duration="0" cancel_rate="10")"
	R"( hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<dash value="35" e="1" accmod2="3000" hoptype="DAMAGE" />)");

/** 2350 "Parting Shot" (:37821-37840), the Gunner's level-19 back dash of 15 m */
const std::string PARTING_SHOT_XML = templateXml(
	R"(skill_id="2350" name="Parting Shot" nameId="2286710" cooldownId="1850" group="GU_BACKSTEPPINGSNIPE" stack="GU_BACKSTEPPINGSNIPE" lvl="1")"
	R"( skilltype="PHYSICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="600" duration="0" ammospeed="80" cancel_rate="10")"
	R"( hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<backdash distance="15" value="155" e="1" accmod2="3000" hoptype="DAMAGE" />)");

/** 3385 "Rune Carve" (:57217-57227), the Assassin's level-13 carver of SIGNET1 (8303, cap 3) */
const std::string RUNE_CARVE_XML = templateXml(
	R"(skill_id="3385" name="Rune Carve" nameId="2287188" cooldownId="807" group="AS_CARVESIGNET" stack="AS_CARVESIGN" skilltype="PHYSICAL")"
	R"( skill_category="CHAIN_SKILL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="70" duration="0" cancel_rate="10")"
	R"( chain_skill_prob="100" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<carvesignet signet="SIGNET1" signet_id="8303" signet_cap="3" value="22" e="1" hoptype="DAMAGE" />)");

/**
 * 3365 "Ripclaw Strike" (:56834-56853), the Assassin's level-43 carver of SIGNET1 with signet_increment 5 and cap 5, then a hostileup of
 * preeffect 1
 */
const std::string RIPCLAW_STRIKE_XML = templateXml(
	R"(skill_id="3365" name="Ripclaw Strike" nameId="2287170" cooldownId="81" group="AS_TIGERBEAT" stack="AS_TIGERBEAT" lvl="1")"
	R"( skilltype="PHYSICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="600" duration="0" cancel_rate="10")"
	R"( hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<carvesignet signet="SIGNET1" signet_id="8303" signet_increment="5" signet_cap="5" value="382" e="1" basiclvl="20" hoptype="DAMAGE" />)"
	R"(<hostileup value="1" e="2" noresist="true" element="FIRE" preeffect="1" hoptype="SKILLLV" hopb="1857" />)");

/** 3385 with prob="0": Rnd.chance() >= 0 always, so no signet is carved */
const std::string RUNE_CARVE_NEVER_XML = templateXml(
	R"(skill_id="7385" name="Rune Carve" nameId="2287188" stack="EFFECTS_AL_DAEVA_CARVESIGN" skilltype="PHYSICAL" skillsubtype="ATTACK")"
	R"( tslot="NONE" activation="ACTIVE" cooldown="70" duration="0")",
	R"(<carvesignet signet="SIGNET1" signet_id="8303" signet_cap="3" prob="0" value="22" e="1" hoptype="DAMAGE" />)");

/**
 * 8303-8307 "Rune Carve I-V" (:81187-81251), stack SIGNET1 of levels 1-5, with their <signet> (SignetEffect, P5-04's, ported by the parallel
 * effects-mz lane) replaced by a <statdown> of the same duration2: CarveSignetEffect only finds the active signet by its stack and reads the
 * level it carved into the Effect. The statdown's PHYSICAL_DEFENSE - 1 shows how many signets hold stat functions: the data's signets have no
 * effectid, so EffectController.addEffect's put replaces the stack's entry without ending the previous one - only CarveSignetEffect's
 * activeSignet.endEffect() does.
 */
std::string runeCarveSignetXml(int32_t skillId, int32_t level) {
	return templateXml(R"(skill_id=")" + std::to_string(skillId) + R"(" name="Rune Carve" nameId="284072" stack="SIGNET1" lvl=")"
			+ std::to_string(level) + R"(" skilltype="MAGICAL" skillsubtype="NONE" tslot="DEBUFF" activation="PROVOKED" cooldown="0" duration="0")",
		R"(<statdown duration2="24000" e="1" noresist="true" element="FIRE" hoptype="SKILLLV">)"
		R"(<change stat="PHYSICAL_DEFENSE" func="ADD" value="-1" /></statdown>)");
}

/**
 * An NpcAI that answers IS_IMMUNE_TO_ABNORMAL_STATES true, as NpcAI.ask does for a boss or a static npc (`isBoss() || hasStatic()`), which the
 * test npc template (rating NORMAL, no static id) is not
 */
class ImmuneNpcAI final : public ai::NpcAI {
public:
	explicit ImmuneNpcAI(Npc& owner) : NpcAI(owner) {}

	bool ask(ai::poll::AIQuestion question) override {
		return question == ai::poll::AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES || NpcAI::ask(question);
	}
};

class DaevaEffectsTest : public DaevaEffectTest {};

// ---- CondSkillLauncherEffect (CondSkillLauncherEffect.java:20-62) -------------------------------------------------------------------------

/**
 * 563 Determination through the enter-world path (PlayerEnterWorldService.activatePassiveSkillEffects: SkillEngine.applyEffectDirectly(template,
 * level, player, player)), which does not catch (W-07): applyEffect adds the passive to the controller and startEffect attaches the
 * HP_CHANGED observer. `hpValue <= value * maxHp / 100` - 10 * maxHp / 100 in int arithmetic - applies 8930 once with a forced duration of 0
 * (the skill is PASSIVE: permanent), a second report below the threshold applies nothing, a report above it ends 8930, and the next one below
 * applies a new 8930. Ending the passive removes the observer, and ObserveController.removeObserver's onRemoved ends the conditional effect.
 */
TEST_F(DaevaEffectsTest, DeterminationLaunchesAPermanentBuffAtTenPercentHpAndEndsItAbove) {
	EFFECT_TEST_SCOPE;
	Ref<Player> gladiator = makePlayer(8101, PlayerClass::GLADIATOR);
	const model::SkillTemplate* determination = bindSkill(DETERMINATION_XML);
	bindSkill(DETERMINATION_EFFECT_XML);
	ASSERT_EQ(effectOf(*determination, 0).javaClassName(), "CondSkillLauncherEffect");
	const int32_t maxHp = gladiator->getLifeStats()->getMaxHp();
	const int32_t threshold = 10 * maxHp / 100;
	ASSERT_GT(threshold, 1);

	Ref<Effect> passive = SkillEngine::getInstance().applyEffectDirectly(determination, 1, *gladiator, *gladiator);
	ASSERT_TRUE(passive->isInSuccessEffects(1));
	EXPECT_EQ(gladiator->getEffectController()->findBySkillId(563), passive) << "CondSkillLauncherEffect.applyEffect: addToEffectedController";
	EXPECT_EQ(passive->getDuration(), 0) << "a passive lasts";

	gladiator->getLifeStats()->setCurrentHp(threshold + 1);
	EXPECT_FALSE(gladiator->getEffectController()->findBySkillId(8930)) << "above value % of the maximum";

	gladiator->getLifeStats()->setCurrentHp(threshold);
	Ptr<Effect> launched = gladiator->getEffectController()->findBySkillId(8930);
	ASSERT_TRUE(launched) << "hpValue <= value * maxHp / 100";
	EXPECT_EQ(launched->getEffector(), gladiator);
	EXPECT_EQ(launched->getEffected(), gladiator);
	EXPECT_EQ(launched->getDuration(), 0) << "PASSIVE: applyEffectDirectly(skillId, effected, effected, 0, null)";
	advance(60000);
	EXPECT_EQ(gladiator->getEffectController()->findBySkillId(8930), launched) << "a forced duration of 0 never ends by time";

	gladiator->getLifeStats()->setCurrentHp(threshold - 1);
	EXPECT_EQ(gladiator->getEffectController()->findBySkillId(8930), launched) << "conditionalEffect != null: nothing is applied again";

	gladiator->getLifeStats()->setCurrentHp(threshold + 1);
	EXPECT_FALSE(gladiator->getEffectController()->findBySkillId(8930)) << "above the threshold: conditionalEffect.endEffect()";

	gladiator->getLifeStats()->setCurrentHp(threshold);
	Ptr<Effect> relaunched = gladiator->getEffectController()->findBySkillId(8930);
	ASSERT_TRUE(relaunched) << "conditionalEffect was set to null: the next report below applies again";
	EXPECT_NE(relaunched, launched);

	passive->endEffect();
	EXPECT_FALSE(gladiator->getEffectController()->findBySkillId(563));
	EXPECT_FALSE(gladiator->getEffectController()->findBySkillId(8930)) << "removeObservers -> removeObserver -> $1.onRemoved";
	gladiator->getLifeStats()->setCurrentHp(threshold - 1);
	EXPECT_FALSE(gladiator->getEffectController()->findBySkillId(8930)) << "the observer went with the passive";
}

/**
 * The same launcher on a template that is not PASSIVE: `Integer duration = permanent ? 0 : null`, so 8930 keeps its own duration2 of 10,000 ms.
 * When it has ended by time, conditionalEffect still names it: HP below the threshold applies nothing until a report above it has ended the
 * (already ended) effect and cleared the field.
 */
TEST_F(DaevaEffectsTest, ANonPassiveLauncherAppliesTheBuffForItsOwnDuration) {
	EFFECT_TEST_SCOPE;
	Ref<Player> gladiator = makePlayer(8111, PlayerClass::GLADIATOR);
	const model::SkillTemplate* launcher = bindSkill(ACTIVE_DETERMINATION_XML);
	bindSkill(DETERMINATION_EFFECT_XML);
	const int32_t threshold = 10 * gladiator->getLifeStats()->getMaxHp() / 100;

	Ref<Effect> effect = cast(*gladiator, *gladiator, launcher, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	gladiator->getLifeStats()->setCurrentHp(threshold);
	Ptr<Effect> launched = gladiator->getEffectController()->findBySkillId(8930);
	ASSERT_TRUE(launched);
	EXPECT_EQ(launched->getDuration(), 10000) << "duration null: the provoker's duration2";
	advance(10000);
	EXPECT_TRUE(launched->isEndedByTime());
	EXPECT_FALSE(gladiator->getEffectController()->findBySkillId(8930));

	gladiator->getLifeStats()->setCurrentHp(threshold - 1);
	EXPECT_FALSE(gladiator->getEffectController()->findBySkillId(8930)) << "conditionalEffect still names the ended effect";
	gladiator->getLifeStats()->setCurrentHp(threshold + 1);
	gladiator->getLifeStats()->setCurrentHp(threshold);
	EXPECT_TRUE(gladiator->getEffectController()->findBySkillId(8930)) << "cleared above the threshold, applied again below it";
	effect->endEffect();
	EXPECT_FALSE(gladiator->getEffectController()->findBySkillId(8930));
}

// ---- AuraEffect (AuraEffect.java:25-102) --------------------------------------------------------------------------------------------------

/**
 * 1809 Celerity Mantra on an online Chanter without a team: startEffect schedules the AuraTask at a fixed rate of 6,500 ms from 0 and keeps it
 * as the effect's periodic task. Every run applies 8998 to the Chanter (SkillEngine.applyEffect(skillId, effected, effected): SPEED + 1000 for
 * 6,500 ms, one effect of the stack at a time) and broadcasts SM_MANTRA_EFFECT(effector, 8998) to the players who see the Chanter - never to
 * the Chanter itself (broadcastPacket with a Creature; the npc case below shows what the others get). Ending the mantra cancels the task
 * (Effect.stopTasks, cycles.toml "AuraEffect.AuraTask.effect"): no run follows, and the last 8998 runs out.
 */
TEST_F(DaevaEffectsTest, CelerityMantraReappliesItsEffectEverySixAndAHalfSeconds) {
	EFFECT_TEST_SCOPE;
	Ref<Player> chanter = makePlayer(8121, PlayerClass::CHANTER);
	cp::RecordingAionConnection& self = connectLast(*chanter);
	const model::SkillTemplate* mantra = bindSkill(CELERITY_MANTRA_XML);
	bindSkill(CELERITY_MANTRA_EFFECT_XML);
	ASSERT_EQ(effectOf(*mantra, 0).javaClassName(), "AuraEffect");
	const int32_t speed = statOf(*chanter, StatEnum::SPEED, 0);
	self.clearSent();

	Ref<Effect> aura = cast(*chanter, *chanter, mantra, 1);
	ASSERT_TRUE(aura->isInSuccessEffects(1));
	EXPECT_EQ(chanter->getEffectController()->findBySkillId(1809), aura) << "AuraEffect.applyEffect: addToEffectedController";
	EXPECT_TRUE(aura->isPeriodic()) << "Effect.setPeriodicTask(AuraTask, position)";
	EXPECT_FALSE(chanter->getEffectController()->hasAbnormalEffect(8998)) << "the first run is due at 0, not inline";

	executor->runReady();
	Ptr<Effect> mantraEffect = chanter->getEffectController()->findBySkillId(8998);
	ASSERT_TRUE(mantraEffect) << "the first run: applyAuraTo(effector)";
	EXPECT_EQ(mantraEffect->getEffector(), chanter);
	EXPECT_EQ(mantraEffect->getDuration(), 6500);
	EXPECT_EQ(statOf(*chanter, StatEnum::SPEED, 0), speed + 1000);

	advance(6499);
	EXPECT_EQ(chanter->getEffectController()->findBySkillId(8998), mantraEffect);
	advance(1);
	Ptr<Effect> secondRun = chanter->getEffectController()->findBySkillId(8998);
	ASSERT_TRUE(secondRun) << "the second run at 6,500 ms";
	EXPECT_NE(secondRun, mantraEffect);
	advance(6500);
	Ptr<Effect> thirdRun = chanter->getEffectController()->findBySkillId(8998);
	ASSERT_TRUE(thirdRun) << "the third run at 13,000 ms";
	EXPECT_NE(thirdRun, secondRun);
	EXPECT_EQ(statOf(*chanter, StatEnum::SPEED, 0), speed + 1000) << "one 8998 at a time";
	EXPECT_TRUE(packetsOf<SM_MANTRA_EFFECT>(self).empty()) << "broadcastPacket(effector, ...) skips the effector";

	aura->endEffect();
	EXPECT_FALSE(aura->isPeriodic()) << "Effect.stopTasks cancelled the AuraTask";
	advance(6499);
	EXPECT_EQ(chanter->getEffectController()->findBySkillId(8998), thirdRun) << "no run after the end";
	advance(1);
	EXPECT_FALSE(chanter->getEffectController()->hasAbnormalEffect(8998)) << "the last 8998 ran out";
	EXPECT_EQ(statOf(*chanter, StatEnum::SPEED, 0), speed);
}

/**
 * The AuraTask's "task check": a Chanter who is not online (no client connection) gets nothing (`!p.isOnline()`: return). A second cast of the
 * mantra while the first runs is refused by AuraEffect.applyEffect (the CM_CASTSPELL abuse audit: findBySkillId(effect.getSkillId()) != null):
 * it is not added to the controller - the first effect stays there - so its startEffect never schedules a second task.
 */
TEST_F(DaevaEffectsTest, TheMantraSkipsAnOfflineChanterAndRefusesASecondCast) {
	EFFECT_TEST_SCOPE;
	Ref<Player> offline = makePlayer(8131, PlayerClass::CHANTER);
	const model::SkillTemplate* mantra = bindSkill(CELERITY_MANTRA_XML);
	bindSkill(CELERITY_MANTRA_EFFECT_XML);
	Ref<Effect> offlineAura = cast(*offline, *offline, mantra, 1);
	ASSERT_TRUE(offlineAura->isPeriodic());
	advance(6500);
	EXPECT_FALSE(offline->getEffectController()->hasAbnormalEffect(8998)) << "!p.isOnline(): return";
	offlineAura->endEffect();

	Ref<Player> chanter = makePlayer(8133, PlayerClass::CHANTER);
	connectLast(*chanter);
	Ref<Effect> first = cast(*chanter, *chanter, mantra, 1);
	Ref<Effect> second = cast(*chanter, *chanter, mantra, 1);
	ASSERT_TRUE(second->isInSuccessEffects(1)) << "calculate lands; applyEffect refuses";
	EXPECT_EQ(chanter->getEffectController()->findBySkillId(1809), first) << "the second effect was not added";
	EXPECT_FALSE(second->isPeriodic()) << "never started";
	EXPECT_TRUE(first->isPeriodic());
	first->endEffect();
}

/**
 * An npc's aura applies the effect to the npc itself (the `effector instanceof Npc` arm, no online check), and the players who see the npc get
 * SM_MANTRA_EFFECT: 0, the effector's object id, the launched skill id (SM_MANTRA_EFFECT.java:20-24)
 */
TEST_F(DaevaEffectsTest, AnNpcsAuraAppliesItsEffectToTheNpc) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700801);
	cp::RecordingAionConnection& observer = observe(*npc, 8141);
	const model::SkillTemplate* mantra = bindSkill(CELERITY_MANTRA_XML);
	bindSkill(CELERITY_MANTRA_EFFECT_XML);
	observer.clearSent();

	Ref<Effect> aura = cast(*npc, *npc, mantra, 1);
	ASSERT_TRUE(aura->isInSuccessEffects(1));
	executor->runReady();
	Ptr<Effect> mantraEffect = npc->getEffectController()->findBySkillId(8998);
	ASSERT_TRUE(mantraEffect);
	EXPECT_EQ(mantraEffect->getEffector(), npc);
	std::vector<std::vector<uint8_t>> mantras = packetsOf<SM_MANTRA_EFFECT>(observer);
	ASSERT_EQ(mantras.size(), 1u);
	network::test::PacketReader reader(cp::bodyOf(mantras[0]));
	EXPECT_EQ(reader.D(), 0);
	EXPECT_EQ(reader.D(), npc->getObjectId());
	EXPECT_EQ(static_cast<uint16_t>(reader.H()), 8998);
	advance(6500);
	EXPECT_EQ(packetsOf<SM_MANTRA_EFFECT>(observer).size(), 2u) << "the second run";
	aura->endEffect();
	advance(6500);
	EXPECT_EQ(packetsOf<SM_MANTRA_EFFECT>(observer).size(), 2u) << "no run after the end";
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(8998));
}

// ---- HostileUpEffect (HostileUpEffect.java:23-65) -----------------------------------------------------------------------------------------

/**
 * 2945 Incite Rage on a monster, derived by hand from the Java (the Templar's and the monster's BOOST_HATE are the base 100):
 * calculate sets tauntHate = value + delta * level = 1 and tempHate = calculateHate(effected, temp_value + temp_delta * level)
 * = 92478 * (1000 + 100) / 1000 = 101725; Effect.initialize's effectHate = calculateHate(effector, hopb 27895) = 30684.
 * applyEffect: the only success effect, so totalHate = 1 + 30684, and addHate(effector, 30685 + 101725 = 132410). The temporary share is taken
 * back after temp_duration 5,000 ms (addHate(effector, -101725): 30685) by a task, and the DeathObserver attached once to the Templar is
 * removed with it.
 */
TEST_F(DaevaEffectsTest, InciteRageAddsTemporaryHateForFiveSeconds) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700811);
	Ref<Player> templar = makePlayer(8151, PlayerClass::TEMPLAR);
	ASSERT_TRUE(KnownListPairing::pair(*npc, *templar));
	ASSERT_EQ(statOf(*templar, StatEnum::BOOST_HATE, 100), 100);
	ASSERT_EQ(statOf(*npc, StatEnum::BOOST_HATE, 100), 100);
	const model::SkillTemplate* inciteRage = bindSkill(INCITE_RAGE_XML);
	ASSERT_EQ(effectOf(*inciteRage, 0).javaClassName(), "HostileUpEffect");
	ASSERT_FALSE(templar->getObserveController()->hasObservers());

	Ref<Effect> effect = calculated(*templar, *npc, inciteRage);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getTauntHate(), 1);
	EXPECT_EQ(effect->getHostileUpTempHate(), 101725) << "92478 * 1100 / 1000";
	EXPECT_EQ(effect->getEffectHate(), 30684) << "27895 * 1100 / 1000";
	effect->applyEffect();
	EXPECT_EQ(npc->getAggroList().getHate(*templar), 132410) << "1 + 30684 + 101725";
	EXPECT_TRUE(templar->getObserveController()->hasObservers()) << "the DeathObserver, attached to the effector";

	advance(4999);
	EXPECT_EQ(npc->getAggroList().getHate(*templar), 132410);
	advance(1);
	EXPECT_EQ(npc->getAggroList().getHate(*templar), 30685) << "the task at temp_duration: addHate(effector, -tempHate)";
	EXPECT_FALSE(templar->getObserveController()->hasObservers()) << "the task removed the DeathObserver";
}

/** The Templar dies before temp_duration: the DeathObserver cancels the task (task.cancel(false)), so the temporary hate stays */
TEST_F(DaevaEffectsTest, TheEffectorsDeathCancelsTheTemporaryHatesRemoval) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700821);
	Ref<Player> templar = makePlayer(8161, PlayerClass::TEMPLAR);
	ASSERT_TRUE(KnownListPairing::pair(*npc, *templar));
	const model::SkillTemplate* inciteRage = bindSkill(INCITE_RAGE_XML);

	cast(*templar, *npc, inciteRage, 1);
	ASSERT_EQ(npc->getAggroList().getHate(*templar), 132410);
	const size_t pending = executor->pendingTaskCount();
	templar->getObserveController()->notifyDeathObservers(*npc);
	EXPECT_EQ(executor->pendingTaskCount(), pending - 1) << "the cancelled removal task";
	EXPECT_FALSE(templar->getObserveController()->hasObservers()) << "attach: one-time use";
	advance(5000);
	EXPECT_EQ(npc->getAggroList().getHate(*templar), 132410) << "never taken back";
}

/**
 * 2981 Taunt has no temp_value: tempHate = 0 is not above 0, so calculate does not scale it and applyEffect schedules nothing; the monster
 * gets tauntHate 1 + effectHate calculateHate(effector, 14873) = 16360. A player effected gets no hate (the `effected instanceof Npc` arm).
 */
TEST_F(DaevaEffectsTest, TauntAddsItsHateWithoutATask) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700831);
	Ref<Player> gladiator = makePlayer(8171, PlayerClass::GLADIATOR);
	ASSERT_TRUE(KnownListPairing::pair(*npc, *gladiator));
	const model::SkillTemplate* taunt = bindSkill(TAUNT_XML);
	const size_t pending = executor->pendingTaskCount();

	Ref<Effect> effect = cast(*gladiator, *npc, taunt, 1);
	EXPECT_EQ(effect->getHostileUpTempHate(), 0);
	EXPECT_EQ(npc->getAggroList().getHate(*gladiator), 16361) << "1 + 14873 * 1100 / 1000";
	EXPECT_EQ(executor->pendingTaskCount(), pending) << "tempHate 0: no removal task";
	EXPECT_FALSE(gladiator->getObserveController()->hasObservers());

	Ref<Player> other = makePlayer(8172, PlayerClass::GLADIATOR);
	Ref<Effect> onPlayer = cast(*gladiator, *other, taunt, 1);
	EXPECT_TRUE(onPlayer->isInSuccessEffects(1));
	EXPECT_EQ(onPlayer->getTauntHate(), 1) << "calculate runs for any effected";
	EXPECT_EQ(executor->pendingTaskCount(), pending);
}

/**
 * 517 Mocking Blast at skill level 2 on a monster: a hostileup at position 2, as the data's taunts have one, derived by hand from the Java.
 * calculate: tauntHate = value 1; tempHate = temp_value + temp_delta * level = 41707 + 1378 * 2 = 44463, scaled by the monster's hate boost:
 * 44463 * 1100 / 1000 = 48909. Two success effects, so HostileUpEffect.applyEffect adds no effectHate (the FIXME workaround is for a template
 * alone; the effect's own hate, calculateHate(effector, 1 + 44463) = 48910, is never broadcast here: applyEffect with success effects does not,
 * and the effect is added to no controller). The hit of position 1 comes first: AggroList.addDamage adds calculateHate(effector, damage * 10) =
 * damage * 11 (hoptype DAMAGE). So the monster's hate is damage * 11 + 1 + 48909, and damage * 11 + 1 after temp_duration 5,000 ms. The effect
 * is forced (SkillEngine.applyEffectDirectly): the physical hit of position 1 (accmod2 -100) would roll a dodge.
 */
TEST_F(DaevaEffectsTest, MockingBlastAddsItsTemporaryHateWithoutTheEffectHate) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700835);
	Ref<Player> gladiator = makePlayer(8175, PlayerClass::GLADIATOR);
	ASSERT_TRUE(KnownListPairing::pair(*npc, *gladiator));
	ASSERT_EQ(statOf(*gladiator, StatEnum::BOOST_HATE, 100), 100);
	ASSERT_EQ(statOf(*npc, StatEnum::BOOST_HATE, 100), 100);
	const model::SkillTemplate* mockingBlast = bindSkill(MOCKING_BLAST_XML);
	ASSERT_EQ(effectOf(*mockingBlast, 1).javaClassName(), "HostileUpEffect");

	Ref<Effect> effect = forced(*gladiator, *npc, mockingBlast, 2);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	ASSERT_TRUE(effect->isInSuccessEffects(2));
	const int32_t damage = effect->getReserveds(1)->getValue();
	ASSERT_GT(damage, 0);
	ASSERT_LT(damage, npc->getLifeStats()->getMaxHp());
	EXPECT_EQ(effect->getHostileUpTempHate(), 48909) << "(41707 + 1378 * 2) * 1100 / 1000";
	EXPECT_EQ(npc->getAggroList().getHate(*gladiator), damage * 11 + 1 + 48909) << "two success effects: no effectHate";

	advance(5000);
	EXPECT_EQ(npc->getAggroList().getHate(*gladiator), damage * 11 + 1) << "the task took tempHate back";
}

// ---- BindEffect (BindEffect.java:17-42) ---------------------------------------------------------------------------------------------------

/**
 * 1574 Binding Word on a monster that is casting a PHYSICAL skill: calculate passes BIND_RESISTANCE; startEffect sets BIND on the effect and the
 * controller and cancels the physical cast (cancelCurrentSkill(effector): the Chanter gets STR_SKILL_TARGET_SKILL_CANCELED); the snare of
 * position 2 adds SNARE. Both hold duration2 5,000 ms; the end unsets BIND. A MAGICAL cast is not cancelled.
 */
TEST_F(DaevaEffectsTest, BindingWordBindsAMonsterAndCancelsItsPhysicalCast) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700841);
	cp::RecordingAionConnection& observer = observe(*npc, 8181);
	Ref<Player> chanter = players.back();
	ASSERT_NO_FATAL_FAILURE(assertNoMagicalResist(*chanter, *npc));
	const model::SkillTemplate* bindingWord = bindSkill(BINDING_WORD_XML);
	const model::SkillTemplate* physical = bindSkill(DASH_ATTACK_XML);
	const model::SkillTemplate* magical = bindSkill(CURSE_OF_ROOTS_XML);
	ASSERT_EQ(effectOf(*bindingWord, 0).javaClassName(), "BindEffect");
	npc->setCasting(model::Skill::create(physical, *npc, 1, Ptr<Creature>(chanter), nullptr));
	observer.clearSent();

	Ref<Effect> effect = cast(*chanter, *npc, bindingWord, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_FALSE(npc->getCastingSkill()) << "a PHYSICAL cast: cancelCurrentSkill(effector)";
	EXPECT_EQ(countMessages(observer, SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_SKILL_CANCELED()), 1)
		<< "the effector, a player, is the lastAttacker who is told (CreatureController.java:525-526)";
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::BIND));
	EXPECT_EQ(effect->getAbnormals(), BIND_ID | SNARE_ID);
	EXPECT_EQ(effect->getDuration(), 5000);
	std::vector<std::vector<uint8_t>> announced = packetsOf<SM_ABNORMAL_EFFECT>(observer);
	ASSERT_EQ(announced.size(), 1u);
	EXPECT_EQ(decodeAbnormalEffect(announced[0]).abnormals & BIND_ID, BIND_ID);
	advance(5000);
	EXPECT_TRUE(effect->isEndedByTime());
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::BIND)) << "BindEffect.endEffect";

	npc->setCasting(model::Skill::create(magical, *npc, 1, Ptr<Creature>(chanter), nullptr));
	Ref<Effect> again = cast(*chanter, *npc, bindingWord, 1);
	ASSERT_TRUE(again->isInSuccessEffects(1));
	EXPECT_TRUE(npc->getCastingSkill()) << "a MAGICAL cast goes on";
	again->endEffect();
}

/** BindEffect.calculate passes BIND_RESISTANCE (BindEffect.java:26), not another resistance */
TEST_F(DaevaEffectsTest, BindResistanceDecidesTheBindsCalculate) {
	EFFECT_TEST_SCOPE;
	Ref<Player> chanter = makePlayer(8191, PlayerClass::CHANTER);
	Ref<Npc> bindResistant = makeMonster(700851);
	addStat(*bindResistant, StatEnum::BIND_RESISTANCE, 1000);
	Ref<Npc> fearResistant = makeMonster(700852, 505, 505);
	addStat(*fearResistant, StatEnum::FEAR_RESISTANCE, 1000);
	const model::SkillTemplate* bindingWord = bindSkill(BINDING_WORD_XML);

	EXPECT_FALSE(calculated(*chanter, *bindResistant, bindingWord)->isInSuccessEffects(1));
	EXPECT_TRUE(calculated(*chanter, *fearResistant, bindingWord)->isInSuccessEffects(1));
}

// ---- BoostSkillCastingTimeEffect (BoostSkillCastingTimeEffect.java:16-33) ----------------------------------------------------------------

/**
 * 1350 Boon of Quickness on the Sorcerer itself: not an enemy, so super.calculate(effect) without a resistance; BufEffect.startEffect adds the
 * PERCENT 50 as a bonus rate (1000 -> 1500 of BOOST_CASTING_TIME_SKILL) for duration2 15,000 ms.
 */
TEST_F(DaevaEffectsTest, BoonOfQuicknessRaisesTheCastingSpeedForFifteenSeconds) {
	EFFECT_TEST_SCOPE;
	Ref<Player> sorcerer = makePlayer(8201, PlayerClass::SORCERER);
	const model::SkillTemplate* boon = bindSkill(BOON_OF_QUICKNESS_XML);
	ASSERT_EQ(effectOf(*boon, 0).javaClassName(), "BoostSkillCastingTimeEffect");
	ASSERT_EQ(statOf(*sorcerer, StatEnum::BOOST_CASTING_TIME_SKILL, 1000), 1000);

	Ref<Effect> effect = cast(*sorcerer, *sorcerer, boon, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(statOf(*sorcerer, StatEnum::BOOST_CASTING_TIME_SKILL, 1000), 1500) << "1000 * 50 / 100 as a bonus";
	EXPECT_EQ(effect->getDuration(), 15000);
	advance(15000);
	EXPECT_EQ(statOf(*sorcerer, StatEnum::BOOST_CASTING_TIME_SKILL, 1000), 1000);
}

/**
 * 873 Dilation Arrow's position 2 on an enemy monster: the monster's BOOST_CASTING_TIME_SKILL is 1000 - 100 % = 0 for 8,000 - Rnd.get(0,
 * 2000) ms. SLOW_RESISTANCE never resists it, although a negative change on an enemy makes calculate pass it (super.calculate(effect,
 * SLOW_RESISTANCE, null)): Effects.normalizeNoResist makes every boostskillcastingtime noresist at load (Effects.java:20-29, 176-179), so
 * isDodgedOrResisted is false. What the SLOW_RESISTANCE arm still decides is the altered-state immunity EffectTemplate.calculate checks for a
 * stat before anything else (EffectTemplate.java:304-306): an npc whose AI answers IS_IMMUNE_TO_ABNORMAL_STATES (a boss or a static npc, NpcAI)
 * refuses the enemy's slow; the plain super.calculate(effect) of an npc that is no enemy (GENERAL: no tribe relation), or of a boost without a
 * negative change (1350), passes no stat and lands on it.
 */
TEST_F(DaevaEffectsTest, ANegativeCastingBoostOnAnEnemyPassesSlowResistanceToCalculate) {
	EFFECT_TEST_SCOPE;
	publishTribeRelationsWithNeutral();
	Ref<Player> ranger = makePlayer(8211, PlayerClass::RANGER);
	Ref<Npc> monster = makeMonster(700861);
	addStat(*monster, StatEnum::SLOW_RESISTANCE, 1000);
	Ref<Npc> immune = makeMonster(700862, 505, 505);
	immune->replaceAi(std::make_unique<ImmuneNpcAI>(*immune));
	Ref<Npc> neutral = makeNpc(700863, {}, 510, 500);
	neutral->replaceAi(std::make_unique<ImmuneNpcAI>(*neutral));
	ASSERT_NO_FATAL_FAILURE(assertNoMagicalResist(*ranger, *monster));
	const model::SkillTemplate* dilation = bindSkill(DILATION_ARROW_XML);
	const model::SkillTemplate* boon = bindSkill(BOON_OF_QUICKNESS_XML);
	ASSERT_EQ(effectOf(*dilation, 1).javaClassName(), "BoostSkillCastingTimeEffect");
	ASSERT_TRUE(effectOf(*dilation, 1).isNoResist()) << "Effects.normalizeNoResist";
	ASSERT_TRUE(immune->isEnemy(*ranger));
	ASSERT_FALSE(neutral->isEnemy(*ranger));

	Ref<Effect> slowed = cast(*ranger, *monster, dilation, 1);
	ASSERT_TRUE(slowed->isInSuccessEffects(1));
	ASSERT_TRUE(slowed->isInSuccessEffects(2)) << "SLOW_RESISTANCE 1000 does not resist a noresist effect";
	EXPECT_EQ(statOf(*monster, StatEnum::BOOST_CASTING_TIME_SKILL, 1000), 0) << "1000 - 1000 * 100 / 100";
	EXPECT_GE(slowed->getDuration(), 6000);
	EXPECT_LE(slowed->getDuration(), 8000) << "duration2 - Rnd.get(0, randomtime)";

	Ref<Effect> refused = calculated(*ranger, *immune, dilation);
	EXPECT_TRUE(refused->isInSuccessEffects(1));
	EXPECT_FALSE(refused->isInSuccessEffects(2)) << "an enemy's negative change: SLOW_RESISTANCE, an altered state the npc is immune to";
	EXPECT_TRUE(calculated(*ranger, *neutral, dilation)->isInSuccessEffects(2)) << "no enemy: super.calculate(effect)";
	EXPECT_TRUE(calculated(*ranger, *immune, boon)->isInSuccessEffects(1)) << "no negative change: super.calculate(effect)";
	slowed->endEffect();
	EXPECT_EQ(statOf(*monster, StatEnum::BOOST_CASTING_TIME_SKILL, 1000), 1000);
}

// ---- DeformEffect (DeformEffect.java:15-39) and its base TransformEffect ---------------------------------------------------------------------

/**
 * 1417 Curse of Roots' deform on a monster: DEFORM_RESISTANCE decides calculate; TransformEffect.startEffect applies model 833227 with
 * cantUseSkills, and DEFORM is set on the controller and the effect for duration2 20,000 ms. The end (TransformEffect.endEffect: no other
 * transform effect, so endTransformation) removes the model and unsets DEFORM.
 */
TEST_F(DaevaEffectsTest, CurseOfRootsDeformsAMonsterForTwentySeconds) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700871);
	cp::RecordingAionConnection& observer = observe(*npc, 8221);
	Ref<Player> sorcerer = players.back();
	ASSERT_NO_FATAL_FAILURE(assertNoMagicalResist(*sorcerer, *npc));
	const model::SkillTemplate* curseOfRoots = bindSkill(CURSE_OF_ROOTS_XML);
	ASSERT_EQ(effectOf(*curseOfRoots, 0).javaClassName(), "DeformEffect");
	ASSERT_FALSE(npc->getTransformModel().isActive());
	observer.clearSent();

	Ref<Effect> effect = cast(*sorcerer, *npc, curseOfRoots, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_TRUE(npc->getTransformModel().isActive());
	EXPECT_EQ(npc->getTransformModel().getModelId(), 833227);
	EXPECT_TRUE(npc->getTransformModel().cantUseSkills());
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::DEFORM));
	EXPECT_EQ(effect->getAbnormals(), DEFORM_ID);
	EXPECT_EQ(effect->getDuration(), 20000);
	std::vector<std::vector<uint8_t>> announced = packetsOf<SM_ABNORMAL_EFFECT>(observer);
	ASSERT_FALSE(announced.empty());
	EXPECT_EQ(decodeAbnormalEffect(announced.back()).abnormals, DEFORM_ID);

	advance(20000);
	EXPECT_TRUE(effect->isEndedByTime());
	EXPECT_FALSE(npc->getTransformModel().isActive()) << "TransformEffect.endEffect: endTransformation";
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::DEFORM)) << "DeformEffect.endEffect";

	Ref<Npc> resistant = makeMonster(700872, 505, 505);
	addStat(*resistant, StatEnum::DEFORM_RESISTANCE, 1000);
	EXPECT_FALSE(calculated(*sorcerer, *resistant, curseOfRoots)->isInSuccessEffects(1)) << "DEFORM_RESISTANCE";
	Ref<Npc> fearResistant = makeMonster(700873, 510, 505);
	addStat(*fearResistant, StatEnum::FEAR_RESISTANCE, 1000);
	EXPECT_TRUE(calculated(*sorcerer, *fearResistant, curseOfRoots)->isInSuccessEffects(1));
}

// ---- DelayedSkillEffect (DelayedSkillEffect.java:11-27) --------------------------------------------------------------------------------------

/**
 * 4285 Syncopated Echo on a monster: the effect waits duration2 5,000 ms in the controller; ended by time, its endEffect applies 8905 through
 * SkillEngine.applyEffectsDirectly(8905, effector, effected, targetX/Y/Z) - a PROVOKED procatk_instant hit (TYPE.DAMAGE, LOG.PROCATKINSTANT) on
 * the monster. Ended any other way (here: endEffect, as a dispel does), it launches nothing.
 */
TEST_F(DaevaEffectsTest, SyncopatedEchoStrikesWhenItRunsOut) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700881);
	cp::RecordingAionConnection& observer = observe(*npc, 8231);
	Ref<Player> bard = players.back();
	ASSERT_NO_FATAL_FAILURE(assertNoMagicalResist(*bard, *npc));
	const model::SkillTemplate* echo = bindSkill(SYNCOPATED_ECHO_XML);
	bindSkill(SYNCOPATED_ECHO_EFFECT_XML);
	ASSERT_EQ(effectOf(*echo, 0).javaClassName(), "DelayedSkillEffect");
	const int32_t maxHp = npc->getLifeStats()->getMaxHp();
	observer.clearSent();

	Ref<Effect> effect = cast(*bard, *npc, echo, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(4285)) << "DelayedSkillEffect.applyEffect: addToEffectedController";
	EXPECT_EQ(effect->getDuration(), 5000);
	advance(4999);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), maxHp);
	EXPECT_TRUE(attackStatusesOf(observer, npc->getObjectId()).empty());

	advance(1);
	EXPECT_TRUE(effect->isEndedByTime());
	EXPECT_LT(npc->getLifeStats()->getCurrentHp(), maxHp) << "8905 hit the monster";
	std::vector<AttackStatusFields> statuses = attackStatusesOf(observer, npc->getObjectId());
	ASSERT_EQ(statuses.size(), 1u);
	EXPECT_EQ(statuses[0].skillId, 8905);
	EXPECT_EQ(statuses[0].type, TYPE_DAMAGE_OR_HP);
	EXPECT_EQ(statuses[0].log, LOG_PROCATKINSTANT);
	EXPECT_EQ(statuses[0].value, npc->getLifeStats()->getCurrentHp() - maxHp) << "TYPE.DAMAGE writes -value";

	Ref<Npc> other = makeMonster(700882, 505, 505);
	cp::RecordingAionConnection& otherObserver = observe(*other, 8232);
	otherObserver.clearSent();
	Ref<Effect> dispelled = cast(*bard, *other, echo, 1);
	ASSERT_TRUE(dispelled->isInSuccessEffects(1));
	dispelled->endEffect();
	EXPECT_FALSE(dispelled->isEndedByTime());
	advance(5000);
	EXPECT_EQ(other->getLifeStats()->getCurrentHp(), other->getLifeStats()->getMaxHp()) << "not ended by time: no 8905";
	EXPECT_TRUE(attackStatusesOf(otherObserver, other->getObjectId()).empty());
}

// ---- AlwaysParryEffect (AlwaysParryEffect.java:16-38) --------------------------------------------------------------------------------------

/**
 * 2529 Nullification Trigger: the AlwaysParryEffect$1 observer (value 1, PARRY) answers the first PARRY check and, its `--value` reaching 0,
 * ends the whole effect - which takes the alwaysresist observer of position 1 with it. Another status is no parry. A second cast answers the
 * RESIST check first, and then no PARRY: the effect has ended.
 */
TEST_F(DaevaEffectsTest, NullificationTriggerParriesOnceAndEnds) {
	EFFECT_TEST_SCOPE;
	Ref<Player> rider = makePlayer(8241, PlayerClass::RIDER);
	cp::RecordingAionConnection& self = connectLast(*rider);
	const model::SkillTemplate* trigger = bindSkill(NULLIFICATION_TRIGGER_XML);
	ASSERT_EQ(effectOf(*trigger, 1).javaClassName(), "AlwaysParryEffect");
	ASSERT_FALSE(rider->getObserveController()->checkAttackStatus(AttackStatus::PARRY));
	self.clearSent();

	Ref<Effect> effect = cast(*rider, *rider, trigger, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(2));
	EXPECT_EQ(effect->getDuration(), 8000);
	std::vector<std::vector<uint8_t>> icons = packetsOf<SM_ABNORMAL_STATE>(self);
	ASSERT_FALSE(icons.empty());
	ASSERT_EQ(decodeAbnormalState(icons.back()).effects.size(), 1u);
	EXPECT_EQ(decodeAbnormalState(icons.back()).effects[0].skillId, 2529);
	EXPECT_FALSE(rider->getObserveController()->checkAttackStatus(AttackStatus::BLOCK)) << "only PARRY";
	EXPECT_TRUE(rider->getEffectController()->hasAbnormalEffect(2529));

	EXPECT_TRUE(rider->getObserveController()->checkAttackStatus(AttackStatus::PARRY)) << "--value: 0";
	EXPECT_FALSE(rider->getEffectController()->hasAbnormalEffect(2529)) << "value <= 0: effect.endEffect()";
	EXPECT_FALSE(rider->getObserveController()->checkAttackStatus(AttackStatus::PARRY));
	EXPECT_FALSE(rider->getObserveController()->checkAttackStatus(AttackStatus::RESIST)) << "position 1's observer went with the effect";

	Ref<Effect> second = cast(*rider, *rider, trigger, 1);
	ASSERT_TRUE(second->isInSuccessEffects(2));
	EXPECT_TRUE(rider->getObserveController()->checkAttackStatus(AttackStatus::RESIST));
	EXPECT_FALSE(rider->getObserveController()->checkAttackStatus(AttackStatus::PARRY)) << "the resist ended the effect first";
}

/**
 * 1759 Perfect Parry (the Chanter's, level 44), whose only position is the alwaysparry: AlwaysParryEffect.applyEffect adds the effect to the
 * controller itself (in 2529 position 1's alwaysresist does so as well). Unused, the parry holds duration2 10,000 ms, and the end removes the
 * observer (Effect.endEffect -> removeObservers).
 */
TEST_F(DaevaEffectsTest, AnUnusedParryEndsWithTheEffect) {
	EFFECT_TEST_SCOPE;
	Ref<Player> chanter = makePlayer(8251, PlayerClass::CHANTER);
	const model::SkillTemplate* perfectParry = bindSkill(PERFECT_PARRY_XML);
	ASSERT_EQ(effectOf(*perfectParry, 0).javaClassName(), "AlwaysParryEffect");
	Ref<Effect> effect = cast(*chanter, *chanter, perfectParry, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(chanter->getEffectController()->findBySkillId(1759), effect) << "AlwaysParryEffect.applyEffect: addToEffectedController";
	EXPECT_EQ(effect->getDuration(), 10000);
	advance(9999);
	EXPECT_TRUE(chanter->getEffectController()->hasAbnormalEffect(1759));
	advance(1);
	EXPECT_TRUE(effect->isEndedByTime());
	EXPECT_FALSE(chanter->getObserveController()->checkAttackStatus(AttackStatus::PARRY));
}

// ---- DashEffect (DashEffect.java:20-38) ------------------------------------------------------------------------------------------------------

/**
 * 3455 Dash Attack from (500, 500, 100) on a monster at (510, 500, 100), derived by hand: the heading towards the monster is 0 (0 degrees), the
 * distance is the Assassin's bound radius max(0.25, 0.25) + the monster's 0 (its template has none) + 1 = 1.25, and the point is
 * (510 + (float) cos(PI) * 1.25, 500 + (float) sin(PI) * 1.25) = (508.75, 500); the empty GeoMap has no collision on the way. The skill gets
 * that target position with heading 0, the Assassin is moved there (World.updatePosition) and the effect's DashStatus is DASH. An effected that
 * is not the skill's first target (a Dash-AoE's second target) moves nobody.
 */
TEST_F(DaevaEffectsTest, DashAttackMovesTheAssassinInFrontOfTheTarget) {
	EFFECT_TEST_SCOPE;
	Ref<Player> assassin = makePlayer(8261, PlayerClass::ASSASSIN, Race::ELYOS, 500, 500, 100);
	Ref<Npc> npc = makeMonster(700891, 510, 500);
	Ref<Npc> second = makeMonster(700892, 510, 510);
	assassin->getPosition()->setH(int8_t{42});
	const model::SkillTemplate* dash = bindSkill(DASH_ATTACK_XML);
	ASSERT_EQ(effectOf(*dash, 0).javaClassName(), "DashEffect");
	Ref<model::Skill> skill = model::Skill::create(dash, *assassin, Ptr<Creature>(npc), 1);

	Ref<Effect> other = Effect::create(*skill, Ptr<Creature>(second));
	other->initialize();
	EXPECT_EQ(other->getDashStatus(), model::DashStatus::NONE) << "not the first target";
	EXPECT_EQ(assassin->getX(), 500.0f);
	EXPECT_EQ(assassin->getHeading(), 42);

	Ref<Effect> effect = Effect::create(*skill, Ptr<Creature>(npc));
	effect->initialize();
	EXPECT_EQ(effect->getDashStatus(), model::DashStatus::DASH);
	EXPECT_EQ(skill->getX(), 508.75f);
	EXPECT_EQ(skill->getY(), 500.0f);
	EXPECT_EQ(skill->getZ(), 100.0f);
	EXPECT_EQ(assassin->getX(), 508.75f) << "World.updatePosition(effector, ...)";
	EXPECT_EQ(assassin->getY(), 500.0f);
	EXPECT_EQ(assassin->getZ(), 100.0f);
	EXPECT_EQ(assassin->getHeading(), 0) << "the heading towards the target";
	EXPECT_TRUE(effect->isInSuccessEffects(1)) << "super.calculate(effect)";
}

/**
 * 3455 Dash Attack from (500, 500, 100) on a monster off the axis, at (510, 505, 100), derived by hand from the Java: calculateAngleFrom =
 * (float) toDegrees(atan2(5, 10)) = 26.565052f, getHeadingTowards = (byte) (26.565052f / 3) = (byte) 8.855018f = 8, convertHeadingToAngle(8)
 * = 24 degrees (the heading's rounding moves the point), radian = toRadians(24.0). The point is (510 + (float) cos(PI + radian) * 1.25f,
 * 505 + (float) sin(PI + radian) * 1.25f) = (510 + -0.9135454f * 1.25f, 505 + -0.40673664f * 1.25f) = (510 - 1.1419318f, 505 - 0.5084208f)
 * = (508.85806f, 504.49158f): on the Assassin's side of the monster on both axes.
 */
TEST_F(DaevaEffectsTest, ADiagonalDashStopsOnTheAssassinsSideOfTheTarget) {
	EFFECT_TEST_SCOPE;
	Ref<Player> assassin = makePlayer(8265, PlayerClass::ASSASSIN, Race::ELYOS, 500, 500, 100);
	Ref<Npc> npc = makeMonster(700895, 510, 505);
	const model::SkillTemplate* dash = bindSkill(DASH_ATTACK_XML);
	Ref<model::Skill> skill = model::Skill::create(dash, *assassin, Ptr<Creature>(npc), 1);

	Ref<Effect> effect = Effect::create(*skill, Ptr<Creature>(npc));
	effect->initialize();
	EXPECT_EQ(effect->getDashStatus(), model::DashStatus::DASH);
	EXPECT_EQ(skill->getX(), 508.858063f);
	EXPECT_EQ(skill->getY(), 504.491577f);
	EXPECT_EQ(skill->getH(), 8);
	EXPECT_EQ(assassin->getX(), 508.858063f) << "World.updatePosition(effector, ...)";
	EXPECT_EQ(assassin->getY(), 504.491577f);
	EXPECT_EQ(assassin->getHeading(), 8) << "(byte) (26.565052f / 3)";
}

// ---- BackDashEffect (BackDashEffect.java:21-37) ------------------------------------------------------------------------------------------------

/**
 * 2350 Parting Shot: the heading towards the monster at (510, 500) is 0, so the back dash goes 15 m towards 0 + 180 degrees through
 * GeoService.findMovementCollision. The test map's GeoMap is empty (geo data off): with no ground under the next step it answers the Gunner's own
 * spot, so the Gunner stays at (500, 500, 100) - the target position the skill gets - but turns to heading 0 (World.updatePosition with h), and
 * the effect's DashStatus is BACKDASH.
 */
TEST_F(DaevaEffectsTest, PartingShotBacksTheGunnerAwayFromTheTarget) {
	EFFECT_TEST_SCOPE;
	Ref<Player> gunner = makePlayer(8271, PlayerClass::GUNNER, Race::ELYOS, 500, 500, 100);
	Ref<Npc> npc = makeMonster(700901, 510, 500);
	gunner->getPosition()->setH(int8_t{42});
	const model::SkillTemplate* partingShot = bindSkill(PARTING_SHOT_XML);
	ASSERT_EQ(effectOf(*partingShot, 0).javaClassName(), "BackDashEffect");
	Ref<model::Skill> skill = model::Skill::create(partingShot, *gunner, Ptr<Creature>(npc), 1);

	Ref<Effect> effect = Effect::create(*skill, Ptr<Creature>(npc));
	effect->initialize();
	EXPECT_EQ(effect->getDashStatus(), model::DashStatus::BACKDASH);
	EXPECT_EQ(skill->getX(), 500.0f);
	EXPECT_EQ(skill->getY(), 500.0f);
	EXPECT_EQ(skill->getZ(), 100.0f);
	EXPECT_EQ(gunner->getX(), 500.0f);
	EXPECT_EQ(gunner->getHeading(), 0) << "World.updatePosition(effector, ..., h)";
	EXPECT_TRUE(effect->isInSuccessEffects(1)) << "super.calculate(effect)";
}

// ---- CarveSignetEffect (CarveSignetEffect.java:17-46) ----------------------------------------------------------------------------------------

/**
 * 3385 Rune Carve, four times on one monster: the first carves signet level signet_increment 1 (8303 + 1 - 1); each next one ends the active
 * SIGNET1 effect and carves min(carved + 1, max(signet_cap 3, carved)): 2 (8304), 3 (8305), then 3 again (8305). Each signet effect carries the
 * level it was carved at, and one signet holds its statdown at a time (activeSignet.endEffect(): the stand-ins' comment). Every carve hits first
 * (super.applyEffect: the monster loses the reserved damage), also when prob="0" then carves nothing (Rnd.chance() >= prob). An effector npc
 * (accuracy 200 against the monster's evasion 45: the physical hit cannot be dodged) carves.
 */
TEST_F(DaevaEffectsTest, RuneCarveCarvesTheSignetUpToItsCap) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700911);
	Ref<Npc> carver = makeNpc(700912, {}, 510, 500);
	const model::SkillTemplate* runeCarve = bindSkill(RUNE_CARVE_XML);
	bindSkill(runeCarveSignetXml(8303, 1));
	bindSkill(runeCarveSignetXml(8304, 2));
	bindSkill(runeCarveSignetXml(8305, 3));
	const model::SkillTemplate* never = bindSkill(RUNE_CARVE_NEVER_XML);
	ASSERT_EQ(effectOf(*runeCarve, 0).javaClassName(), "CarveSignetEffect");
	const int32_t maxHp = npc->getLifeStats()->getMaxHp();
	const int32_t defence = statOf(*npc, StatEnum::PHYSICAL_DEFENSE, 1000);

	Ref<Effect> none = cast(*carver, *npc, never, 1);
	EXPECT_FALSE(npc->getEffectController()->getAbnormalEffect("SIGNET1")) << "prob 0: return before the carve";
	int32_t dealt = none->getReserveds(1)->getValue();
	ASSERT_GT(dealt, 0);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), maxHp - dealt) << "super.applyEffect(effect) comes before the prob roll";

	const std::vector<std::pair<int32_t, int32_t>> expected{{8303, 1}, {8304, 2}, {8305, 3}, {8305, 3}};
	Ptr<Effect> previous;
	for (const auto& [skillId, carved] : expected) {
		Ref<Effect> carve = cast(*carver, *npc, runeCarve, 1);
		dealt += carve->getReserveds(1)->getValue();
		EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), maxHp - dealt) << "the carve's hit";
		Ptr<Effect> signet = npc->getEffectController()->getAbnormalEffect("SIGNET1");
		ASSERT_TRUE(signet);
		EXPECT_EQ(signet->getSkillId(), skillId);
		EXPECT_EQ(signet->getCarvedSignet(), carved);
		EXPECT_EQ(signet->getEffector(), carver);
		EXPECT_EQ(statOf(*npc, StatEnum::PHYSICAL_DEFENSE, 1000), defence - 1) << "activeSignet.endEffect() took the previous statdown";
		if (previous) {
			EXPECT_NE(signet, previous);
			EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect([&previous](Effect& e) { return &e == previous.rawPointer(); }))
				<< "the new signet took the stack's entry";
		}
		previous = signet;
	}
}

/**
 * 3365 Ripclaw Strike (signet_increment 5, cap 5) with 3385 Rune Carve (increment 1, cap 3) on one monster, derived by hand from the Java:
 * Rune Carve carves 1 (8303); Ripclaw Strike min(1 + 5, max(5, 1)) = 5 (8307: signet_id + 5 - 1); Rune Carve on that signet min(5 + 1,
 * max(3, 5)) = 5 - a signet carved above this skill's cap keeps its level (8307 again). On a fresh monster, Ripclaw Strike carves its increment,
 * 5, at once. The carves hit (the reserved damage), and one signet holds its statdown at a time. Position 2's hostileup lands and adds no hate:
 * the carver is a neutral npc (AggroList.isAware).
 */
TEST_F(DaevaEffectsTest, RipclawStrikeCarvesFiveLevelsAndKeepsASignetAboveTheCap) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700915);
	Ref<Npc> fresh = makeMonster(700916, 505, 505);
	Ref<Npc> carver = makeNpc(700917, {}, 510, 500);
	const model::SkillTemplate* runeCarve = bindSkill(RUNE_CARVE_XML);
	const model::SkillTemplate* ripclaw = bindSkill(RIPCLAW_STRIKE_XML);
	for (int32_t level = 1; level <= 5; ++level)
		bindSkill(runeCarveSignetXml(8302 + level, level));
	ASSERT_EQ(effectOf(*ripclaw, 0).javaClassName(), "CarveSignetEffect");
	const int32_t maxHp = npc->getLifeStats()->getMaxHp();
	const int32_t defence = statOf(*npc, StatEnum::PHYSICAL_DEFENSE, 1000);

	struct Carve {
		const model::SkillTemplate* skill;
		int32_t signetSkillId;
		int32_t carved;
		const char* why;
	};
	const std::vector<Carve> carves{{runeCarve, 8303, 1, "signet_increment 1"}, {ripclaw, 8307, 5, "min(1 + 5, max(5, 1))"},
		{runeCarve, 8307, 5, "min(5 + 1, max(3, 5)): above Rune Carve's cap 3"}};
	int32_t dealt = 0;
	for (const Carve& c : carves) {
		Ref<Effect> carve = cast(*carver, *npc, c.skill, 1);
		ASSERT_TRUE(carve->isInSuccessEffects(1));
		dealt += carve->getReserveds(1)->getValue();
		EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), maxHp - dealt) << c.why;
		Ptr<Effect> signet = npc->getEffectController()->getAbnormalEffect("SIGNET1");
		ASSERT_TRUE(signet) << c.why;
		EXPECT_EQ(signet->getSkillId(), c.signetSkillId) << c.why;
		EXPECT_EQ(signet->getCarvedSignet(), c.carved) << c.why;
		EXPECT_EQ(statOf(*npc, StatEnum::PHYSICAL_DEFENSE, 1000), defence - 1) << c.why;
	}

	cast(*carver, *fresh, ripclaw, 1);
	Ptr<Effect> signet = fresh->getEffectController()->getAbnormalEffect("SIGNET1");
	ASSERT_TRUE(signet);
	EXPECT_EQ(signet->getSkillId(), 8307) << "no active signet: nextSignetLevel = signet_increment 5";
	EXPECT_EQ(signet->getCarvedSignet(), 5);
}

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
