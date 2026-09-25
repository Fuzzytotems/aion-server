// P5-03, M5e stage 1, item E-01 (m5e-plan.md §2.4, §15.4): the dispel family and the delayed hit of this chunk that a Daeva of levels 13-20
// reaches - AbstractDispelEffect with DispelDebuffEffect (3935 Cleanse, Cleric 13) and DispelDebuffPhysicalEffect (4443 Purifying Paean, Bard 19;
// the potion 9909), DispelBuffCounterAtkEffect (3570 Dispel Magic, Spiritmaster 16; the monster skill 16702) and DelayedSpellAttackInstantEffect
// (1421 Delayed Blast, Sorcerer 19) - and the four classes only the monsters of Verteron and Altgard reach (§2.4's refresh review):
// AlwaysBlockEffect (16423 Crouch), CurseEffect (16436 Death Curse), FallEffect (16905 Crash) and FpAttackInstantEffect (17235 Air
// Turbulence; the Gunner's 1995 Wing Clip for its flat value), each cast from an Npc effector as in the game.
//
// Each case drives real Effects (EffectClassTestSupport.h, DaevaEffectsTestSupport.h) on templates copied verbatim from skill_templates.xml
// (line cited per template; <properties>, conditions and motions left out) and asserts what the Java bodies do, with the golden values of the
// arithmetic derived by hand from the Java expressions. Where a golden damage runs through AttackUtil the effector is an npc and the effected an
// npc of EffectTestSupport.h's template (tests/effects_al/DamageEffectsTest.cpp's file comment derives the chain): for such a pair a magical base
// B deals B - mdef / 10, exactly when the result is below 12.5 (the npc's random share (int) (damage * 0.08f) is 0 there). The fixture fails a
// case that reaches an AION_UNPORTED site (TearDown).

#include "DaevaEffectsTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/AttackCalcObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/FlyState.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::effecttest {
namespace {

using controllers::attack::AttackStatus;
using effect::AbnormalState;
using gameserver::model::PlayerClass;
using gameserver::model::gameobjects::state::CreatureState;
using gameserver::model::gameobjects::state::FlyState;
using gameserver::model::stats::container::StatEnum;
using model::Effect;
using network::aion::serverpackets::SM_ABNORMAL_EFFECT;
using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t NPC_MAX_HP = 2522; // EffectTestSupport.h's npc template
constexpr int32_t CURSE_ID = 1 << 10; // AbnormalState.CURSE.getId()
constexpr int32_t TYPE_DELAYDAMAGE = 10; // SM_ATTACK_STATUS.TYPE.DELAYDAMAGE
constexpr int32_t TYPE_FP_DAMAGE = 26; // SM_ATTACK_STATUS.TYPE.FP_DAMAGE
constexpr int32_t LOG_DELAYEDSPELLATKINSTANT = 97;
constexpr int32_t LOG_FPATTACK = 137;

// ---- the skills of the data (skill_templates.xml) ----------------------------------------------------------------------------------------

/** 3935 "Cleanse" (:67042-67054), the Cleric's level-13 dispel of up to 100 debuffs, dispel_level 1, power 10 */
const std::string CLEANSE_XML = templateXml(
	R"(skill_id="3935" name="Cleanse" nameId="2286212" cooldownId="1215" group="PR_PURIFY" stack="PR_PURIFY" lvl="1" skilltype="MAGICAL")"
	R"( skill_category="DISPELL" skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="5" duration="0" cancel_rate="20")"
	R"( hostile_type="INDIRECT" apply_casting_time_bonus="true")",
	R"(<dispeldebuff dispel_level="1" power="10" value="100" e="1" noresist="true" hoptype="SKILLLV" hopb="345" />)");

/** 4443 "Purifying Paean" (:75588-75603), the Bard's level-19 dispel of physical debuffs, dispel_level 1, power 20 */
const std::string PURIFYING_PAEAN_XML = templateXml(
	R"(skill_id="4443" name="Purifying Paean" nameId="2286069" cooldownId="1287" group="BA_SONGOFPURIFY" stack="BA_SONGOFPURIFY" lvl="1")"
	R"( skilltype="MAGICAL" skill_category="DISPELL" skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="80" duration="0")"
	R"( cancel_rate="20" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true" apply_casting_time_bonus="true")",
	R"(<dispeldebuffphysical dispel_level="1" power="20" value="100" e="1" noresist="true" hoptype="SKILLLV" hopb="1556" />)");

/** 9909 "Physical Altered State Healing" (:92155-92166), a potion: no power or value, dpower 10 and delta 1 per level */
const std::string ALTERED_STATE_HEALING_XML = templateXml(
	R"(skill_id="9909" name="Physical Altered State Healing" nameId="702135" stack="ITEM_POTION_CURE_PHYSICAL" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0")",
	R"(<dispeldebuffphysical dispel_level="1" dpower="10" delta="1" e="1" noresist="true" element="WATER" />)");

/** 8539 "Blindness" (:84004-84016): DEBUFF_PHYSICAL, req_dispel_level 1, req_dispel_count 10 */
const std::string BLINDNESS_XML = templateXml(
	R"(skill_id="8539" name="Blindness" nameId="701866" stack="ITEM_SKILL_PROC_BLIND_L1_40A" lvl="1" skilltype="MAGICAL" skillsubtype="NONE")"
	R"( tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10" activation="PROVOKED" cooldown="0")"
	R"( duration="0" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")",
	R"(<blind value="60" duration2="10000" effectid="20107" e="1" accmod2="100" element="FIRE" />)");

/** 8361 "Arrow Flurry I Effect" (:81819-81827): a stun, DEBUFF_PHYSICAL 1 / 10 */
const std::string ARROW_FLURRY_STUN_XML = templateXml(
	R"(skill_id="8361" name="Arrow Flurry I Effect" nameId="286457" stack="RA_RAPIDBOW_PROC" lvl="1" skilltype="MAGICAL" skillsubtype="DEBUFF")"
	R"( tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10" activation="PROVOKED" cooldown="0" duration="0")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<stun duration2="2000" effectid="20000" e="1" noresist="true" element="FIRE" />)");

/** 8542 "Poison Slash" (:84048-84060): a poison, DEBUFF_PHYSICAL 1 / 10 */
const std::string POISON_SLASH_XML = templateXml(
	R"(skill_id="8542" name="Poison Slash" nameId="701869" stack="ITEM_SKILL_PROC_POISON_L1_40A" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="NONE" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10" activation="PROVOKED")"
	R"( cooldown="0" duration="0" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")",
	R"(<poison checktime="2000" value="38" duration2="20000" effectid="82001" e="1" accmod2="100" element="FIRE" />)");

/** 16704 "Fear Casting" (:122487-122499): DEBUFF_MENTAL, req_dispel_level 1, req_dispel_count 20 */
const std::string FEAR_CASTING_XML = templateXml(
	R"(skill_id="16704" name="Fear Casting" nameId="285567" cooldownId="12" stack="NEL_FEARTA_NR" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="DEBUFF_MENTAL" req_dispel_level="1" req_dispel_count="20" activation="ACTIVE")"
	R"( cooldown="0" duration="2000" ammospeed="25" cancel_rate="40" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true")",
	R"(<fear resistchance="75" duration2="3000" effectid="20103" e="1" element="WATER" hoptype="SKILLLV" hopb="60" hopa="60" />)");

