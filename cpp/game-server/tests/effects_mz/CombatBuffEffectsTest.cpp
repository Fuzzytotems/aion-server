// P5-04, M5e stage 1, E-02 (m5e-plan.md §2.4, §15.4): the effect classes of the lane that change a character's next attacks, its cooldowns,
// its mount and its target - OneTimeBoostSkillAttackEffect (the Assassin's 3468 Killer's Eye at 10, the Ranger's stigma 813 Focused Shots,
// the Gunner's 2046 Stopping Power, the Chanter's 8406 Inspiring Mantra I Effect on another player), SkillCooltimeResetEffect (the Gunner's 2053 Reload), RideRobotEffect (the Rider's 2767 Embark at 10) and
// TargetChangeEffect (the Gladiator's 2981 Taunt at 10, the Assassin's 3236 Shimmerbomb).
//
// Each case drives a real Effect through calculate -> applyEffect -> startEffect -> endEffect (EffectsMzTestSupport.h) on the skill templates
// of skill_templates.xml, whose <effects> are copied verbatim below (without their <properties> and motions; 2383 keeps the <ride_robot/> use
// condition RideRobotEffect.endEffect looks for), and asserts what the Java bodies do: OneTimeBoostSkillAttackEffect.java:27-63 (the
// AttackCalcObserver $1 and the 100 ms removal of the lambda @L61:44), SkillCooltimeResetEffect.java:24-45, RideRobotEffect.java:19-52 (the
// ActionObserver(UNEQUIP) $1) and TargetChangeEffect.java:15-29.

#include "EffectsMzTestSupport.h"

#include <cstdint>
#include <string>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RIDE_ROBOT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_COOLDOWN.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/effect/OneTimeBoostSkillAttackEffect.h"
#include "aion/gameserver/skillengine/effect/RideRobotEffect.h"
#include "aion/gameserver/skillengine/effect/SkillCooltimeResetEffect.h"
#include "aion/gameserver/skillengine/effect/TargetChangeEffect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"

