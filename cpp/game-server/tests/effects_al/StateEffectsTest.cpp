// P5-03, M5b-3 stage 1, items E-01, E-02, E-03 and E-04 (m5b3-plan.md §2.5, §2.6, §5): the effect classes of this chunk that items and
// materials reach - HiPassEffect (the third position of the starter Administrator's Boon, 10350), BlindEffect with its AttackStatusObserver (a
// godstone proc, 8539), and the two material-skill classes of D7 (W but recommended): DispelEffect (262 Remove Poison and four more material
// skills; the other dispel types on their data skills 518, 9928, 20355, 16600 and 18464) and FearEffect with its ATTACKED observer and its
// FearTask (304 Material Test Fear; 16704 Fear Casting for resistchance 75; 2918 Shield of Vengeance for a reflected fear). DispelEffect also
// runs EvadeEffect, Java's `class EvadeEffect extends DispelEffect {}` (a data-only generated struct here): the <evade> of the 10 templates that
// carry one, e.g. 283 Remove Shock I, which every class learns at level 40.
//
// Each case drives a real Effect through calculate -> applyEffect -> startEffect -> endEffect (EffectClassTestSupport.h) on skill templates
// whose <effects> are copied verbatim from skill_templates.xml (line cited per template; <properties>, conditions and motions left out: no
// Effect reads them), and asserts what the Java bodies do: HiPassEffect.java:5-11, BlindEffect.java:19-53, DispelEffect.java:15-84 and
// FearEffect.java:37-123. The fixture also fails a case that reaches an AION_UNPORTED site (TearDown).

#include "EffectClassTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/gameobjects/state/FlyState.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_POSITION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_CANCEL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/effect/BlindEffect.h"
#include "aion/gameserver/skillengine/effect/DispelEffect.h"
#include "aion/gameserver/skillengine/effect/FearEffect.h"
#include "aion/gameserver/skillengine/effect/HiPassEffect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/stats/StatFunctions.h"
#include "aion/gameserver/world/geo/GeoService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::effecttest {
namespace {

using controllers::attack::AttackStatus;
using effect::AbnormalState;
using gameserver::model::gameobjects::state::CreatureState;
using gameserver::model::gameobjects::state::CreatureVisualState;
using gameserver::model::stats::container::StatEnum;
using model::Effect;
using network::aion::serverpackets::SM_ABNORMAL_EFFECT;
using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_MOVE;
using network::aion::serverpackets::SM_POSITION;
using network::aion::serverpackets::SM_SKILL_CANCEL;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

/** Java AbnormalState.getId() of the states these bodies set (AbnormalState.java:9-40) */
constexpr int32_t POISON_ID = 1 << 0;
constexpr int32_t BLIND_ID = 1 << 5;
constexpr int32_t FEAR_ID = 1 << 9;

/**
 * A skill of the data with only its <skill_template> attributes and its <effects>, verbatim (the attributes Effect reads: tslot,
 * dispel_category, req_dispel_level, req_dispel_count, activation, skilltype; the rest is copied along)
 */
std::string dataSkill(std::string_view attributes, std::string_view effects) {
	return "<skill_template " + std::string(attributes) + "><effects>" + std::string(effects) + "</effects></skill_template>";
}

// ---- the skills of the data (skill_templates.xml) --------------------------------------------------------------------------------------

/** 10350 "Administrator's Boon" (:99663-99677), the skill of the starter 164002039 */
const std::string ADMINISTRATORS_BOON_XML = dataSkill(
	R"(skill_id="10350" name="Administrator's Boon" nameId="770429" stack="CASH_ITEM_START_KIT" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
	R"( tslot="BOOST" dispel_category="NEVER" activation="ACTIVE" cooldown="0" duration="0" noremoveatdie="true")",
	R"(<noresurrectpenalty duration2="3600000" effectid="2103501" e="1" basiclvl="1" noresist="true" />)"
	R"(<nodeathpenalty duration2="3600000" effectid="2103502" e="2" basiclvl="1" noresist="true" preeffect="1" />)"
	R"(<hipass duration2="3600000" effectid="2103503" e="3" basiclvl="1" noresist="true" preeffect="2" />)");

/** 8539 "Blindness" (:84004-84016), a godstone proc */
const std::string BLINDNESS_XML = dataSkill(
	R"(skill_id="8539" name="Blindness" nameId="701866" stack="ITEM_SKILL_PROC_BLIND_L1_40A" lvl="1" skilltype="MAGICAL" skillsubtype="NONE")"
	R"( tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10" activation="PROVOKED" cooldown="0")"
	R"( duration="0" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")",
	R"(<blind value="60" duration2="10000" effectid="20107" e="1" accmod2="100" element="FIRE" />)");

/** 8542 "Poison Slash" (:84048-84060), a godstone proc (PoisonEffect, P5-04) */
const std::string POISON_SLASH_XML = dataSkill(
	R"(skill_id="8542" name="Poison Slash" nameId="701869" stack="ITEM_SKILL_PROC_POISON_L1_40A" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="NONE" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10" activation="PROVOKED")"
	R"( cooldown="0" duration="0" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")",
	R"(<poison checktime="2000" value="38" duration2="20000" effectid="82001" e="1" accmod2="100" element="FIRE" />)");

/** 262 "Object Skill_TypeA Remove Poison" (:3283-3293), a material skill */
const std::string REMOVE_POISON_XML = dataSkill(
	R"(skill_id="262" name="Object Skill_TypeA Remove Poison" nameId="288207" stack="O_MAT_TYPEA_DISPELPOISON" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="10" duration="0" cancel_rate="20" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true")",
	R"(<dispel count="255" dispeltype="EFFECTTYPE" e="1" noresist="true" element="WATER"><effecttype>POISON</effecttype></dispel>)");

/** 518 "Ferocity" (:5587-5610): an EFFECTID dispel of 119682, then its own statup and statdown */
const std::string FEROCITY_XML = dataSkill(
	R"(skill_id="518" name="Ferocity" nameId="2287629" cooldownId="225" group="FI_FIGHTERRAGE" stack="FI_FIGHTERRAGE" lvl="1" skilltype="MAGICAL")"
	R"( skill_category="CHAIN_SKILL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10")"
	R"( activation="ACTIVE" cooldown="600" duration="0" cancel_rate="20" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true")",
	R"(<dispel count="255" dispeltype="EFFECTID" e="1" noresist="true" element="FIRE"><effectids>119682</effectids></dispel>)"
	R"(<statup duration2="8000" effectid="102252" e="2" noresist="true" preeffect="1" hoptype="SKILLLV" hopb="665">)"
	R"(<change stat="PHYSICAL_ATTACK" func="PERCENT" value="100" /></statup>)"
	R"(<statdown duration2="8000" effectid="102253" e="3" noresist="true" element="FIRE" preeffect="2">)"
	R"(<change stat="PHYSICAL_DEFENSE" func="PERCENT" value="-30" /></statdown>)");

/**
 * 283 "Remove Shock I" (:3493-3518): position 1 is an <evade> - EvadeEffect, Java's `class EvadeEffect extends DispelEffect {}`, so
 * DispelEffect.applyEffect runs it (an EFFECTTYPE dispel over the five stun states, count 255, dispel_level 1, the default power 100) - and
 * position 2 its statup of effect id 119682, preeffect 1
 */
const std::string REMOVE_SHOCK_XML = dataSkill(
	R"(skill_id="283" name="Remove Shock I" nameId="295006" cooldownId="1968" group="ALL_SHOCKREFLECT" stack="ALL_SHOCKREFLECT" lvl="1")"
	R"( skilltype="MAGICAL" skill_category="CHAIN_SKILL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1")"
	R"( req_dispel_count="10" activation="ACTIVE" cooldown="600" duration="0" cancel_rate="20" chain_skill_prob="100" hostile_type="INDIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<evade count="255" dispel_level="1" dispeltype="EFFECTTYPE" e="1" noresist="true" element="FIRE" hoptype="SKILLLV" hopb="578">)"
	R"(<effecttype>STUN</effecttype><effecttype>STAGGER</effecttype><effecttype>STUMBLE</effecttype><effecttype>SPIN</effecttype>)"
	R"(<effecttype>OPENAERIAL</effecttype></evade>)"
	R"(<statup duration2="7000" effectid="119682" e="2" noresist="true" preeffect="1"><change stat="STUN_RESISTANCE" func="ADD" value="1000" />)"
	R"(<change stat="STUMBLE_RESISTANCE" func="ADD" value="1000" /><change stat="STAGGER_RESISTANCE" func="ADD" value="1000" />)"
	R"(<change stat="SPIN_RESISTANCE" func="ADD" value="1000" /><change stat="OPENAERIAL_RESISTANCE" func="ADD" value="1000" /></statup>)");

/** 8361 "Arrow Flurry I Effect" (:81819-81827): a stun of req_dispel_level 1, req_dispel_count 10 */
const std::string ARROW_FLURRY_STUN_XML = dataSkill(
	R"(skill_id="8361" name="Arrow Flurry I Effect" nameId="286457" stack="RA_RAPIDBOW_PROC" lvl="1" skilltype="MAGICAL" skillsubtype="DEBUFF")"
	R"( tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10" activation="PROVOKED" cooldown="0" duration="0")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<stun duration2="2000" effectid="20000" e="1" noresist="true" element="FIRE" />)");

/** 16600 "Antidote" (:120900-120914): an EFFECTTYPE dispel of POISON, count 3, dispel level 3, power 50 */
const std::string ANTIDOTE_XML = dataSkill(
	R"(skill_id="16600" name="Antidote" nameId="284672" cooldownId="7" stack="NGR_DISPEL_POISON" lvl="1" skilltype="MAGICAL" skillsubtype="NONE")"
	R"( tslot="NONE" activation="ACTIVE" cooldown="0" duration="3000" cancel_rate="30" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true")",
	R"(<dispel count="3" dispel_level="3" power="50" dispeltype="EFFECTTYPE" e="1" element="WATER" hoptype="DAMAGE">)"
	R"(<effecttype>POISON</effecttype></dispel>)");