/** 18892 "Weeping Curtain" (:154864-154878): DEBUFF_MENTAL, req_dispel_level 5, req_dispel_count 100 */
const std::string WEEPING_CURTAIN_XML = templateXml(
	R"(skill_id="18892" name="Weeping Curtain" nameId="295289" cooldownId="1" stack="IDCATACOMBS_SPECTRE_AREAHEALNUFF" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="DEBUFF_MENTAL" req_dispel_level="5" req_dispel_count="100" activation="ACTIVE")"
	R"( cooldown="0" duration="0" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<deboostheal duration2="15000" effectid="10184622" e="1" noresist="true" element="WIND">)"
	R"(<change stat="HEAL_SKILL_DEBOOST" func="ADD" value="-99999" /></deboostheal>)");

/** 1574 "Binding Word" (:23302-23318): DEBUFF_PHYSICAL, req_dispel_level 1, req_dispel_count 20 */
const std::string BINDING_WORD_XML = templateXml(
	R"(skill_id="1574" name="Binding Word" nameId="2286341" cooldownId="1504" group="CH_SWORDBIND" stack="CH_SWORDBIND" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="20")"
	R"( activation="ACTIVE" cooldown="300" duration="0" pvp_duration="80" cancel_rate="20" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true" apply_casting_time_bonus="true")",
	R"(<bind duration2="5000" effectid="20014" e="1" accmod2="400" element="FIRE" hoptype="SKILLLV" hopb="800" />)"
	R"(<snare duration2="5000" effectid="20007" e="2" accmod2="240" element="EARTH" preeffect="1">)"
	R"(<change stat="SPEED" func="PERCENT" value="-50" /><change stat="FLY_SPEED" func="PERCENT" value="-50" /></snare>)");

/** 1350 "Boon of Quickness" (:19695-19709): a BUFF, dispel_category BUFF 1 / 10 */
const std::string BOON_OF_QUICKNESS_XML = templateXml(
	R"(skill_id="1350" name="Boon of Quickness" nameId="2288034" cooldownId="1588" group="WI_ICYVEINS" stack="WI_DARK_ICYVEINS" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE")"
	R"( cooldown="3000" duration="0" stigma="BASIC" cancel_rate="20" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true" apply_casting_time_bonus="true")",
	R"(<boostskillcastingtime duration2="15000" effectid="173" e="1" noresist="true" hoptype="SKILLLV" hopb="684">)"
	R"(<change stat="BOOST_CASTING_TIME_SKILL" func="PERCENT" value="50" /></boostskillcastingtime>)");

/** 16423 "Crouch" (:118314-118329), a self-buff of 210112, 210266, 210267 and 210334: PHYSICAL_DEFENSE + 1000 % and ten blocks, BUFF 1 / 10 */
const std::string CROUCH_XML = templateXml(
	R"(skill_id="16423" name="Crouch" nameId="282852" cooldownId="2" stack="NKN_STATUPPHYSICALDEFENSE_CR" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE" cooldown="0")"
	R"( duration="2500" cancel_rate="35" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<statup duration2="5000" effectid="30152" e="1" noresist="true" hoptype="SKILLLV" hopb="796" hopa="88">)"
	R"(<change stat="PHYSICAL_DEFENSE" func="PERCENT" value="1000" /></statup>)"
	R"(<alwaysblock value="10" duration2="5000" effectid="144" e="2" noresist="true" preeffect="1" />)");

/** 2974 "Shield of Faith" (:50042-50055), the Templar's level-45 buff: an alwaysblock alone, ten blocks for 30,000 ms */
const std::string SHIELD_OF_FAITH_XML = templateXml(
	R"(skill_id="2974" name="Shield of Faith" nameId="2286936" cooldownId="413" group="KN_INVINSIBLESHIELD" stack="KN_INVINSIBLESHIELD" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE")"
	R"( cooldown="1836" cooldown_delta_lv="-36" duration="0" stigma="ADVANCED" cancel_rate="10" hostile_type="INDIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<alwaysblock value="10" duration2="30000" effectid="144" e="1" basiclvl="20" noresist="true" hoptype="SKILLLV" hopb="12750" />)");

/** 16702 "Curse of Blessing" (:122457-122469), a monster skill: dispels up to 3 buffs of dispel_level 4, power 50, hitvalue 49 + hitdelta 1 */
const std::string CURSE_OF_BLESSING_XML = templateXml(
	R"(skill_id="16702" name="Curse of Blessing" nameId="285569" cooldownId="10" stack="NEL_DISBUFFCOATKTA_NR" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="3500" ammospeed="25" cancel_rate="25" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<dispelbuffcounteratk hitvalue="49" hitdelta="1" dispel_level="4" power="50" value="3" e="1" element="WATER" hoptype="DAMAGE" />)");

/** 16702 with critprobmod2 100000: a rolled magical critical would pass on criticalSeed(); DispelBuffCounterAtkEffect never rolls one */
const std::string CRITICAL_CURSE_OF_BLESSING_XML = templateXml(
	R"(skill_id="7702" name="Curse of Blessing" nameId="285569" stack="EFFECTS_AL_DISPEL_CRITICAL" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="3500" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true")",
	R"(<dispelbuffcounteratk hitvalue="49" hitdelta="1" dispel_level="4" power="50" value="3" e="1" critprobmod2="100000" element="WATER")"
	R"( hoptype="DAMAGE" />)");

/** 3570 "Dispel Magic" (:60647-60665), the Spiritmaster's level-16 dispel of one buff with a hit of 192 */
const std::string DISPEL_MAGIC_XML = templateXml(
	R"(skill_id="3570" name="Dispel Magic" nameId="2287344" cooldownId="1038" group="EL_DISPEL" stack="EL_DISPEL" lvl="1" skilltype="MAGICAL")"
	R"( skill_category="CHAIN_SKILL" skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="100" duration="500" cancel_rate="30")"
	R"( chain_skill_prob="100" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")"
	R"( apply_casting_time_bonus="true")",
	R"(<dispelbuffcounteratk hitvalue="192" dispel_level="1" power="10" value="1" e="1" noresist="true" element="EARTH" hoptype="DAMAGE" />)");

/** 1421 "Delayed Blast" (:20924-20939), the Sorcerer's level-19 hit of 350 after 4,000 ms */
const std::string DELAYED_BLAST_XML = templateXml(
	R"(skill_id="1421" name="Delayed Blast" nameId="2288047" cooldownId="272" group="WI_DELAYEDEXPLOSION" stack="WI_DELAYEDEXPLOSION" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="300" duration="2000" cancel_rate="30")"
	R"( hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true" apply_casting_time_bonus="true")",
	R"(<delaydamage delay="4000" value="350" e="1" element="FIRE" hoptype="DAMAGE" />)");

/** 1421 with value 13 + delta 2 per level: at level 2 a base of 17, 10 on the test npc (the file comment) */
const std::string SMALL_DELAYED_BLAST_XML = templateXml(
	R"(skill_id="7421" name="Delayed Blast" nameId="2288047" stack="EFFECTS_AL_DELAYED_BLAST" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK")"
	R"( tslot="NONE" activation="ACTIVE" cooldown="300" duration="2000" apply_magical_critical="true")",
	R"(<delaydamage delay="4000" value="13" delta="2" e="1" element="FIRE" hoptype="DAMAGE" />)");