namespace aion::gameserver::skillengine::effect::mztest {
namespace {

using gameserver::model::PlayerClass;
using gameserver::model::Race;
using gameserver::model::gameobjects::Item;
using gameserver::model::gameobjects::VisibleObject;
using gameserver::model::gameobjects::player::detail::MAIN_HAND;
using network::aion::serverpackets::SM_RIDE_ROBOT;
using network::aion::serverpackets::SM_SKILL_COOLDOWN;

// ------------------------------------------------------------------------------------------------------------------------- skill templates

/**
 * The skills of the cases, their <effects> verbatim from skill_templates.xml: 3468 "Killer's Eye", 813 "Focused Shots", 2046 "Stopping Power",
 * 8406 "Inspiring Mantra I Effect" (the Chanter's mantra, cast on its target), 2053 "Reload" and 1957 "Gunshot" (the skill of the cooldown id 1802 it resets), 2767 "Embark" and 2383 "Aimbot Assist" (a buff with the
 * <ride_robot/> use condition), 2981 "Taunt" and 3236 "Shimmerbomb".
 */
constexpr const char* COMBAT_BUFF_SKILLS_XML =
	R"(<skill_template skill_id="3468" name="Killer's Eye" nameId="2287217" cooldownId="641" group="AS_STRIKINGINTENTION")"
	R"( stack="AS_STRIKINGINTENTION" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1")"
	R"( req_dispel_count="10" activation="ACTIVE" cooldown="300" duration="0" cancel_rate="20" hostile_type="INDIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<onetimeboostskillattack count="1" type="PHYSICAL" value="50" duration2="15000" effectid="177" e="1" noresist="true" hoptype="SKILLLV")"
	R"( hopb="248" />)"
	R"(<statup duration2="15000" effectid="108392" e="2" noresist="true" preeffect="1">)"
	R"(<change stat="PHYSICAL_ACCURACY" func="ADD" value="500" /><change stat="MAGICAL_ACCURACY" func="ADD" value="300" />)"
	R"(<change stat="STUN_RESISTANCE_PENETRATION" func="ADD" value="500" /></statup>)"
	R"(<statup duration2="15000" effectid="108393" e="3" noresist="true" preeffect="1">)"
	R"(<change stat="OPENAERIAL_RESISTANCE_PENETRATION" func="ADD" value="500" /><change stat="SPIN_RESISTANCE_PENETRATION" func="ADD" value="500" />)"
	R"(</statup></effects></skill_template>)"
	R"(<skill_template skill_id="813" name="Focused Shots" nameId="2287800" cooldownId="2022" group="SC_TRUESHOTMIND" stack="SC_TRUESHOTMIND")"
	R"( lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10")"
	R"( activation="ACTIVE" cooldown="10" duration="500" stigma="BASIC" cancel_rate="20" hostile_type="INDIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true" apply_casting_time_bonus="true"><effects>)"
	R"(<onetimeboostskillattack count="5" type="PHYSICAL" value="30" duration2="60000" effectid="177" e="1" noresist="true" hoptype="SKILLLV")"
	R"( hopb="684" />)"
	R"(<statdown duration2="60000" effectid="106842" e="2" noresist="true" element="FIRE" preeffect="1">)"
	R"(<change stat="PHYSICAL_DEFENSE" func="PERCENT" value="-30" /></statdown>)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="2046" name="Stopping Power" nameId="2286579" cooldownId="1817" group="GU_EMPOWERMAGIC" stack="GU_EMPOWERMAGIC")"
	R"( lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10")"
	R"( activation="ACTIVE" cooldown="606" cooldown_delta_lv="-6" duration="0" stigma="ADVANCED" cancel_rate="20" hostile_type="INDIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<onetimeboostskillattack count="5" type="MAGICAL" value="10" duration2="30000" effectid="131971" e="1" noresist="true" hoptype="SKILLLV")"
	R"( hopb="684" />)"
	R"(<statup duration2="8000" effectid="131972" e="2" noresist="true" preeffect="1"><change stat="PVP_ATTACK_RATIO" func="ADD" value="150" />)"
	R"(</statup></effects></skill_template>)"
	R"(<skill_template skill_id="8406" name="Inspiring Mantra I Effect" nameId="287830" stack="CH_CASTERSPRAISE_EFFECT" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="NONE" tslot="CHANT" activation="PROVOKED" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true"><effects>)"
	R"(<boostskillcastingtime duration2="6500" effectid="184061" e="1" noresist="true">)"
	R"(<change stat="BOOST_CASTING_TIME_ATTACK" func="PERCENT" value="20" /></boostskillcastingtime>)"
	R"(<onetimeboostskillattack count="255" type="MAGICAL" value="10" duration2="6500" effectid="177" e="2" noresist="true" preeffect="1" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="2053" name="Reload" nameId="2286586" cooldownId="1860" group="GU_FIRECHAINRELOAD" stack="GU_FIRECHAINRELOAD")"
	R"( lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="300" duration="0" cancel_rate="20")"
	R"( hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true" apply_casting_time_bonus="true"><effects>)"
	R"(<skillcooltimereset last_cd="1802" first_cd="1802" delta="100" e="1" noresist="true" hoptype="SKILLLV" hopb="407" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="1957" name="Gunshot" nameId="2286538" cooldownId="1802" group="EN_DOUBLEFIRECHAIN" stack="EN_DOUBLEFIRECHAIN")"
	R"( lvl="1" skilltype="PHYSICAL" skill_category="CHAIN_SKILL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="160")"
	R"( duration="0" ammospeed="40" cancel_rate="10" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true">)"
	R"(<effects><spellatkinstant value="195" e="1" accmod2="200" element="FIRE" hoptype="DAMAGE" /></effects></skill_template>)"
	R"(<skill_template skill_id="2767" name="Embark" nameId="2286860" cooldownId="1756" group="RI_SUMMONARMOR" stack="RI_SUMMONARMOR" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="BUFF" tslot="NOSHOW" dispel_category="BUFF" req_dispel_level="100" req_dispel_count="100")"
	R"( activation="TOGGLE" cooldown="100" duration="0" hostile_type="INDIRECT"><effects>)"
	R"(<riderobot effectid="936011" e="1" noresist="true" hoptype="SKILLLV" hopb="102" />)"
	R"(<weaponstatboost effectid="936022" e="2" noresist="true" preeffect="1"><change stat="ATTACK_RANGE" func="ADD" value="4000">)"
	R"(<conditions><weapon weapon="KEYBLADE" /></conditions></change></weaponstatboost>)"
	R"(<statup effectid="936013" e="3" noresist="true" preeffect="1" hoptype="SKILLLV">)"
	R"(<change stat="ABNORMAL_RESISTANCE_ALL" func="ADD" value="100" /><change stat="PARRY" func="ADD" value="100" />)"
	R"(<change stat="PHYSICAL_DEFENSE" func="ADD" value="150" /></statup>)"
	R"(<statup effectid="936014" e="4" noresist="true" preeffect="1">)"
	R"(<change stat="FEAR_RESISTANCE" func="ADD" value="-100" /><change stat="SLEEP_RESISTANCE" func="ADD" value="-100" />)"
	R"(<change stat="MAGICAL_ACCURACY" func="ADD" value="20" /></statup>)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="2383" name="Aimbot Assist" nameId="2286724" cooldownId="1700" group="RI_TUNESENSOR" stack="RI_TUNESENSOR" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE")"
	R"( cooldown="1836" cooldown_delta_lv="-36" duration="0" stigma="BASIC" cancel_rate="20" hostile_type="INDIRECT">)"
	R"(<useconditions><ride_robot /></useconditions><effects>)"
	R"(<statup duration2="60000" effectid="132691" e="1" noresist="true" hoptype="SKILLLV" hopb="658">)"
	R"(<change stat="MAGICAL_ACCURACY" func="ADD" value="150" /></statup>)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="2981" name="Taunt" nameId="2286947" cooldownId="125" group="WA_PROVOKE" stack="WA_PROVOKE" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="100" duration="0" cancel_rate="20" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<hostileup value="1" e="1" noresist="true" element="FIRE" hoptype="SKILLLV" hopb="14873" />)"
	R"(<targetchange delta="1" e="2" preeffect="1" preeffect_prob="20" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="3236" name="Shimmerbomb" nameId="2287076" cooldownId="803" group="AS_WIDENEWBLINDINGBURST")"
	R"( stack="AS_WIDENEWBLINDINGBURST" lvl="1" skilltype="MAGICAL" skill_category="PHYSICAL_DEBUFF" skillsubtype="DEBUFF" tslot="DEBUFF")"
	R"( dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE" cooldown="1836" cooldown_delta_lv="-36")"
	R"( duration="0" cancel_rate="20" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<blind value="60" duration2="3000" effectid="20107" e="1" basiclvl="20" accmod2="1000" element="EARTH" hoptype="SKILLLV" hopb="2934" />)"
	R"(<statdown duration2="3000" effectid="108022" e="2" noresist="true" element="EARTH" preeffect="1">)"
	R"(<change stat="MAGICAL_ACCURACY" func="ADD" value="-300" /></statdown>)"
	R"(<targetchange e="3" preeffect="1" />)"
	R"(</effects></skill_template>)"
	// The test templates. 64501: a boost of both skill types (like the Blessing: Successful Attack scrolls 13194.., type ALL, without their
	// other positions), two skills of 20 %; 64502: a <onetimeboostskillattack> without type (none in the data: Java's switch throws)
	R"(<skill_template skill_id="64501" name="mz all boost" nameId="1" stack="MZ_ALL_BOOST" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
	R"( tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
	R"(<onetimeboostskillattack count="2" type="ALL" value="20" duration2="5000" e="1" noresist="true" /></effects></skill_template>)"
	R"(<skill_template skill_id="64502" name="mz untyped boost" nameId="1" stack="MZ_UNTYPED_BOOST" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
	R"( tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
	R"(<onetimeboostskillattack count="1" value="20" duration2="5000" e="1" noresist="true" /></effects></skill_template>)"
	// 64505: Killer's Eye's boost with a <change> (none of the 10 <onetimeboostskillattack> of the data has one), so BufEffect.startEffect's
	// modifiers show: MAXHP + 100 while the boost lasts
	R"(<skill_template skill_id="64505" name="mz boost with change" nameId="1" stack="MZ_CHANGE_BOOST" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="BUFF" tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
	R"(<onetimeboostskillattack count="1" type="PHYSICAL" value="50" duration2="15000" e="1" noresist="true">)"
	R"(<change stat="MAXHP" func="ADD" value="100" /></onetimeboostskillattack></effects></skill_template>)"
	// 64503, 64504: cooldown resets of the ids 1802..1803 by a flat 3,000 ms (delta 0) and by a delta of 50, which Java's int division
	// `delta / 100` makes 0
	R"(<skill_template skill_id="64503" name="mz flat reset" nameId="1" stack="MZ_FLAT_RESET" lvl="1" skilltype="MAGICAL" skillsubtype="NONE")"
	R"( tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
	R"(<skillcooltimereset first_cd="1802" last_cd="1803" value="3000" e="1" noresist="true" /></effects></skill_template>)"
	R"(<skill_template skill_id="64504" name="mz half reset" nameId="1" stack="MZ_HALF_RESET" lvl="1" skilltype="MAGICAL" skillsubtype="NONE")"
	R"( tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
	R"(<skillcooltimereset first_cd="1802" last_cd="1802" delta="50" value="3000" e="1" noresist="true" /></effects></skill_template>)";