/** 8252 "Apply Poison I Effect" (:80372-80384): a poison of effect id 108522, req 1 / 10 */
const std::string APPLY_POISON_XML = dataSkill(
	R"(skill_id="8252" name="Apply Poison I Effect" nameId="282737" stack="AS_PROCPOISONEFFECT" lvl="1" skilltype="MAGICAL" skillsubtype="NONE")"
	R"( tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10" activation="PROVOKED" cooldown="0" duration="0")"
	R"( hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<poison checktime="3000" value="60" delta="2" duration2="9000" effectid="108522" e="1" noresist="true" element="FIRE" />)");

/** 18803 "Spore Shower" (:153589-153601): a poison of effect id 10188031, req 1 / 50 */
const std::string SPORE_SHOWER_XML = dataSkill(
	R"(skill_id="18803" name="Spore Shower" nameId="294892" cooldownId="10" stack="BNKN_POISONGAS_NR" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="50" activation="ACTIVE")"
	R"( cooldown="0" duration="2000" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<poison checktime="3000" value="328" duration2="15100" effectid="10188031" e="1" noresist="true" element="WATER" hoptype="SKILLLV")"
	R"( hopb="60" hopa="60" />)");

/** 17573 "Weight of Abyss" (:135580-135592): a poison of value 0 and effect id 10173231, req 1 / 10 */
const std::string WEIGHT_OF_ABYSS_XML = dataSkill(
	R"(skill_id="17573" name="Weight of Abyss" nameId="291646" cooldownId="13" stack="GFI_CANNOTFLY_FAKE" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE")"
	R"( cooldown="0" duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<poison checktime="3000" value="0" duration2="86400000" effectid="10173231" e="1" noresist="true" element="EARTH" />)");

/** 18464 "Happy Memory" (:148880-148895): an EFFECTID dispel of 10184622 and 10191102, count 1, dispel level 5, power 100 */
const std::string HAPPY_MEMORY_XML = dataSkill(
	R"(skill_id="18464" name="Happy Memory" nameId="293219" cooldownId="7" stack="NGR_DISPEL_MELEEDEBUFF_PRINCESS" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0" hostile_type="INDIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<dispel count="1" dispel_level="5" power="100" dispeltype="EFFECTID" e="1" noresist="true" element="EARTH" hoptype="SKILLLV" hopb="60")"
	R"( hopa="60"><effectids>10184622</effectids><effectids>10191102</effectids></dispel>)");

/** 18892 "Weeping Curtain" (:154864-154878): a <deboostheal> of effect id 10184622, req 5 / 100 */
const std::string WEEPING_CURTAIN_XML = dataSkill(
	R"(skill_id="18892" name="Weeping Curtain" nameId="295289" cooldownId="1" stack="IDCATACOMBS_SPECTRE_AREAHEALNUFF" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="DEBUFF_MENTAL" req_dispel_level="5" req_dispel_count="100" activation="ACTIVE")"
	R"( cooldown="0" duration="0" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<deboostheal duration2="15000" effectid="10184622" e="1" noresist="true" element="WIND">)"
	R"(<change stat="HEAL_SKILL_DEBOOST" func="ADD" value="-99999" /></deboostheal>)");

/** 19110 "Holy Shield" (:158059-158074): a shield, then a statup of effect id 10191102, req 5 / 100 */
const std::string HOLY_SHIELD_XML = dataSkill(
	R"(skill_id="19110" name="Holy Shield" nameId="296096" cooldownId="108" stack="FGC_SHIELD_CHIEF" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
	R"( tslot="BUFF" dispel_category="BUFF" req_dispel_level="5" req_dispel_count="100" activation="ACTIVE" cooldown="0" duration="0")"
	R"( hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<shield percent="true" hitvalue="30" value="90000000" duration2="86400000" effectid="154" e="1" basiclvl="250" noresist="true")"
	R"( hittype="EVERYHIT" hoptype="DAMAGE" />)"
	R"(<statup duration2="86400000" effectid="10191102" e="2" noresist="true" preeffect="1" hoptype="SKILLLV" hopb="60" hopa="60">)"
	R"(<change stat="PHYSICAL_DEFENSE" func="PERCENT" value="15" /></statup>)");

/** 2918 "Shield of Vengeance" (:49110-49122): a skill reflector (reflectType 1: ShieldType.SKILL_REFLECTOR) of 30 m for 10 s */
const std::string SHIELD_OF_VENGEANCE_XML = dataSkill(
	R"(skill_id="2918" name="Shield of Vengeance" nameId="2286898" cooldownId="1437" group="KN_ICYSHIELD" stack="KN_SKILLREFLECTOR" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE")"
	R"( cooldown="1836" cooldown_delta_lv="-36" duration="0" cancel_rate="20" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true")",
	R"(<reflector radius="30" value="0" duration2="10000" effectid="153" e="1" basiclvl="220" noresist="true" hittype="SKILL" hoptype="SKILLLV")"
	R"( hopb="1073" reflectType="1" />)");

/** 9928 "Dispel Magic" (:92409-92424): an EFFECTIDRANGE dispel of 10164471..10164472, count 2, dispel level 5, power 10 */
const std::string DISPEL_MAGIC_XML = dataSkill(
	R"(skill_id="9928" name="Dispel Magic" nameId="702154" stack="ITEM_QUEST_DISPELL" lvl="1" skilltype="MAGICAL" skillsubtype="NONE")"
	R"( tslot="NONE" activation="ACTIVE" cooldown="0" duration="0")",
	R"(<dispel count="2" dispel_level="5" power="10" dispeltype="EFFECTIDRANGE" e="1" noresist="true" element="WATER">)"
	R"(<effectids>10164471</effectids><effectids>10164472</effectids></dispel>)");

/** 16447 "Spout Sticky Protection Fluid" (:118657-118677), the buff 9928 dispels: two statups, effect ids 10164471 and 10164472 */
const std::string STICKY_FLUID_XML = dataSkill(
	R"(skill_id="16447" name="Spout Sticky Protection Fluid" nameId="283145" cooldownId="2" stack="QUEST_STATUPDEFENSE" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="5" req_dispel_count="10" activation="ACTIVE")"
	R"( cooldown="0" duration="700" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<statup duration2="86400000" effectid="10164471" e="1" noresist="true"><change stat="PHYSICAL_DEFENSE" func="PERCENT" value="4000" />)"
	R"(</statup><statup duration2="540000" effectid="10164472" e="2" noresist="true" preeffect="1">)"
	R"(<change stat="WATER_RESISTANCE" func="ADD" value="900" /><change stat="WIND_RESISTANCE" func="ADD" value="900" />)"
	R"(<change stat="FIRE_RESISTANCE" func="ADD" value="900" /><change stat="EARTH_RESISTANCE" func="ADD" value="900" /></statup>)");

/** 20355 "Cancel Torrent" (:174992-175006): a SLOTTYPE dispel of every BUFF */
const std::string CANCEL_TORRENT_XML = dataSkill(
	R"(skill_id="20355" name="Cancel Torrent" nameId="293433" cooldownId="7" stack="LDF4_DISPEL_STANDBY" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="DEBUFF" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0" hostile_type="INDIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
	R"(<dispel count="255" dispeltype="SLOTTYPE" e="1" noresist="true" element="WATER" hoptype="SKILLLV" hopb="60" hopa="60">)"
	R"(<slottype>BUFF</slottype></dispel>)");

/** 304 "Material Test Fear" (:3748-3757), a material skill: resistchance 100 (the default) */
const std::string MATERIAL_TEST_FEAR_XML = dataSkill(
	R"(skill_id="304" name="Material Test Fear" nameId="292645" stack="TEST_MATERIALFEAR" lvl="1" skilltype="MAGICAL" skillsubtype="DEBUFF")"
	R"( tslot="DEBUFF" activation="PROVOKED" cooldown="0" duration="0" cancel_rate="20" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true")",
	R"(<fear duration2="10000" effectid="20103" e="1" element="FIRE" />)");

/** 16704 "Fear Casting" (:122487-122499): resistchance 75, so an attack may end it */
const std::string FEAR_CASTING_XML = dataSkill(
	R"(skill_id="16704" name="Fear Casting" nameId="285567" cooldownId="12" stack="NEL_FEARTA_NR" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="DEBUFF_MENTAL" req_dispel_level="1" req_dispel_count="20" activation="ACTIVE")"
	R"( cooldown="0" duration="2000" ammospeed="25" cancel_rate="40" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true")",
	R"(<fear resistchance="75" duration2="3000" effectid="20103" e="1" element="WATER" hoptype="SKILLLV" hopb="60" hopa="60" />)");

/** 3222 Stealth (:54396-54414) without its conditions and randomtime (the hideXml of BuffEffectsTest), in the given visual state */
std::string stealthXml(int32_t skillId, std::string_view state) {
	return dataSkill(R"(skill_id=")" + std::to_string(skillId) + R"(" name="Stealth" nameId="2287794" cooldownId="175" group="SC_HIDE")"
			+ R"( stack="EFFECTS_AL_STATE_HIDE_)" + std::to_string(skillId) + R"(" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF")"
			+ R"( dispel_category="EXTRA" activation="ACTIVE" cooldown="600" duration="0" cancel_rate="20" hostile_type="INDIRECT")"
			+ R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true")",
		R"(<hide state=")" + std::string(state) + R"(" duration2="50000" effectid="165" e="1" basiclvl="1" noresist="true" hoptype="SKILLLV">)"
			R"(<change stat="SPEED" func="PERCENT" value="-40" /></hide>)");
}