/** 16436 "Death Curse" (:118497-118513), a monster skill of 210743: a hit, then MAXHP and MAXMP - 35 % for 8,500 + 120 * level ms */
const std::string DEATH_CURSE_XML = templateXml(
	R"(skill_id="16436" name="Death Curse" nameId="282949" cooldownId="1" stack="NEL_CURSE_DARK" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="20" activation="ACTIVE")"
	R"( cooldown="0" duration="4000" ammospeed="25" cancel_rate="35" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true")",
	R"(<spellatkinstant mode="PERCENT" value="92" e="1" element="EARTH" hoptype="DAMAGE" />)"
	R"(<curse duration2="8500" duration1="120" effectid="20102" e="2" element="EARTH" preeffect="1" hoptype="SKILLLV" hopb="60" hopa="60">)"
	R"(<change stat="MAXHP" func="PERCENT" value="-35" /><change stat="MAXMP" func="PERCENT" value="-35" /></curse>)");

/** 16905 "Crash" (:125565-125578), a monster skill of 210347, 210520 and 210521: a hit, then the fall of a flying player */
const std::string CRASH_XML = templateXml(
	R"(skill_id="16905" name="Crash" nameId="288188" cooldownId="1" stack="NWI_FLY_DEADSHOT_GARGOYLE_LC" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="2500" ammospeed="35" cancel_rate="35" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<spellatkinstant mode="PERCENT" value="92" e="1" element="FIRE" hoptype="DAMAGE" />)"
	R"(<fall e="2" noresist="true" element="FIRE" preeffect="1" />)");

/** 17235 "Air Turbulence" (:130579-130595), a monster skill: a hit with 8217's stagger, then 35 % of the flight time */
const std::string AIR_TURBULENCE_XML = templateXml(
	R"(skill_id="17235" name="Air Turbulence" nameId="289839" cooldownId="13" stack="NAR_FPATKINSKD_PO" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="500" ammospeed="25" cancel_rate="65" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<skillatk mode="PERCENT" value="17" e="1" accmod2="-15" hoptype="DAMAGE"><subeffect skill_id="8217" /></skillatk>)"
	R"(<fpatkinstant percent="true" value="35" e="2" noresist="true" element="WIND" preeffect="1" hoptype="DAMAGE" />)");

/** 8217 "Stunned" (:79903-79915), the stagger 17235's first position launches */
const std::string STUNNED_XML = templateXml(
	R"(skill_id="8217" name="Stunned" nameId="280503" stack="NORMALATTACK_STAGGER" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE")"
	R"( tslot="DEBUFF" dispel_category="STUN" activation="ACTIVE" cooldown="0" duration="3000" cancel_rate="5" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<stagger duration1="2000" effectid="20010" e="1" noresist="true" element="WIND" hoptype="SKILLLV" hopb="1000" hopa="100" />)");

/** 1995 "Wing Clip" (:30697-30717), the Gunner's: a hit, 15 flight time, FLY_SPEED - 2000 */
const std::string WING_CLIP_XML = templateXml(
	R"(skill_id="1995" name="Wing Clip" nameId="2286553" cooldownId="1819" group="GU_AIMWING" stack="GU_AIMWING" lvl="1" skilltype="PHYSICAL")"
	R"( skill_category="PHYSICAL_DEBUFF" skillsubtype="ATTACK" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1")"
	R"( req_dispel_count="10" activation="ACTIVE" cooldown="600" duration="0" ammospeed="40" cancel_rate="10" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<spellatkinstant value="206" e="1" accmod2="100" element="WIND" hoptype="DAMAGE" />)"
	R"(<fpatkinstant value="15" e="2" accmod2="500" element="WIND" preeffect="1" />)"
	R"(<statdown duration2="12000" effectid="935952" e="3" element="FIRE" preeffect="1" hoptype="SKILLLV" hopb="500">)"
	R"(<change stat="FLY_SPEED" func="ADD" value="-2000" /></statdown>)");

// ---- helpers --------------------------------------------------------------------------------------------------------------------------------

/** Records the ATTACK notifications a creature's ObserveController hands its observers: the skill and the object id of the creature attacked */
struct AttackRecorder final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	static Ref<AttackRecorder> create() { return runtime::makeRef<AttackRecorder>(); }

	void attack(Creature& creature, int32_t skillId) override {
		attacks.push_back(skillId);
		targets.push_back(creature.getObjectId());
	}

	std::vector<int32_t> attacks;
	std::vector<int32_t> targets;

protected:
	AttackRecorder() : ActionObserver(controllers::observer::ObserverType::ATTACK) {}
	~AttackRecorder() override = default;
};

/** Records the ATTACKED notifications of a creature (CreatureController.onAttack: a hit with notifyAttack): the attacker's object id, the skill */
struct AttackedRecorder final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	static Ref<AttackedRecorder> create() { return runtime::makeRef<AttackedRecorder>(); }

	void attacked(Creature& creature, int32_t skillId) override {
		attackers.push_back(creature.getObjectId());
		skills.push_back(skillId);
	}

	std::vector<int32_t> attackers;
	std::vector<int32_t> skills;

protected:
	AttackedRecorder() : ActionObserver(controllers::observer::ObserverType::ATTACKED) {}
	~AttackedRecorder() override = default;
};

/** An AttackCalcObserver of the effected standing in for a shield: it lets 3 of the damage through (tests/effects_al/DamageEffectsTest.cpp) */
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

/** Sets gameserver.geodata.fear.enable (GeoDataConfig.FEAR_ENABLE) for one case and restores the value it had */
class FearEnable {
public:
	explicit FearEnable(bool value) : previous(configs::main::GeoDataConfig::FEAR_ENABLE.load()) {
		configs::main::GeoDataConfig::FEAR_ENABLE.store(value);
	}
	~FearEnable() { configs::main::GeoDataConfig::FEAR_ENABLE.store(previous); }
	FearEnable(const FearEnable&) = delete;
	FearEnable& operator=(const FearEnable&) = delete;

private:
	const bool previous;
};

/** A seed whose first Rnd.nextInt(1000) is below 500: a magical critical roll of critprobmod2 100000 passes (DamageEffectsTest.cpp) */
uint64_t criticalSeed() {
	uint64_t seed = 1;
	for (;; ++seed) {
		Rnd::seedCurrentThreadForTests(seed);
		if (Rnd::nextInt(1000) < 500)
			return seed;
	}
}

class DispelAndMonsterEffectsTest : public DaevaEffectTest {};

// ---- AbstractDispelEffect, DispelDebuffEffect (AbstractDispelEffect.java:17-32, DispelDebuffEffect.java:16-22) ------------------------------

/**
 * 3935 Cleanse on a player with four effects: DispelDebuffEffect passes DispelCategoryType.ALL and the DEBUFF slot to AbstractDispelEffect,
 * which dispels count = value 100 effects with dispel_level 1 and power + dpower * level = 10. Blindness (DEBUFF_PHYSICAL, req 1 / 10) loses
 * its power 10 and ends; Fear Casting (DEBUFF_MENTAL, req 1 / 20) keeps 10 of its 20 (STR_MSG_NOT_ENOUGH_DISPELCOUNT); Weeping Curtain
 * (req_dispel_level 5) is above the level (STR_MSG_NOT_ENOUGH_DISPELLEVEL); the BUFF is not in the DEBUFF slot. A second Cleanse ends the fear.
 */
