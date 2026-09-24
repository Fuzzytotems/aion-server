#pragma once

// Test support of the npc skill rotation (m5b2-plan.md N-04, P5-05): AiWorldTest plus the static data an npc skill fight reads, and nothing
// else. Every row below is a VERBATIM excerpt of the Java tree's data/static_data (file:line at each), never an invented one: the M5b-1 lesson
// of AiWorldTestSupport.h's MONSTER row is that a fabricated relation makes every AI case test an invented world. The one edit is named where
// it is made: the <equipment> element of the npc templates that have one (231105, 235763, 210161, 210162, 297192, 230745 and 209556) is left
// out, because its item IDREFs need the item templates bound in the same context and nothing on the skill rotation reads an npc's gear
// (WeaponCondition skips npcs, WeaponCondition.cpp:38).
//
// The npcs, and what each one's shipped npc_skills row exercises:
// - 210133 "striped kerub" (Poeta, level 1, the X9 npc of m5b2-plan.md §10): one skill, 16419 Brandish, prob 25 - the plain rotation of the
//   two start maps (all 29 of their npc skill ids are prob 25 rows without conditions, m5b2-plan.md §2.4 (b)), and the one cast driven end to
//   end. Its opponent is 282949 "tamed pagati", tribe HOSTILEONLYMONSTER, whose real row `<hostile>MONSTER</hostile>` is what makes the kerub's
//   ENEMY target relation true (Npc.isEnemy -> TribeRelationService.isHostile, TribeRelationService.java:185).
// - 855799 "apparition of lanmark": a chain (17179 opens chain 1, 17182 follows it) whose follow-up has TARGET_IS_AETHERS_HOLD.
// - 231105 "pashid advance protector" (Eternal Bastion): HELP_FRIEND hp_below 50 on a prio 1 skill, prob 100 everywhere - deterministic.
// - 235763 "runaway hirakiki leader": NPC_IS_ALIVE npc_id 856059 on a prio 1 skill, beside four prio 0 skills.
// - 206292 (no name, ai "general", no attack range): GeneralNpcAI's alwaysRandomSkill arm - one of the 955 npcs that own skills and have
//   attack range 0 (measured over npc_templates.xml and npc_skills/**) - and the attack-range-0 TARGET_TOOFAR arms of SkillAttackManager.
// - 210161 "tursin loudmouth" (Poeta): 16424 Shout has first_target ME - one of the 9 start-map skills that turn the npc to itself.
// - 210162 "supervisor duaguru" (Poeta): 16856 Blessing of Rock has target_relation FRIEND but the default npc_skill target MOST_HATED, so the
//   engine refuses every cast of it at the npc's enemy - skillAction's `!success` arm, live on the start maps.
// - 297192 "ahserion troopers sorcerer": chain 1 has two follow-ups with priorities (16989 prio 24 with max_hp 35, 17335 prio 23).
// - 283139 "divisive creation": two prob 100 skills of priority 0, so only the priority walk's shuffle decides which one is asked first.
// - 230745 "pashid assault tribuni protector": 20556 Protective Shield with target="FRIEND" (the one shipped FRIEND row that is not commented
//   out); its friend is 231105 (IDF5_TD_ASSULT, a <friend> of VRITRASUPPORT) and its enemy 209556 "granir's disciple" (IDF5_TD_GUARD_DARK,
//   whose row aggroes VRITRASUPPORT).
//
// Conditions no shipped row uses are not given a fabricated row here: TARGET_IS_SLEEPING and TARGET_IS_POISONED appear in 0 of the npc_skills
// files (grep over data/static_data/npc_skills/**), and their conditionReady arms are covered by EffectControllerTest (P5-02b). The carved-signet
// boundary and spawn_npc's count are not re-tested here either; P5-02b's fixtures stand for them (EffectControllerTest.cpp,
// ACarvedSignetConditionNeedsASignetAboveItsLevel and AnNpcSkillSpawnDrawsItsCountOnlyForAMaxCountAboveOneAndALivingNpc): a shipped signet row
// (21412 Pain Rune, 301130000_Sauro_Supply_Base.xml:269-271) needs a carved signet on the target, which only CarveSignetEffect places and which
// is still AION_UNPORTED, and a shipped spawn_npc row needs SpawnEngine to place real npcs in the world, which this fixture does not model.
//
// Target attributes without a small shipped row: SECOND_MOST_HATED and THIRD_MOST_HATED appear only in 277224 Ahserion's 119-entry list
// (400030000_Transidium_Annex.xml:94-278), RANDOM_EXCEPT_CURRENT_TARGET and NONE only beside skills this fixture does not carry. Their
// skillAction arms are driven through a queued skill instead, the entry Npc.queueSkill(skillId, level, nextSkillTime, target) builds
// (Npc.java:172-174: new NpcSkillTemplateEntry(new QueuedNpcSkillTemplate(...))) around the kerub's own Brandish - the path the handler scripts
// take (e.g. GuardCaptainAhuradim.java:42). queueSkill() below composes it by hand because that Npc overload is still AION_UNPORTED (P4-11a).

#include <gtest/gtest.h>

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/NpcSkillData.bind.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/model/skill/NpcSkillTemplateEntry.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTargetAttribute.h"
#include "aion/gameserver/model/templates/npcskill/QueuedNpcSkillTemplate.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

#include "AiWorldTestSupport.h"