/** A test skill of one BUFF or DEBUFF effect; `effects` is its <effects> body */
std::string testSkill(int32_t skillId, std::string_view slot, std::string_view extra, std::string_view effects) {
	return dataSkill(R"(skill_id=")" + std::to_string(skillId) + R"(" name="al state test" nameId="1" stack="EFFECTS_AL_STATE_)"
			+ std::to_string(skillId) + R"(" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot=")" + std::string(slot)
			+ R"(" activation="ACTIVE" cooldown="0" duration="0" )" + std::string(extra),
		effects);
}

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

/**
 * An NpcAI that records the ATTACK creature events it is handed (AITemplate.handleAttack, the DummyAI's, does nothing with them): the creature's
 * object id and the AI state the event arrived in
 */
class AttackRecordingAI final : public ai::NpcAI {
public:
	explicit AttackRecordingAI(Npc& owner) : NpcAI(owner) {}

	std::vector<std::pair<int32_t, ai::AIState>> attacks;

protected:
	void handleAttack(Ptr<Creature> creature) override { attacks.emplace_back(creature->getObjectId(), getState()); }
};

class StateEffectsTest : public EffectClassTest {
protected:
	void SetUp() override {
		EffectClassTest::SetUp();
		EFFECT_TEST_SCOPE;
		static const bool geoInitialised = [] {
			world::geo::GeoService::getInstance().init(); // geo data off: one empty GeoMap per test map (FearTask asks it for the flee point)
			return true;
		}();
		static_cast<void>(geoInitialised);
		runtime::resetUnportedHitsForTests();
	}

	void TearDown() override {
		// every path a case drives through these classes is ported to its end: no AION_UNPORTED site was reached
		EXPECT_EQ(runtime::unportedHitCount(), 0u) << [] {
			std::string sites;
			for (const runtime::UnportedHit& hit : runtime::unportedHits())
				sites += "\n  " + hit.file + ":" + std::to_string(hit.line) + " " + hit.function;
			return sites;
		}();
		EffectClassTest::TearDown();
	}

	/**
	 * Binds a <skill_template> through the real binder and publishes it in DataManager::SKILL_DATA, with every template this case bound before,
	 * and answers the published template: EffectController.removeEffect(skillId) (FearEffect$1, the case cleanups) finds its effect through
	 * SKILL_DATA. The holder is immortal, so a republish only forgets the previous one and the templates already in use stay valid.
	 */
	const model::SkillTemplate* bindSkill(const std::string& xmlText) {
		const std::string::size_type at = xmlText.find(R"(skill_id=")");
		EXPECT_NE(at, std::string::npos);
		const int32_t skillId = std::stoi(xmlText.substr(at + 10));
		publishedXml += xmlText;
		dataholders::DataManager::SKILL_DATA.resetForTests();
		publishSkillData(publishedXml);
		return dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId);
	}

	/** A seed whose first Rnd.chance() of the calling thread satisfies `accept` (the thread is left seeded with it, the stream not advanced) */
	template <class Predicate>
	static void seedWhereFirstChance(Predicate accept) {
		for (uint64_t seed = 1; seed < 100000; ++seed) {
			Rnd::seedCurrentThreadForTests(seed);
			if (accept(Rnd::chance())) {
				Rnd::seedCurrentThreadForTests(seed);
				return;
			}
		}
		ADD_FAILURE() << "no seed found";
	}

	/** Java `new Effect(effector, effected, template, level); effect.initialize();` without applyEffect */
	static Ref<Effect> calculated(Creature& effector, Creature& effected, const model::SkillTemplate* skill, int32_t level = 1) {
		Ref<Effect> effect = Effect::create(effector, Ptr<Creature>(effected), skill, level);
		effect->initialize();
		return effect;
	}

	/** The magical resist rate of the magical debuffs below leaves nothing to roll: the effected's resist is not above the caster's accuracy */
	static void assertNoMagicalResist(Creature& caster, Creature& target) {
		ASSERT_LE(target.getGameStats()->getMResist()->getCurrent() - caster.getGameStats()->getMAccuracy()->getCurrent(), 0);
	}

	std::string publishedXml;
};

// ---- HiPassEffect (HiPassEffect.java:5-11) --------------------------------------------------------------------------------------------------

/**
 * 10350 Administrator's Boon: its third position, <hipass>, only adds its success (no super.calculate), so the effect carries it into the BOOST
 * slot for an hour and Effect.isHiPass - TeleportService's "all flight/teleport prices are 1 kinah" check - finds it. A <hipass> whose pre-effect
 * is missing (preeffect="2" without a position 2) lands too, where EffectTemplate.calculate's validatePreEffects would refuse it.
 */
TEST_F(StateEffectsTest, TheBoonsHiPassLandsWithoutEffectTemplateCalculatesChecksAndLastsAnHour) {
	EFFECT_TEST_SCOPE;
	Ref<Player> newcomer = makePlayer(7601);
	const model::SkillTemplate* boon = bindSkill(ADMINISTRATORS_BOON_XML);
	ASSERT_EQ(effectOf(*boon, 2).javaClassName(), "HiPassEffect");
	Ref<Effect> effect = cast(*newcomer, *newcomer, boon, 1);
	EXPECT_TRUE(effect->isInSuccessEffects(3));
	EXPECT_TRUE(newcomer->getEffectController()->hasAbnormalEffect([](Effect& e) { return e.isHiPass(); }));
	EXPECT_EQ(effect->getDuration(), 3600000);
	advance(3600000);
	EXPECT_TRUE(effect->isEndedByTime());
	EXPECT_FALSE(newcomer->getEffectController()->hasAbnormalEffect([](Effect& e) { return e.isHiPass(); }));

	const model::SkillTemplate* lone = bindSkill(testSkill(7602, "BOOST", "",
		R"(<hipass duration2="3600000" effectid="2103503" e="1" basiclvl="1" noresist="true" preeffect="2" />)"));
	Ref<Effect> probe = Effect::create(*newcomer, Ptr<Creature>(newcomer), lone, 1);
	EXPECT_FALSE(probe->getEffectTemplates()[0]->calculate(*probe, std::nullopt, std::nullopt))
		<< "EffectTemplate.calculate refuses a position whose pre-effect is not a success";
	Ref<Effect> landed = cast(*newcomer, *newcomer, lone, 1);
	EXPECT_TRUE(landed->isInSuccessEffects(1));
	EXPECT_TRUE(newcomer->getEffectController()->hasAbnormalEffect([](Effect& e) { return e.isHiPass(); }));
	landed->endEffect();
}

// ---- BlindEffect (BlindEffect.java:19-53) ---------------------------------------------------------------------------------------------------

/**
 * 8539 Blindness, a godstone proc on a monster: calculate passes BLIND_RESISTANCE; startEffect sets BLIND on the effect and on the controller and
 * adds the AttackStatusObserver(60, DODGE) whose checkAttackerStatus answers Rnd.chance() < 60 - the blinded creature is the attacker whose hit
 * StatFunctions.checkIsDodgedHit asks it about, and a first chance below 60 makes its attack a dodge. It holds 10,000 ms; the end clears BLIND
 * and Effect.endEffect takes the observer off, after which the same chance no longer dodges.
 */
TEST_F(StateEffectsTest, ABlindedMonsterMissesSixtyPercentOfItsAttacksForTenSeconds) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700701);
	cp::RecordingAionConnection& observer = observe(*npc, 7611);
	Ref<Player> mage = players.back();
	ASSERT_NO_FATAL_FAILURE(assertNoMagicalResist(*mage, *npc));
	const model::SkillTemplate* blindness = bindSkill(BLINDNESS_XML);
	ASSERT_EQ(effectOf(*blindness, 0).javaClassName(), "BlindEffect");
	ASSERT_FALSE(npc->getObserveController()->checkAttackerStatus(AttackStatus::DODGE));
	observer.clearSent();

	Ref<Effect> blind = cast(*mage, *npc, blindness, 1);
	ASSERT_TRUE(blind->isInSuccessEffects(1));
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::BLIND));
	EXPECT_EQ(blind->getAbnormals(), BLIND_ID);
	EXPECT_EQ(blind->getDuration(), 10000);
	std::vector<std::vector<uint8_t>> announced = packetsOf<SM_ABNORMAL_EFFECT>(observer);
	ASSERT_EQ(announced.size(), 1u);
	EXPECT_EQ(decodeAbnormalEffect(announced[0]).abnormals, BLIND_ID);

	seedWhereFirstChance([](float chance) { return chance < 60.0f; });
	EXPECT_TRUE(npc->getObserveController()->checkAttackerStatus(AttackStatus::DODGE)) << "Rnd.chance() < value";
	seedWhereFirstChance([](float chance) { return chance >= 60.0f; });
	EXPECT_FALSE(npc->getObserveController()->checkAttackerStatus(AttackStatus::DODGE));
	seedWhereFirstChance([](float chance) { return chance < 60.0f; });
	EXPECT_TRUE(utils::stats::StatFunctions::checkIsDodgedHit(*npc, *mage, 0)) << "the blinded attacker's hit is a dodge";
	EXPECT_FALSE(mage->getObserveController()->checkAttackerStatus(AttackStatus::DODGE)) << "only the effected is blinded";

	advance(10000);
	EXPECT_TRUE(blind->isEndedByTime());
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::BLIND)) << "BlindEffect.endEffect";
	seedWhereFirstChance([](float chance) { return chance < 60.0f; });
	EXPECT_FALSE(npc->getObserveController()->checkAttackerStatus(AttackStatus::DODGE)) << "the observer went with the effect";
}

/** BlindEffect.calculate passes BLIND_RESISTANCE (BlindEffect.java:31), not another resistance */
TEST_F(StateEffectsTest, BlindResistanceDecidesTheBlindsCalculate) {
	EFFECT_TEST_SCOPE;
	Ref<Player> mage = makePlayer(7621);
	Ref<Npc> blindResistant = makeMonster(700702);
	addStat(*blindResistant, StatEnum::BLIND_RESISTANCE, 1000);
	Ref<Npc> fearResistant = makeMonster(700703, 505, 505);
	addStat(*fearResistant, StatEnum::FEAR_RESISTANCE, 1000);
	const model::SkillTemplate* blindness = bindSkill(BLINDNESS_XML);

	EXPECT_FALSE(calculated(*mage, *blindResistant, blindness)->isInSuccessEffects(1));
	EXPECT_TRUE(calculated(*mage, *fearResistant, blindness)->isInSuccessEffects(1));
}