TEST_F(DispelAndMonsterEffectsTest, CleanseDispelsTheDebuffsItHasTheLevelAndPowerFor) {
	EFFECT_TEST_SCOPE;
	FearEnable fearDisabled(false);
	Ref<Player> victim = daeva(8301, PlayerClass::SORCERER);
	cp::RecordingAionConnection& self = connectLast(*victim);
	Ref<Player> cleric = daeva(8302, PlayerClass::CLERIC);
	Ref<Npc> npc = makeMonster(700931);
	const model::SkillTemplate* cleanse = bindSkill(CLEANSE_XML);
	ASSERT_EQ(effectOf(*cleanse, 0).javaClassName(), "DispelDebuffEffect");
	forced(*npc, *victim, bindSkill(BLINDNESS_XML));
	forced(*npc, *victim, bindSkill(FEAR_CASTING_XML));
	forced(*npc, *victim, bindSkill(WEEPING_CURTAIN_XML));
	cast(*victim, *victim, bindSkill(BOON_OF_QUICKNESS_XML), 1);
	for (int32_t skillId : {8539, 16704, 18892, 1350})
		ASSERT_TRUE(victim->getEffectController()->hasAbnormalEffect(skillId)) << skillId;
	self.clearSent();

	Ref<Effect> first = cast(*cleric, *victim, cleanse, 1);
	ASSERT_TRUE(first->isInSuccessEffects(1));
	EXPECT_FALSE(victim->getEffectController()->hasAbnormalEffect(8539)) << "power 10 - 10 <= 0";
	EXPECT_TRUE(victim->getEffectController()->hasAbnormalEffect(16704)) << "power 20 - 10 > 0";
	EXPECT_EQ(victim->getEffectController()->findBySkillId(16704)->getPower(), 10);
	EXPECT_TRUE(victim->getEffectController()->hasAbnormalEffect(18892)) << "req_dispel_level 5 > dispel_level 1";
	EXPECT_TRUE(victim->getEffectController()->hasAbnormalEffect(1350)) << "a BUFF is not in the DEBUFF slot";
	EXPECT_EQ(countMessages(self, SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_DISPELCOUNT()), 1);
	EXPECT_EQ(countMessages(self, SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_DISPELLEVEL()), 1);
	self.clearSent();

	cast(*cleric, *victim, cleanse, 1);
	EXPECT_FALSE(victim->getEffectController()->hasAbnormalEffect(16704)) << "the rest of its power: 10 - 10";
	EXPECT_TRUE(victim->getEffectController()->hasAbnormalEffect(18892));
	EXPECT_EQ(countMessages(self, SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_DISPELCOUNT()), 0);
	EXPECT_EQ(countMessages(self, SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_DISPELLEVEL()), 1);
}

// ---- DispelDebuffPhysicalEffect (DispelDebuffPhysicalEffect.java:16-22) ------------------------------------------------------------------------

/**
 * 4443 Purifying Paean passes DEBUFF_PHYSICAL: power 20 ends Blindness (req 1 / 10) and Binding Word (req 1 / 20, which Cleanse's power 10 only
 * weakens); a DEBUFF_MENTAL effect is not of the category, which EffectController reports as an insufficient level.
 */
TEST_F(DispelAndMonsterEffectsTest, PurifyingPaeanDispelsOnlyPhysicalDebuffs) {
	EFFECT_TEST_SCOPE;
	FearEnable fearDisabled(false);
	Ref<Player> victim = daeva(8311, PlayerClass::SORCERER);
	cp::RecordingAionConnection& self = connectLast(*victim);
	Ref<Player> bard = daeva(8312, PlayerClass::BARD);
	Ref<Npc> npc = makeMonster(700941);
	const model::SkillTemplate* paean = bindSkill(PURIFYING_PAEAN_XML);
	ASSERT_EQ(effectOf(*paean, 0).javaClassName(), "DispelDebuffPhysicalEffect");
	forced(*npc, *victim, bindSkill(BLINDNESS_XML));
	forced(*npc, *victim, bindSkill(BINDING_WORD_XML));
	forced(*npc, *victim, bindSkill(FEAR_CASTING_XML));
	for (int32_t skillId : {8539, 1574, 16704})
		ASSERT_TRUE(victim->getEffectController()->hasAbnormalEffect(skillId)) << skillId;
	self.clearSent();

	cast(*bard, *victim, paean, 1);
	EXPECT_FALSE(victim->getEffectController()->hasAbnormalEffect(8539));
	EXPECT_FALSE(victim->getEffectController()->hasAbnormalEffect(1574)) << "power 20 - 20 <= 0";
	EXPECT_TRUE(victim->getEffectController()->hasAbnormalEffect(16704)) << "DEBUFF_MENTAL is not DEBUFF_PHYSICAL";
	EXPECT_EQ(countMessages(self, SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_DISPELCOUNT()), 0);
	EXPECT_EQ(countMessages(self, SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_DISPELLEVEL()), 1);
}

/**
 * 9909 has neither power nor value: AbstractDispelEffect's count is calculateBaseValue = 0 + delta 1 * level and its power 0 + dpower 10 * level.
 * At level 1 one of three physical debuffs (req 1 / 10) ends; at level 2 two do, with power 20; Binding Word's 20 needs level 2 as well.
 */
TEST_F(DispelAndMonsterEffectsTest, ThePotionsCountAndPowerGrowWithTheLevel) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700951);
	const model::SkillTemplate* potion = bindSkill(ALTERED_STATE_HEALING_XML);
	const model::SkillTemplate* blindness = bindSkill(BLINDNESS_XML);
	const model::SkillTemplate* stun = bindSkill(ARROW_FLURRY_STUN_XML);
	const model::SkillTemplate* poison = bindSkill(POISON_SLASH_XML);
	const model::SkillTemplate* bindingWord = bindSkill(BINDING_WORD_XML);
	auto remaining = [](Player& player, std::initializer_list<int32_t> skillIds) {
		int32_t count = 0;
		for (int32_t skillId : skillIds)
			count += player.getEffectController()->hasAbnormalEffect(skillId) ? 1 : 0;
		return count;
	};

	for (int32_t level : {1, 2}) {
		Ref<Player> victim = daeva(8320 + level, PlayerClass::SORCERER);
		forced(*npc, *victim, blindness);
		forced(*npc, *victim, stun);
		forced(*npc, *victim, poison);
		ASSERT_EQ(remaining(*victim, {8539, 8361, 8542}), 3);
		Ref<Effect> effect = cast(*victim, *victim, potion, level);
		ASSERT_TRUE(effect->isInSuccessEffects(1));
		EXPECT_EQ(remaining(*victim, {8539, 8361, 8542}), 3 - level) << "count = 0 + 1 * " << level;
	}

	Ref<Player> bound = daeva(8325, PlayerClass::SORCERER);
	forced(*npc, *bound, bindingWord);
	cast(*bound, *bound, potion, 1);
	EXPECT_TRUE(bound->getEffectController()->hasAbnormalEffect(1574)) << "power 0 + 10 * 1 < 20";
	cast(*bound, *bound, potion, 2);
	EXPECT_FALSE(bound->getEffectController()->hasAbnormalEffect(1574)) << "10 left, power 0 + 10 * 2";
}