namespace aion::gameserver::ai::testing {

inline constexpr int32_t STRIPED_KERUB_NPC_ID = 210133;
inline constexpr int32_t TAMED_PAGATI_NPC_ID = 282949;
inline constexpr int32_t LANMARK_NPC_ID = 855799;
inline constexpr int32_t PASHID_PROTECTOR_NPC_ID = 231105;
inline constexpr int32_t HIRAKIKI_LEADER_NPC_ID = 235763;
inline constexpr int32_t HIRAKIKI_WATCHED_NPC_ID = 856059;
inline constexpr int32_t RANGELESS_CASTER_NPC_ID = 206292;
inline constexpr int32_t TURSIN_LOUDMOUTH_NPC_ID = 210161;
inline constexpr int32_t SUPERVISOR_DUAGURU_NPC_ID = 210162;
inline constexpr int32_t TROOPERS_SORCERER_NPC_ID = 297192;
inline constexpr int32_t DIVISIVE_CREATION_NPC_ID = 283139;
inline constexpr int32_t TRIBUNI_PROTECTOR_NPC_ID = 230745;
inline constexpr int32_t GRANIRS_DISCIPLE_NPC_ID = 209556;

inline constexpr int32_t BRANDISH = 16419;
inline constexpr int32_t AIONS_JUDGMENT = 17179;
inline constexpr int32_t FALL = 17182;
inline constexpr int32_t MIDNIGHT_ROBE = 20700;
inline constexpr int32_t WIDE_POWER_ATTACK = 21284;
inline constexpr int32_t PROTECTIVE_SHIELD = 20556;
inline constexpr int32_t DARK_SHIELD = 21799;
inline constexpr int32_t FLAME_BLAZE = 21128;
inline constexpr int32_t SHOUT = 16424;
inline constexpr int32_t BLESSING_OF_ROCK = 16856;
inline constexpr int32_t COLD_ATTACK = 16989;
inline constexpr int32_t FLAME_BOLT = 17335;
inline constexpr int32_t AGRINT_CURSE = 21287;
inline constexpr int32_t SMASH_CASTING = 20985;
inline constexpr int32_t SUMMON_ROCK = 20986;

/** npc_templates.xml, verbatim except the <equipment> of the templates that have one (see the file comment) */
inline const char* const NPC_SKILL_NPC_TEMPLATES_XML =
	// npcs/npc_templates.xml:54505-54510
	R"(<npc_template npc_id="210133" level="1" name="striped kerub" name_id="300110" height="1.372" group_drop="CHERUBIM" rank="DISCIPLINED")"
	R"( rating="NORMAL" race="MAGICALMONSTER" tribe="MONSTER" type="MONSTER" ai="aggressive" srange="7" sangle="240" arange="2" attack_speed="2100")"
	R"( hpgauge="3"><stats maxHp="143"><speeds walk="0.6" group_walk="0.6" run="7" run_fight="5.5" group_run_fight="7" /></stats>)"
	R"(<bound_radius front="0.525" side="0.275" upper="1.372" /></npc_template>)"
	// npcs/npc_templates.xml:372957-372962
	R"(<npc_template npc_id="282949" level="1" name="tamed pagati" name_id="342548" height="2.1" group_drop="DRAKANSTRIDER" rank="NOVICE")"
	R"( rating="NORMAL" tribe="HOSTILEONLYMONSTER" ai="homing" srange="10" arange="3" attack_speed="2400" hpgauge="2"><stats maxHp="5547">)"
	R"(<speeds walk="2.1" group_walk="2.1" run="10" run_fight="10" group_run_fight="10" /></stats>)"
	R"(<bound_radius front="0.525" side="1.75" upper="2.1" /></npc_template>)"
	// npcs/npc_templates.xml:567937-567942
	R"(<npc_template npc_id="855799" level="65" name="apparition of lanmark" name_id="333755" height="2.86" title_id="334663")"
	R"( group_drop="DEATHKNIGHT" rank="MASTER" rating="HERO" tribe="AGGRESSIVESINGLEMONSTER" type="MONSTER" ai="aggressive_stonespear" srange="50")"
	R"( arange="2" attack_speed="1900" hpgauge="22" cancel_level="0"><stats maxHp="2357195" msup="1150"><speeds walk="1.491" group_walk="1.491")"
	R"( run="6" run_fight="12" group_run_fight="6" /></stats><bound_radius front="0.6825" side="0.455" upper="2.86" /></npc_template>)"
	// npcs/npc_templates.xml:143349-143357 (without <equipment><item>100001461</item></equipment>)
	R"(<npc_template npc_id="231105" level="65" name="pashid advance protector" name_id="329273" height="3.77" group_drop="DRAKANFIGHTER")"
	R"( rank="SEASONED" rating="ELITE" race="SIEGEDRAKAN" tribe="IDF5_TD_ASSULT" type="MONSTER" ai="eternal_bastion_assaulter" srange="7")"
	R"( sangle="240" arange="2" attack_speed="2100" hpgauge="12" cancel_level="30"><stats maxHp="140514" msup="151" attack="2124" pdef="1494")"
	R"( matk="447" mdef="532" accuracy="3263" macc="1853" strike_resist="556" spell_resist="167"><speeds walk="1.37" group_walk="1.37" run="8")"
	R"( run_fight="8" group_run_fight="10" /></stats><bound_radius front="1.3" side="0.845" upper="3.77" /></npc_template>)"
	// npcs/npc_templates.xml:191216-191224 (without <equipment><item>100001144</item></equipment>)
	R"(<npc_template npc_id="235763" level="65" name="runaway hirakiki leader" name_id="333834" height="1.63" title_id="349943")"
	R"( group_drop="SHULACKPRIEST" rank="VETERAN" rating="HERO" race="SHULACK" tribe="IDF5U1_VRITRA" type="MONSTER" ai="aggressive" srange="20")"
	R"( arange="2" attack_speed="2040" hpgauge="21"><stats maxHp="671820" msup="957"><speeds walk="1.6" group_walk="1.6" run="7" run_fight="5.5")"
	R"( group_run_fight="7" /></stats><bound_radius front="0.9" side="0.4" upper="1.63" /></npc_template>)"
	// npcs/npc_templates.xml:569703-569706
	R"(<npc_template npc_id="856059" level="1" name=" " name_id="350000" height="1" group_drop="NONE" rank="EXPERT" rating="NORMAL" tribe="USEALL")"
	R"( type="MONSTER" ai="aggressive" srange="5" attack_speed="2000" hpgauge="5" cancel_level="80"><stats maxHp="112" />)"
	R"(<bound_radius front="0.5" side="0.5" upper="1" /></npc_template>)"
	// npcs/npc_templates.xml:36469-36473
	R"(<npc_template npc_id="206292" level="80" name=" " name_id="350000" height="2" group_drop="NONE" rank="EXPERT" rating="NORMAL" tribe="MONSTER")"
	R"( type="GENERAL" ai="general" srange="10" attack_speed="2000" hpgauge="5" cancel_level="80"><stats maxHp="55684" />)"
	R"(<bound_radius front="0.25" side="0.35" upper="2" /><talk_info distance="5" can_talk_invisible="false" /></npc_template>)"
	// npcs/npc_templates.xml:54673-54681 (without <equipment><item>100000008</item></equipment>)
	R"(<npc_template npc_id="210161" level="9" name="tursin loudmouth" name_id="300132" height="2.87" group_drop="KRALLWARRIOR" rank="DISCIPLINED")"
	R"( rating="NORMAL" race="KRALL" tribe="KRALL" type="MONSTER" ai="aggressive" srange="8" sangle="240" arange="2" attack_speed="2100")"
	R"( hpgauge="3"><stats maxHp="1183"><speeds walk="2" group_walk="2" run="7.6" run_fight="5.5" group_run_fight="7.6" /></stats>)"
	R"(<bound_radius front="1.35" side="0.7" upper="2.87" /></npc_template>)"
	// npcs/npc_templates.xml:54682-54690 (without <equipment><item>100000008</item></equipment>)
	R"(<npc_template npc_id="210162" level="10" name="supervisor duaguru" name_id="300133" height="3.075" group_drop="KRALLWARRIOR")"
	R"( rank="SEASONED" rating="NORMAL" race="KRALL" tribe="KRALL" type="MONSTER" ai="aggressive" srange="8" sangle="240" arange="2")"
	R"( attack_speed="2100" hpgauge="4" cancel_level="90"><stats maxHp="1821"><speeds walk="2" group_walk="2" run="7.6" run_fight="5.5")"
	R"( group_run_fight="7.6" /></stats><bound_radius front="2.025" side="1.05" upper="3.075" /></npc_template>)"
	// npcs/npc_templates.xml:435272-435280 (without <equipment><item>100501195</item></equipment>)
	R"(<npc_template npc_id="297192" level="65" name="ahserion troopers sorcerer" name_id="330922" height="2.45" title_id="332402")"
	R"( group_drop="DRAKANMAGEF" rank="VETERAN" rating="ELITE" tribe="GAB1_SUB_DRAKAN" ai="ahserion_sorcerer" srange="15" arange="37")"
	R"( attack_speed="2300" hpgauge="14" cancel_level="20"><stats maxHp="239546" msup="817" attack="4282" matk="4282" accuracy="4091")"
	R"( macc="2260"><speeds walk="1.5" group_walk="1.5" run="6.8" run_fight="6.8" group_run_fight="6.8" /></stats>)"
	R"(<bound_radius front="0.7" side="0.525" upper="2.45" /></npc_template>)"
	// npcs/npc_templates.xml:373871-373876
	R"(<npc_template npc_id="283139" level="60" name="divisive creation" name_id="326675" height="2.46" group_drop="CYCLOPS" rank="VETERAN")"
	R"( rating="ELITE" tribe="XDRAKAN" type="MONSTER" ai="divisive_creation" srange="15" arange="2" attack_speed="2205" hpgauge="14")"
	R"( cancel_level="20"><stats maxHp="217224" msup="207"><speeds walk="1.07" group_walk="1.38" run="9" run_fight="12" group_run_fight="7" />)"
	R"(</stats><bound_radius front="0.72" side="1.8" upper="2.46" /></npc_template>)"
	// npcs/npc_templates.xml:140385-140393 (without <equipment><item>100001461</item></equipment>)
	R"(<npc_template npc_id="230745" level="65" name="pashid assault tribuni protector" name_id="343185" height="4.35" title_id="344518")"
	R"( group_drop="DRAKANFIGHTER" rank="VETERAN" rating="ELITE" race="DRAKAN" tribe="VRITRASUPPORT" type="MONSTER")"
	R"( ai="eternal_bastion_assaulter" srange="10" sangle="240" arange="2" attack_speed="2100" hpgauge="14" cancel_level="20">)"
	R"(<stats maxHp="420643" msup="817" attack="3009" pdef="1551" mdef="553" accuracy="3576" macc="1950" strike_resist="700")"
	R"( spell_resist="237"><speeds walk="1.37" group_walk="1.37" run="8" run_fight="8" group_run_fight="10" /></stats>)"
	R"(<bound_radius front="1.45" side="1.16" upper="4.35" /></npc_template>)"
	// npcs/npc_templates.xml:50452-50467 (without the <equipment> of eight items)
	R"(<npc_template npc_id="209556" level="65" name="granir's disciple" name_id="463153" height="2.3" title_id="463660" group_drop="DARK")"
	R"( rank="EXPERT" rating="NORMAL" race="ASMODIANS" tribe="IDF5_TD_GUARD_DARK" type="ABYSS_GUARD" ai="eternal_bastion_aggressive")"
	R"( srange="10" arange="4" attack_speed="2100" hpgauge="5" cancel_level="80"><stats maxHp="32333" attack="1340" pdef="1316" matk="374")"
	R"( mdef="469" accuracy="2662" macc="1575" strike_resist="272" spell_resist="105"><speeds walk="1.5" group_walk="1.5" run="6" run_fight="8")"
	R"( group_run_fight="6" /></stats><bound_radius front="0.2875" side="0.4025" upper="2.3" /></npc_template>)";

/** npc_skills/**, verbatim */
inline const char* const NPC_SKILL_ROWS_XML =
	R"(<npc_skill_templates>)"
	// npc_skills/npc_skills.xml:2816-2818
	R"(<npc_skills npc_ids="210133"><npc_skill id="16419" lv="1" prob="25" /></npc_skills>)"
	// npc_skills/npc_skills.xml:63170-63175
	R"(<npc_skills npc_ids="855799">)"
	R"(<npc_skill id="17179" lv="65" prob="60" cd="15000" next_chain_id="1" next_skill_time="0" target="RANDOM" />)"
	R"(<npc_skill id="17182" lv="65" prob="100" cd="5000" chain_id="1" next_skill_time="5000"><cond cond_type="TARGET_IS_AETHERS_HOLD" />)"
	R"(</npc_skill></npc_skills>)"
	// npc_skills/instances/300540000_Eternal_Bastion.xml:105-111
	R"(<npc_skills npc_ids="231105">)"
	R"(<npc_skill id="20700" lv="1" prob="0" is_post_spawn="true" /><!-- Midnight Robe -->)"
	R"(<npc_skill id="21284" lv="1" prob="100" cd="10000" /><!-- Wide Power Attack -->)"
	R"(<npc_skill id="20556" lv="1" prob="100" prio="1" cd="8000"><!-- Protective Shield (limit 3x on retail) -->)"
	R"(<cond cond_type="HELP_FRIEND" hp_below="50" /></npc_skill></npc_skills>)"
	// npc_skills/npc_skills.xml:63745-63753
	R"(<npc_skills npc_ids="235763">)"
	R"(<npc_skill id="17340" lv="65" prob="70" cd="25000" />)"
	R"(<npc_skill id="17365" lv="65" prob="70" cd="40000" />)"
	R"(<npc_skill id="17860" lv="65" prob="70" cd="25000" target="RANDOM" />)"
	R"(<npc_skill id="19075" lv="65" prob="100" cd="25000" />)"
	R"(<npc_skill id="21799" lv="65" prob="70" cd="35000" prio="1"><cond cond_type="NPC_IS_ALIVE" npc_id="856059" /></npc_skill>)"
	R"(</npc_skills>)"
	// npc_skills/npc_skills.xml:1344-1346
	R"(<npc_skills npc_ids="206292"><npc_skill id="21128" lv="18" prob="100" /></npc_skills>)"
	// npc_skills/npc_skills.xml:2877-2880
	R"(<npc_skills npc_ids="210161"><npc_skill id="16424" lv="1" prob="25" /><npc_skill id="16602" lv="1" prob="25" /></npc_skills>)"
	// npc_skills/npc_skills.xml:2881-2886
	R"(<npc_skills npc_ids="210162"><npc_skill id="16602" lv="1" prob="25" /><npc_skill id="16608" lv="1" prob="25" />)"
	R"(<npc_skill id="16742" lv="1" prob="25" /><npc_skill id="16856" lv="1" prob="25" /></npc_skills>)"
	// npc_skills/open_worlds/400030000_Transidium_Annex.xml:339-345
	R"(<npc_skills npc_ids="297192">)"
	R"(<npc_skill id="16989" lv="65" prob="100" prio="24" chain_id="1" next_chain_id="10" next_skill_time="0" max_hp="35")"
	R"( cd="180000" /><!-- Cold Attack -->)"
	R"(<npc_skill id="17335" lv="56" prob="100" prio="23" chain_id="1" next_chain_id="10" next_skill_time="0" /><!-- Flame Bolt -->)"
	R"(<npc_skill id="21288" lv="65" prob="100" prio="21" chain_id="10" next_chain_id="1" next_skill_time="0" /><!-- Fire Burst -->)"
	R"(<npc_skill id="21287" lv="56" prob="100" prio="7" next_chain_id="1" next_skill_time="0" /><!-- Agrint Curse -->)"
	R"(</npc_skills>)"
	// npc_skills/instances/300520000_Dragon_Lords_Refuge.xml:44-47
	R"(<npc_skills npc_ids="283139"><npc_skill id="20985" lv="60" prob="100" cd="9000" /><npc_skill id="20986" lv="60" prob="100" />)"
	R"(</npc_skills>)"
	// npc_skills/instances/300540000_Eternal_Bastion.xml:523-533
	R"(<npc_skills npc_ids="230745">)"
	R"(<npc_skill id="20700" lv="1" prob="0" is_post_spawn="true" /><!-- Midnight Robe -->)"
	R"(<npc_skill id="20556" lv="1" prob="100" next_chain_id="1" next_skill_time="9000" target="FRIEND" /><!-- Protective Shield -->)"
	R"(<npc_skill id="21284" lv="1" prob="100" chain_id="1" next_chain_id="2" next_skill_time="12000" /><!-- Wide Power Attack -->)"
	R"(<npc_skill id="17295" lv="1" prob="100" chain_id="2" next_chain_id="3" next_skill_time="6000" /><!-- Ferocious Strike II -->)"
	R"(<npc_skill id="17294" lv="1" prob="100" chain_id="3" next_chain_id="4" next_skill_time="9500" /><!-- Ferocious Strike I -->)"
	R"(<npc_skill id="20556" lv="1" prob="100" chain_id="4" next_chain_id="5" next_skill_time="5000" /><!-- Protective Shield -->)"
	R"(<npc_skill id="17295" lv="1" prob="100" chain_id="5" next_chain_id="6" next_skill_time="7000" /><!-- Ferocious Strike II -->)"
	R"(<npc_skill id="21284" lv="1" prob="100" chain_id="6" next_chain_id="7" next_skill_time="10000" /><!-- Wide Power Attack -->)"
	R"(<npc_skill id="17294" lv="1" prob="100" chain_id="7" next_chain_id="1" next_skill_time="4500" /><!-- Ferocious Strike I -->)"
	R"(</npc_skills>)"
	R"(</npc_skill_templates>)";

/** skills/skill_templates.xml, verbatim: every skill id of the rows above, so every NpcSkillList is the one the server builds */
inline const char* const NPC_SKILL_TEMPLATES_XML =
	R"(<skill_data>)"
	// skills/skill_templates.xml:118255-118267
	R"(<skill_template skill_id="16419" name="Brandish" nameId="282847" cooldownId="1" stack="NFI_SMALLBLOW_BRANDISH" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="2500" cancel_rate="35" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="2" target_relation="ENEMY" target_type="ONLYONE" target_maxcount="1" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><skillatk mode="PERCENT" value="85" e="1" accmod2="0" hoptype="DAMAGE" /></effects>)"
	R"(<motion name="poweratk" /></skill_template>)"
	// skills/skill_templates.xml:129768-129785
	R"(<skill_template skill_id="17179" name="Aion's Judgment" nameId="289553" cooldownId="2" stack="GAS_OPENAERIAL_NR" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="DEBUFF" tslot="NONE" activation="ACTIVE" cooldown="0" duration="2000" cancel_rate="30" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="4" target_relation="ENEMY" target_type="ONLYONE" target_maxcount="1" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><skillatk mode="PERCENT" value="21" e="1" accmod2="0" hoptype="DAMAGE"><subeffect skill_id="8224" />)"
	R"(<subconditions><noflying /></subconditions></skillatk></effects>)"
	R"(<motion name="openaerial" /></skill_template>)"
	// skills/skill_templates.xml:129826-129847
	R"(<skill_template skill_id="17182" name="Fall" nameId="289561" cooldownId="3" stack="GAS_CLOSEAERIALTA_NR" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="1000" cancel_rate="65" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="4" target_relation="ENEMY" target_type="AREA" target_maxcount="36")"
	R"( effective_altitude="7" effective_range="7" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><skillatk mode="PERCENT" rnddmg="3" value="30" e="1" accmod2="0" hoptype="DAMAGE">)"
	R"(<modifiers><abnormaldamage state="OPENAERIAL" value="71" /></modifiers></skillatk>)"
	R"(<closeaerial e="2" noresist="true" element="FIRE" preeffect="1" hoptype="SKILLLV" hopb="60" hopa="60">)"
	R"(<conditions><abnormal value="OPENAERIAL" /></conditions></closeaerial></effects>)"
	R"(<motion name="closeaerial" /></skill_template>)"
	// skills/skill_templates.xml:179343-179359
	R"(<skill_template skill_id="20700" name="Midnight Robe" nameId="2280531" cooldownId="1500" stack="VRITRA_COMMON_BUFF_STUMDDOG" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE")"
	R"( cooldown="0" duration="0">)"
	R"(<properties first_target="ME" first_target_range="2" target_relation="FRIEND" target_type="ONLYONE" target_maxcount="1")"
	R"( target_species="NPC" />)"
	R"(<startconditions><target value="NPC" />)"
	R"(<weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><statup duration2="86400000" e="1" noresist="true"><change stat="PHYSICAL_DEFENSE" func="ADD" value="200" />)"
	R"(<change stat="MAGICAL_DEFEND" func="ADD" value="200" /></statup></effects>)"
	R"(<motion name="normalfire" /></skill_template>)"
	// skills/skill_templates.xml:187106-187118
	R"(<skill_template skill_id="21284" name="Wide Power Attack" nameId="285613" cooldownId="1" stack="DGFI_SATK_NR_SA" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="3000" cancel_rate="20" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="2" target_relation="ENEMY" target_type="AREA" target_maxcount="8")"
	R"( effective_altitude="5" effective_angle="240" effective_dist="5" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><skillatk mode="PERCENT" value="127" e="1" accmod2="0" hoptype="DAMAGE" /></effects>)"
	R"(<motion name="chainatk1" /></skill_template>)"
	// skills/skill_templates.xml:177640-177649
	R"(<skill_template skill_id="20556" name="Protective Shield" nameId="2280257" cooldownId="1345" stack="IDARENA_TEAM01_S5_ROAMER_SHIELD")"
	R"( lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10")"
	R"( activation="ACTIVE" cooldown="0" duration="1000" cancel_rate="20" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="2" target_relation="FRIEND" target_type="ONLYONE" target_maxcount="1" />)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><shield percent="true" hitvalue="50" value="5000" duration2="5000" duration1="30000" effectid="154" e="1" basiclvl="200")"
	R"( noresist="true" hittype="EVERYHIT" hoptype="SKILLLV" /></effects>)"
	R"(<motion name="buff" /></skill_template>)"
	// skills/skill_templates.xml:132123-132139
	R"(<skill_template skill_id="17340" name="Winter Binding" nameId="290149" cooldownId="2" stack="DGWI_SPELLLONGICEROOT_TA" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10")"
	R"( activation="ACTIVE" cooldown="0" duration="3500" cancel_rate="25" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="37" target_relation="ENEMY" target_type="AREA" target_maxcount="24")"
	R"( effective_altitude="7" effective_range="7" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><spellatkinstant mode="PERCENT" value="18" e="1" element="WATER" hoptype="DAMAGE" />)"
	R"(<snare duration2="4000" duration1="80" effectid="20007" e="2" preeffect="1"><change stat="SPEED" func="PERCENT" value="-90" />)"
	R"(<change stat="FLY_SPEED" func="PERCENT" value="-90" /></snare></effects>)"
	R"(<motion name="pointfire3" /></skill_template>)"
	// skills/skill_templates.xml:132486-132498
	R"(<skill_template skill_id="17365" name="Healing Wind II" nameId="290200" cooldownId="7" stack="DGPR_HEALMID_TA" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="HEAL" tslot="NONE" activation="ACTIVE" cooldown="0" duration="3500" cancel_rate="30" hostile_type="INDIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="37" target_relation="FRIEND" target_type="AREA" target_maxcount="4")"
	R"( effective_altitude="10" effective_range="10" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><healinstant percent="true" value="20" e="1" noresist="true" hoptype="DAMAGE" /></effects>)"
	R"(<motion name="normalfire" /></skill_template>)"
	// skills/skill_templates.xml:139492-139512
	R"(<skill_template skill_id="17860" name="Absorb Vitality" nameId="291529" cooldownId="6" stack="BNEL_SPELLDRAINMPATTSDDEFTA10_CH1" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="20")"
	R"( activation="ACTIVE" cooldown="0" duration="1000" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="37" target_relation="ENEMY" target_type="AREA" target_maxcount="24")"
	R"( effective_altitude="7" effective_range="3" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><spellatkdraininstant hp_percent="200" mode="PERCENT" value="50" e="1" element="EARTH" critprobmod2="0" hoptype="DAMAGE">)"
	R"(<modifiers><abnormaldamage state="PARALYZE" value="100" /></modifiers></spellatkdraininstant>)"
	R"(<statdown duration2="60100" effectid="10178603" e="2" noresist="true" element="WATER" preeffect="1" hoptype="SKILLLV" hopb="60" hopa="60">)"
	R"(<change stat="PHYSICAL_DEFENSE" func="PERCENT" value="-10" /><change stat="MAGICAL_RESIST" func="PERCENT" value="-10" /></statdown>)"
	R"(</effects><motion name="pointfire" speed="70" /></skill_template>)"
	// skills/skill_templates.xml:157540-157553
	R"(<skill_template skill_id="19075" name="Earthly Grudge" nameId="296021" cooldownId="113" stack="IDCATACOMBS_NORMAL_PRIESTMAGIC1" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="ATTACK" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10")"
	R"( activation="ACTIVE" cooldown="0" duration="3000" ammospeed="8" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="20" target_relation="ENEMY" target_type="ONLYONE" target_maxcount="1" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><spellatkinstant value="611" e="1" element="EARTH" hoptype="DAMAGE" />)"
	R"(<spellatk checktime="2000" value="1702" duration2="10000" effectid="10190501" e="2" element="WIND" preeffect="1" hoptype="SKILLLV")"
	R"( hopb="60" hopa="60" /></effects>)"
	R"(<motion name="normalfire" /></skill_template>)"
	// skills/skill_templates.xml:193526-193535
	R"(<skill_template skill_id="21799" name="Dark Shield" nameId="2285178" stack="IDLDF5_UNDER_02_BOSS_WI_DARKSHIELD" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="BUFF" tslot="BUFF" dispel_category="NPC_BUFF" req_dispel_level="5" req_dispel_count="50" activation="ACTIVE" cooldown="0")"
	R"( duration="1000" hostile_type="DIRECT">)"
	R"(<properties first_target="ME" first_target_range="2" target_relation="ALL" target_type="ONLYONE" target_maxcount="1" />)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><shield hitdelta="1700" value="0" delta="4000" duration2="25000" effectid="154" e="1" noresist="true" hittype="EVERYHIT" />)"
	R"(</effects><motion name="normalfire" /></skill_template>)"
	// skills/skill_templates.xml:184959-184971
	R"(<skill_template skill_id="21128" name="Flame Blaze" nameId="2281968" cooldownId="2" stack="IDF5_R2_SYNC1_SPELLATK_SA" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="ATTACK" tslot="DEBUFF" dispel_category="STUN" req_dispel_level="2" req_dispel_count="30")"
	R"( activation="ACTIVE" cooldown="0" duration="2000" hostile_type="DIRECT">)"
	R"(<properties first_target="TARGET" first_target_range="15" target_relation="ENEMY" target_type="AREA" target_maxcount="8")"
	R"( effective_altitude="10" effective_range="21" />)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><spellatkinstant value="900" delta="100" e="1" noresist="true" element="FIRE" hoptype="DAMAGE">)"
	R"(<subeffect skill_id="8635" chance="65" /></spellatkinstant>)"
	R"(<spellatk checktime="3000" value="500" delta="50" duration2="6100" effectid="10183892" e="2" noresist="true" element="FIRE")"
	R"( preeffect="1" hoptype="DAMAGE" /></effects>)"
	R"(<motion name="pointfire" /></skill_template>)"
	// skills/skill_templates.xml:118330-118344
	R"(<skill_template skill_id="16424" name="Shout" nameId="282853" cooldownId="3" stack="NKN_HELPCALL" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE" cooldown="0")"
	R"( duration="1000" cancel_rate="40" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="ME" first_target_range="2" target_relation="FRIEND" target_type="ONLYONE" target_maxcount="6" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><statup duration2="1000" effectid="30152" e="1" noresist="true" hoptype="SKILLLV" hopb="796" hopa="88">)"
	R"(<change stat="PHYSICAL_DEFENSE" func="ADD" delta="10" value="10" /></statup></effects>)"
	R"(<motion name="normalfire" /></skill_template>)"
	// skills/skill_templates.xml:120930-120942
	R"(<skill_template skill_id="16602" name="Strike" nameId="284676" cooldownId="1" stack="GNWA_SATK_NR" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="2500" cancel_rate="35" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="2" target_relation="ENEMY" target_type="ONLYONE" target_maxcount="1" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><skillatk mode="PERCENT" value="85" e="1" accmod2="0" hoptype="DAMAGE" /></effects>)"
	R"(<motion name="poweratk" /></skill_template>)"
	// skills/skill_templates.xml:121016-121028
	R"(<skill_template skill_id="16608" name="Crippling Wave" nameId="284684" cooldownId="2" stack="GNFI_SATKSA_NR" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="2500" cancel_rate="25" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="ME" first_target_range="2" target_relation="ENEMY" target_type="AREA" target_maxcount="8" effective_altitude="5")"
	R"( effective_angle="240" effective_dist="5" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><skillatk mode="PERCENT" value="60" e="1" accmod2="0" hoptype="DAMAGE" /></effects>)"
	R"(<motion name="poweratk" /></skill_template>)"
	// skills/skill_templates.xml:123086-123100
	R"(<skill_template skill_id="16742" name="Wide Thrust" nameId="286633" cooldownId="2" stack="GNKN_SKNOCKBACKSA" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="3500" cancel_rate="35" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="2" target_relation="ENEMY" target_type="AREA" target_maxcount="3")"
	R"( effective_altitude="5" effective_angle="240" effective_dist="5" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><skillatk mode="PERCENT" value="15" e="1" accmod2="0" hoptype="DAMAGE"><subeffect skill_id="8217" /></skillatk></effects>)"
	R"(<motion name="poweratk" /></skill_template>)"
	// skills/skill_templates.xml:124842-124854
	R"(<skill_template skill_id="16856" name="Blessing of Rock" nameId="288001" cooldownId="449" stack="GNKN_GUARDIANST_NR" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="20" activation="ACTIVE")"
	R"( cooldown="0" duration="2500" cancel_rate="35" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="2" target_relation="FRIEND" target_type="ONLYONE" target_maxcount="1" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><provoker skill_id="16393" provoke_target="ME" duration2="5000" duration1="100" effectid="10168561" e="1" noresist="true")"
	R"( hittype="EVERYHIT" element="EARTH" hoptype="SKILLLV" hopb="60" hopa="60" /></effects>)"
	R"(<motion name="normalfire" /></skill_template>)"
	// skills/skill_templates.xml:126795-126812
	R"(<skill_template skill_id="16989" name="Cold Attack" nameId="288732" cooldownId="4" stack="NEL_SPELLDOTICESNARETA_NR" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10")"
	R"( activation="ACTIVE" cooldown="0" duration="2500" ammospeed="25" cancel_rate="35" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="16" target_relation="ENEMY" target_type="AREA" target_maxcount="4")"
	R"( effective_altitude="5" effective_range="5" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><spellatkinstant mode="PERCENT" value="49" e="1" element="WIND" hoptype="DAMAGE" />)"
	R"(<spellatk checktime="2000" value="10" delta="1" duration2="8100" duration1="200" effectid="10169892" e="2" noresist="true" element="WIND")"
	R"( preeffect="1" hoptype="DAMAGE" />)"
	R"(<snare duration2="8100" duration1="200" effectid="20007" e="3" noresist="true" element="WIND" preeffect="2" hoptype="SKILLLV" hopb="60")"
	R"( hopa="60"><change stat="SPEED" func="PERCENT" value="-30" /><change stat="FLY_SPEED" func="PERCENT" value="-30" /></snare></effects>)"
	R"(<motion name="pointfire" /></skill_template>)"
	// skills/skill_templates.xml:132058-132070
	R"(<skill_template skill_id="17335" name="Flame Bolt" nameId="290164" cooldownId="1" stack="DGWI_SPELLLONG_FIRE" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="3000" ammospeed="30" cancel_rate="20" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="37" target_relation="ENEMY" target_type="ONLYONE" target_maxcount="1" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><spellatkinstant mode="PERCENT" value="108" e="1" element="FIRE" hoptype="DAMAGE" /></effects>)"
	R"(<motion name="pointfire2" /></skill_template>)"
	// skills/skill_templates.xml:187148-187162
	R"(<skill_template skill_id="21287" name="Agrint Curse" nameId="293801" cooldownId="12" stack="DNWI_SLEEPDEFORMSPELLDELAY6STA_NO" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="DEBUFF_MENTAL" req_dispel_level="1" req_dispel_count="20")"
	R"( activation="ACTIVE" cooldown="0" duration="2500" cancel_rate="25" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="16" target_relation="ENEMY" target_type="AREA" target_maxcount="2")"
	R"( effective_altitude="5" effective_range="5" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><sleep duration2="6100" effectid="20106" e="1" element="EARTH" hoptype="SKILLLV" hopb="60" hopa="60" />)"
	R"(<deform model="214869" cantUseSkills="true" duration2="6100" effectid="175" e="2" noresist="true" element="EARTH" preeffect="1")"
	R"( hoptype="SKILLLV" hopb="60" hopa="60" />)"
	R"(<spellatk checktime="6000" value="15" delta="94" duration2="6100" effectid="1018175" e="3" noresist="true" element="EARTH" preeffect="1")"
	R"( hoptype="DAMAGE" /></effects>)"
	R"(<motion name="pointfire" /></skill_template>)"
	// skills/skill_templates.xml:187163-187176
	R"(<skill_template skill_id="21288" name="Fire Burst" nameId="284548" cooldownId="1" stack="DNWI_SPELLLONGFIRE_TA" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE")"
	R"( cooldown="0" duration="2500" ammospeed="25" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="16" target_relation="ENEMY" target_type="AREA" target_maxcount="8")"
	R"( effective_altitude="5" effective_range="5" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><spellatkinstant mode="PERCENT" value="99" e="1" element="FIRE" hoptype="DAMAGE" />)"
	R"(<spellatk checktime="2000" value="15" delta="1" duration2="4100" duration1="80" effectid="10165762" e="2" element="FIRE" preeffect="1" />)"
	R"(</effects><motion name="pointfire" /></skill_template>)"
	// skills/skill_templates.xml:183013-183031
	R"(<skill_template skill_id="20985" name="Smash Casting" nameId="2281440" stack="IDTIAMAT_TIAMAT_CYCLOPS_POWERATK" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0">)"
	R"(<properties first_target="TARGET" first_target_range="4" target_relation="ENEMY" target_type="ONLYONE" target_maxcount="1" />)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><spellatkinstant value="3600" delta="1" e="1" element="EARTH"><modifiers><targetclass class="RANGER" value="3500" />)"
	R"(<targetclass class="ASSASSIN" value="3500" /><targetclass class="CLERIC" value="3500" /><targetclass class="CHANTER" value="3500" />)"
	R"(<targetclass class="SORCERER" value="3500" /><targetclass class="SPIRIT_MASTER" value="3500" /></modifiers></spellatkinstant></effects>)"
	R"(<motion name="poweratk" /></skill_template>)"
	// skills/skill_templates.xml:183032-183050
	R"(<skill_template skill_id="20986" name="Summon Rock" nameId="2281441" stack="IDTIAMAT_TIAMAT_CYCLOPS_AREAATK" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0">)"
	R"(<properties first_target="ME" first_target_range="1" target_relation="ENEMY" target_type="AREA" target_maxcount="4" effective_altitude="10")"
	R"( effective_range="5" />)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><spellatkinstant value="4500" delta="1" e="1" element="EARTH"><modifiers><targetclass class="RANGER" value="1000" />)"
	R"(<targetclass class="ASSASSIN" value="1000" /><targetclass class="CLERIC" value="1000" /><targetclass class="CHANTER" value="1000" />)"
	R"(<targetclass class="SORCERER" value="1000" /><targetclass class="SPIRIT_MASTER" value="1000" /></modifiers></spellatkinstant></effects>)"
	R"(<motion name="normalfire" delay="800" /></skill_template>)"
	// skills/skill_templates.xml:131450-131462
	R"(<skill_template skill_id="17294" name="Ferocious Strike I" nameId="290048" cooldownId="1" stack="DGFI_SATK_NR" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="2500" cancel_rate="25" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="4" target_relation="ENEMY" target_type="ONLYONE" target_maxcount="1" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><skillatk mode="PERCENT" value="85" e="1" accmod2="0" hoptype="DAMAGE" /></effects>)"
	R"(<motion name="chainatk1" /></skill_template>)"
	// skills/skill_templates.xml:131463-131475
	R"(<skill_template skill_id="17295" name="Ferocious Strike II" nameId="290049" cooldownId="1" stack="DGFI_SATK_MID" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="3000" cancel_rate="25" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<properties first_target="TARGET" first_target_range="4" target_relation="ENEMY" target_type="ONLYONE" target_maxcount="1" />)"
	R"(<startconditions><weapon weapon="GREATSWORD SPELLBOOK BOW DAGGER MACE ORB POLEARM STAFF SWORD GUN CANNON HARP KEYBLADE" /></startconditions>)"
	R"(<useconditions><move_casting allow="false" /></useconditions>)"
	R"(<effects><skillatk mode="PERCENT" value="181" e="1" accmod2="0" hoptype="DAMAGE" /></effects>)"
	R"(<motion name="chainatk2" /></skill_template>)"
	R"(</skill_data>)";