/**
 * BlindEffect.applyEffect ends the effected's hide effects when its visual state without the BLINKING bit is below HIDE10 (BlindEffect.java:23-25):
 * a monster in 3222 Stealth's HIDE1 is shown again, one in HIDE10 stays hidden; both are blinded.
 */
TEST_F(StateEffectsTest, BlindnessEndsAHideBelowHide10) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> shallow = makeMonster(700711);
	observe(*shallow, 7631);
	Ref<Player> mage = players.back();
	Ref<Npc> deep = makeMonster(700712, 505, 505);
	const model::SkillTemplate* hide1 = bindSkill(stealthXml(7632, "HIDE1"));
	const model::SkillTemplate* hide10 = bindSkill(stealthXml(7633, "HIDE10"));
	const model::SkillTemplate* blindness = bindSkill(BLINDNESS_XML);
	cast(*shallow, *shallow, hide1, 1);
	cast(*deep, *deep, hide10, 1);
	ASSERT_TRUE(shallow->isInVisualState(CreatureVisualState::HIDE1));
	ASSERT_TRUE(deep->isInVisualState(CreatureVisualState::HIDE10));

	Ref<Effect> shallowBlind = cast(*mage, *shallow, blindness, 1);
	ASSERT_TRUE(shallowBlind->isInSuccessEffects(1));
	EXPECT_FALSE(shallow->getEffectController()->hasAbnormalEffect(7632)) << "removeHideEffects: 1 < HIDE10";
	EXPECT_FALSE(shallow->isInVisualState(CreatureVisualState::HIDE1));
	EXPECT_TRUE(shallow->getEffectController()->isAbnormalSet(AbnormalState::BLIND));

	Ref<Effect> deepBlind = cast(*mage, *deep, blindness, 1);
	ASSERT_TRUE(deepBlind->isInSuccessEffects(1));
	EXPECT_TRUE(deep->getEffectController()->hasAbnormalEffect(7633)) << "10 is not below HIDE10";
	EXPECT_TRUE(deep->isInVisualState(CreatureVisualState::HIDE10));
	EXPECT_TRUE(deep->getEffectController()->isAbnormalSet(AbnormalState::BLIND));
	shallowBlind->endEffect();
	deepBlind->endEffect();
	deep->getEffectController()->removeEffect(7633);
}

/**
 * BlindEffect.applyEffect leaves the BLINKING bit out of the comparison (`getVisualState() & ~BLINKING.getId()`, BlindEffect.java:23): a player
 * who blinks (PlayerController sets BLINKING on entering the world or teleporting) and hides in 3222 Stealth's HIDE1 has the visual state 65,
 * which is not below HIDE10, but 65 & ~64 = 1 is, so a monster's 8539 Blindness still ends the hide.
 */
TEST_F(StateEffectsTest, BlindnessEndsTheHideOfABlinkingPlayer) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700713);
	Ref<Player> hider = makePlayer(7634);
	const model::SkillTemplate* hide1 = bindSkill(stealthXml(7635, "HIDE1"));
	const model::SkillTemplate* blindness = bindSkill(BLINDNESS_XML);
	hider->setVisualState(CreatureVisualState::BLINKING);
	cast(*hider, *hider, hide1, 1);
	ASSERT_EQ(hider->getVisualState(), 65) << "BLINKING (64) | HIDE1 (1)";
	ASSERT_NO_FATAL_FAILURE(assertNoMagicalResist(*npc, *hider));

	Ref<Effect> blind = cast(*npc, *hider, blindness, 1);
	ASSERT_TRUE(blind->isInSuccessEffects(1));
	EXPECT_FALSE(hider->getEffectController()->hasAbnormalEffect(7635)) << "removeHideEffects: 65 & ~64 = 1 < HIDE10";
	EXPECT_FALSE(hider->isInVisualState(CreatureVisualState::HIDE1));
	EXPECT_TRUE(hider->isInVisualState(CreatureVisualState::BLINKING)) << "the blink is not the hide's";
	EXPECT_TRUE(hider->getEffectController()->isAbnormalSet(AbnormalState::BLIND));
	blind->endEffect();
}

// ---- DispelEffect (DispelEffect.java:15-84) -------------------------------------------------------------------------------------------------

/**
 * 262 Remove Poison, the material skill: dispeltype EFFECTTYPE with the one type POISON, count 255, dispel level and power 100 (the defaults):
 * EffectController.removeByDispelEffect(POISON, null, 255, 100, 100) ends every effect whose skill has a <poison> and whose req_dispel_level and
 * req_dispel_count the 100 cover - 8542 Poison Slash (1 and 10) - and leaves 8539 Blindness.
 */
TEST_F(StateEffectsTest, RemovePoisonEndsThePoisonAndLeavesTheBlindness) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700721);
	Ref<Player> victim = makePlayer(7641);
	const model::SkillTemplate* poisonSlash = bindSkill(POISON_SLASH_XML);
	const model::SkillTemplate* blindness = bindSkill(BLINDNESS_XML);
	const model::SkillTemplate* removePoison = bindSkill(REMOVE_POISON_XML);
	ASSERT_EQ(effectOf(*removePoison, 0).javaClassName(), "DispelEffect");
	Ref<Effect> poison = cast(*npc, *victim, poisonSlash, 1);
	Ref<Effect> blind = cast(*npc, *victim, blindness, 1);
	ASSERT_TRUE(victim->getEffectController()->isAbnormalSet(AbnormalState::POISON));
	ASSERT_EQ(poison->getAbnormals(), POISON_ID);
	ASSERT_TRUE(victim->getEffectController()->hasAbnormalEffect(8539));

	Ref<Effect> cure = cast(*victim, *victim, removePoison, 1);
	ASSERT_TRUE(cure->isInSuccessEffects(1));
	EXPECT_FALSE(victim->getEffectController()->hasAbnormalEffect(8542)) << "the poison was dispelled";
	EXPECT_FALSE(victim->getEffectController()->isAbnormalSet(AbnormalState::POISON));
	EXPECT_TRUE(victim->getEffectController()->hasAbnormalEffect(8539)) << "not a <poison>";
	blind->endEffect();
}

/**
 * 16600 Antidote: EFFECTTYPE POISON, count 3, dispel level 3, power 50 - removeByDispelEffect(POISON, null, 3, 3, 50) passes the level and the
 * power apart (DispelEffect.java:74). On four poisons of req_dispel_level 1 in the controller's order 8542 Poison Slash (req_dispel_count 10),
 * 8252 Apply Poison I Effect (10), 18803 Spore Shower (50: 50 - 50 <= 0 still removes it) and 17573 Weight of Abyss (10), each of the first three
 * passes the level check (1 <= 3), is removed (EffectController.removePower: power - 50 <= 0) and counts down, so the count of 3 is spent before
 * the fourth, which keeps POISON set. A dispel level of 50 and a power of 3 would wear each down (10 - 3, 50 - 3) and remove none; a larger count
 * would remove the fourth as well.
 */
TEST_F(StateEffectsTest, AntidoteRemovesThreePoisonsWithItsDispelLevelThreeAndPowerFifty) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700722);
	Ref<Player> victim = makePlayer(7642);
	ASSERT_NO_FATAL_FAILURE(assertNoMagicalResist(*victim, *victim));
	const model::SkillTemplate* poisonSlash = bindSkill(POISON_SLASH_XML);
	const model::SkillTemplate* applyPoison = bindSkill(APPLY_POISON_XML);
	const model::SkillTemplate* sporeShower = bindSkill(SPORE_SHOWER_XML);
	const model::SkillTemplate* weightOfAbyss = bindSkill(WEIGHT_OF_ABYSS_XML);
	const model::SkillTemplate* antidote = bindSkill(ANTIDOTE_XML);
	for (const model::SkillTemplate* poison : {poisonSlash, applyPoison, sporeShower, weightOfAbyss}) {
		ASSERT_TRUE(cast(*npc, *victim, poison, 1)->isInSuccessEffects(1)) << poison->getSkillId();
		ASSERT_TRUE(victim->getEffectController()->hasAbnormalEffect(poison->getSkillId())) << poison->getSkillId();
	}

	Ref<Effect> cure = cast(*victim, *victim, antidote, 1);
	ASSERT_TRUE(cure->isInSuccessEffects(1));
	EXPECT_FALSE(victim->getEffectController()->hasAbnormalEffect(8542)) << "req 1 / 10: 1 <= 3 and 10 - 50 <= 0";
	EXPECT_FALSE(victim->getEffectController()->hasAbnormalEffect(8252)) << "req 1 / 10";
	EXPECT_FALSE(victim->getEffectController()->hasAbnormalEffect(18803)) << "req 1 / 50: 50 - 50 <= 0";
	EXPECT_TRUE(victim->getEffectController()->hasAbnormalEffect(17573)) << "count 3 was spent on the three before it";
	EXPECT_TRUE(victim->getEffectController()->isAbnormalSet(AbnormalState::POISON)) << "17573's poison";

	cast(*victim, *victim, antidote, 1);
	EXPECT_FALSE(victim->getEffectController()->hasAbnormalEffect(17573)) << "a second Antidote";
	EXPECT_FALSE(victim->getEffectController()->isAbnormalSet(AbnormalState::POISON));
}

/**
 * 283 Remove Shock I as the data has it: its <evade> is an EvadeEffect (Java's `class EvadeEffect extends DispelEffect {}`), so DispelEffect's
 * EFFECTTYPE arm runs it over STUN, STAGGER, STUMBLE, SPIN and OPENAERIAL with count 255, dispel_level 1 and the default power 100:
 * removeByDispelEffect(STUN, null, 255, 1, 100) ends 8361 Arrow Flurry I Effect's stun (req_dispel_level 1 <= 1, req_dispel_count 10 - 100 <= 0).
 * A dispel level of 100 and a power of 1 would only wear it down to 9. Its statup of preeffect 1 then lands.
 */