// ---- DispelBuffCounterAtkEffect (DispelBuffCounterAtkEffect.java:14-74) -------------------------------------------------------------------

/**
 * 16702 Curse of Blessing on a monster with two buffs (dispel BUFF, req 1 / 10): calculateDamage marks both for this effect
 * (calculateBuffsOrEffectorDebuffsToRemove with count 3, dispel_level 4, power 50) and hits for hitvalue + (hitvalue / 2) * (2 - 1) +
 * hitdelta * level = 49 + 24 + 1 = 74. The monster's mdef is raised to 640, so the reserve is 74 - 64 = 10 exactly (the file comment).
 * applyEffect deals it (TYPE.REGULAR: an ACTIVE skill) and ends the marked buffs (EffectController.dispelBuffCounterAtkEffect).
 */
TEST_F(DispelAndMonsterEffectsTest, CurseOfBlessingHitsHarderForEveryBuffItDispels) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700961);
	cp::RecordingAionConnection& observer = observe(*npc, 8331);
	Ref<Npc> caster = makeNpc(700962, {}, 510, 500);
	addStat(*npc, StatEnum::MAGICAL_DEFEND, 570);
	ASSERT_EQ(npc->getGameStats()->getMDef()->getCurrent(), 640);
	const model::SkillTemplate* curse = bindSkill(CURSE_OF_BLESSING_XML);
	ASSERT_EQ(effectOf(*curse, 0).javaClassName(), "DispelBuffCounterAtkEffect");
	cast(*npc, *npc, bindSkill(BOON_OF_QUICKNESS_XML), 1);
	cast(*npc, *npc, bindSkill(CROUCH_XML), 1);
	observer.clearSent();

	Ref<Effect> effect = calculated(*caster, *npc, curse);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getReserveds(1)->getValue(), 10) << "49 + 24 * 1 + 1 * 1 = 74, minus 640 / 10";
	EXPECT_EQ(npc->getEffectController()->findBySkillId(1350)->getDesignatedDispelEffect(), effect);
	EXPECT_EQ(npc->getEffectController()->findBySkillId(16423)->getDesignatedDispelEffect(), effect);
	effect->applyEffect();
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP - 10);
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(1350)) << "dispelBuffCounterAtkEffect ended the marked buffs";
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(16423));
	std::vector<AttackStatusFields> statuses = attackStatusesOf(observer, npc->getObjectId());
	ASSERT_EQ(statuses.size(), 1u);
	EXPECT_EQ(statuses[0].value, 10);
	EXPECT_EQ(statuses[0].type, TYPE_REGULAR);
	EXPECT_EQ(statuses[0].skillId, 16702);
}

/** One buff: 49 + 24 * 0 + 1 = 50, minus mdef 420 / 10 = 8; no buff: a hit of 0 (the damage is floored at 0) */
TEST_F(DispelAndMonsterEffectsTest, CurseOfBlessingWithOneBuffAndWithNone) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700971);
	Ref<Npc> caster = makeNpc(700972, {}, 510, 500);
	addStat(*npc, StatEnum::MAGICAL_DEFEND, 350);
	const model::SkillTemplate* curse = bindSkill(CURSE_OF_BLESSING_XML);
	cast(*npc, *npc, bindSkill(BOON_OF_QUICKNESS_XML), 1);

	Ref<Effect> one = calculated(*caster, *npc, curse);
	EXPECT_EQ(one->getReserveds(1)->getValue(), 8) << "50 - 42";
	one->applyEffect();
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(1350));
	EXPECT_EQ(calculated(*caster, *npc, curse)->getReserveds(1)->getValue(), 0) << "dispelledEffectCount 0: valueWithDelta 0";
}

/**
 * Unlike the delayed hit, the counter attack asks the effected's shields: calculateDamage passes ignoreShield false to
 * AttackUtil.calculateSkillResult (DispelBuffCounterAtkEffect.java:46), so a shield that lets 3 through takes the one-buff case's 8 down to 3
 */
TEST_F(DispelAndMonsterEffectsTest, TheCounterAttackIsCheckedAgainstShields) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(701071);
	Ref<Npc> caster = makeNpc(701072, {}, 510, 500);
	addStat(*npc, StatEnum::MAGICAL_DEFEND, 350);
	const model::SkillTemplate* curse = bindSkill(CURSE_OF_BLESSING_XML);
	cast(*npc, *npc, bindSkill(BOON_OF_QUICKNESS_XML), 1);
	Ref<CappingShield> shield = CappingShield::create();
	npc->getObserveController()->addAttackCalcObserver(*shield);

	Ref<Effect> effect = calculated(*caster, *npc, curse);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(shield->checks, 1) << "ignoreShield false: checkShieldStatus";
	EXPECT_EQ(effect->getReserveds(1)->getValue(), 3) << "the shield's 3, not 50 - 42";
	npc->getObserveController()->removeAttackCalcObserver(*shield);
}

/**
 * 3570 Dispel Magic dispels count = value 1 buff of two, and hits for its hitvalue 192 (one buff: no hitvalue / 2 share, no hitdelta) - EARTH on
 * the monster's mdef 70: 185 plus the npc effector's random share of (int) (185 * 0.08f) = 14 either way.
 */
TEST_F(DispelAndMonsterEffectsTest, DispelMagicDispelsOneBuff) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700981);
	Ref<Npc> caster = makeNpc(700982, {}, 510, 500);
	const model::SkillTemplate* dispelMagic = bindSkill(DISPEL_MAGIC_XML);
	cast(*npc, *npc, bindSkill(BOON_OF_QUICKNESS_XML), 1);
	cast(*npc, *npc, bindSkill(CROUCH_XML), 1);

	Ref<Effect> effect = cast(*caster, *npc, dispelMagic, 1);
	const int32_t damage = effect->getReserveds(1)->getValue();
	EXPECT_GE(damage, 171);
	EXPECT_LE(damage, 199);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP - damage);
	const int32_t left = (npc->getEffectController()->hasAbnormalEffect(1350) ? 1 : 0) + (npc->getEffectController()->hasAbnormalEffect(16423) ? 1 : 0);
	EXPECT_EQ(left, 1) << "count 1";
}

/**
 * DispelBuffCounterAtkEffect.resolveMagicalCritical does nothing: with critprobmod2 100000 and a draw that DamageEffect's roll would pass, the
 * effect is no critical. Its endEffect resets the marks it left (EffectController.resetDesignatedDispelEffect): marked buffs are not
 * dispellable (a second calculate finds none), and after the first effect ended a third finds both again.
 */
TEST_F(DispelAndMonsterEffectsTest, TheCounterAttackNeverCritsAndItsEndClearsItsMarks) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700991);
	Ref<Npc> caster = makeNpc(700992, {}, 510, 500);
	addStat(*npc, StatEnum::MAGICAL_DEFEND, 570);
	const model::SkillTemplate* critical = bindSkill(CRITICAL_CURSE_OF_BLESSING_XML);
	const model::SkillTemplate* curse = bindSkill(CURSE_OF_BLESSING_XML);
	cast(*npc, *npc, bindSkill(BOON_OF_QUICKNESS_XML), 1);
	cast(*npc, *npc, bindSkill(CROUCH_XML), 1);

	Rnd::seedCurrentThreadForTests(criticalSeed());
	Ref<Effect> first = calculated(*caster, *npc, critical);
	ASSERT_TRUE(first->isInSuccessEffects(1));
	EXPECT_FALSE(first->isMagicalCritical(1)) << "no roll";
	EXPECT_EQ(first->getReserveds(1)->getValue(), 10);

	EXPECT_EQ(calculated(*caster, *npc, curse)->getReserveds(1)->getValue(), 0) << "both buffs are marked for the first effect";
	first->endEffect();
	EXPECT_FALSE(npc->getEffectController()->findBySkillId(1350)->getDesignatedDispelEffect()) << "resetDesignatedDispelEffect";
	EXPECT_FALSE(npc->getEffectController()->findBySkillId(16423)->getDesignatedDispelEffect());
	EXPECT_EQ(calculated(*caster, *npc, curse)->getReserveds(1)->getValue(), 10) << "both dispellable again";
}

