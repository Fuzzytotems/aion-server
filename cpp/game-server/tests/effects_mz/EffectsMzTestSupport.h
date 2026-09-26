#pragma once

// Test support of the effect classes M-Z (P5-04, M5b-2 stage 1 part 3, m5b2-plan.md F-03, F-05 and D13): real Effects built from bound
// <skill_template> elements - the skills of the gate (1328 Root, the Warrior passives 37/39/43/140, 8291 Soul Sickness) and of D13 (8217, 8218,
// 8751), copied verbatim from skill_templates.xml into the inline strings below (not loaded from the data file, so a later data change does not
// reach these cases), plus test templates where a case needs a value the data does not carry - driven through
// Effect.initialize (the templates' calculate), Effect.applyEffect (applyEffect -> addToEffectedController -> startEffect) and the end task or
// Effect.endEffect, between spawned players and npcs of a real map instance.
//
// - The world, the executor and the creatures are the effect engine's (tests/skills/P5-02b/EffectTestSupport.h, EffectWorldTest: a Poeta map
//   instance with a GeneralInstanceHandler, a DeterministicExecutor on a ManualClock). Its players get the parts the M-Z bodies read that the
//   effect engine's did not need: a level (PlayerGameStats interns the stats template of the class and level at construction), the
//   FlyController (RootEffect, StunEffect, StumbleEffect and StaggerEffect stop gliding) and a real AionConnection whose IO never started
//   (tests/cm_ak/InWorldPacketRunSupport.h), so the packets a body sends can be read back.
// - The npcs are level 1 monsters with the stats written below, so no level difference enters the damage arithmetic.
// - The world maps are the world chunk's test maps (tests/world/WorldTestSupport.h), published before EffectWorldTest looks for them (it
//   publishes its own only when none are), because ReturnEffect teleports through the World singleton, which reads them once per process.
// - GeoService gets its empty GeoMaps (init() with geo data off): StumbleEffect and StaggerEffect ask it for the closest collision.
// - The cast lane's helpers (tests/skills/P5-02a/CastTestSupport.h) add stat bonuses and override configs.
//
// Everything is in an unnamed namespace: the fixture derives EffectWorldTest, which is in one.

#include <gtest/gtest.h>