TEST_F(StateEffectsTest, RemoveShockEvadesAStunWithDispelLevelOneAndPowerAHundred) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700723);
	Ref<Player> fighter = makePlayer(7643);
	fighter->setFlyController(std::make_unique<controllers::FlyController>(*fighter)); // StunEffect.startEffect stops a player's glide
	const model::SkillTemplate* arrowFlurry = bindSkill(ARROW_FLURRY_STUN_XML);
	const model::SkillTemplate* removeShock = bindSkill(REMOVE_SHOCK_XML);
	ASSERT_EQ(effectOf(*removeShock, 0).javaClassName(), "EvadeEffect");
	ASSERT_NE(dynamic_cast<const effect::DispelEffect*>(&effectOf(*removeShock, 0)), nullptr) << "EvadeEffect extends DispelEffect";
	Ref<Effect> stun = cast(*npc, *fighter, arrowFlurry, 1);
	ASSERT_TRUE(stun->isInSuccessEffects(1));
	ASSERT_TRUE(fighter->getEffectController()->isAbnormalSet(AbnormalState::STUN));

	Ref<Effect> shock = cast(*fighter, *fighter, removeShock, 1);
	ASSERT_TRUE(shock->isInSuccessEffects(1));
	EXPECT_FALSE(fighter->getEffectController()->hasAbnormalEffect(8361)) << "the <evade> ended the stun";
	EXPECT_FALSE(fighter->getEffectController()->isAbnormalSet(AbnormalState::STUN));
	EXPECT_TRUE(shock->isInSuccessEffects(2)) << "the statup of preeffect 1";
	EXPECT_EQ(statOf(*fighter, StatEnum::STUN_RESISTANCE, 0), 1000);
	shock->endEffect();
}

/**
 * 518 Ferocity: dispeltype EFFECTID, effect id 119682, count 255 - removeByEffectId(119682, 100, 100) ends 283 Remove Shock I (its statup of
 * effect id 119682, req 1 / 10; its <evade> found no stun to end). An EFFECTID dispel ends at most `count` effects in the order of its ids
 * (DispelEffect.java:55-61): with count 1 and the ids 10164471 and 10164472, only the first id's effect goes. The dispel level is compared with
 * each effect's req_dispel_level.
 */
TEST_F(StateEffectsTest, AnEffectIdDispelEndsTheEffectsOfItsIdsUpToItsCountAndDispelLevel) {
	EFFECT_TEST_SCOPE;
	Ref<Player> fighter = makePlayer(7651);
	const model::SkillTemplate* removeShock = bindSkill(REMOVE_SHOCK_XML);
	const model::SkillTemplate* ferocity = bindSkill(FEROCITY_XML);
	Ref<Effect> shock = cast(*fighter, *fighter, removeShock, 1);
	ASSERT_TRUE(shock->isInSuccessEffects(2));
	ASSERT_TRUE(fighter->getEffectController()->hasAbnormalEffect(283));
	ASSERT_EQ(statOf(*fighter, StatEnum::STUN_RESISTANCE, 0), 1000);

	Ref<Effect> rage = cast(*fighter, *fighter, ferocity, 1);
	ASSERT_TRUE(rage->isInSuccessEffects(1));
	EXPECT_FALSE(fighter->getEffectController()->hasAbnormalEffect(283)) << "effect id 119682 dispelled";
	EXPECT_EQ(statOf(*fighter, StatEnum::STUN_RESISTANCE, 0), 0);
	EXPECT_TRUE(fighter->getEffectController()->hasAbnormalEffect(518)) << "518's own positions 2 and 3 landed after the dispel";
	rage->endEffect();

	const model::SkillTemplate* first = bindSkill(testSkill(7652, "BUFF", R"(req_dispel_level="1" req_dispel_count="10")",
		R"(<statup duration2="60000" effectid="10164471" e="1" noresist="true"><change stat="MAXHP" func="ADD" value="1" /></statup>)"));
	const model::SkillTemplate* second = bindSkill(testSkill(7653, "BUFF", R"(req_dispel_level="1" req_dispel_count="10")",
		R"(<statup duration2="60000" effectid="10164472" e="1" noresist="true"><change stat="MAXMP" func="ADD" value="1" /></statup>)"));
	const model::SkillTemplate* guarded = bindSkill(testSkill(7654, "BUFF", R"(req_dispel_level="5" req_dispel_count="10")",
		R"(<statup duration2="60000" effectid="10164473" e="1" noresist="true"><change stat="MAXHP" func="ADD" value="2" /></statup>)"));
	const model::SkillTemplate* oneOfTwo = bindSkill(testSkill(7655, "NONE", "",
		R"(<dispel count="1" dispeltype="EFFECTID" e="1" noresist="true"><effectids>10164471</effectids><effectids>10164472</effectids>)"
		R"(<effectids>10164473</effectids></dispel>)"));
	const model::SkillTemplate* lowLevel = bindSkill(testSkill(7656, "NONE", "",
		R"(<dispel count="255" dispel_level="4" dispeltype="EFFECTID" e="1" noresist="true"><effectids>10164473</effectids></dispel>)"));
	cast(*fighter, *fighter, first, 1);
	cast(*fighter, *fighter, second, 1);
	cast(*fighter, *fighter, guarded, 1);
	cast(*fighter, *fighter, oneOfTwo, 1);
	EXPECT_FALSE(fighter->getEffectController()->hasAbnormalEffect(7652)) << "the first id's effect";
	EXPECT_TRUE(fighter->getEffectController()->hasAbnormalEffect(7653)) << "count 1: the loop breaks before the second id";
	cast(*fighter, *fighter, lowLevel, 1);
	EXPECT_TRUE(fighter->getEffectController()->hasAbnormalEffect(7654)) << "req_dispel_level 5 > dispel_level 4";
	fighter->getEffectController()->removeEffect(7653);
	fighter->getEffectController()->removeEffect(7654);
}

/**
 * 18464 Happy Memory: EFFECTID over 10184622 then 10191102, count 1, dispel level 5, power 100 - removeByEffectId(id, 5, 100) (DispelEffect.java:59,
 * the level before the power). 18892 Weeping Curtain's <deboostheal> (effect id 10184622, req 5 / 100) goes first: 5 <= 5 and 100 - 100 <= 0.
 * The count of 1 then ends the loop before the second id, so 19110 Holy Shield's statup (10191102, req 5 / 100) stays until a second cast, whose
 * first id finds nothing. A dispel level of 100 and a power of 5 would wear both down to 95 and remove neither; a larger count would remove both
 * at once.
 */
TEST_F(StateEffectsTest, HappyMemoryEndsOneEffectOfItsIdsPerCastWithDispelLevelFiveAndPowerAHundred) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700732);
	Ref<Player> player = makePlayer(7666);
	const model::SkillTemplate* weepingCurtain = bindSkill(WEEPING_CURTAIN_XML);
	const model::SkillTemplate* holyShield = bindSkill(HOLY_SHIELD_XML);
	const model::SkillTemplate* happyMemory = bindSkill(HAPPY_MEMORY_XML);
	ASSERT_TRUE(cast(*npc, *player, weepingCurtain, 1)->isInSuccessEffects(1));
	ASSERT_TRUE(cast(*player, *player, holyShield, 1)->isInSuccessEffects(2));
	ASSERT_TRUE(player->getEffectController()->hasAbnormalEffect(18892));
	ASSERT_TRUE(player->getEffectController()->hasAbnormalEffect(19110));

	Ref<Effect> memory = cast(*player, *player, happyMemory, 1);
	ASSERT_TRUE(memory->isInSuccessEffects(1));
	EXPECT_FALSE(player->getEffectController()->hasAbnormalEffect(18892)) << "10184622: req 5 / 100 against dispel level 5, power 100";
	EXPECT_TRUE(player->getEffectController()->hasAbnormalEffect(19110)) << "count 1: the loop breaks before 10191102";

	cast(*player, *player, happyMemory, 1);
	EXPECT_FALSE(player->getEffectController()->hasAbnormalEffect(19110)) << "the second cast: 10184622 finds nothing, 10191102 the statup";
}

/**
 * 9928 Dispel Magic: dispeltype EFFECTIDRANGE from effectids[0] to effectids[1] inclusive, count 2, dispel level 5, power 10 + dpower * level.
 * On 16447 Spout Sticky Protection Fluid (both effect ids in one effect, req 5 / 10) the first id ends the whole effect. On two effects of the
 * two ids, count 2 ends both (the upper bound is inclusive). A power of 10 only wears down an effect of req_dispel_count 20 (EffectController
 * .removePower: removed when its power drops to 0 or below); a dpower of 5 at skill level 2 makes it 20, which removes it at once.
 */