/** tribe/tribe_relations.xml, verbatim: the tribes of the npcs above, and PC/PC_DARK for the region-activating players of AiWorldTest */
inline const char* const NPC_SKILL_TRIBE_RELATIONS_XML =
	R"(<tribe_relations>)"
	// tribe/tribe_relations.xml:29-31
	R"(<tribe name="AGGRESSIVESINGLEMONSTER" base="MONSTER"><aggro>PC PC_DARK</aggro></tribe>)"
	// tribe/tribe_relations.xml:867-869
	R"(<tribe name="HOSTILEONLYMONSTER" base="USEALL"><hostile>MONSTER</hostile></tribe>)"
	// tribe/tribe_relations.xml:977-981
	R"(<tribe name="IDF5U1_VRITRA" base="GUARD_DRAGON"><aggro>PC GUARD PC_DARK GUARD_DARK ESCORT IDF5U1_TANK IDF5U1_PCFLAG</aggro>)"
	R"(<neutral>XDRAKAN</neutral><support>IDF5U1_VRITRA IDF5U1_VRITRAFLAG IDF5U1_VRITRAWEAPON</support></tribe>)"
	// tribe/tribe_relations.xml:1079-1083
	R"(<tribe name="IDF5_TD_ASSULT" base="MONSTER">)"
	R"(<aggro>PC GENERAL GUARD PC_DARK GENERAL_DARK GUARD_DARK IDF5_TD_WEAPON_PC IDF5_TD_WEAPON_PC_DARK</aggro>)"
	R"(<friend>VRITRA IDF5_TD_SIEGE VRITRASUPPORT</friend><support>IDF5_TD_ASSULT</support></tribe>)"
	// tribe/tribe_relations.xml:2170-2173
	R"(<tribe name="MONSTER"><hostile>YUN_GUARD</hostile><friend>POLYMORPHPARROT USEALL_TELEPORTER_LI USEALL_TELEPORTER_DA</friend></tribe>)"
	// tribe/tribe_relations.xml:2302-2310
	R"(<tribe name="PC"><friend>LIGHT_SUR_MOB LIGHT_LICH</friend><none>LASBERG NEUTRAL_DGUARD YDUMMY_DGUARD YDUMMY2_DGUARD)"
	R"( LDF4B_SPARRING_DGUARD LDF4B_SPARRING_DGUARD2 XDRAKAN_UNATTACK LDF5_DUMMY1_DGUARD LDF5_DUMMY2_DGUARD LDF5_SPARRING1_DGUARD)"
	R"( LDF5_SPARRING2_DGUARD</none></tribe>)"
	R"(<tribe name="PC_DARK"><friend>DARK_SUR_MOB DARK_LICH</friend><neutral>FIELD_OBJECT_ALL FIELD_OBJECT_ALL_HOSTILEMONSTER</neutral>)"
	R"(<none>NEUTRAL_LGUARD YDUMMY_LGUARD YDUMMY2_LGUARD LDF4B_SPARRING_GUARD LDF4B_SPARRING_GUARD2 XDRAKAN_UNATTACK LDF5_DUMMY1_LGUARD)"
	R"( LDF5_DUMMY2_LGUARD LDF5_SPARRING1_LGUARD LDF5_SPARRING2_LGUARD</none></tribe>)"
	// tribe/tribe_relations.xml:2596
	R"(<tribe name="USEALL"/>)"
	// tribe/tribe_relations.xml:693-697
	R"(<tribe name="GAB1_SUB_DRAKAN"><aggro>GAB1_01_POINT_01 GAB1_02_POINT_01 GAB1_03_POINT_01 GAB1_04_POINT_01</aggro>)"
	R"(<hostile>GAB1_SUB_DEST_69 GAB1_SUB_DEST_70 GAB1_SUB_DEST_71 GAB1_SUB_DEST_72 GAB1_SUB_DEST_69_AGGRESSIVE GAB1_SUB_DEST_70_AGGRESSIVE)"
	R"( GAB1_SUB_DEST_71_AGGRESSIVE GAB1_SUB_DEST_72_AGGRESSIVE</hostile><support>GAB1_SUB_DRAKAN GAB1_SUB_NONAGGRESSIVE_DRAKAN</support></tribe>)"
	// tribe/tribe_relations.xml:1100-1105
	R"(<tribe name="IDF5_TD_GUARD_DARK" base="GUARD_DARK"><aggro>IDF5_TD_ASSULT IDF5_TD_SIEGE VRITRA VRITRASUPPORT</aggro>)"
	R"(<hostile>MONSTER</hostile><friend>PC_DARK GENERAL_DARK</friend><support>GUARD_DARK IDF5_TD_GUARD_DARK</support></tribe>)"
	// tribe/tribe_relations.xml:1592-1596
	R"(<tribe name="KRALL" base="MONSTER"><aggro>PC GUARD PC_DARK GUARD_DARK</aggro><support>KRALL KRALLMASTER KRALL_TRAINING</support>)"
	R"(<none>KRALLWIZARDCY</none></tribe>)"
	// tribe/tribe_relations.xml:2620-2625
	R"(<tribe name="VRITRASUPPORT" base="GUARD_DRAGON"><aggro>PC GUARD PC_DARK GUARD_DARK</aggro>)"
	R"(<friend>VRITRA NONAGRRESSIVEFRIENDLYVRITRA IDVRITRA_BASE_REBIRTH IDF5_TD_ASSULT IDF5_TD_SIEGE AGRRESSIVEFRIENDLYVRITRA)"
	R"( AGRRESSIVEFRIENDLYVRITRA2</friend><neutral>XDRAKAN</neutral><support>VRITRASUPPORT</support></tribe>)"
	// tribe/tribe_relations.xml:2655-2660
	R"(<tribe name="XDRAKAN" base="MONSTER"><aggro>PC GUARD PC_DARK GUARD_DARK ESCORT</aggro><hostile>IDCATACOMBS_TAROS</hostile>)"
	R"(<neutral>DRAKAN_LGUARD DRAKAN_DGUARD</neutral><support>XDRAKAN XDRAKAN_ELEMENTALIST XDRAKAN_PRIEST DRAGON</support></tribe>)"
	R"(</tribe_relations>)";