#include "../cm_ak/InWorldPacketRunSupport.h"
#include "../skills/P5-02a/CastTestSupport.h"
#include "../skills/P5-02b/EffectTestSupport.h"
#include "../world/WorldTestSupport.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/detail/ItemSlotMasks.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/FlyState.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroupInfo.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_EFFECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_STATE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::effect::mztest {
namespace {

namespace cp = network::aion::clientpackets::testing;
using effecttest::KnownListPairing;
using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::stats::container::StatEnum;
using model::Effect;
using runtime::Ptr;
using runtime::Ref;
namespace Rnd = commons::utils::Rnd;

// ------------------------------------------------------------------------------------------------------------------------- skill templates

/**
 * The skill templates of the cases. The real ones are copied from skill_templates.xml without their <properties>, conditions and motions,
 * which no Effect reads; 242 and 18191 lose the effect positions of classes outside this lane (242's <nofly>, 18191's statup and shield).
 * The 64xxx templates are written for the cases: each comment names the value it adds.
 */
inline std::string effectsMzSkills() {
	return
		// ---- the gate's skills and D13's, as the data has them
		R"(<skill_template skill_id="1328" name="Root" nameId="2287465" cooldownId="277" group="MA_ROOT" stack="MA_ROOT" lvl="1" skilltype="MAGICAL")"
		R"( skill_category="PHYSICAL_DEBUFF" skillsubtype="DEBUFF" tslot="DEBUFF" dispel_category="ALL" req_dispel_level="1" req_dispel_count="10")"
		R"( activation="ACTIVE" cooldown="600" duration="0" cancel_rate="20" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")"
		R"( apply_magical_critical="true" apply_casting_time_bonus="true"><effects>)"
		R"(<root resistchance="10" duration2="20000" effectid="20003" e="1" accmod2="500" element="WATER" hoptype="SKILLLV" hopb="1239"/>)"
		R"(</effects></skill_template>)"
		R"(<skill_template skill_id="37" name="Basic Sword Training" nameId="281815" group="P_EQUIP_ENHANCEDSWORD" stack="P_EQUIP_ENHANCEDSWORD" lvl="1")"
		R"( skilltype="PHYSICAL" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" cooldown="0" duration="0")"
		R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects><wpnmastery weapon="SWORD" effectid="101" e="1" basiclvl="1">)"
		R"(<change stat="PHYSICAL_ATTACK" func="PERCENT" value="16"/></wpnmastery></effects></skill_template>)"
		R"(<skill_template skill_id="39" name="Basic Mace Training" nameId="281819" group="P_EQUIP_ENHANCEDMACE" stack="P_EQUIP_ENHANCEDMACE" lvl="1")"
		R"( skilltype="PHYSICAL" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" cooldown="0" duration="0")"
		R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects><wpnmastery weapon="MACE" effectid="103" e="1" basiclvl="1">)"
		R"(<change stat="PHYSICAL_ATTACK" func="PERCENT" value="20"/></wpnmastery></effects></skill_template>)"
		R"(<skill_template skill_id="43" name="Basic Shield Training" nameId="281827" group="P_EQUIP_ENHANCEDSHIELD" stack="P_EQUIP_ENHANCEDSHIELD")"
		R"( lvl="1" skilltype="PHYSICAL" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" cooldown="0" duration="0")"
		R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects><shieldmastery effectid="126" e="1" basiclvl="1">)"
		R"(<change stat="DAMAGE_REDUCE" func="PERCENT" value="0"/></shieldmastery></effects></skill_template>)"
		R"(<skill_template skill_id="50" name="Advanced Shield Training I" nameId="281841" group="P_EQUIP_ENHANCEDSHIELD" stack="P_EQUIP_SHIELD" lvl="1")"
		R"( skilltype="PHYSICAL" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" cooldown="0" duration="0")"
		R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects><shieldmastery effectid="126" e="1" basiclvl="2">)"
		R"(<change stat="DAMAGE_REDUCE" func="PERCENT" value="5"/></shieldmastery></effects></skill_template>)"
		R"(<skill_template skill_id="51" name="Advanced Greatsword Training I" nameId="281843" group="P_EQUIP_2HSWORD" stack="P_EQUIP_2HSWORD" lvl="1")"
		R"( skilltype="PHYSICAL" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" cooldown="0" duration="0")"
		R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
		R"(<wpnmastery weapon="GREATSWORD" effectid="104" e="1" basiclvl="1">)"
		R"(<change stat="PHYSICAL_ATTACK" func="PERCENT" value="4"/></wpnmastery></effects></skill_template>)"
		R"(<skill_template skill_id="55" name="Advanced Dual-Wielding I" nameId="281851" group="P_EQUIP_DUAL" stack="P_EQUIP_DUAL" lvl="1")"
		R"( skilltype="PHYSICAL" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" cooldown="0" duration="0")"
		R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
		R"(<wpndual value="70" effectid="130" e="1" skill_efficiency="40" max_damage_chance="400"/>)"
		R"(</effects></skill_template>)"
		R"(<skill_template skill_id="70" name="Advanced Dual-Wielding" nameId="281881" group="SC_EQUIP_ENHANCEDDUAL" stack="SC_EQUIP_ENHANCEDDUAL")"
		R"( lvl="1" skilltype="PHYSICAL" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" cooldown="0" duration="0")"
		R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
		R"(<wpndual value="63" delta="2" effectid="130" e="1" skill_efficiency="50" max_damage_chance="20" max_damage_delta="80"/>)"
		R"(</effects></skill_template>)"
		R"(<skill_template skill_id="140" name="Boost Physical Attack I" nameId="282029" group="P_STATBOOSTPHYSICALOFFENSE")"
		R"( stack="P_STATBOOSTPHYSICALOFFENSE" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" cooldown="0")"
		R"( duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects><statboost effectid="103041" e="1">)"
		R"(<change stat="PHYSICAL_ATTACK" func="ADD" value="7"/></statboost></effects></skill_template>)"
		R"(<skill_template skill_id="243" name="Return" nameId="282691" cooldownId="1150" stack="RETURNHOME1" lvl="1" skilltype="MAGICAL")"
		R"( skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="12000" duration="6000" cancel_rate="100000")"
		R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects><return e="1" noresist="true"/></effects></skill_template>)"
		R"(<skill_template skill_id="8291" name="Soul Sickness" nameId="282709" stack="CH_RESURRECTDEBUFF" lvl="1" skilltype="MAGICAL")"
		R"( skillsubtype="NONE" tslot="SPEC2" activation="PROVOKED" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true")"
		R"( apply_magical_critical="true"><effects>)"
		R"(<statdown duration2="40000" duration1="20000" e="1" noresist="true" element="FIRE"><change stat="MAXHP" func="PERCENT" value="-30"/>)"
		R"(</statdown>)"
		R"(<statdown duration2="40000" duration1="20000" e="2" noresist="true" element="FIRE" preeffect="1">)"
		R"(<change stat="MAXMP" func="PERCENT" value="-30"/></statdown>)"
		R"(<statdown duration2="40000" duration1="20000" e="3" noresist="true" element="FIRE" preeffect="1">)"
		R"(<change stat="SPEED" func="PERCENT" value="-50"/><change stat="FLY_SPEED" func="PERCENT" value="-50"/></statdown>)"
		R"(</effects></skill_template>)"
		R"(<skill_template skill_id="8217" name="Stunned" nameId="280503" stack="NORMALATTACK_STAGGER" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE")"
		R"( tslot="DEBUFF" dispel_category="STUN" activation="ACTIVE" cooldown="0" duration="3000" cancel_rate="5" hostile_type="DIRECT")"
		R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
		R"(<stagger duration1="2000" effectid="20010" e="1" noresist="true" element="WIND" hoptype="SKILLLV" hopb="1000" hopa="100"/>)"
		R"(</effects></skill_template>)"
		R"(<skill_template skill_id="8218" name="Stumble" nameId="281599" stack="NORMALATTACK_STUMBLE" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE")"
		R"( tslot="DEBUFF" dispel_category="STUN" activation="ACTIVE" cooldown="0" duration="3000" cancel_rate="5" hostile_type="DIRECT")"
		R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
		R"(<stumble duration1="2000" effectid="20011" e="1" noresist="true" element="WIND" hoptype="SKILLLV" hopb="1000" hopa="100"/>)"
		R"(</effects></skill_template>)"
		R"(<skill_template skill_id="8751" name="Light of Repose" nameId="2280015" stack="MATERIAL_HOUSING_PROCDPHEAL_INSTANT" lvl="1")"
		R"( skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" activation="PROVOKED" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true">)"
		R"(<effects><procvphealinstant value2="15" percent="true" value="1" e="1" noresist="true"/>)"
		R"(<heal percent="true" checktime="2000" value="1" duration2="12000" e="2" preeffect="1"/>)"
		R"(<mpheal percent="true" checktime="2000" value="1" duration2="12000" e="3" preeffect="1"/></effects></skill_template>)"
		R"(<skill_template skill_id="2864" name="Ferocious Strike" nameId="2287659" cooldownId="122" group="WA_ROBUSTHIT" stack="WA_ROBUSTHIT" lvl="1")"
		R"( skilltype="PHYSICAL" skill_category="CHAIN_SKILL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="100" duration="0")"
		R"( cancel_rate="10" chain_skill_prob="100" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
		R"(<effects><skillatk value="27" e="1" accmod2="0" hoptype="DAMAGE"/></effects></skill_template>)"
		R"(<skill_template skill_id="1282" name="Flame Bolt" nameId="2287999" cooldownId="271" group="MA_FLAMEBOLT" stack="MA_FLAMEBOLT" lvl="1")"
		R"( skilltype="MAGICAL" skill_category="CHAIN_SKILL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="2000")"
		R"( ammospeed="30" cancel_rate="20" chain_skill_prob="100" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")"
		R"( apply_magical_critical="true" apply_casting_time_bonus="true"><effects>)"
		R"(<spellatkinstant value="141" e="1" element="FIRE" hoptype="DAMAGE"/></effects></skill_template>)"
		R"(<skill_template skill_id="264" name="Trajanus' Blessing" nameId="288312" stack="Q_PROTECTOFTRAJANUS" lvl="1" skilltype="MAGICAL")"
		R"( skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" cooldown="10" duration="0" cancel_rate="20" apply_magical_skill_boost_bonus="true")"
		R"( apply_magical_critical="true"><effects>)"
		R"(<shield percent="true" hitvalue="50" value="10000" duration2="900000" effectid="154" e="1" noresist="true" hittype="EVERYHIT"/>)"
		R"(</effects></skill_template>)"
		R"(<skill_template skill_id="620" name="Armor of Attrition" nameId="2287735" cooldownId="91" group="FI_FORTITUDE" stack="FI_FORTITUDE" lvl="1")"
		R"( skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE")"
		R"( cooldown="1800" duration="0" cancel_rate="20" hostile_type="INDIRECT"><effects>)"
		R"(<reflector hitvalue="70" radius="30" duration2="15000" effectid="153" e="1" noresist="true" hittype="EVERYHIT" hoptype="SKILLLV")"
		R"( hopb="17880"/>)"
		R"(<provoker skill_id="8929" provoke_target="ME" duration2="20000" effectid="123452" e="2" noresist="true" hittype="EVERYHIT" element="WATER")"
		R"( preeffect="1"/></effects></skill_template>)"
		R"(<skill_template skill_id="868" name="Tactical Retreat" nameId="2287822" cooldownId="177" group="SC_SPIRITOFGALE" stack="SC_SPIRITOFGALE")"
		R"( lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10")"
		R"( activation="ACTIVE" cooldown="1200" duration="0" cancel_rate="20" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true")"
		R"( apply_magical_critical="true"><effects>)"
		R"(<provoker skill_id="8502" provoke_target="ME" duration2="30000" effectid="105601" e="1" noresist="true" hittype="EVERYHIT" element="WIND")"
		R"( hoptype="SKILLLV" hopb="467"/></effects></skill_template>)"
		R"(<skill_template skill_id="8502" name="Tactical Retreat I Effect" nameId="291517" stack="SC_SPIRITOFGALE_PROC" lvl="1" skilltype="MAGICAL")"
		R"( skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10" activation="PROVOKED" cooldown="0")"
		R"( duration="0" apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
		R"(<statup duration2="5000" effectid="185021" e="1" noresist="true"><change stat="SPEED" func="PERCENT" value="30"/></statup>)"
		R"(</effects></skill_template>)"
		R"(<skill_template skill_id="242" name="Drakan Transformation" nameId="295747" stack="Q_POLYMORPH_DRAKAN_02" lvl="1" skilltype="MAGICAL")"
		R"( skillsubtype="BUFF" tslot="BUFF" dispel_category="NPC_BUFF" activation="ACTIVE" cooldown="600" duration="0")"
		R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
		R"(<polymorph model="281812" type="NONE" cantUseSkills="true" duration2="60000" effectid="175" e="1" basiclvl="100" noresist="true")"
		R"( element="WIND"/></effects></skill_template>)"
		R"(<skill_template skill_id="18191" name="Captain's Pride" nameId="293936" cooldownId="1" stack="BNFI_INVINCIBLE_CAPTAIN" lvl="1")"
		R"( skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="NPC_BUFF" req_dispel_level="5" req_dispel_count="100")"
		R"( activation="ACTIVE" cooldown="0" duration="0" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
		R"(<effects><sanctuary duration2="86400000" e="1" noresist="true" element="WIND"/></effects></skill_template>)"
		// ---- the test templates
		// 64001: a stun (3246 Quickening Doom's <stun> without its preeffect and condition); accmod2 10000 makes the dodge rate negative, so the
		// physical dodge roll can never succeed (StatFunctions.checkIsDodgedHit), and the STUN_RESISTANCE roll is the only one that can resist it
		R"(<skill_template skill_id="64001" name="mz stun" nameId="1" stack="MZ_STUN" lvl="1" skilltype="PHYSICAL" skillsubtype="DEBUFF" tslot="DEBUFF")"
		R"( dispel_category="STUN" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<stun duration2="3000" effectid="20000" e="1" accmod2="10000"/></effects></skill_template>)"
		// 64002: 873 Dilation Arrow's <slow> without randomtime (a random subtraction of the duration) and preeffect, accmod2 10000 as 64001
		R"(<skill_template skill_id="64002" name="mz slow" nameId="1" stack="MZ_SLOW" lvl="1" skilltype="MAGICAL" skillsubtype="DEBUFF" tslot="DEBUFF")"
		R"( activation="ACTIVE" cooldown="0" duration="0"><effects><slow duration2="6000" effectid="20008" e="1" accmod2="10000" element="FIRE">)"
		R"(<change stat="ATTACK_SPEED" func="PERCENT" value="50"/></slow></effects></skill_template>)"
		// 64003: 303 Material Test Snare's <snare>, accmod2 10000 against the magical resist rate
		R"(<skill_template skill_id="64003" name="mz snare" nameId="1" stack="MZ_SNARE" lvl="1" skilltype="MAGICAL" skillsubtype="DEBUFF")"
		R"( tslot="DEBUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<snare duration2="10000" effectid="20007" e="1" accmod2="10000" element="FIRE"><change stat="SPEED" func="PERCENT" value="-50"/>)"
		R"(<change stat="FLY_SPEED" func="PERCENT" value="-50"/></snare></effects></skill_template>)"
		// 64004: a statup of two changes, one with a per-level delta
		R"(<skill_template skill_id="64004" name="mz statup" nameId="1" stack="MZ_STATUP" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
		R"( tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects><statup duration2="120000" effectid="107922" e="1" noresist="true">)"
		R"(<change stat="MAXHP" func="PERCENT" value="50"/><change stat="PHYSICAL_ATTACK" func="ADD" value="25" delta="5"/></statup>)"
		R"(</effects></skill_template>)"
		// 64006: a percent shield with a per-level hit and total (264's shape with hitdelta and delta)
		R"(<skill_template skill_id="64006" name="mz shield" nameId="1" stack="MZ_SHIELD" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
		R"( tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<shield percent="true" hitvalue="50" hitdelta="10" value="100" delta="20" duration2="900000" effectid="154" e="1" noresist="true")"
		R"( hittype="EVERYHIT"/></effects></skill_template>)"
		// 64007: a flat shield: at most 30 per hit, 50 in all
		R"(<skill_template skill_id="64007" name="mz flat shield" nameId="1" stack="MZ_FLAT_SHIELD" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
		R"( tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<shield hitvalue="30" value="50" duration2="900000" effectid="154" e="1" noresist="true" hittype="EVERYHIT"/></effects></skill_template>)"
		// 64008: 620's <reflector> with a per-level hit, a percentage `value` and a `delta` that Java's reflector does not read
		R"(<skill_template skill_id="64008" name="mz reflector" nameId="1" stack="MZ_REFLECTOR" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
		R"( tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<reflector hitvalue="70" hitdelta="5" value="30" delta="10" radius="30" duration2="15000" effectid="153" e="1" noresist="true")"
		R"( hittype="EVERYHIT"/></effects></skill_template>)"
		// 64009..64013: provokers of each hit type and target, all applying 64010 (a provoked statdown)
		R"(<skill_template skill_id="64009" name="mz provoker opponent" nameId="1" stack="MZ_PROVOKER_OPPONENT" lvl="1" skilltype="MAGICAL")"
		R"( skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<provoker skill_id="64010" provoke_target="OPPONENT" duration2="30000" e="1" noresist="true" hittype="EVERYHIT"/></effects></skill_template>)"
		R"(<skill_template skill_id="64010" name="mz provoked" nameId="1" stack="MZ_PROVOKED" lvl="1" skilltype="MAGICAL" skillsubtype="DEBUFF")"
		R"( tslot="DEBUFF" activation="PROVOKED" cooldown="0" duration="0"><effects><statdown duration2="5000" e="1" noresist="true">)"
		R"(<change stat="PHYSICAL_ATTACK" func="ADD" value="-10"/></statdown></effects></skill_template>)"
		R"(<skill_template skill_id="64011" name="mz provoker attack" nameId="1" stack="MZ_PROVOKER_ATTACK" lvl="1" skilltype="MAGICAL")"
		R"( skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<provoker skill_id="64010" provoke_target="OPPONENT" duration2="30000" e="1" noresist="true" hittype="NMLATK" hittypeprob2="50"/>)"
		R"(</effects></skill_template>)"
		R"(<skill_template skill_id="64012" name="mz provoker physical" nameId="1" stack="MZ_PROVOKER_PHYSICAL" lvl="1" skilltype="MAGICAL")"
		R"( skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<provoker skill_id="64010" provoke_target="ME" duration2="30000" e="1" noresist="true" hittype="PHHIT" radius="10"/>)"
		R"(</effects></skill_template>)"
		R"(<skill_template skill_id="64013" name="mz provoker magical" nameId="1" stack="MZ_PROVOKER_MAGICAL" lvl="1" skilltype="MAGICAL")"
		R"( skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<provoker skill_id="64010" provoke_target="ME" duration2="30000" e="1" noresist="true" hittype="MAHIT"/></effects></skill_template>)"
		R"(<skill_template skill_id="64014" name="mz provoker back" nameId="1" stack="MZ_PROVOKER_BACK" lvl="1" skilltype="MAGICAL")"
		R"( skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<provoker skill_id="64010" provoke_target="OPPONENT" duration2="30000" e="1" noresist="true" hittype="BACKATK"/></effects></skill_template>)"
		// 64015: 1447 Erosion's <spellatk> on its own position 1, with the magic boost and the magical critical flag off (the critical is rolled
		// anyway: SpellAttackEffect.resolveMagicalCritical ignores the flag)
		R"(<skill_template skill_id="64015" name="mz spellatk" nameId="1" stack="MZ_SPELLATK" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK")"
		R"( tslot="DEBUFF" activation="ACTIVE" cooldown="0" duration="0" apply_magical_skill_boost_bonus="false" apply_magical_critical="false">)"
		R"(<effects><spellatk checktime="3000" value="81" delta="4" duration2="15000" effectid="113582" e="1" noresist="true" element="EARTH"/>)"
		R"(</effects></skill_template>)"
		// 64016, 64017: a magical <skillatk> (no physical rolls), with and without cannotmiss and without noresist; 64018: rnddmg="3"
		R"(<skill_template skill_id="64016" name="mz skillatk cannotmiss" nameId="1" stack="MZ_SKILLATK_1" lvl="1" skilltype="PHYSICAL")"
		R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0" apply_magical_skill_boost_bonus="false")"
		R"( apply_magical_critical="false"><effects><skillatk value="100" e="1" element="FIRE" cannotmiss="true"/></effects></skill_template>)"
		R"(<skill_template skill_id="64017" name="mz skillatk" nameId="1" stack="MZ_SKILLATK_2" lvl="1" skilltype="PHYSICAL")"
		R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0" apply_magical_skill_boost_bonus="false")"
		R"( apply_magical_critical="false"><effects><skillatk value="100" e="1" element="FIRE"/></effects></skill_template>)"
		R"(<skill_template skill_id="64018" name="mz skillatk rnddmg" nameId="1" stack="MZ_SKILLATK_3" lvl="1" skilltype="PHYSICAL")"
		R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0" apply_magical_skill_boost_bonus="false")"
		R"( apply_magical_critical="false"><effects><skillatk value="100" e="1" element="FIRE" noresist="true" rnddmg="3"/></effects></skill_template>)"
		// 64019: a second polymorph (another model, another stack) for TransformEffect.endEffect's search
		R"(<skill_template skill_id="64019" name="mz polymorph" nameId="1" stack="MZ_POLYMORPH" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
		R"( tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<polymorph model="281813" type="NONE" cantMove="true" duration2="30000" e="1" noresist="true"/></effects></skill_template>)"
		// 64020: an MP heal over time with a larger percentage than 8751's
		R"(<skill_template skill_id="64020" name="mz mpheal" nameId="1" stack="MZ_MPHEAL" lvl="1" skilltype="PHYSICAL" skillsubtype="HEAL")"
		R"( tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<mpheal percent="true" checktime="2000" value="10" duration2="12000" e="1" noresist="true"/></effects></skill_template>)"
		// 64021: a flat repose heal (8751's procvphealinstant without percent)
		R"(<skill_template skill_id="64021" name="mz repose" nameId="1" stack="MZ_REPOSE" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
		R"( tslot="BUFF" activation="PROVOKED" cooldown="0" duration="0"><effects>)"
		R"(<procvphealinstant value2="15" value="50" delta="10" e="1" noresist="true"/></effects></skill_template>)"
		// 64022: a sanctuary of 8 s (18191 lasts a day)
		R"(<skill_template skill_id="64022" name="mz sanctuary" nameId="1" stack="MZ_SANCTUARY" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
		R"( tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects><sanctuary duration2="8000" e="1" noresist="true"/></effects>)"
		R"(</skill_template>)"
		// 64023, 64024: 8218's <stumble> and 8217's <stagger> without noresist; accmod2 10000 against the magical resist rate (element WIND)
		R"(<skill_template skill_id="64023" name="mz stumble" nameId="1" stack="MZ_STUMBLE" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE")"
		R"( tslot="DEBUFF" dispel_category="STUN" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<stumble duration1="2000" effectid="20011" e="1" accmod2="10000" element="WIND"/></effects></skill_template>)"
		R"(<skill_template skill_id="64024" name="mz stagger" nameId="1" stack="MZ_STAGGER" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE")"
		R"( tslot="DEBUFF" dispel_category="STUN" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<stagger duration1="2000" effectid="20010" e="1" accmod2="10000" element="WIND"/></effects></skill_template>)"
		// 64025: 37 without its <change>; 64026: 37 without its weapon attribute; 64027: 43 without its <change>
		R"(<skill_template skill_id="64025" name="mz mastery without change" nameId="1" stack="MZ_MASTERY_1" lvl="1" skilltype="PHYSICAL")"
		R"( skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" cooldown="0" duration="0"><effects>)"
		R"(<wpnmastery weapon="SWORD" effectid="101" e="1" basiclvl="1"/></effects></skill_template>)"
		R"(<skill_template skill_id="64026" name="mz mastery without weapon" nameId="1" stack="MZ_MASTERY_2" lvl="1" skilltype="PHYSICAL")"
		R"( skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" cooldown="0" duration="0"><effects><wpnmastery effectid="101" e="1" basiclvl="1">)"
		R"(<change stat="PHYSICAL_ATTACK" func="PERCENT" value="16"/></wpnmastery></effects></skill_template>)"
		R"(<skill_template skill_id="64027" name="mz shield mastery without change" nameId="1" stack="MZ_MASTERY_3" lvl="1" skilltype="PHYSICAL")"
		R"( skillsubtype="NONE" tslot="NOSHOW" activation="PASSIVE" cooldown="0" duration="0"><effects>)"
		R"(<shieldmastery effectid="126" e="1" basiclvl="1"/></effects></skill_template>)"
		// 64028: a FORM1 transformation with a panel (TransformEffect.applyEffect's removeTransformEffects arm)
		R"(<skill_template skill_id="64028" name="mz form" nameId="1" stack="MZ_FORM" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF")"
		R"( activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<polymorph model="281813" type="FORM1" panelid="1" duration2="30000" e="1" noresist="true"/></effects></skill_template>)"
		// 64029: a root whose skill also carries a <paralyze> (Effect.isParalyzeEffect: SkillTemplate.hasAnyEffect(PARALYZE)); the cases calculate
		// and apply only the root position (rootedWithParalyze), so they see the root alone (ParalyzeEffect has its own cases since M5b-3,
		// ProcEffectsTest.cpp)
		R"(<skill_template skill_id="64029" name="mz paralyzing root" nameId="1" stack="MZ_PARALYZE" lvl="1" skilltype="MAGICAL")"
		R"( skillsubtype="DEBUFF" tslot="DEBUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<root resistchance="10" duration2="20000" e="1" noresist="true"/><paralyze duration2="20000" e="2" noresist="true"/>)"
		R"(</effects></skill_template>)"
		// 64030: a percent shield for physical hits only (hittype PHHIT); 64031: one that absorbs with hittypeprob2 50
		R"(<skill_template skill_id="64030" name="mz physical shield" nameId="1" stack="MZ_SHIELD_PHYSICAL" lvl="1" skilltype="MAGICAL")"
		R"( skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<shield percent="true" hitvalue="50" value="10000" duration2="900000" effectid="154" e="1" noresist="true" hittype="PHHIT"/>)"
		R"(</effects></skill_template>)"
		R"(<skill_template skill_id="64031" name="mz shield of chance" nameId="1" stack="MZ_SHIELD_CHANCE" lvl="1" skilltype="MAGICAL")"
		R"( skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<shield percent="true" hitvalue="50" value="10000" duration2="900000" effectid="154" e="1" noresist="true" hittype="EVERYHIT")"
		R"( hittypeprob2="50"/></effects></skill_template>)"
		// 64032: a reflector of at least 10 between 3 m (minradius) and 30 m, with hittypeprob2 50
		R"(<skill_template skill_id="64032" name="mz near reflector" nameId="1" stack="MZ_REFLECTOR_NEAR" lvl="1" skilltype="MAGICAL")"
		R"( skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<reflector hitvalue="10" minradius="3" radius="30" duration2="15000" effectid="153" e="1" noresist="true" hittype="EVERYHIT")"
		R"( hittypeprob2="50"/></effects></skill_template>)"
		// 64033: a third polymorph (model 281814, no NPC_DATA template: no tribe) that cannot move, for TransformEffect.endEffect's search
		R"(<skill_template skill_id="64033" name="mz third polymorph" nameId="1" stack="MZ_POLYMORPH_3" lvl="1" skilltype="MAGICAL")"
		R"( skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
		R"(<polymorph model="281814" type="NONE" cantMove="true" duration2="30000" e="1" noresist="true"/></effects></skill_template>)";
}