class CombatBuffEffectsTest : public EffectsMzTest {
protected:
	void SetUp() override {
		EffectsMzTest::SetUp();
		EFFECT_TEST_SCOPE;
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the base published the lane's templates; the holder is immortal, only forgotten
		publishSkillData(effectsMzSkills() + COMBAT_BUFF_SKILLS_XML);
	}

	/** Adds the skill (level 1) to the player's skill list, as the learn path would (the fixture's equip() shape) */
	void learn(Player& p, int32_t skillId) {
		namespace m = gameserver::model;
		std::vector<Ptr<m::skill::PlayerSkillEntry>> skillEntries;
		for (const Ptr<m::skill::PlayerSkillEntry>& known : p.getSkillList()->getAllSkills())
			skillEntries.push_back(known);
		Ref<m::skill::PlayerSkillEntry> entry = m::skill::PlayerSkillEntry::create(skillId, 1, 0, m::gameobjects::Persistable_PersistentState::NOACTION);
		skillEntries.push_back(Ptr<m::skill::PlayerSkillEntry>(entry));
		learned.push_back(std::move(entry));
		p.setSkillList(m::skill::PlayerSkillList::create(skillEntries));
	}

	/**
	 * Equips a keyblade whose template carries `robot` (102100181 Cipher-Blade for Training has robot="2500002", item_templates.xml:177237) in
	 * the main hand, the way the fixture's equip() does (Equipment.onLoadHandler, the skills the group requires learned first)
	 */
	Ref<Item> equipKeyblade(Player& p, int32_t robotId, int32_t itemObjId) {
		namespace m = gameserver::model;
		for (int32_t skillId : m::templates::item::enums::getRequiredSkills(m::templates::item::enums::ItemGroup::KEYBLADE))
			learn(p, skillId);
		const std::string xmlText = R"(<item_template id="102100181" item_group="KEYBLADE" attack_type="MAGICAL_EARTH" robot=")"
			+ std::to_string(robotId) + R"("><weapon_stats hit_count="1" attack_range="2000" boost_magical_skill="20" magical_accuracy="54" parry="213")"
			R"( critical="30" attack_speed="2400" max_damage="31" min_damage="29"/></item_template>)";
		const m::templates::item::ItemTemplate* itemTemplate = xml::bindString<m::templates::item::ItemTemplate>(itemContext, xmlText).release();
		Ref<Item> item = Item::create(itemObjId, itemTemplate);
		item->setEquipmentSlot(MAIN_HAND);
		p.getEquipment().onLoadHandler(*item);
		equipped.push_back(item);
		return item;
	}

	/** An armor item (a shield: EquipType.ARMOR), not equipped */
	Ref<Item> shieldItem(int32_t itemObjId) {
		const gameserver::model::templates::item::ItemTemplate* itemTemplate = xml::bindString<gameserver::model::templates::item::ItemTemplate>(
			itemContext, R"(<item_template id="115000001" item_group="SHIELD"/>)")
			.release();
		Ref<Item> item = Item::create(itemObjId, itemTemplate);
		equipped.push_back(item);
		return item;
	}

	/** SM_RIDE_ROBOT as SM_RIDE_ROBOT.java:23-26 writes it: the rider's object id and the robot id */
	std::pair<int32_t, int32_t> decodeRideRobot(const std::vector<uint8_t>& bytes) {
		network::test::PacketReader reader(cp::bodyOf(bytes));
		int32_t objectId = reader.D();
		int32_t robotId = reader.D();
		EXPECT_EQ(reader.remaining(), 0u) << "SM_RIDE_ROBOT consumed exactly";
		return {objectId, robotId};
	}
};