TEST_F(StateEffectsTest, AnEffectIdRangeDispelCoversBothEndsWithItsPowerPlusDpowerTimesTheLevel) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700731);
	Ref<Player> hunter = makePlayer(7661);
	const model::SkillTemplate* dispelMagic = bindSkill(DISPEL_MAGIC_XML);
	const model::SkillTemplate* stickyFluid = bindSkill(STICKY_FLUID_XML);
	Ref<Effect> fluid = cast(*npc, *npc, stickyFluid, 1);
	ASSERT_TRUE(fluid->isInSuccessEffects(2));
	ASSERT_TRUE(npc->getEffectController()->hasAbnormalEffect(16447));

	Ref<Effect> dispel = cast(*hunter, *npc, dispelMagic, 1);
	ASSERT_TRUE(dispel->isInSuccessEffects(1));
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(16447));

	const model::SkillTemplate* low = bindSkill(testSkill(7662, "BUFF", R"(req_dispel_level="5" req_dispel_count="10")",
		R"(<statup duration2="60000" effectid="10164471" e="1" noresist="true"><change stat="MAXHP" func="ADD" value="1" /></statup>)"));
	const model::SkillTemplate* high = bindSkill(testSkill(7663, "BUFF", R"(req_dispel_level="5" req_dispel_count="10")",
		R"(<statup duration2="60000" effectid="10164472" e="1" noresist="true"><change stat="MAXMP" func="ADD" value="1" /></statup>)"));
	cast(*npc, *npc, low, 1);
	cast(*npc, *npc, high, 1);
	cast(*hunter, *npc, dispelMagic, 1);
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(7662)) << "effectids[0]";
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(7663)) << "effectids[1]: the range includes its upper end";

	const model::SkillTemplate* tough = bindSkill(testSkill(7664, "BUFF", R"(req_dispel_level="5" req_dispel_count="20")",
		R"(<statup duration2="60000" effectid="10164471" e="1" noresist="true"><change stat="MAXHP" func="ADD" value="3" /></statup>)"));
	cast(*npc, *npc, tough, 1);
	cast(*hunter, *npc, dispelMagic, 1);
	EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(7664)) << "power 10 against 20: worn down to 10, not removed";
	cast(*hunter, *npc, dispelMagic, 1);
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(7664)) << "the second 10 removes it";

	const model::SkillTemplate* stronger = bindSkill(testSkill(7665, "NONE", "",
		R"(<dispel count="2" dispel_level="5" power="10" dpower="5" dispeltype="EFFECTIDRANGE" e="1" noresist="true">)"
		R"(<effectids>10164471</effectids><effectids>10164472</effectids></dispel>)"));
	cast(*npc, *npc, tough, 1);
	cast(*hunter, *npc, stronger, 2);
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(7664)) << "10 + 5 * 2 = 20 removes it at once";
	cast(*npc, *npc, tough, 1);
	cast(*hunter, *npc, stronger, 1);
	EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(7664)) << "10 + 5 * 1 = 15 does not";
	npc->getEffectController()->removeEffect(7664);
}

/**
 * 20355 Cancel Torrent: dispeltype SLOTTYPE with the one slot BUFF, count 255 - removeByDispelEffect(null, BUFF, 255, 100, 100) ends the effects
 * in the BUFF target slot (16447's) and leaves a DEBUFF (8539 Blindness).
 */
TEST_F(StateEffectsTest, ASlotTypeDispelEndsTheEffectsOfItsSlot) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = makeMonster(700741);
	Ref<Player> hunter = makePlayer(7671);
	const model::SkillTemplate* stickyFluid = bindSkill(STICKY_FLUID_XML);
	const model::SkillTemplate* blindness = bindSkill(BLINDNESS_XML);
	const model::SkillTemplate* cancelTorrent = bindSkill(CANCEL_TORRENT_XML);
	cast(*npc, *npc, stickyFluid, 1);
	Ref<Effect> blind = cast(*hunter, *npc, blindness, 1);
	ASSERT_TRUE(npc->getEffectController()->hasAbnormalEffect(8539));

	Ref<Effect> cancel = cast(*hunter, *npc, cancelTorrent, 1);
	ASSERT_TRUE(cancel->isInSuccessEffects(1));
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(16447)) << "a BUFF";
	EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(8539)) << "a DEBUFF";
	blind->endEffect();
}

/**
 * The SLOTTYPE arm passes its count, dispel level and power apart as well (DispelEffect.java:79). The data's four SLOTTYPE dispels all have count
 * 255 and level and power 100, so this test skill differs from 20355 only there: count 1, dispel level 5, power 10. On two BUFFs of req 5 / 10,
 * removeByDispelEffect(null, BUFF, 1, 5, 10) removes the first the controller holds (5 <= 5, 10 - 10 <= 0) and its count is spent; a second cast
 * removes the other. A dispel level of 10 and a power of 5 would wear the first down to 5 and remove nothing; a larger count would take both.
 */
TEST_F(StateEffectsTest, ASlotTypeDispelPassesItsCountDispelLevelAndPowerApart) {
	EFFECT_TEST_SCOPE;
	Ref<Player> player = makePlayer(7672);
	const model::SkillTemplate* first = bindSkill(testSkill(7673, "BUFF", R"(req_dispel_level="5" req_dispel_count="10")",
		R"(<statup duration2="60000" effectid="10164481" e="1" noresist="true"><change stat="MAXHP" func="ADD" value="1" /></statup>)"));
	const model::SkillTemplate* second = bindSkill(testSkill(7674, "BUFF", R"(req_dispel_level="5" req_dispel_count="10")",
		R"(<statup duration2="60000" effectid="10164482" e="1" noresist="true"><change stat="MAXMP" func="ADD" value="1" /></statup>)"));
	const model::SkillTemplate* cancel = bindSkill(testSkill(7675, "NONE", "",
		R"(<dispel count="1" dispel_level="5" power="10" dispeltype="SLOTTYPE" e="1" noresist="true"><slottype>BUFF</slottype></dispel>)"));
	cast(*player, *player, first, 1);
	cast(*player, *player, second, 1);
	ASSERT_TRUE(player->getEffectController()->hasAbnormalEffect(7673));
	ASSERT_TRUE(player->getEffectController()->hasAbnormalEffect(7674));

	cast(*player, *player, cancel, 1);
	EXPECT_FALSE(player->getEffectController()->hasAbnormalEffect(7673)) << "req 5 / 10 against dispel level 5, power 10";
	EXPECT_TRUE(player->getEffectController()->hasAbnormalEffect(7674)) << "count 1 was spent on the first";
	cast(*player, *player, cancel, 1);
	EXPECT_FALSE(player->getEffectController()->hasAbnormalEffect(7674));
}

/**
 * The early returns of DispelEffect.applyEffect (DispelEffect.java:36-49): no dispeltype, or the type's list absent (JAXB null, an empty bound
 * list) - nothing is dispelled. And EFFECTIDRANGE reads effectids.get(1): a range of one id throws Java's IndexOutOfBoundsException.
 */
TEST_F(StateEffectsTest, ADispelWithoutItsTypeOrItsListDispelsNothingAndAOneIdRangeThrows) {
	EFFECT_TEST_SCOPE;
	Ref<Player> p = makePlayer(7681);
	const model::SkillTemplate* buff = bindSkill(testSkill(7682, "BUFF", R"(req_dispel_level="1" req_dispel_count="10")",
		R"(<statup duration2="60000" effectid="10164471" e="1" noresist="true"><change stat="MAXHP" func="ADD" value="1" /></statup>)"));
	cast(*p, *p, buff, 1);
	int32_t skillId = 7683;
	for (std::string_view dispel : {R"(<dispel count="255" e="1" noresist="true"><effectids>10164471</effectids></dispel>)",
			 R"(<dispel count="255" dispeltype="EFFECTID" e="1" noresist="true"/>)",
			 R"(<dispel count="255" dispeltype="EFFECTIDRANGE" e="1" noresist="true"/>)",
			 R"(<dispel count="255" dispeltype="EFFECTTYPE" e="1" noresist="true"/>)",
			 R"(<dispel count="255" dispeltype="SLOTTYPE" e="1" noresist="true"/>)"}) {
		const model::SkillTemplate* skill = bindSkill(testSkill(skillId++, "NONE", "", dispel));
		Ref<Effect> effect = cast(*p, *p, skill, 1);
		EXPECT_TRUE(effect->isInSuccessEffects(1)) << dispel;
		EXPECT_TRUE(p->getEffectController()->hasAbnormalEffect(7682)) << dispel;
	}
	const model::SkillTemplate* oneId = bindSkill(testSkill(7689, "NONE", "",
		R"(<dispel count="255" dispeltype="EFFECTIDRANGE" e="1" noresist="true"><effectids>10164471</effectids></dispel>)"));
	Ref<Effect> range = Effect::create(*p, Ptr<Creature>(p), oneId, 1);
	EXPECT_THROW(range->getEffectTemplates()[0]->applyEffect(*range), runtime::IndexOutOfBoundsException);
	p->getEffectController()->removeEffect(7682);
}

// ---- FearEffect (FearEffect.java:37-123) ----------------------------------------------------------------------------------------------------

/**
 * 304 Material Test Fear on a casting, moving monster, with gameserver.geodata.fear.enable on: calculate passes FEAR_RESISTANCE; startEffect
 * cancels the cast (the effector, a player, is told STR_SKILL_TARGET_SKILL_CANCELED: CreatureController.cancelCurrentSkill's lastAttacker), sets
 * FEAR on the effect and the controller, stops the move, puts the npc in WEAPON_EQUIPPED (EmoteManager.emoteStartAttacking: the two
 * SM_EMOTIONs) and in AIState.FEAR, and schedules the FearTask at a fixed rate of 1 s from 0 ms: while the npc is under fear and within 40 m of
 * the effector it moves to the flee point (NpcMoveController.moveToPoint: the npc starts moving; without geo terrain the point is where it
 * stands, see AFearedPlayerStopsGlidingLeavesWalkModeAndRunsAway). resistchance is 100, so no ATTACKED observer.
 * After duration2 = 10,000 ms the end clears FEAR, stops the move, sends SM_POSITION to those who see the npc, sets AIState.IDLE and hands the
 * AI an ATTACK event; the task was cancelled with the effect.
 */