/** Npc id of the monsters (their template is bound once, below) and of the two npc templates PolymorphEffect reads from NPC_DATA */
inline constexpr int32_t MZ_MONSTER = 290001;
inline constexpr int32_t DRAKAN_MODEL = 281812;
inline constexpr int32_t SECOND_MODEL = 281813;
inline constexpr int32_t THIRD_MODEL = 281814;

/** npc_templates.xml reduced to the two models of 242 and 64019 and their tribes (281812 is DRAKANPOLYMORPH in the data) */
inline constexpr std::string_view POLYMORPH_NPCS_XML =
	R"(<npc_templates><npc_template npc_id="281812" level="55" name="mitrakand drakan destroyer" name_id="320388" rank="DISCIPLINED")"
	R"( rating="ELITE" tribe="DRAKANPOLYMORPH"><stats maxHp="100" maxMp="100"><speeds walk="1" run="2"/></stats></npc_template>)"
	R"(<npc_template npc_id="281813" level="1" name="mz model" name_id="1" rank="NOVICE" rating="NORMAL" tribe="MONSTER">)"
	R"(<stats maxHp="100" maxMp="100"><speeds walk="1" run="2"/></stats></npc_template></npc_templates>)";

// ------------------------------------------------------------------------------------------------------------------------- packets