// ---- OneTimeBoostSkillAttackEffect (OneTimeBoostSkillAttackEffect.java:18-64) ---------------------------------------------------------------

/**
 * 3468 Killer's Eye (the Assassin's level 10 buff: count 1, PHYSICAL, value 50): BufEffect.startEffect, then an AttackCalcObserver with
 * percent = 1.0f + 50 / 100.0f = 1.5f on the effected. Its getBasePhysicalDamageMultiplier(isSkill) answers 1.5f for the first skill only:
 * an auto attack (isSkill false) and a magical skill (type PHYSICAL) are answered 1.0f without being counted (the && short-circuits before
 * `boostCount++`); the boost that reaches the count schedules removeEffect's task, which removes the effect 100 ms later
 * (ThreadPoolManager.schedule(..., 100)); a skill in between is no longer boosted.
 */
TEST_F(CombatBuffEffectsTest, KillersEyeBoostsTheNextPhysicalSkillByHalfOnce) {
	EFFECT_TEST_SCOPE;
	Ref<Player> assassin = player(8101, PlayerClass::SCOUT);
	ASSERT_FALSE(assassin->getObserveController()->hasObservers());
	Ref<Effect> effect = applied(3468, *assassin, *assassin);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	ASSERT_NE(dynamic_cast<const OneTimeBoostSkillAttackEffect*>(effect->getEffectTemplates()[0]), nullptr);
	EXPECT_EQ(effect->getDuration(), 15000);
	controllers::ObserveController& observers = *assassin->getObserveController();
	EXPECT_TRUE(observers.hasObservers()) << "the AttackCalcObserver";

	EXPECT_EQ(observers.getBasePhysicalDamageMultiplier(false), 1.0f) << "an auto attack is not boosted";
	EXPECT_EQ(observers.getBaseMagicalDamageMultiplier(), 1.0f) << "a PHYSICAL boost leaves magical skills";
	EXPECT_EQ(observers.getBasePhysicalDamageMultiplier(true), 1.5f) << "the first physical skill (neither above was counted)";
	EXPECT_TRUE(assassin->getEffectController()->hasAbnormalEffect(3468)) << "removed by a task, not at once";
	EXPECT_EQ(observers.getBasePhysicalDamageMultiplier(true), 1.0f) << "the count is used up";
	advance(99);
	EXPECT_TRUE(assassin->getEffectController()->hasAbnormalEffect(3468));
	advance(1);
	EXPECT_FALSE(assassin->getEffectController()->hasAbnormalEffect(3468)) << "removeEffect(effect.getSkillId()) after 100 ms";
	EXPECT_FALSE(effect->isEndedByTime());
	EXPECT_FALSE(observers.hasObservers()) << "the observer went with the effect";
}

/**
 * 813 Focused Shots (count 5, PHYSICAL, 30 %): five physical skills are boosted by 1.0f + 30 / 100.0f; only the fifth schedules the removal
 * (`boostCount == count` after the increment), so the buff survives the first four for as long as it lasts.
 */
TEST_F(CombatBuffEffectsTest, FocusedShotsBoostsFivePhysicalSkillsAndGoesAfterTheFifth) {
	EFFECT_TEST_SCOPE;
	Ref<Player> ranger = player(8111, PlayerClass::SCOUT);
	applied(813, *ranger, *ranger);
	controllers::ObserveController& observers = *ranger->getObserveController();
	const float percent = 1.0f + 30 / 100.0f;
	for (int32_t shot = 1; shot <= 4; ++shot) {
		EXPECT_EQ(observers.getBasePhysicalDamageMultiplier(true), percent) << shot;
		advance(500);
		EXPECT_TRUE(ranger->getEffectController()->hasAbnormalEffect(813)) << shot;
	}
	EXPECT_EQ(observers.getBasePhysicalDamageMultiplier(true), percent) << "the fifth";
	advance(100);
	EXPECT_FALSE(ranger->getEffectController()->hasAbnormalEffect(813));
	EXPECT_EQ(observers.getBasePhysicalDamageMultiplier(true), 1.0f);
}

/**
 * 2046 Stopping Power (count 5, MAGICAL, 10 %): physical skills are not boosted (`type != SkillType.MAGICAL` is false) and not counted; five
 * magical skills are boosted by 1.0f + 10 / 100.0f.
 */
TEST_F(CombatBuffEffectsTest, StoppingPowerBoostsMagicalSkillsOnly) {
	EFFECT_TEST_SCOPE;
	Ref<Player> gunner = player(8121, PlayerClass::ENGINEER);
	applied(2046, *gunner, *gunner);
	controllers::ObserveController& observers = *gunner->getObserveController();
	for (int32_t i = 0; i < 6; ++i)
		EXPECT_EQ(observers.getBasePhysicalDamageMultiplier(true), 1.0f) << i;
	for (int32_t i = 1; i <= 5; ++i)
		EXPECT_EQ(observers.getBaseMagicalDamageMultiplier(), 1.0f + 10 / 100.0f) << i;
	EXPECT_EQ(observers.getBaseMagicalDamageMultiplier(), 1.0f) << "the sixth";
	advance(100);
	EXPECT_FALSE(gunner->getEffectController()->hasAbnormalEffect(2046));
}