TEST_F(StateEffectsTest, MaterialTestFearMakesAMonsterFleeForTenSeconds) {
	EFFECT_TEST_SCOPE;
	FearEnable fearEnabled(true);
	Ref<Npc> npc = makeMonster(700751);
	cp::RecordingAionConnection& observer = observe(*npc, 7691);
	Ref<Player> mage = players.back();
	ASSERT_NO_FATAL_FAILURE(assertNoMagicalResist(*mage, *npc));
	const model::SkillTemplate* fear = bindSkill(MATERIAL_TEST_FEAR_XML);
	ASSERT_EQ(effectOf(*fear, 0).javaClassName(), "FearEffect");
	npc->setCasting(model::Skill::create(fear, *npc, 1, Ptr<Creature>(mage), nullptr));
	ASSERT_FALSE(npc->isInState(CreatureState::WEAPON_EQUIPPED));
	npc->getMoveController()->moveToPoint(510, 500, 100);
	ASSERT_TRUE(npc->getMoveController()->isInMove());
	observer.clearSent();

	Ref<Effect> effect = cast(*mage, *npc, fear, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_FALSE(npc->getCastingSkill()) << "effected.getController().cancelCurrentSkill(effector)";
	EXPECT_EQ(packetsOf<SM_SKILL_CANCEL>(observer).size(), 1u);
	const std::vector<uint8_t> canceled = cp::serialized(SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_SKILL_CANCELED(), &observer);
	const std::vector<std::vector<uint8_t>> messages = packetsOf<SM_SYSTEM_MESSAGE>(observer);
	EXPECT_EQ(std::count(messages.begin(), messages.end(), canceled), 1) << "the effector is the lastAttacker cancelCurrentSkill tells";
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::FEAR));
	EXPECT_TRUE(npc->getEffectController()->isUnderFear());
	EXPECT_EQ(effect->getAbnormals(), FEAR_ID);
	EXPECT_EQ(effect->getDuration(), 10000);
	EXPECT_TRUE(npc->isInState(CreatureState::WEAPON_EQUIPPED)) << "EmoteManager.emoteStartAttacking";
	EXPECT_EQ(packetsOf<SM_EMOTION>(observer).size(), 2u) << "CHANGE_SPEED and ATTACKMODE_IN_MOVE";
	EXPECT_TRUE(npc->getAi().isInState(ai::AIState::FEAR));
	EXPECT_FALSE(npc->getMoveController()->isInMove()) << "startEffect's abortMove stopped the walk; the task has not run yet";

	advance(1);
	EXPECT_TRUE(npc->getMoveController()->isInMove()) << "the FearTask's first run: moveToPoint";

	observer.clearSent();
	advance(9999);
	EXPECT_TRUE(effect->isEndedByTime());
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::FEAR)) << "FearEffect.endEffect";
	EXPECT_FALSE(npc->getMoveController()->isInMove()) << "abortMove";
	EXPECT_GE(packetsOf<SM_POSITION>(observer).size(), 1u) << "broadcastPacketAndReceive(effected, new SM_POSITION(effected))";
	EXPECT_FALSE(npc->getAi().isInState(ai::AIState::FEAR)) << "setStateIfNot(IDLE), then the ATTACK event";
	advance(3000);
	EXPECT_FALSE(npc->getMoveController()->isInMove()) << "the FearTask ended with the effect";
}

/** With gameserver.geodata.fear.enable off, FearEffect.startEffect schedules no FearTask: the feared npc stays where it is */
TEST_F(StateEffectsTest, WithoutFearEnableAFearedMonsterDoesNotFlee) {
	EFFECT_TEST_SCOPE;
	FearEnable fearDisabled(false);
	Ref<Npc> npc = makeMonster(700752);
	Ref<Player> mage = makePlayer(7692);
	Ref<Effect> effect = cast(*mage, *npc, bindSkill(MATERIAL_TEST_FEAR_XML), 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_TRUE(npc->getEffectController()->isUnderFear());
	advance(3000);
	EXPECT_FALSE(npc->getMoveController()->isInMove());
	effect->endEffect();
}

/**
 * FearEffect.startEffect adds its ATTACKED observer only below resistchance 100 (FearEffect.java:79-88): under 16704 Fear Casting (75) an attack
 * with a first Rnd.chance() of 75 or more ends the fear (removeEffect(skillId)), one below 75 does not; under 304 (100) no attack ends it. FEAR
 * resistance decides calculate.
 */
TEST_F(StateEffectsTest, AFearOfResistChanceBelowAHundredBreaksOnAnAttack) {
	EFFECT_TEST_SCOPE;
	FearEnable fearDisabled(false);
	Ref<Npc> npc = makeMonster(700761);
	Ref<Player> mage = makePlayer(7701);
	const model::SkillTemplate* fearCasting = bindSkill(FEAR_CASTING_XML);
	const model::SkillTemplate* materialFear = bindSkill(MATERIAL_TEST_FEAR_XML);

	ASSERT_FALSE(npc->getObserveController()->hasObservers());
	Ref<Effect> breakable = cast(*mage, *npc, fearCasting, 1);
	ASSERT_TRUE(breakable->isInSuccessEffects(1));
	EXPECT_TRUE(npc->getObserveController()->hasObservers()) << "resistchance 75: the ATTACKED observer";
	seedWhereFirstChance([](float chance) { return chance < 75.0f; });
	npc->getObserveController()->notifyAttackedObservers(*mage, 0);
	EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(16704)) << "a chance below 75 keeps it";
	seedWhereFirstChance([](float chance) { return chance >= 75.0f; });
	npc->getObserveController()->notifyAttackedObservers(*mage, 0);
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(16704)) << "Rnd.chance() >= resistchance: removeEffect";
	EXPECT_FALSE(npc->getEffectController()->isUnderFear());

	ASSERT_FALSE(npc->getObserveController()->hasObservers()) << "the ended fear took its observer along (Effect.removeObservers)";
	Ref<Effect> unbreakable = cast(*mage, *npc, materialFear, 1);
	ASSERT_TRUE(unbreakable->isInSuccessEffects(1));
	EXPECT_FALSE(npc->getObserveController()->hasObservers()) << "resistchance 100: `if (resistchance < 100)` adds none";
	seedWhereFirstChance([](float chance) { return chance >= 75.0f; });
	npc->getObserveController()->notifyAttackedObservers(*mage, 0);
	EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(304)) << "resistchance 100: no observer";
	unbreakable->endEffect();

	Ref<Npc> fearResistant = makeMonster(700762, 505, 505);
	addStat(*fearResistant, StatEnum::FEAR_RESISTANCE, 1000);
	Ref<Npc> blindResistant = makeMonster(700763, 505, 495);
	addStat(*blindResistant, StatEnum::BLIND_RESISTANCE, 1000);
	EXPECT_FALSE(calculated(*mage, *fearResistant, materialFear)->isInSuccessEffects(1));
	EXPECT_TRUE(calculated(*mage, *blindResistant, materialFear)->isInSuccessEffects(1));
}

/**
 * A feared player (an npc's 304): applyEffect stops a glide; startEffect cancels the cast, leaves WALK_MODE with SM_EMOTION RUN to the player
 * and those who see it, and the FearTask sets the player's new direction to GeoService.findMovementCollision's flee point and starts the
 * controlled move (PlayableMoveController.startMovingToDestination: SM_MOVE). This world has no geo terrain (GeoService.init with geo data
 * off), so GeoMap.findMovementCollision finds no ground 1 m ahead and answers the start position itself (GeoMap.java's last line): the
 * direction the character runs in is the geo gate's to see, not this case's; the new direction is the character's own position instead of
 * the (0, 0) it had. The end stops it.
 */