/** One effect entry of SM_ABNORMAL_STATE / SM_ABNORMAL_EFFECT (effectorId is 0 where the packet has none) */
struct AbnormalEntry {
	int32_t effectorId = 0;
	int32_t skillId = 0;
	int32_t level = 0;
	int32_t targetSlotOrdinal = 0;
	int32_t remainingTime = 0;
};

/** SM_ABNORMAL_STATE as SM_ABNORMAL_STATE.java:25-38 writes it */
struct AbnormalStateFields {
	int32_t abnormals = 0;
	int32_t slot = 0;
	std::vector<AbnormalEntry> effects;
};

inline AbnormalStateFields decodeAbnormalState(const std::vector<uint8_t>& bytes) {
	network::test::PacketReader reader(cp::bodyOf(bytes));
	AbnormalStateFields f;
	f.abnormals = reader.D();
	reader.D();
	reader.D(); // 4.5
	f.slot = reader.C();
	int32_t count = static_cast<uint16_t>(reader.H());
	for (int32_t i = 0; i < count; ++i) {
		AbnormalEntry e;
		e.effectorId = reader.D();
		e.skillId = static_cast<uint16_t>(reader.H());
		e.level = reader.C();
		e.targetSlotOrdinal = reader.C();
		e.remainingTime = reader.D();
		f.effects.push_back(e);
	}
	EXPECT_EQ(reader.remaining(), 0u) << "SM_ABNORMAL_STATE consumed exactly";
	return f;
}