// ---- DelayedSpellAttackInstantEffect (DelayedSpellAttackInstantEffect.java:19-40) ----------------------------------------------------------

/**
 * 1421 Delayed Blast: applyEffect calculates the hit at once (AttackUtil.calculateSkillResult with value + delta * level, ignoring shields) and
 * a task deals it 4,000 ms later: TYPE.DELAYDAMAGE and LOG.DELAYEDSPELLATKINSTANT with notifyAttack true (CreatureController.onAttack then
 * notifies the monster's ATTACKED observers of the caster and the skill, CreatureController.java:223-241), then the effector's ATTACK observers
 * of the effected. FIRE 350 on mdef 70: 343 plus the npc effector's random share of (int) (343 * 0.08f) = 27 either way. calculateDamage does
 * nothing, so calculate reserves nothing.
 */
TEST_F(DispelAndMonsterEffectsTest, DelayedBlastHitsFourSecondsLater) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(701001);
	cp::RecordingAionConnection& observer = observe(*npc, 8341);
	Ref<Npc> caster = makeNpc(701002, {}, 510, 500);
	Ref<AttackRecorder> attacks = AttackRecorder::create();
	caster->getObserveController()->addObserver(*attacks);
	Ref<AttackedRecorder> attacked = AttackedRecorder::create();
	npc->getObserveController()->addObserver(*attacked);
	const model::SkillTemplate* delayedBlast = bindSkill(DELAYED_BLAST_XML);
	ASSERT_EQ(effectOf(*delayedBlast, 0).javaClassName(), "DelayedSpellAttackInstantEffect");
	observer.clearSent();

	Ref<Effect> effect = calculated(*caster, *npc, delayedBlast);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getReserveds(1)->getValue(), 0) << "calculateDamage is empty";
	effect->applyEffect();
	const int32_t damage = effect->getReserveds(1)->getValue();
	EXPECT_GE(damage, 316);
	EXPECT_LE(damage, 370);
	advance(3999);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP);
	EXPECT_TRUE(attackStatusesOf(observer, npc->getObjectId()).empty());
	EXPECT_TRUE(attacks->attacks.empty());

	advance(1);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP - damage);
	std::vector<AttackStatusFields> statuses = attackStatusesOf(observer, npc->getObjectId());
	ASSERT_EQ(statuses.size(), 1u);
	EXPECT_EQ(statuses[0].value, -damage) << "TYPE.DELAYDAMAGE writes -value";
	EXPECT_EQ(statuses[0].type, TYPE_DELAYDAMAGE);
	EXPECT_EQ(statuses[0].log, LOG_DELAYEDSPELLATKINSTANT);
	EXPECT_EQ(statuses[0].skillId, 1421);
	EXPECT_EQ(attacks->attacks, std::vector<int32_t>{1421}) << "notifyAttackObservers(effected, skillId)";
	EXPECT_EQ(attacks->targets, std::vector<int32_t>{npc->getObjectId()}) << "the effector's observers are told of the effected";
	EXPECT_EQ(attacked->attackers, std::vector<int32_t>{caster->getObjectId()}) << "notifyAttack true: notifyAttackedObservers(attacker, ...)";
	EXPECT_EQ(attacked->skills, std::vector<int32_t>{1421});
	caster->getObserveController()->removeObserver(*attacks);
	npc->getObserveController()->removeObserver(*attacked);
}

/**
 * The delayed hit's hate, derived by hand from the Java: 1421 is hoptype DAMAGE, so CreatureController.onAttack's AggroList.addDamage (notifyAttack
 * true, hopType DAMAGE, AggroList.java:47-49) adds calculateHate(effector, damage * 10) = damage * 10 * (1000 + BOOST_HATE 100) / 1000 = damage
 * * 11, and then broadcastHate() (TYPE.DELAYDAMAGE, CreatureController.java:258-259) the effect's hate: calculateHate(effector, max(1, 0 + 0))
 * = 1 (the template has no hopb). Before the hit the monster has no hate for the Sorcerer: the effect is not added to a controller, so nothing
 * broadcast it.
 */
TEST_F(DispelAndMonsterEffectsTest, TheDelayedHitAddsTenTimesItsDamageAsHate) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(701091);
	Ref<Player> sorcerer = daeva(8411, PlayerClass::SORCERER);
	ASSERT_TRUE(KnownListPairing::pair(*npc, *sorcerer));
	ASSERT_EQ(statOf(*sorcerer, StatEnum::BOOST_HATE, 100), 100);
	ASSERT_NO_FATAL_FAILURE(assertNoMagicalResist(*sorcerer, *npc));
	const model::SkillTemplate* delayedBlast = bindSkill(DELAYED_BLAST_XML);

	Ref<Effect> effect = cast(*sorcerer, *npc, delayedBlast, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	const int32_t damage = effect->getReserveds(1)->getValue();
	ASSERT_GT(damage, 0);
	ASSERT_LT(damage, NPC_MAX_HP);
	ASSERT_EQ(npc->getAggroList().getHate(*sorcerer), 0);
	advance(4000);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP - damage);
	EXPECT_EQ(npc->getAggroList().getHate(*sorcerer), damage * 11 + 1) << "calculateHate(effector, damage * 10) + the effect's hate 1";
}

/**
 * The golden hit: value 13 + delta 2 * level 2 = 17 is 10 on the test npc; a shield that lets 3 through is not asked (ignoreShield true, where
 * DamageEffect.calculateDamage passes false: DamageEffectsTest.cpp's shield case reserves 3)
 */
TEST_F(DispelAndMonsterEffectsTest, TheDelayedHitIgnoresShields) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(701011);
	Ref<Npc> caster = makeNpc(701012, {}, 510, 500);
	Ref<CappingShield> shield = CappingShield::create();
	npc->getObserveController()->addAttackCalcObserver(*shield);
	const model::SkillTemplate* small = bindSkill(SMALL_DELAYED_BLAST_XML);

	Ref<Effect> effect = cast(*caster, *npc, small, 2);
	EXPECT_EQ(effect->getReserveds(1)->getValue(), 10) << "17 - 70 / 10";
	EXPECT_EQ(shield->checks, 0);
	advance(4000);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), NPC_MAX_HP - 10);
	npc->getObserveController()->removeAttackCalcObserver(*shield);
}

// ---- AlwaysBlockEffect (AlwaysBlockEffect.java:16-39), a monster's buff ------------------------------------------------------------------------