TEST_F(StateEffectsTest, AFearedPlayerStopsGlidingLeavesWalkModeAndRunsAway) {
	EFFECT_TEST_SCOPE;
	FearEnable fearEnabled(true);
	Ref<Npc> npc = makeMonster(700771, 500, 500);
	Ref<Player> target = makePlayer(7711, gameserver::model::PlayerClass::MAGE, gameserver::model::Race::ELYOS, 503, 500, 100);
	target->setFlyController(std::make_unique<controllers::FlyController>(*target)); // EffectWorldTest.makePlayer builds none
	cp::RecordingAionConnection& client = connect(*target, *accounts.back());
	const model::SkillTemplate* fear = bindSkill(MATERIAL_TEST_FEAR_XML);
	target->setCasting(model::Skill::create(fear, *target, 1, Ptr<Creature>(npc), nullptr));
	target->setState(CreatureState::WALK_MODE);
	target->setFlyState(gameserver::model::gameobjects::state::FlyState::GLIDING);
	target->setState(CreatureState::GLIDING);
	target->getMoveController()->setNewDirection(520, 510, 100, 0); // the player was walking somewhere
	target->getMoveController()->setInMove(true);
	ASSERT_FLOAT_EQ(target->getMoveController()->getTargetX2(), 520.0f);
	client.clearSent();

	Ref<Effect> effect = cast(*npc, *target, fear, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_FALSE(target->isInGlidingState()) << "Fear stops gliding (applyEffect)";
	EXPECT_FALSE(target->getCastingSkill());
	EXPECT_FALSE(target->isInState(CreatureState::WALK_MODE));
	const std::vector<uint8_t> run = cp::serialized(SM_EMOTION(*target, gameserver::model::EmotionType::RUN), &client);
	const std::vector<std::vector<uint8_t>> emotions = packetsOf<SM_EMOTION>(client);
	EXPECT_EQ(std::count(emotions.begin(), emotions.end(), run), 1) << "SM_EMOTION RUN, to the player itself as well";
	EXPECT_TRUE(target->getEffectController()->isUnderFear());

	EXPECT_FALSE(target->getMoveController()->isInMove()) << "startEffect: abortMove (PlayableMoveController: setAndSendStopMove)";
	EXPECT_FLOAT_EQ(target->getMoveController()->getTargetX2(), 0.0f) << "no direction before the task: abortMove reset the walk's";
	EXPECT_FLOAT_EQ(target->getMoveController()->getTargetY2(), 0.0f);
	EXPECT_TRUE(packetsOf<SM_MOVE>(client).empty());
	client.clearSent();
	advance(1);
	EXPECT_FLOAT_EQ(target->getMoveController()->getTargetX2(), 503.0f) << "setNewDirection(the flee point): the start, without terrain";
	EXPECT_FLOAT_EQ(target->getMoveController()->getTargetY2(), 500.0f);
	EXPECT_GE(packetsOf<SM_MOVE>(client).size(), 1u) << "the controlled move starts (sendForcedMovePacket)";

	effect->endEffect();
	EXPECT_FALSE(target->getEffectController()->isUnderFear());
	EXPECT_GE(packetsOf<SM_POSITION>(client).size(), 1u);
}

/** FearEffect.applyEffect first ends the effected's hide effects (FearEffect.java:40): a monster in 3222 Stealth's HIDE1 is shown again */
TEST_F(StateEffectsTest, AFearEndsTheHideOfItsEffected) {
	EFFECT_TEST_SCOPE;
	FearEnable fearDisabled(false);
	Ref<Npc> npc = makeMonster(700772);
	Ref<Player> mage = makePlayer(7712);
	const model::SkillTemplate* hide1 = bindSkill(stealthXml(7713, "HIDE1"));
	const model::SkillTemplate* fear = bindSkill(MATERIAL_TEST_FEAR_XML);
	cast(*npc, *npc, hide1, 1);
	ASSERT_TRUE(npc->isInVisualState(CreatureVisualState::HIDE1));
	ASSERT_TRUE(npc->getEffectController()->hasAbnormalEffect(7713));

	Ref<Effect> effect = cast(*mage, *npc, fear, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(7713)) << "removeHideEffects";
	EXPECT_FALSE(npc->isInVisualState(CreatureVisualState::HIDE1));
	EXPECT_TRUE(npc->getEffectController()->isUnderFear());
	effect->endEffect();
}

/**
 * A reflected fear (FearEffect.java:60, `effect.isReflected() ? effect.getOriginalEffected() : effect.getEffector()`): a monster casts 304 at a
 * player under 2918 Shield of Vengeance, whose AttackShieldObserver of SKILL_REFLECTOR (hittype SKILL, 30 m) reflects the whole effect in
 * EffectTemplate.calculateDamage (and ends the shield). Effect.getEffected() is then the monster, and the creature it fears is the player it
 * cast at, its original effected: cancelCurrentSkill tells the player (STR_SKILL_TARGET_SKILL_CANCELED), EmoteManager.emoteStartAttacking
 * sends its two SM_EMOTIONs (CHANGE_SPEED and ATTACKMODE_IN_MOVE write no target id, SM_EMOTION.java), and the FearTask flees from the player
 * only while the player is within 40 m of the monster: with the player 55 m away its run leaves the monster standing, back at 3 m it runs.
 */
TEST_F(StateEffectsTest, AReflectedFearFearsTheCasterOfTheCreatureItWasCastAt) {
	EFFECT_TEST_SCOPE;
	FearEnable fearEnabled(true);
	Ref<Npc> npc = makeMonster(700773);
	cp::RecordingAionConnection& client = observe(*npc, 7714);
	Ref<Player> templar = players.back();
	ASSERT_NO_FATAL_FAILURE(assertNoMagicalResist(*npc, *templar));
	const model::SkillTemplate* shieldOfVengeance = bindSkill(SHIELD_OF_VENGEANCE_XML);
	const model::SkillTemplate* fear = bindSkill(MATERIAL_TEST_FEAR_XML);
	ASSERT_TRUE(cast(*templar, *templar, shieldOfVengeance, 1)->isInSuccessEffects(1));
	ASSERT_TRUE(templar->getEffectController()->hasAbnormalEffect(2918));
	npc->setCasting(model::Skill::create(fear, *npc, 1, Ptr<Creature>(templar), nullptr));
	client.clearSent();

	Ref<Effect> effect = cast(*npc, *templar, fear, 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	ASSERT_TRUE(effect->isReflected()) << "calculateDamage: the SKILL_REFLECTOR bit of a DEBUFF skill";
	EXPECT_EQ(effect->getEffected().get(), npc.get());
	EXPECT_FALSE(templar->getEffectController()->hasAbnormalEffect(2918)) << "one skill reflection ends the shield";
	EXPECT_TRUE(npc->getEffectController()->isUnderFear()) << "the caster is feared";
	EXPECT_FALSE(templar->getEffectController()->isUnderFear());
	EXPECT_FALSE(npc->getCastingSkill());
	const std::vector<uint8_t> canceled = cp::serialized(SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_SKILL_CANCELED(), &client);
	const std::vector<std::vector<uint8_t>> messages = packetsOf<SM_SYSTEM_MESSAGE>(client);
	EXPECT_EQ(std::count(messages.begin(), messages.end(), canceled), 1) << "cancelCurrentSkill(the original effected)";
	const std::vector<std::vector<uint8_t>> emotions = packetsOf<SM_EMOTION>(client);
	ASSERT_EQ(emotions.size(), 2u);
	EXPECT_EQ(emotions[0], cp::serialized(SM_EMOTION(*npc, gameserver::model::EmotionType::CHANGE_SPEED, 0, templar->getObjectId()), &client))
		<< "emoteStartAttacking";
	EXPECT_EQ(emotions[1],
		cp::serialized(SM_EMOTION(*npc, gameserver::model::EmotionType::ATTACKMODE_IN_MOVE, 0, templar->getObjectId()), &client));

	place(*templar, 560, 500, 100);
	advance(1);
	EXPECT_FALSE(npc->getMoveController()->isInMove()) << "FearTask: isInRange(npc, the original effected 55 m away, 40) is false";
	place(*templar, 502, 500, 100);
	advance(1000);
	EXPECT_TRUE(npc->getMoveController()->isInMove()) << "the next run, with the player 3 m away: moveToPoint";
	effect->endEffect();
}

/**
 * FearTask.run moves the effected only while it is under fear (FearEffect.java:112). With the data, single-threaded, a FearTask never runs after
 * its fear's FEAR state is gone (Effect.endEffect stops the task before FearEffect.endEffect unsets FEAR); in Java the guard also covers a run
 * that races the end. The state it guards is built here the one way EffectController allows: 21345 Fascination's <fear> (:187906-187916) alone at
 * position 1 of a test skill of cooldownId 1 (the npc skills' shared cooldown id), so a second cast of it ends nothing - no effect id for
 * EffectController.searchConflict, cooldown id 1 for checkEffectCooldownId - and addEffect's put replaces the first under their common stack
 * without ending it. The first keeps its end task: at 3,000 ms its endEffect counts the FEAR effects of the controller (unsetAbnormal), finds
 * only the second, and clears FEAR, while the second's FearTask keeps running every second from 1,500 ms. Its run at 3,500 ms finds no fear
 * and leaves the npc standing.
 */
TEST_F(StateEffectsTest, TheFearTaskMovesNoOneWhoIsNoLongerUnderFear) {
	EFFECT_TEST_SCOPE;
	FearEnable fearEnabled(true);
	Ref<Npc> npc = makeMonster(700774);
	Ref<Player> mage = makePlayer(7715);
	const model::SkillTemplate* fascination = bindSkill(testSkill(7716, "DEBUFF", R"(cooldownId="1")",
		R"(<fear duration2="3000" e="1" noresist="true" element="EARTH" hoptype="SKILLLV" hopb="11737" />)"));
	Ref<Effect> first = cast(*mage, *npc, fascination, 1);
	ASSERT_TRUE(first->isInSuccessEffects(1));
	advance(1);
	ASSERT_TRUE(npc->getMoveController()->isInMove()) << "the first's FearTask";
	advance(1499);

	Ref<Effect> second = cast(*mage, *npc, fascination, 1);
	ASSERT_TRUE(second->isInSuccessEffects(1));
	advance(1);
	ASSERT_TRUE(npc->getMoveController()->isInMove()) << "the second's FearTask";
	advance(1499);
	ASSERT_TRUE(first->isEndedByTime()) << "3,000 ms";
	ASSERT_FALSE(second->isEndedByTime());
	ASSERT_TRUE(npc->getEffectController()->hasAbnormalEffect(7716)) << "the second is the controller's";
	EXPECT_FALSE(npc->getEffectController()->isUnderFear()) << "unsetAbnormal counted one FEAR effect";
	EXPECT_FALSE(npc->getMoveController()->isInMove()) << "the first's endEffect: abortMove";

	advance(500);
	EXPECT_FALSE(second->isEndedByTime());
	EXPECT_FALSE(npc->getMoveController()->isInMove()) << "the second's run at 3,500 ms: isUnderFear() is false, no moveToPoint";
	second->endEffect();
}

/**
 * FearEffect.endEffect of an npc sets AIState.IDLE and then hands the AI an ATTACK event whose creature is the npc itself (FearEffect.java:95-98;
 * AttackEventHandler.onAttack then lets a monster fight again). The DummyAI of the other cases ignores the event; an NpcAI that records it sees
 * one ATTACK on the npc, arriving in IDLE.
 */
TEST_F(StateEffectsTest, TheEndOfAFearHandsTheNpcsAiAnAttackEventOnTheNpcItself) {
	EFFECT_TEST_SCOPE;
	FearEnable fearDisabled(false);
	Ref<Npc> npc = makeMonster(700775);
	auto installed = std::make_unique<AttackRecordingAI>(*npc);
	AttackRecordingAI& recorder = *installed;
	npc->replaceAi(std::move(installed));
	Ref<Player> mage = makePlayer(7717);

	Ref<Effect> effect = cast(*mage, *npc, bindSkill(MATERIAL_TEST_FEAR_XML), 1);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_TRUE(recorder.isInState(ai::AIState::FEAR));
	EXPECT_TRUE(recorder.attacks.empty());

	advance(10000);
	ASSERT_TRUE(effect->isEndedByTime());
	ASSERT_EQ(recorder.attacks.size(), 1u) << "onCreatureEvent(ATTACK, effected)";
	EXPECT_EQ(recorder.attacks[0].first, npc->getObjectId()) << "the npc itself";
	EXPECT_EQ(recorder.attacks[0].second, ai::AIState::IDLE) << "after setStateIfNot(IDLE)";
	EXPECT_TRUE(recorder.isInState(ai::AIState::IDLE));
}

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