/** SM_ABNORMAL_EFFECT as SM_ABNORMAL_EFFECT.java:38-61 writes it (effectType 1: an npc, 2: a player) */
struct AbnormalEffectFields {
	int32_t effectedId = 0;
	int32_t effectType = 0;
	int32_t abnormals = 0;
	int32_t slots = 0;
	std::vector<AbnormalEntry> effects;
};

inline AbnormalEffectFields decodeAbnormalEffect(const std::vector<uint8_t>& bytes) {
	network::test::PacketReader reader(cp::bodyOf(bytes));
	AbnormalEffectFields f;
	f.effectedId = reader.D();
	f.effectType = reader.C();
	reader.D(); // time
	f.abnormals = reader.D();
	reader.D();
	f.slots = reader.C();
	int32_t count = static_cast<uint16_t>(reader.H());
	for (int32_t i = 0; i < count; ++i) {
		AbnormalEntry e;
		if (f.effectType == 2)
			e.effectorId = reader.D();
		e.skillId = static_cast<uint16_t>(reader.H());
		e.level = reader.C();
		if (f.effectType == 1 || f.effectType == 2) {
			e.targetSlotOrdinal = reader.C();
			e.remainingTime = reader.D();
		}
		f.effects.push_back(e);
	}
	EXPECT_EQ(reader.remaining(), 0u) << "SM_ABNORMAL_EFFECT consumed exactly";
	return f;
}