/**
 * The boost reaches the damage: AttackUtil.calculateSkillResult multiplies a magical skill's damage by
 * ObserveController.getBaseMagicalDamageMultiplier (for templates whose shouldUseOneTimeBoostSkillAttack is true). 1282 Flame Bolt's golden
 * 152 (DamageEffectsTest: 162 after knowledge and BOOST_SPELL_ATTACK, minus 10 of mdef) becomes (int) (152 * 1.1f) = (int) 167.2f = 167 under
 * Stopping Power; the next Flame Bolt after the buff has gone is 152 again.
 */
TEST_F(CombatBuffEffectsTest, StoppingPowerRaisesFlameBoltsDamageByATenth) {
	EFFECT_TEST_SCOPE;
	Ref<Player> mage = player(8131);
	Ref<Npc> npc = monster();
	skillengine::test::addStat(*npc, StatEnum::MAGICAL_CRITICAL_RESIST, 1000); // no magical critical can be rolled against it
	ASSERT_EQ(mage->getGameStats()->getKnowledge()->getCurrent(), 115);
	ASSERT_EQ(npc->getGameStats()->getMDef()->getCurrent(), 100);
	EXPECT_EQ(calculated(1282, *mage, *npc)->getReserveds(1)->getValue(), 152) << "without the boost";
	applied(2046, *mage, *mage);
	EXPECT_EQ(calculated(1282, *mage, *npc)->getReserveds(1)->getValue(), 167);
	for (int32_t i = 0; i < 4; ++i)
		calculated(1282, *mage, *npc);
	advance(100);
	ASSERT_FALSE(mage->getEffectController()->hasAbnormalEffect(2046));
	EXPECT_EQ(calculated(1282, *mage, *npc)->getReserveds(1)->getValue(), 152);
}

/** 64501 (type ALL, count 2, 20 %): physical and magical skills are boosted and share one count */
TEST_F(CombatBuffEffectsTest, AnAllTypeBoostCountsPhysicalAndMagicalSkillsTogether) {
	EFFECT_TEST_SCOPE;
	Ref<Player> gunner = player(8141, PlayerClass::ENGINEER);
	applied(64501, *gunner, *gunner);
	controllers::ObserveController& observers = *gunner->getObserveController();
	EXPECT_EQ(observers.getBasePhysicalDamageMultiplier(true), 1.0f + 20 / 100.0f);
	EXPECT_EQ(observers.getBaseMagicalDamageMultiplier(), 1.0f + 20 / 100.0f);
	EXPECT_EQ(observers.getBasePhysicalDamageMultiplier(true), 1.0f);
	EXPECT_EQ(observers.getBaseMagicalDamageMultiplier(), 1.0f);
	advance(100);
	EXPECT_FALSE(gunner->getEffectController()->hasAbnormalEffect(64501));
}

/**
 * A template without type (64502): Java's `switch (type)` unboxes null, a NullPointerException after BufEffect.startEffect; no observer is
 * added. (startEffect is called on the template itself here: Effect.applyEffect would wrap the exception.)
 */
TEST_F(CombatBuffEffectsTest, ABoostWithoutTypeThrowsInStartEffect) {
	EFFECT_TEST_SCOPE;
	Ref<Player> gunner = player(8151, PlayerClass::ENGINEER);
	Ref<Effect> effect = calculated(64502, *gunner, *gunner);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_THROW(effect->getEffectTemplates()[0]->startEffect(*effect), runtime::NullPointerException);
	EXPECT_FALSE(gunner->getObserveController()->hasObservers());
}

/**
 * 8406 Inspiring Mantra I Effect, which the Chanter's mantra casts on another player (first_target TARGET): position 2's AttackCalcObserver goes
 * to the effected (`effect.addObserver(effect.getEffected(), ...)`), so the mage's next 255 magical skills are boosted by 1.0f + 10 / 100.0f and
 * the caster gets no observer at all. The 255th boost schedules the removal, whose task takes the effect off the effected's EffectController
 * (`effect.getEffected().getEffectController().removeEffect(effect.getSkillId())`) 100 ms later.
 */
TEST_F(CombatBuffEffectsTest, InspiringMantraBoostsTheTargetsMagicalSkillsNotTheCasters) {
	EFFECT_TEST_SCOPE;
	Ref<Player> chanter = player(8161, PlayerClass::PRIEST);
	Ref<Player> mage = player(8162, PlayerClass::MAGE, 1, 505, 500, 100);
	Ref<Effect> effect = applied(8406, *chanter, *mage);
	ASSERT_TRUE(effect->isInSuccessEffects(2));
	ASSERT_NE(dynamic_cast<const OneTimeBoostSkillAttackEffect*>(effect->getEffectTemplates()[1]), nullptr);
	ASSERT_EQ(effect->getEffected().get(), mage.get());
	EXPECT_TRUE(mage->getEffectController()->hasAbnormalEffect(8406));
	EXPECT_TRUE(mage->getObserveController()->hasObservers()) << "the AttackCalcObserver is the effected's";
	EXPECT_FALSE(chanter->getObserveController()->hasObservers()) << "not the caster's";
	EXPECT_EQ(chanter->getObserveController()->getBaseMagicalDamageMultiplier(), 1.0f);

	controllers::ObserveController& observers = *mage->getObserveController();
	int32_t boosted = 0;
	for (int32_t i = 0; i < 255; ++i)
		if (observers.getBaseMagicalDamageMultiplier() == 1.0f + 10 / 100.0f)
			++boosted;
	EXPECT_EQ(boosted, 255);
	EXPECT_EQ(observers.getBaseMagicalDamageMultiplier(), 1.0f) << "the 256th";
	advance(99);
	EXPECT_TRUE(mage->getEffectController()->hasAbnormalEffect(8406));
	advance(1);
	EXPECT_FALSE(mage->getEffectController()->hasAbnormalEffect(8406)) << "removed from the effected's EffectController";
	EXPECT_FALSE(observers.hasObservers());
}