/**
 * AiWorldTest with the shipped rows above: NPC_DATA gains the real templates (AiWorldTest's own stay, so its SPARKIE_NPC_ID and GUARD_NPC_ID
 * are still there), and SKILL_DATA, NPC_SKILL_DATA and TRIBE_RELATIONS_DATA hold only real rows. The thread's Rnd generator is restored in
 * TearDown, because the rotation's chance draws are seeded per case.
 */
class NpcSkillWorldTest : public AiWorldTest {
protected:
	void SetUp() override {
		AiWorldTest::SetUp();
		savedGenerator = commons::utils::Rnd::generator();
		std::string npcTemplates = aiNpcTemplatesXml();
		npcTemplates.insert(npcTemplates.rfind("</npc_templates>"), NPC_SKILL_NPC_TEMPLATES_XML);
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), npcTemplates));
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(
			xml::bindString<dataholders::TribeRelationsData>(contexts.emplace_back(), NPC_SKILL_TRIBE_RELATIONS_XML));
		// the Npc constructor builds its NpcSkillList from NPC_SKILL_DATA and drops the rows SKILL_DATA has no template for
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(contexts.emplace_back(), NPC_SKILL_TEMPLATES_XML));
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.publish(xml::bindString<dataholders::NpcSkillData>(contexts.emplace_back(), NPC_SKILL_ROWS_XML));
	}

	void TearDown() override {
		AiWorldTest::TearDown();
		queuedTemplates.clear(); // after the npcs and their queued entries are gone
		dataholders::DataManager::SKILL_DATA.resetForTests();
		commons::utils::Rnd::generator() = savedGenerator;
	}

	/** the entry of the npc's skill list (NpcSkillList keeps the order of the npc_skills row) */
	static runtime::Ptr<model::skill::NpcSkillEntry> entry(model::gameobjects::Npc& npc, int32_t index) {
		return npc.getSkillList()->getNpcSkills()->get(index);
	}

	/** the first entry of the npc's skill list with this skill id */
	static runtime::Ptr<model::skill::NpcSkillEntry> entryOf(model::gameobjects::Npc& npc, int32_t skillId) {
		for (const runtime::Ptr<model::skill::NpcSkillEntry>& skill : *npc.getSkillList()->getNpcSkills())
			if (skill->getSkillId() == skillId)
				return skill;
		return nullptr;
	}

	/**
	 * A seed for this thread's Rnd under which `draws` - the random draws the code under test makes, in its order - answer `wanted`. The draw
	 * sequence is the production code's own, so the seed replays it exactly: SkillAttackManager::chooseNextSkill draws
	 * NpcGameStats::getInitialSkillDelay's Rnd.get(attackSpeed, 3 * attackSpeed) first and then NpcSkillTemplateEntry::chanceReady's
	 * Rnd.chance() for each entry it asks.
	 */
	static uint64_t seedWhere(const std::function<bool()>& draws, bool wanted) {
		for (uint64_t seed = 20260924;; ++seed) {
			commons::utils::Rnd::seedCurrentThreadForTests(seed);
			if (draws() == wanted)
				return seed;
		}
	}

	/** The rotation's draws for an npc whose first asked entry has `probability`: the initial skill delay, then that entry's chance */
	static std::function<bool()> chanceDraws(int32_t attackSpeed, int32_t probability) {
		return [attackSpeed, probability] {
			commons::utils::Rnd::get(attackSpeed, 3 * attackSpeed);
			return commons::utils::Rnd::chance() < static_cast<float>(probability);
		};
	}

	/** Sets the npc's current HP to the lowest value whose getHpPercentage() is `percent` */
	static void setHpPercentage(model::gameobjects::Npc& npc, int32_t percent) {
		int32_t maxHp = npc.getLifeStats()->getMaxHp();
		int32_t hp = static_cast<int32_t>((static_cast<int64_t>(maxHp) * percent + 99) / 100);
		npc.getLifeStats()->setCurrentHp(hp);
		ASSERT_EQ(npc.getLifeStats()->getHpPercentage(), percent) << hp << " of " << maxHp;
	}

	/**
	 * Java npc.queueSkill(skillId, level, nextSkillTime, target) (Npc.java:172-174): queues new NpcSkillTemplateEntry(new
	 * QueuedNpcSkillTemplate(...)), prob 100. That Npc overload is still AION_UNPORTED (P4-11a), so the fixture composes it from the ported
	 * queueSkill(NpcSkillEntry&) and keeps the template alive, which the C++ entry only points to.
	 */
	runtime::Ptr<model::skill::NpcSkillEntry> queueSkill(model::gameobjects::Npc& npc, int32_t skillId, int32_t level, int32_t nextSkillTime,
		model::templates::npcskill::NpcSkillTargetAttribute target) {
		const model::templates::npcskill::QueuedNpcSkillTemplate& queued = *queuedTemplates.emplace_back(
			std::make_unique<model::templates::npcskill::QueuedNpcSkillTemplate>(skillId, level, nextSkillTime, target));
		runtime::Ref<model::skill::NpcSkillTemplateEntry> queuedEntry = model::skill::NpcSkillTemplateEntry::create(queued);
		npc.queueSkill(*queuedEntry);
		return queuedEntry;
	}

	commons::utils::Rnd::Xoshiro256PlusPlus savedGenerator{0};
	std::deque<std::unique_ptr<model::templates::npcskill::QueuedNpcSkillTemplate>> queuedTemplates;
};

} // namespace aion::gameserver::ai::testing