/** Java AbnormalState.getId() of the states these bodies set (AbnormalState.java:9-40) */
inline constexpr int32_t ROOT_ID = 1 << 4;
inline constexpr int32_t STUN_ID = 1 << 12;
inline constexpr int32_t STUMBLE_ID = 1 << 14;
inline constexpr int32_t STAGGER_ID = 1 << 15;
inline constexpr int32_t SNARE_ID = 1 << 17;
inline constexpr int32_t SLOW_ID = 1 << 18;
inline constexpr int32_t SANCTUARY_ID = static_cast<int32_t>(1u << 31);

/** Java SkillTargetSlot ordinals and ids (SkillTargetSlot.java: BUFF(1), DEBUFF(2), ..., SPEC2(16)) */
inline constexpr int32_t BUFF_ORDINAL = 0;
inline constexpr int32_t DEBUFF_ORDINAL = 1;
inline constexpr int32_t SPEC2_ORDINAL = 4;
inline constexpr int32_t BUFF_SLOT_ID = 1;
inline constexpr int32_t DEBUFF_SLOT_ID = 2;

// ------------------------------------------------------------------------------------------------------------------------- fixture

/** Npc templates are immortal static data; bound on first use (not during static initialization, CastTestSupport.h's note) */
inline const gameserver::model::templates::npc::NpcTemplate* mzMonsterTemplate() {
	static const gameserver::model::templates::npc::NpcTemplate* bound = [] {
		xml::LoadContext context;
		return xml::bindString<gameserver::model::templates::npc::NpcTemplate>(context,
			R"(<npc_template name_id="1" npc_id="290001" level="1" name="mz monster" attack_speed="2000" arange="2" rating="NORMAL" tribe="MONSTER">)"
			R"(<stats maxHp="1000" maxMp="100" pdef="100" mdef="100" attack="16" evasion="0" parry="0" block="0" accuracy="200" macc="60" pcrit="10")"
			R"( mcrit="20"><speeds walk="0.8" run="2.0" run_fight="3.0" group_walk="0.5" group_run_fight="2.5" fly="4.0"/></stats></npc_template>)")
			.release();
	}();
	return bound;
}