/**
 * OneTimeBoostSkillAttackEffect.startEffect begins with super.startEffect (BufEffect.startEffect adds the template's <change> modifiers to the
 * effected), which no template of the data shows. 64505 is Killer's Eye's boost with MAXHP + 100: the stat rises with the boost and falls again
 * when the used-up boost's removal ends the effect (BufEffect.endEffect, which the class does not override).
 */
TEST_F(CombatBuffEffectsTest, TheBoostAddsItsChangesAsABufEffect) {
	EFFECT_TEST_SCOPE;
	Ref<Player> assassin = player(8171, PlayerClass::SCOUT);
	const int32_t maxHp = assassin->getGameStats()->getMaxHp()->getCurrent();
	Ref<Effect> effect = applied(64505, *assassin, *assassin);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(assassin->getGameStats()->getMaxHp()->getCurrent(), maxHp + 100) << "BufEffect.startEffect";
	EXPECT_EQ(assassin->getObserveController()->getBasePhysicalDamageMultiplier(true), 1.5f);
	advance(100);
	EXPECT_FALSE(assassin->getEffectController()->hasAbnormalEffect(64505));
	EXPECT_EQ(assassin->getGameStats()->getMaxHp()->getCurrent(), maxHp) << "BufEffect.endEffect";
}

// ---- SkillCooltimeResetEffect (SkillCooltimeResetEffect.java:18-46) ---------------------------------------------------------------------------

/**
 * 2053 Reload (delta 100, cooldown ids 1802..1802): the running cooldown of 1802 (1957 Gunshot's) has `delay -= delay * (100 / 100)`, i.e. it
 * ends now (setSkillCoolDown(1802, 0 + now)), and the gunner is sent SM_SKILL_COOLDOWN(player, {1802: now}, true): the skills of its skill list
 * with that cooldown id - Gunshot, 0 seconds left, its cooldown 160 * 100 = 16,000 ms as the animation's duration, with the notification.
 */
TEST_F(CombatBuffEffectsTest, ReloadEndsTheGunshotCooldown) {
	EFFECT_TEST_SCOPE;
	Ref<Player> gunner = player(8201, PlayerClass::ENGINEER);
	learn(*gunner, 1957);
	const int64_t before = commons::utils::currentTimeMillis();
	gunner->setSkillCoolDown(1802, before + 12000);
	clearSent(*gunner);

	Ref<Effect> effect = applied(2053, *gunner, *gunner);
	const int64_t after = commons::utils::currentTimeMillis();
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	ASSERT_NE(dynamic_cast<const SkillCooltimeResetEffect*>(effect->getEffectTemplates()[0]), nullptr);
	EXPECT_GE(gunner->getSkillCoolDown(1802), before) << "delay 0 + now";
	EXPECT_LE(gunner->getSkillCoolDown(1802), after);
	std::vector<std::vector<uint8_t>> packets = sentTo<SM_SKILL_COOLDOWN>(*gunner);
	ASSERT_EQ(packets.size(), 1u);
	network::test::PacketReader reader(cp::bodyOf(packets[0]));
	EXPECT_EQ(reader.H(), 1) << "one skill of the list has the cooldown id";
	EXPECT_EQ(reader.C(), 1) << "notify";
	EXPECT_EQ(static_cast<uint16_t>(reader.H()), 1957);
	EXPECT_EQ(reader.D(), 0) << "no seconds left";
	EXPECT_EQ(reader.D(), 16000);
	EXPECT_EQ(reader.remaining(), 0u);
}

/**
 * 64503 (delta 0, value 3000, ids 1802..1803): `delay -= value` takes 3,000 ms off the running 1802 (from 10,000 ms to 7,000); 1803 does not
 * run (`delay <= 0: continue`) and is neither set nor sent. 64504 (delta 50): `delay * (delta / 100)` is delay * 0 in Java's int division, so
 * the cooldown keeps its end - and is still sent, since it was running. On a monster the cooldown changes all the same (no packet).
 */