/**
 * 16423 Crouch, cast by a monster on itself: PHYSICAL_DEFENSE + 1000 % (1000 -> 11000) and the AlwaysBlockEffect$1 observer (value 10, BLOCK):
 * ten BLOCK checks answer true, the tenth ends the effect (`--value <= 0`), which takes the statup too; the eleventh is no block. Each observer
 * counts its own value: a second monster's Crouch still has its ten blocks.
 */
TEST_F(DispelAndMonsterEffectsTest, CrouchBlocksTenAttacks) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(701021);
	cp::RecordingAionConnection& observer = observe(*npc, 8351);
	Ref<Npc> other = makeMonster(701022, 505, 505);
	const model::SkillTemplate* crouch = bindSkill(CROUCH_XML);
	ASSERT_EQ(effectOf(*crouch, 1).javaClassName(), "AlwaysBlockEffect");
	ASSERT_EQ(statOf(*npc, StatEnum::PHYSICAL_DEFENSE, 1000), 1000);
	observer.clearSent();

	Ref<Effect> effect = cast(*npc, *npc, crouch, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(2));
	cast(*other, *other, crouch, 1);
	EXPECT_EQ(statOf(*npc, StatEnum::PHYSICAL_DEFENSE, 1000), 11000);
	EXPECT_EQ(effect->getDuration(), 5000);
	std::vector<std::vector<uint8_t>> announced = packetsOf<SM_ABNORMAL_EFFECT>(observer);
	ASSERT_FALSE(announced.empty());
	ASSERT_EQ(decodeAbnormalEffect(announced.back()).effects.size(), 1u);
	EXPECT_EQ(decodeAbnormalEffect(announced.back()).effects[0].skillId, 16423);
	EXPECT_FALSE(npc->getObserveController()->checkAttackStatus(AttackStatus::PARRY)) << "only BLOCK";

	for (int32_t block = 1; block <= 9; ++block)
		ASSERT_TRUE(npc->getObserveController()->checkAttackStatus(AttackStatus::BLOCK)) << block;
	EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(16423));
	EXPECT_TRUE(npc->getObserveController()->checkAttackStatus(AttackStatus::BLOCK)) << "the tenth";
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(16423)) << "value 0: effect.endEffect()";
	EXPECT_EQ(statOf(*npc, StatEnum::PHYSICAL_DEFENSE, 1000), 1000);
	EXPECT_FALSE(npc->getObserveController()->checkAttackStatus(AttackStatus::BLOCK));

	for (int32_t block = 1; block <= 10; ++block)
		ASSERT_TRUE(other->getObserveController()->checkAttackStatus(AttackStatus::BLOCK)) << "the other monster's own counter: " << block;
	EXPECT_FALSE(other->getObserveController()->checkAttackStatus(AttackStatus::BLOCK));
}

/** Unused blocks end with the effect after duration2 5,000 ms (Effect.endEffect -> removeObservers) */
TEST_F(DispelAndMonsterEffectsTest, UnusedBlocksEndWithCrouch) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(701031);
	const model::SkillTemplate* crouch = bindSkill(CROUCH_XML);
	Ref<Effect> effect = cast(*npc, *npc, crouch, 1);
	ASSERT_TRUE(npc->getObserveController()->checkAttackStatus(AttackStatus::BLOCK));
	advance(5000);
	EXPECT_TRUE(effect->isEndedByTime());
	EXPECT_FALSE(npc->getObserveController()->checkAttackStatus(AttackStatus::BLOCK));
}

/**
 * 2974 Shield of Faith (the Templar's, level 45: the first skill_tree row with an alwaysblock), whose only position is the alwaysblock:
 * AlwaysBlockEffect.applyEffect adds the effect to the controller itself (in 16423 position 1's statup does so as well); its ten blocks hold
 * duration2 30,000 ms.
 */
TEST_F(DispelAndMonsterEffectsTest, ShieldOfFaithIsAnAlwaysBlockAlone) {
	EFFECT_TEST_SCOPE;
	Ref<Player> templar = daeva(8391, PlayerClass::TEMPLAR);
	const model::SkillTemplate* shieldOfFaith = bindSkill(SHIELD_OF_FAITH_XML);
	ASSERT_EQ(effectOf(*shieldOfFaith, 0).javaClassName(), "AlwaysBlockEffect");
	Ref<Effect> effect = cast(*templar, *templar, shieldOfFaith, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(templar->getEffectController()->findBySkillId(2974), effect) << "AlwaysBlockEffect.applyEffect: addToEffectedController";
	EXPECT_EQ(effect->getDuration(), 30000);
	EXPECT_TRUE(templar->getObserveController()->checkAttackStatus(AttackStatus::BLOCK));
	advance(30000);
	EXPECT_TRUE(effect->isEndedByTime());
	EXPECT_FALSE(templar->getObserveController()->checkAttackStatus(AttackStatus::BLOCK));
}

// ---- CurseEffect (CurseEffect.java:15-34), a monster's debuff ----------------------------------------------------------------------------------

/**
 * 16436 Death Curse from a monster: its curse passes CURSE_RESISTANCE; BufEffect.startEffect adds MAXHP and MAXMP - 35 % (1000 -> 650) and
 * CURSE is set on the effect and the controller for duration2 + duration1 * level = 8,500 + 120 = 8,620 ms; the end unsets CURSE and the stat
 * functions go with the effect. CURSE_RESISTANCE 1000 resists the curse (effectPower 0), the hit of position 1 stays.
 */
TEST_F(DispelAndMonsterEffectsTest, DeathCurseShrinksTheMaximaForEightAndAHalfSeconds) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(701041);
	cp::RecordingAionConnection& observer = observe(*npc, 8361);
	Ref<Npc> caster = makeMonster(701042, 510, 500);
	const model::SkillTemplate* deathCurse = bindSkill(DEATH_CURSE_XML);
	ASSERT_EQ(effectOf(*deathCurse, 1).javaClassName(), "CurseEffect");
	ASSERT_EQ(statOf(*npc, StatEnum::MAXHP, 1000), 1000);
	ASSERT_EQ(statOf(*npc, StatEnum::MAXMP, 1000), 1000);
	observer.clearSent();

	Ref<Effect> effect = cast(*caster, *npc, deathCurse, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(2));
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::CURSE));
	EXPECT_EQ(effect->getAbnormals(), CURSE_ID);
	EXPECT_EQ(effect->getDuration(), 8620) << "8500 + 120 * 1";
	EXPECT_EQ(statOf(*npc, StatEnum::MAXHP, 1000), 650) << "1000 * -35 / 100 as a bonus";
	EXPECT_EQ(statOf(*npc, StatEnum::MAXMP, 1000), 650);
	std::vector<std::vector<uint8_t>> announced = packetsOf<SM_ABNORMAL_EFFECT>(observer);
	ASSERT_FALSE(announced.empty());
	EXPECT_EQ(decodeAbnormalEffect(announced.back()).abnormals, CURSE_ID);

	advance(8619);
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::CURSE));
	advance(1);
	EXPECT_TRUE(effect->isEndedByTime());
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::CURSE)) << "CurseEffect.endEffect";
	EXPECT_EQ(statOf(*npc, StatEnum::MAXHP, 1000), 1000);

	Ref<Npc> resistant = makeMonster(701043, 505, 505);
	addStat(*resistant, StatEnum::CURSE_RESISTANCE, 1000);
	Ref<Effect> resisted = calculated(*caster, *resistant, deathCurse);
	EXPECT_TRUE(resisted->isInSuccessEffects(1));
	EXPECT_FALSE(resisted->isInSuccessEffects(2)) << "CURSE_RESISTANCE";
	Ref<Npc> fearResistant = makeMonster(701044, 510, 505);
	addStat(*fearResistant, StatEnum::FEAR_RESISTANCE, 1000);
	EXPECT_TRUE(calculated(*caster, *fearResistant, deathCurse)->isInSuccessEffects(2));
}