class EffectsMzTest : public effecttest::EffectWorldTest {
protected:
	void SetUp() override {
		// before EffectWorldTest looks for world maps: the World singleton (ReturnEffect's teleport) reads them once per process
		ASSERT_TRUE(world::test::publishTestStaticData()) << "this process published the real static data (run the test on its own)";
		EffectWorldTest::SetUp();
		EFFECT_TEST_SCOPE;
		static const bool geoInitialised = [] {
			world::geo::GeoService::getInstance().init(); // geo data off: one empty GeoMap per test map
			return true;
		}();
		static_cast<void>(geoInitialised);
		publishSkillData(effectsMzSkills());
		publishTribeRelations();
		xml::LoadContext context;
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(context, cp::PLAYER_EXPERIENCE_TABLE_XML));
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
		{
			EFFECT_TEST_SCOPE;
			for (Player* inRegion : regionMembers)
				inRegion->getPosition()->getMapRegion()->remove(*inRegion);
			for (const Ref<Player>& player : players) {
				player->setCasting(nullptr); // a cast in progress holds its caster (Skill.effector)
				player->setClientConnection(nullptr);
			}
		}
		regionMembers.clear();
		clientOf.clear();
		clients.clear();
		equipped.clear();
		learned.clear();
		if (npcDataPublished)
			dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		EffectWorldTest::TearDown();
	}

	/**
	 * A spawned Elyos player of the class and level with a FlyController and a client connection (EffectWorldTest.makePlayer builds level 0
	 * characters without either). The level is set before the Player is created: PlayerGameStats interns its stats template then.
	 */
	Ref<Player> player(int32_t objectId, gameserver::model::PlayerClass playerClass = gameserver::model::PlayerClass::MAGE, int32_t level = 1,
		float x = 500, float y = 500, float z = 100, gameserver::model::Race race = gameserver::model::Race::ELYOS) {
		namespace m = gameserver::model;
		Ref<m::account::Account> account = m::account::Account::create(9000 + objectId);
		Ref<m::gameobjects::player::PlayerCommonData> commonData = m::gameobjects::player::PlayerCommonData::create(objectId);
		commonData->setName("Mz" + std::to_string(objectId));
		commonData->setRace(race);
		commonData->setPlayerClass(playerClass);
		if (level >= 10)
			commonData->setDaeva(true); // only a daeva passes level 9 (setExp); PlayerCommonData.updateDaeva would ask the quest DAO
		commonData->setLevel(level);
		Ref<m::gameobjects::player::PlayerAppearance> appearance = m::gameobjects::player::PlayerAppearance::create();
		account->addPlayerAccountData(std::make_unique<m::account::PlayerAccountData>(*account, *commonData, *appearance));
		account->setAccountWarehouse(std::make_unique<m::items::storage::PlayerStorage>(*account, m::items::storage::StorageType::ACCOUNT_WAREHOUSE));
		Ref<Player> result = m::gameobjects::VisibleObject::create<Player>(*account->getPlayerAccountData(objectId), *account);
		result->setKnownlist(std::make_unique<world::knownlist::KnownList>(*result));
		result->setSkillList(m::skill::PlayerSkillList::create());
		result->setEffectController(std::make_unique<controllers::effect::PlayerEffectController>(*result));
		result->setFlyController(std::make_unique<controllers::FlyController>(*result));
		place(*result, x, y, z);
		accounts.push_back(account);
		commonDatas.push_back(commonData);
		appearances.push_back(appearance);
		players.push_back(result);
		auto client = std::make_unique<cp::TestClient>();
		client->enterWorld(*result, *account);
		clientOf.push_back({result.get(), client.get()});
		clients.push_back(std::move(client));
		return result;
	}

	/** A spawned level 1 MONSTER (mzMonsterTemplate) with the npc parts VisibleObjectSpawner gives it */
	Ref<Npc> monster(float x = 505, float y = 500, float z = 100) {
		Ref<gameserver::model::templates::spawns::SpawnGroup> group =
			gameserver::model::templates::spawns::SpawnGroup::create(effecttest::POETA, MZ_MONSTER, 0, nullptr);
		gameserver::model::templates::spawns::SpawnTemplate& spawnTemplate =
			group->addSpawnTemplate(std::make_unique<effecttest::EffectTestSpawnTemplate>(*group, x, y, z));
		Ref<Npc> npc = gameserver::model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawnTemplate,
			mzMonsterTemplate());
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		place(*npc, x, y, z);
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		return npc;
	}

	/**
	 * The two-sided known-list insert of the server's pair sites (EffectTestSupport.h's KnownListPairing), made while the player has no
	 * connection: the player's see() sends SM_NPC_INFO, whose serialization asks TownService, which loads from the database.
	 */
	void pair(Npc& npc, Player& p) {
		std::shared_ptr<network::aion::AionConnection> connected = p.getClientConnection();
		p.setClientConnection(nullptr);
		EXPECT_TRUE(KnownListPairing::pair(npc, p));
		p.setClientConnection(connected);
	}

	/** The connection of a player made by player() */
	cp::RecordingAionConnection& connection(Player& p) {
		for (const auto& [owner, client] : clientOf)
			if (owner == &p)
				return **client;
		throw std::logic_error("not a player of this fixture");
	}

	/** The packets of type P the player was sent since its connection was last cleared */
	template <class P>
	std::vector<std::vector<uint8_t>> sentTo(Player& p) {
		return skillengine::test::packetsOf<P>(connection(p).sentBytes());
	}

	void clearSent(Player& p) { connection(p).clearSent(); }

	const model::SkillTemplate* skillTemplate(int32_t skillId) { return dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId); }

	/** Java Skill.endCast's `new Effect(...)` and `effect.initialize()` (the templates' calculate) */
	Ref<Effect> calculated(int32_t skillId, Creature& effector, Creature& effected, int32_t level = 1) {
		Ref<Effect> effect = Effect::create(effector, Ptr<Creature>(effected), skillTemplate(skillId), level);
		effect->initialize();
		return effect;
	}

	/** calculated() followed by Java's `effect.applyEffect()` (applyEffect -> addToEffectedController -> startEffect) */
	Ref<Effect> applied(int32_t skillId, Creature& effector, Creature& effected, int32_t level = 1) {
		Ref<Effect> effect = calculated(skillId, effector, effected, level);
		effect->applyEffect();
		return effect;
	}

	/**
	 * 64029 on the effected: a root whose skill also carries a <paralyze>, so Effect.isParalyzeEffect answers true and
	 * EffectController.removeParalyzeEffects ends it. The root's calculate stands in for Effect.initialize's loop, which would calculate the
	 * <paralyze> as well (ParalyzeEffect, ported in M5b-3), and Effect.applyEffect / startEffect / endEffect then run the success effects only,
	 * i.e. the root: the cases see a root and no PARALYZE state.
	 */
	Ref<Effect> rootedWithParalyze(Creature& effector, Creature& effected) {
		Ref<Effect> effect = Effect::create(effector, Ptr<Creature>(effected), skillTemplate(64029), 1);
		effect->getEffectTemplates()[0]->calculate(*effect);
		EXPECT_TRUE(effect->isInSuccessEffects(1));
		effect->applyEffect();
		return effect;
	}

	/** Puts the player in the gliding state FlyController.onStopGliding looks for (isInGlidingState), not flying */
	static void glide(Player& p) {
		p.setFlyState(gameserver::model::gameobjects::state::FlyState::GLIDING);
		p.setState(gameserver::model::gameobjects::state::CreatureState::GLIDING);
	}

	/**
	 * Adds the player to the objects of its map region (World.spawn's MapRegion.add), which activates the region and its neighbours. place()
	 * leaves the regions empty, and NpcKnownList.update clears an npc's known list in an inactive region, so without this an npc moved by
	 * World.updatePosition forgets the players it was paired with and broadcasts to no one. TearDown takes the player out again.
	 */
	void addToRegion(Player& p) {
		p.getPosition()->getMapRegion()->add(p);
		regionMembers.push_back(&p);
	}

	/** Publishes NPC_DATA with POLYMORPH_NPCS_XML (PolymorphEffect's tribe lookup); TearDown forgets it */
	void publishPolymorphNpcs() {
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(npcDataContext, std::string(POLYMORPH_NPCS_XML)));
		npcDataPublished = true;
	}

	/**
	 * Equips a weapon or a shield as CriticalProcEffectTest.equipMainHand does (Equipment.onLoadHandler, the load path without packets): the
	 * player's skill list gains the skills the group requires, which checkAvailableEquipSkills asks for.
	 */
	void equip(Player& p, gameserver::model::templates::item::enums::ItemGroup group, int32_t itemId, int32_t itemObjId, int64_t slot) {
		namespace m = gameserver::model;
		std::vector<Ptr<m::skill::PlayerSkillEntry>> skillEntries;
		for (const Ptr<m::skill::PlayerSkillEntry>& known : p.getSkillList()->getAllSkills())
			skillEntries.push_back(known);
		for (int32_t skillId : m::templates::item::enums::getRequiredSkills(group)) {
			Ref<m::skill::PlayerSkillEntry> entry =
				m::skill::PlayerSkillEntry::create(skillId, 1, 0, m::gameobjects::Persistable_PersistentState::NOACTION);
			skillEntries.push_back(Ptr<m::skill::PlayerSkillEntry>(entry));
			learned.push_back(std::move(entry));
		}
		p.setSkillList(m::skill::PlayerSkillList::create(skillEntries));
		// a weapon carries the Training Sword's (100000094) attack type and weapon stats: SM_STATS_INFO, which every stat change sends, reads them
		const bool weapon = group != m::templates::item::enums::ItemGroup::SHIELD;
		const std::string xmlText = R"(<item_template id=")" + std::to_string(itemId) + R"(" item_group=")"
			+ std::string(xml::EnumTraits<m::templates::item::enums::ItemGroup>::names[static_cast<size_t>(group)]) + R"(")"
			+ (weapon ? R"( attack_type="PHYSICAL"><weapon_stats hit_count="2" attack_range="1500" parry="173" physical_accuracy="52" critical="50")"
						R"( attack_speed="1400" max_damage="20" min_damage="16"/></item_template>)"
					  : "/>");
		const m::templates::item::ItemTemplate* itemTemplate = xml::bindString<m::templates::item::ItemTemplate>(itemContext, xmlText).release();
		Ref<m::gameobjects::Item> item = m::gameobjects::Item::create(itemObjId, itemTemplate);
		item->setEquipmentSlot(slot);
		p.getEquipment().onLoadHandler(*item);
		equipped.push_back(std::move(item));
	}

	/** A seed whose first Rnd.chance() of the calling thread satisfies `accept` (the thread is left seeded with it, the stream not advanced) */
	template <class Predicate>
	uint64_t seedWhereFirstChance(Predicate accept) {
		for (uint64_t seed = 1; seed < 100000; ++seed) {
			Rnd::seedCurrentThreadForTests(seed);
			if (accept(Rnd::chance())) {
				Rnd::seedCurrentThreadForTests(seed);
				return seed;
			}
		}
		ADD_FAILURE() << "no seed found";
		return 0;
	}

	std::vector<std::unique_ptr<cp::TestClient>> clients;
	std::vector<std::pair<Player*, cp::TestClient*>> clientOf;
	std::vector<Player*> regionMembers;
	std::vector<Ref<gameserver::model::gameobjects::Item>> equipped;
	std::vector<Ref<gameserver::model::skill::PlayerSkillEntry>> learned;
	xml::LoadContext itemContext;
	xml::LoadContext npcDataContext;
	bool npcDataPublished = false;
};

} // namespace
} // namespace aion::gameserver::skillengine::effect::mztest