TEST_F(CombatBuffEffectsTest, ACooldownResetTakesItsValueOffOrItsDeltaInWholeHundreds) {
	EFFECT_TEST_SCOPE;
	Ref<Player> gunner = player(8211, PlayerClass::ENGINEER);
	learn(*gunner, 1957);
	int64_t start = commons::utils::currentTimeMillis();
	const int64_t end = start + 10000;
	gunner->setSkillCoolDown(1802, end);
	clearSent(*gunner);
	applied(64503, *gunner, *gunner);
	int64_t elapsed = commons::utils::currentTimeMillis() - start;
	EXPECT_GE(gunner->getSkillCoolDown(1802), end - 3000) << "(end - now1) - 3000 + now2";
	EXPECT_LE(gunner->getSkillCoolDown(1802), end - 3000 + elapsed);
	EXPECT_EQ(gunner->getSkillCoolDown(1803), 0) << "a cooldown that is not running is skipped";
	ASSERT_EQ(sentTo<SM_SKILL_COOLDOWN>(*gunner).size(), 1u);

	const int64_t reduced = gunner->getSkillCoolDown(1802);
	start = commons::utils::currentTimeMillis();
	clearSent(*gunner);
	applied(64504, *gunner, *gunner);
	elapsed = commons::utils::currentTimeMillis() - start;
	EXPECT_GE(gunner->getSkillCoolDown(1802), reduced) << "50 / 100 == 0: nothing taken off";
	EXPECT_LE(gunner->getSkillCoolDown(1802), reduced + elapsed);
	EXPECT_EQ(sentTo<SM_SKILL_COOLDOWN>(*gunner).size(), 1u);

	gunner->setSkillCoolDown(1802, commons::utils::currentTimeMillis() - 1);
	clearSent(*gunner);
	applied(64503, *gunner, *gunner);
	EXPECT_TRUE(sentTo<SM_SKILL_COOLDOWN>(*gunner).empty()) << "nothing was reset: no packet";

	Ref<Npc> npc = monster();
	start = commons::utils::currentTimeMillis();
	npc->setSkillCoolDown(1802, start + 10000);
	applied(64503, *gunner, *npc);
	EXPECT_LE(npc->getSkillCoolDown(1802), start + 7000 + (commons::utils::currentTimeMillis() - start));
	EXPECT_GE(npc->getSkillCoolDown(1802), start + 7000);
}

// ---- RideRobotEffect (RideRobotEffect.java:17-53) -------------------------------------------------------------------------------------------

/**
 * 2767 Embark (the Rider's level 10 toggle): startEffect takes the robot id of the main-hand weapon's skin (the keyblade's robot 2500002), sends
 * SM_RIDE_ROBOT(rider, 2500002) to the rider and those who see it, and adds the UNEQUIP observer. Taking off armor keeps the ride; taking off
 * a weapon ends the effect (effect.endEffect()), whose endEffect sets robot id 0, sends SM_RIDE_ROBOT(rider, 0) and ends every abnormal effect
 * whose skill has a <ride_robot/> use condition (2383 Aimbot Assist) - not the others (64004).
 */
TEST_F(CombatBuffEffectsTest, EmbarkRidesTheKeybladesRobotUntilTheWeaponIsTakenOff) {
	EFFECT_TEST_SCOPE;
	Ref<Player> rider = player(8301, PlayerClass::ENGINEER);
	Ref<Item> keyblade = equipKeyblade(*rider, 2500002, 83011);
	ASSERT_TRUE(rider->getEquipment().getMainHandWeapon());
	ASSERT_EQ(rider->getRobotId(), 0);
	clearSent(*rider);

	Ref<Effect> embark = applied(2767, *rider, *rider);
	ASSERT_TRUE(embark->isInSuccessEffects(1));
	ASSERT_NE(dynamic_cast<const RideRobotEffect*>(embark->getEffectTemplates()[0]), nullptr);
	EXPECT_EQ(rider->getRobotId(), 2500002);
	EXPECT_TRUE(rider->isInRobotMode());
	std::vector<std::vector<uint8_t>> rides = sentTo<SM_RIDE_ROBOT>(*rider);
	ASSERT_EQ(rides.size(), 1u);
	EXPECT_EQ(decodeRideRobot(rides[0]), std::make_pair(rider->getObjectId(), 2500002));
	EXPECT_TRUE(rider->getObserveController()->hasObservers()) << "the UNEQUIP observer";

	Ref<Effect> aimbot = applied(2383, *rider, *rider);
	ASSERT_NE(aimbot->getSkillTemplate()->getRideRobotCondition(), nullptr);
	Ref<Effect> other = applied(64004, *rider, *rider);
	ASSERT_EQ(other->getSkillTemplate()->getRideRobotCondition(), nullptr);
	clearSent(*rider);

	rider->getObserveController()->notifyItemUnEquip(*shieldItem(83012), *rider);
	EXPECT_TRUE(rider->getEffectController()->hasAbnormalEffect(2767)) << "an armor item: the ride goes on";
	EXPECT_EQ(rider->getRobotId(), 2500002);

	rider->getObserveController()->notifyItemUnEquip(*keyblade, *rider);
	EXPECT_FALSE(rider->getEffectController()->hasAbnormalEffect(2767)) << "a weapon: effect.endEffect()";
	EXPECT_EQ(rider->getRobotId(), 0) << "player.setRobotId(0)";
	EXPECT_FALSE(rider->isInRobotMode());
	rides = sentTo<SM_RIDE_ROBOT>(*rider);
	ASSERT_EQ(rides.size(), 1u);
	EXPECT_EQ(decodeRideRobot(rides[0]), std::make_pair(rider->getObjectId(), 0));
	EXPECT_FALSE(rider->getEffectController()->hasAbnormalEffect(2383)) << "the skills that need the robot end with it";
	EXPECT_TRUE(rider->getEffectController()->hasAbnormalEffect(64004)) << "the others stay";
}

/**
 * startEffect reads the robot of the weapon's skin, getItemSkinTemplate().getRobotId() (RideRobotEffect.java:25): a keyblade whose own template
 * has robot 2500002 (102100181 Cipher-Blade for Training), reskinned as 102100017 Worthy Noble Titanium Cipher-Blade (robot="2500004",
 * item_templates.xml:175658), rides robot 2500004 and sends it in SM_RIDE_ROBOT.
 */