// ---- FallEffect (FallEffect.java:16-32), a monster's skill ----------------------------------------------------------------------------------------

/**
 * 16905 Crash from a gargoyle on a flying player: the fall (noresist) lands and applyEffect ends the flight (FlyController.endFly(true): FLYING
 * off, and the landing broadcast, which the player gets as well). A player in INVULNERABLE_WING dodges the fall although it is noresist
 * (FallEffect.isDodgedOrResisted answers true first); the hit of position 1, a DamageEffect, stays a success. An npc effected is not a player:
 * the fall lands and does nothing.
 */
TEST_F(DispelAndMonsterEffectsTest, CrashEndsTheFlightUnlessTheWingsAreInvulnerable) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> gargoyle = makeMonster(701051, 510, 500);
	addStat(*gargoyle, StatEnum::MAGICAL_ACCURACY, 10000); // the hit of position 1 is never resisted
	const model::SkillTemplate* crash = bindSkill(CRASH_XML);
	ASSERT_EQ(effectOf(*crash, 1).javaClassName(), "FallEffect");
	auto fly = [](Player& player) {
		player.setFlyState(FlyState::FLYING);
		player.setState(CreatureState::FLYING);
	};

	Ref<Player> flyer = daeva(8371, PlayerClass::SORCERER);
	cp::RecordingAionConnection& client = connectLast(*flyer);
	fly(*flyer);
	client.clearSent();
	Ref<Effect> effect = cast(*gargoyle, *flyer, crash, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(2));
	EXPECT_FALSE(flyer->isInFlyState(FlyState::FLYING)) << "endFly";
	EXPECT_FALSE(flyer->isInState(CreatureState::FLYING));
	const std::vector<uint8_t> land = cp::serialized(SM_EMOTION(*flyer, gameserver::model::EmotionType::LAND), &client);
	const std::vector<std::vector<uint8_t>> emotions = packetsOf<SM_EMOTION>(client);
	EXPECT_EQ(std::count(emotions.begin(), emotions.end(), land), 1)
		<< "endFly(true): SM_EMOTION LAND to the sighted players and the player itself (FlyController.java:54-55)";

	Ref<Player> winged = daeva(8372, PlayerClass::SORCERER);
	fly(*winged);
	winged->getEffectController()->setAbnormal(AbnormalState::INVULNERABLE_WING);
	Ref<Effect> dodged = cast(*gargoyle, *winged, crash, 1);
	EXPECT_TRUE(dodged->isInSuccessEffects(1));
	EXPECT_FALSE(dodged->isInSuccessEffects(2)) << "INVULNERABLE_WING";
	EXPECT_TRUE(winged->isInFlyState(FlyState::FLYING));

	Ref<Npc> npc = makeMonster(701052);
	Ref<Effect> onNpc = forced(*gargoyle, *npc, crash);
	EXPECT_TRUE(onNpc->isInSuccessEffects(2)) << "applyEffect: not a player, nothing to do";
}

// ---- FpAttackInstantEffect (FpAttackInstantEffect.java:20-50), a monster's skill --------------------------------------------------------------

/**
 * 17235 Air Turbulence on a player: calculate reserves percent 35 of the flight time as FP damage, (maxFp * 35) / 100 in int arithmetic, and
 * applyEffect takes it (PlayerLifeStats.reduceFp: TYPE.FP_DAMAGE, LOG.FPATTACK, SM_ATTACK_STATUS to the player). 1995 Wing Clip's position 2
 * takes its flat value 15. On an npc (no FP) calculate reserves nothing and adds no success. The effects are forced (SkillEngine
 * .applyEffectDirectly): the physical hit of 17235's first position would roll a dodge.
 */
TEST_F(DispelAndMonsterEffectsTest, AirTurbulenceTakesAThirdOfTheFlightTime) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(701061, 510, 500);
	const model::SkillTemplate* turbulence = bindSkill(AIR_TURBULENCE_XML);
	bindSkill(STUNNED_XML);
	const model::SkillTemplate* wingClip = bindSkill(WING_CLIP_XML);
	ASSERT_EQ(effectOf(*turbulence, 1).javaClassName(), "FpAttackInstantEffect");

	Ref<Player> flyer = daeva(8381, PlayerClass::GUNNER);
	cp::RecordingAionConnection& self = connectLast(*flyer);
	// the flight time: gameserver.base.flytime is 60 (CustomConfig.java:80), unbound in this binary, so the 60 is a FLY_TIME bonus here
	addStat(*flyer, StatEnum::FLY_TIME, 60);
	const int32_t maxFp = flyer->getLifeStats()->getMaxFp();
	ASSERT_EQ(maxFp, 60);
	flyer->getLifeStats()->setCurrentFp(maxFp);
	self.clearSent();
	Ref<Effect> effect = forced(*npc, *flyer, turbulence);
	ASSERT_TRUE(effect->isInSuccessEffects(2));
	const int32_t taken = 21;
	EXPECT_EQ(effect->getReserveds(2)->getValue(), taken) << "(maxFP * value) / 100 = 60 * 35 / 100";
	EXPECT_EQ(effect->getReserveds(2)->getType(), model::EffectReserved::ResourceType::FP);
	EXPECT_EQ(flyer->getLifeStats()->getCurrentFp(), maxFp - taken);
	std::vector<AttackStatusFields> statuses = attackStatusesOf(self, flyer->getObjectId());
	const auto fp = std::find_if(statuses.begin(), statuses.end(), [](const AttackStatusFields& s) { return s.log == LOG_FPATTACK; });
	ASSERT_NE(fp, statuses.end());
	EXPECT_EQ(fp->type, TYPE_FP_DAMAGE);
	EXPECT_EQ(fp->value, -taken);
	EXPECT_EQ(fp->skillId, 17235);

	Ref<Player> gunnerTarget = daeva(8382, PlayerClass::SORCERER);
	addStat(*gunnerTarget, StatEnum::FLY_TIME, 60);
	addStat(*gunnerTarget, StatEnum::MAXHP, 100000); // the hit of position 1 must not kill the level-1 character
	gunnerTarget->getLifeStats()->setCurrentHp(gunnerTarget->getLifeStats()->getMaxHp());
	gunnerTarget->getLifeStats()->setCurrentFp(maxFp);
	Ref<Effect> clipped = forced(*npc, *gunnerTarget, wingClip);
	EXPECT_EQ(clipped->getReserveds(2)->getValue(), 15) << "percent false: value";
	EXPECT_EQ(gunnerTarget->getLifeStats()->getCurrentFp(), maxFp - 15);

	Ref<Npc> other = makeMonster(701062);
	Ref<Effect> onNpc = forced(*npc, *other, turbulence);
	EXPECT_TRUE(onNpc->isInSuccessEffects(1));
	EXPECT_FALSE(onNpc->isInSuccessEffects(2)) << "only players have FP";
	EXPECT_EQ(onNpc->getReserveds(2)->getValue(), 0);
}

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