TEST_F(CombatBuffEffectsTest, AReskinnedKeybladeRidesTheRobotOfItsSkin) {
	EFFECT_TEST_SCOPE;
	Ref<Player> rider = player(8321, PlayerClass::ENGINEER);
	Ref<Item> keyblade = equipKeyblade(*rider, 2500002, 83021);
	keyblade->setItemSkinTemplate(xml::bindString<gameserver::model::templates::item::ItemTemplate>(itemContext,
		R"(<item_template id="102100017" item_group="KEYBLADE" attack_type="MAGICAL_EARTH" robot="2500004"><weapon_stats hit_count="1")"
		R"( attack_range="2000" boost_magical_skill="370" magical_accuracy="232" parry="521" critical="30" attack_speed="2400" max_damage="165")"
		R"( min_damage="158"/></item_template>)")
			.release());
	ASSERT_TRUE(keyblade->isSkinnedItem());
	ASSERT_EQ(keyblade->getItemTemplate()->getRobotId(), 2500002);
	clearSent(*rider);

	Ref<Effect> embark = applied(2767, *rider, *rider);
	ASSERT_TRUE(embark->isInSuccessEffects(1));
	EXPECT_EQ(rider->getRobotId(), 2500004) << "the skin's robot, not the item's 2500002";
	std::vector<std::vector<uint8_t>> rides = sentTo<SM_RIDE_ROBOT>(*rider);
	ASSERT_EQ(rides.size(), 1u);
	EXPECT_EQ(decodeRideRobot(rides[0]), std::make_pair(rider->getObjectId(), 2500004));
}

/** The ride of a Player only: `(Player) effect.getEffected()` in startEffect and endEffect is a ClassCastException for a monster */
TEST_F(CombatBuffEffectsTest, OnlyAPlayerRides) {
	EFFECT_TEST_SCOPE;
	Ref<Player> rider = player(8311, PlayerClass::ENGINEER);
	Ref<Npc> npc = monster();
	Ref<Effect> effect = Effect::create(*rider, Ptr<Creature>(npc), skillTemplate(2767), 1);
	EXPECT_THROW(effect->getEffectTemplates()[0]->startEffect(*effect), runtime::ClassCastException);
	EXPECT_THROW(effect->getEffectTemplates()[0]->endEffect(*effect), runtime::ClassCastException);
}

// ---- TargetChangeEffect (TargetChangeEffect.java:13-30) -------------------------------------------------------------------------------------

/**
 * 2981 Taunt's position 2 (<targetchange delta="1">): a taunted player targets the taunter (`case 1: target = effect.getEffector()`); a
 * taunted monster keeps its target (only a Player's is set). The template is applied on its own: position 1 of Taunt is <hostileup>
 * (HostileUpEffect, P5-03, the effects-al lane's), and applyEffect is the class's only body.
 */
TEST_F(CombatBuffEffectsTest, TauntMakesThePlayerTargetTheTaunter) {
	EFFECT_TEST_SCOPE;
	Ref<Player> gladiator = player(8401, PlayerClass::WARRIOR);
	Ref<Player> enemy = player(8402, PlayerClass::MAGE, 1, 505, 500, 100, Race::ASMODIANS);
	Ref<Npc> npc = monster(510, 500, 100);
	enemy->setTarget(Ptr<VisibleObject>(npc));
	Ref<Effect> effect = Effect::create(*gladiator, Ptr<Creature>(enemy), skillTemplate(2981), 1);
	const EffectTemplate* targetChange = effect->getEffectTemplates()[1];
	ASSERT_NE(dynamic_cast<const TargetChangeEffect*>(targetChange), nullptr);
	targetChange->applyEffect(*effect);
	EXPECT_EQ(enemy->getTarget().get(), gladiator.get()) << "player.setTarget(effect.getEffector())";

	Ref<Npc> taunted = monster(505, 505, 100);
	taunted->setTarget(Ptr<VisibleObject>(npc));
	Ref<Effect> onNpc = Effect::create(*gladiator, Ptr<Creature>(taunted), skillTemplate(2981), 1);
	onNpc->getEffectTemplates()[1]->applyEffect(*onNpc);
	EXPECT_EQ(taunted->getTarget().get(), npc.get()) << "a monster's target is not changed";
}

/**
 * 3236 Shimmerbomb end to end (a monster's cast at a player here): <blind> and <statdown> land, and position 3 (<targetchange>, delta 0:
 * "Shimmerbomb sets target to null") clears the player's target.
 */
TEST_F(CombatBuffEffectsTest, ShimmerbombClearsThePlayersTarget) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = monster(500, 500, 100);
	Ref<Player> target = player(8411, PlayerClass::MAGE, 1, 505, 500, 100);
	target->setTarget(Ptr<VisibleObject>(npc));
	Ref<Effect> effect = calculated(3236, *npc, *target);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	ASSERT_TRUE(effect->isInSuccessEffects(3));
	ASSERT_NE(dynamic_cast<const TargetChangeEffect*>(effect->getEffectTemplates()[2]), nullptr);
	effect->applyEffect();
	EXPECT_FALSE(target->getTarget()) << "player.setTarget(null)";
	EXPECT_TRUE(target->getEffectController()->isAbnormalSet(AbnormalState::BLIND));
}

} // namespace
} // namespace aion::gameserver::skillengine::effect::mztest
