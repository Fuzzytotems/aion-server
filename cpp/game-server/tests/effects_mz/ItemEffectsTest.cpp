// P5-04, M5b-3 stage 1, items E-01 and E-04 (m5b3-plan.md §2.5, §5): the effect classes of the starter items and the potions -
// ProcHealInstantEffect (every life potion, 162000002 Minor Life Potion's skill 9889), ProcMPHealInstantEffect (every mana potion, 162000007's
// 9894), XPBoostEffect (the starter Lodas Amulet's 10249) and the two penalty flags of the starter Administrator's Boon (10350):
// NoDeathPenaltyEffect and NoResurrectPenaltyEffect (its third position, HiPassEffect, is P5-03's: tests/effects_al/StateEffectsTest.cpp).
//
// Each case drives a real Effect through calculate -> applyEffect -> startEffect -> endEffect (EffectsMzTestSupport.h) on the skill templates of
// skill_templates.xml, whose <effects> are copied verbatim below (their <properties>, conditions and motions left out: no Effect reads them),
// and asserts what the Java bodies do: ProcHealInstantEffect.java:17-40 and ProcMPHealInstantEffect.java:17-35 over
// AbstractHealEffect.java:27-87 (the reserve capped at the missing HP or MP, and the item heal's TYPE.HP / TYPE.MP on the wire where a skill heal
// sends TYPE.REGULAR / TYPE.HEAL_MP), XPBoostEffect.java:16-19, NoDeathPenaltyEffect.java:7-10 and NoResurrectPenaltyEffect.java:7-10 over
// BufEffect.java:33-90. The heal values are template constants: no heal boost for an item heal (allowHpHealBoost false) and no randomness.

#include "EffectsMzTestSupport.h"

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/effect/NoDeathPenaltyEffect.h"
#include "aion/gameserver/skillengine/effect/NoResurrectPenaltyEffect.h"
#include "aion/gameserver/skillengine/effect/ProcHealInstantEffect.h"
#include "aion/gameserver/skillengine/effect/ProcMPHealInstantEffect.h"
#include "aion/gameserver/skillengine/effect/XPBoostEffect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"

namespace aion::gameserver::skillengine::effect::mztest {
namespace {

using network::aion::serverpackets::SM_ATTACK_STATUS;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

// ------------------------------------------------------------------------------------------------------------------------- skill templates

/**
 * The item skills, their <effects> verbatim from skill_templates.xml: 9889 "Healing" (:91905-91917, the skill of 162000002 Minor Life Potion,
 * item_templates.xml:830724-830729), 9894 "Mana Treatment" (:91970-91982, 162000007 Minor Mana Potion, item_templates.xml:830754-830759),
 * 10249 "Experience Boost" (:97982-97998, 169620005 [Event] Lodas Amulet III, item_templates.xml:857862-857867), 10350 "Administrator's Boon"
 * (:99663-99677, 164002039, item_templates.xml:834664-834669), 8683 "Dispel Acid" (:85831-85843, a material skill for npcs) and 8929 "Armor of
 * Attrition Effect" (:89070-89081, the skill 620 Armor of Attrition's provoker applies). 10350 is replaced here by its full version: the fixture's
 * OtherEffectsTest publishes a one-position copy of it under the same id in its own case only.
 */
constexpr const char* ITEM_SKILLS_XML =
	R"(<skill_template skill_id="9889" name="Healing" nameId="702131" stack="ITEM_REMEDY_HP_10" lvl="1" skilltype="MAGICAL" skillsubtype="NONE")"
	R"( tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
	R"(<prochealinstant value="37" e="1" noresist="true" element="WATER" />)"
	R"(<heal checktime="2000" value="37" duration2="20000" effectid="209582" e="2" noresist="true" element="WATER" preeffect="1" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="9894" name="Mana Treatment" nameId="702132" stack="ITEM_REMEDY_MP_10" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="NONE" tslot="BUFF" dispel_category="BUFF" req_dispel_level="1" req_dispel_count="10" activation="ACTIVE" cooldown="0")"
	R"( duration="0"><effects>)"
	R"(<procmphealinstant value="59" e="1" noresist="true" element="WATER" />)"
	R"(<mpheal checktime="2000" value="59" duration2="20000" effectid="218232" e="2" noresist="true" element="WATER" preeffect="1" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="10249" name="Experience Boost" nameId="748697" stack="XP_BOOST_02" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
	R"( tslot="BOOST" activation="ACTIVE" cooldown="0" duration="0" hostile_type="INDIRECT"><effects>)"
	R"(<xpboost duration2="7200000" e="1" basiclvl="1" noresist="true">)"
	R"(<change stat="BOOST_CRAFTING_XP_RATE" func="ADD" value="20" />)"
	R"(<change stat="BOOST_GATHERING_XP_RATE" func="ADD" value="20" />)"
	R"(<change stat="BOOST_GROUP_HUNTING_XP_RATE" func="ADD" value="20" />)"
	R"(<change stat="BOOST_HUNTING_XP_RATE" func="ADD" value="20" />)"
	R"(</xpboost></effects></skill_template>)"
	R"(<skill_template skill_id="10350" name="Administrator's Boon" nameId="770429" stack="CASH_ITEM_START_KIT" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="BUFF" tslot="BOOST" dispel_category="NEVER" activation="ACTIVE" cooldown="0" duration="0" noremoveatdie="true"><effects>)"
	R"(<noresurrectpenalty duration2="3600000" effectid="2103501" e="1" basiclvl="1" noresist="true" />)"
	R"(<nodeathpenalty duration2="3600000" effectid="2103502" e="2" basiclvl="1" noresist="true" preeffect="1" />)"
	R"(<hipass duration2="3600000" effectid="2103503" e="3" basiclvl="1" noresist="true" preeffect="2" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="8683" name="Dispel Acid" nameId="296322" stack="MATERIAL_SKILL_ACIDHEAL_1" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="NONE" tslot="NONE" activation="PROVOKED" cooldown="0" duration="0" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true"><effects>)"
	R"(<prochealinstant value="25000" e="1" noresist="true" hoptype="SKILLLV" hopb="60" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="8929" name="Armor of Attrition Effect" nameId="2287738" stack="FI_N_FORTITUDE_PROC" lvl="1" skilltype="MAGICAL")"
	R"( skill_category="HEAL" skillsubtype="HEAL" tslot="NONE" activation="PROVOKED" cooldown="0" duration="0"><effects>)"
	R"(<prochealinstant value="150" e="1" noresist="true" element="WATER" />)"
	R"(</effects></skill_template>)"
	// 1838 "Healing Light" (:27691-27706), a skill heal whose template has apply_heal_boost_bonus: the contrast of the item heal's boost
	R"(<skill_template skill_id="1838" name="Healing Light" nameId="2286489" cooldownId="1553" group="CL_HEAL" stack="CL_HEAL" lvl="1")"
	R"( skilltype="MAGICAL" skill_category="HEAL" skillsubtype="HEAL" tslot="NONE" activation="ACTIVE" cooldown="0" duration="2000")"
	R"( cancel_rate="20" hostile_type="INDIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true" apply_heal_boost_bonus="true")"
	R"( apply_casting_time_bonus="true"><effects>)"
	R"(<healinstant value="110" e="1" noresist="true" element="WATER" hoptype="SKILLLV" hopb="6023" />)"
	R"(</effects></skill_template>)"
	// The test templates. 64101..64103: one position of the boon or the amulet, alone at e="1", with preeffect="2" and no position 2, so the
	// pre-effect check of EffectTemplate.calculate (validatePreEffects: position 2 is not a success) would refuse them - and Effect.initialize
	// then clears every success, since position 1 failed; the three classes skip super.calculate and land anyway
	R"(<skill_template skill_id="64101" name="mz lone nodeathpenalty" nameId="1" stack="MZ_LONE_NODEATHPENALTY" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="BUFF" tslot="BOOST" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
	R"(<nodeathpenalty duration2="3600000" effectid="2103502" e="1" basiclvl="1" noresist="true" preeffect="2" /></effects></skill_template>)"
	R"(<skill_template skill_id="64102" name="mz lone noresurrectpenalty" nameId="1" stack="MZ_LONE_NORESURRECTPENALTY" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="BUFF" tslot="BOOST" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
	R"(<noresurrectpenalty duration2="3600000" effectid="2103501" e="1" basiclvl="1" noresist="true" preeffect="2" /></effects>)"
	R"(</skill_template>)"
	R"(<skill_template skill_id="64103" name="mz lone xpboost" nameId="1" stack="MZ_LONE_XPBOOST" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF")"
	R"( tslot="BOOST" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
	R"(<xpboost duration2="7200000" e="1" basiclvl="1" noresist="true" preeffect="2">)"
	R"(<change stat="BOOST_HUNTING_XP_RATE" func="ADD" value="20" /></xpboost></effects></skill_template>)";

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

// SM_ATTACK_STATUS.TYPE and .LOG values (SM_ATTACK_STATUS.java:22-89): TYPE.HP and TYPE.DAMAGE share 7, TYPE.REGULAR is 5, TYPE.MP 21,
// TYPE.HEAL_MP 19, TYPE.NATURAL_HP 3 and TYPE.NATURAL_MP 22 (the regeneration, which the cases leave out)
constexpr int32_t TYPE_HP = 7;
constexpr int32_t TYPE_REGULAR = 5;
constexpr int32_t TYPE_MP = 21;
constexpr int32_t TYPE_HEAL_MP = 19;
constexpr int32_t TYPE_NATURAL_HP = 3;
constexpr int32_t TYPE_NATURAL_MP = 22;
constexpr int32_t LOG_HEAL = 3;
constexpr int32_t LOG_MPHEAL = 4;
constexpr int32_t LOG_REGULAR = 191;

class ItemEffectsTest : public EffectsMzTest {
protected:
	void SetUp() override {
		EffectsMzTest::SetUp();
		EFFECT_TEST_SCOPE;
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the base published the lane's templates; the holder is immortal, only forgotten
		publishSkillData(effectsMzSkills() + ITEM_SKILLS_XML);
	}

	/** The SM_ATTACK_STATUS packets the player was sent about `objectId`, without the natural regeneration the HP or MP drop started */
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

	/** getStat(stat, 100): the base Rates.calcXpRate reads the XP boost stats with (Rates.java:175-181) */
	static int32_t rateOf(Player& p, StatEnum stat) { return p.getGameStats()->getStat(stat, 100)->getCurrent(); }
};

// ---- ProcHealInstantEffect (ProcHealInstantEffect.java:15-41) -------------------------------------------------------------------------------

/**
 * 162000002 Minor Life Potion's skill 9889 on the drinker itself, 100 HP below the maximum: position 1 (<prochealinstant value="37">) reserves
 * 37 of HP (AbstractHealEffect.calculate with HealType.HP; no percent, no delta, no heal boost for an item heal, no deboost function) and
 * applyEffect heals it at once through increaseHp(TYPE.HP, 37, effector): one SM_ATTACK_STATUS of TYPE.HP (7, not the skill heal's REGULAR 5),
 * LOG.REGULAR, skill id 0. Position 2 (<heal>, the ported HealEffect) then lasts duration2 + 1000 = 21,000 ms (the first positive duration
 * of the success effects, Effect.calculateTemplateDuration) and heals 37 with its skill id and LOG.HEAL every 2 s from 2,300 ms on.
 */
TEST_F(ItemEffectsTest, MinorLifePotionHealsThirtySevenAtOnceAsAnItemHealThenOverTime) {
	EFFECT_TEST_SCOPE;
	Ref<Player> drinker = player(9401);
	const int32_t maxHp = drinker->getLifeStats()->getMaxHp();
	ASSERT_GT(maxHp, 100);
	drinker->getLifeStats()->setCurrentHp(maxHp - 100);
	clearSent(*drinker);

	Ref<Effect> potion = calculated(9889, *drinker, *drinker);
	ASSERT_NE(dynamic_cast<const ProcHealInstantEffect*>(potion->getEffectTemplates()[0]), nullptr)
		<< "<prochealinstant> binds ProcHealInstantEffect (Effects.java:51)";
	ASSERT_TRUE(potion->isInSuccessEffects(1));
	ASSERT_TRUE(potion->isInSuccessEffects(2)) << "preeffect 1 landed";
	EXPECT_EQ(reserved(*potion), 37);
	EXPECT_EQ(potion->getReserveds(1)->getType(), model::EffectReserved::ResourceType::HP) << "ResourceType.of(HealType.HP)";
	EXPECT_FALSE(potion->getReserveds(1)->isDamage());
	EXPECT_EQ(drinker->getLifeStats()->getCurrentHp(), maxHp - 100) << "calculate heals nothing";

	potion->applyEffect();
	EXPECT_EQ(drinker->getLifeStats()->getCurrentHp(), maxHp - 63);
	std::vector<StatusFields> statuses = statusesOf(*drinker, drinker->getObjectId());
	ASSERT_EQ(statuses.size(), 1u);
	EXPECT_EQ(statuses[0].value, 37);
	EXPECT_EQ(statuses[0].type, TYPE_HP) << "an item heal: TYPE.HP, not TYPE.REGULAR (AbstractHealEffect.java:38-39)";
	EXPECT_NE(statuses[0].type, TYPE_REGULAR);
	EXPECT_EQ(statuses[0].log, LOG_REGULAR);
	EXPECT_EQ(statuses[0].skillId, 0) << "increaseHp(type, value, effector) passes skill id 0";
	EXPECT_TRUE(drinker->getEffectController()->hasAbnormalEffect(9889)) << "the <heal> position enters the controller";
	EXPECT_EQ(potion->getDuration(), 21000);

	clearSent(*drinker);
	advance(2300);
	statuses = statusesOf(*drinker, drinker->getObjectId());
	ASSERT_EQ(statuses.size(), 1u) << "the first tick of the heal over time";
	EXPECT_EQ(statuses[0].value, 37);
	EXPECT_EQ(statuses[0].type, TYPE_HP);
	EXPECT_EQ(statuses[0].log, LOG_HEAL);
	EXPECT_EQ(statuses[0].skillId, 9889);
	advance(18700);
	EXPECT_TRUE(potion->isEndedByTime());
	EXPECT_FALSE(drinker->getEffectController()->hasAbnormalEffect(9889));
}

/**
 * AbstractHealEffect.calculateHealValue caps the reserve at getMaxStatValue - getCurrentStatValue, ProcHealInstantEffect's two overrides: the
 * effected's MAXHP stat and its current HP. 20 HP below the maximum the potion reserves 20 and heals the character full; at the maximum it
 * reserves 0, and increaseHp announces nothing (no change and skill id 0). The <heal> position lands either way.
 */
TEST_F(ItemEffectsTest, ThePotionsInstantHealIsCappedAtTheMissingHp) {
	EFFECT_TEST_SCOPE;
	Ref<Player> drinker = player(9402);
	const int32_t maxHp = drinker->getLifeStats()->getMaxHp();
	ASSERT_EQ(maxHp, drinker->getGameStats()->getMaxHp()->getCurrent());
	drinker->getLifeStats()->setCurrentHp(maxHp - 20);
	clearSent(*drinker);

	Ref<Effect> nearlyFull = calculated(9889, *drinker, *drinker);
	EXPECT_EQ(reserved(*nearlyFull), 20) << "min(maxHp - currentHp, 37)";
	nearlyFull->applyEffect();
	EXPECT_EQ(drinker->getLifeStats()->getCurrentHp(), maxHp);
	std::vector<StatusFields> statuses = statusesOf(*drinker, drinker->getObjectId());
	ASSERT_EQ(statuses.size(), 1u);
	EXPECT_EQ(statuses[0].value, 20);
	nearlyFull->endEffect();

	clearSent(*drinker);
	Ref<Effect> full = calculated(9889, *drinker, *drinker);
	EXPECT_EQ(reserved(*full), 0) << "min(0, 37)";
	EXPECT_TRUE(full->isInSuccessEffects(2));
	full->applyEffect();
	EXPECT_EQ(drinker->getLifeStats()->getCurrentHp(), maxHp);
	EXPECT_TRUE(statusesOf(*drinker, drinker->getObjectId()).empty()) << "no change, skill id 0: no packet (CreatureLifeStats.increaseHp)";
	full->endEffect();
}

/**
 * An item heal takes no heal boost: ProcHealInstantEffect.allowHpHealBoost answers false (ProcHealInstantEffect.java:37-40), so
 * HealEffectTemplate.calculateSnapshotHealValue skips the effector's HEAL_BOOST. With HEAL_BOOST 500 on the drinker (its own effector) and 200
 * HP missing, 9889's instant position still reserves 37, where the boost would add (int) (37 * 500 / 1000f) = 18. A skill heal of
 * apply_heal_boost_bonus, 1838 Healing Light (<healinstant value="110">, AbstractHealEffect.allowHpHealBoost), takes it from the same stat:
 * 110 + (int) (110 * 500 / 1000f) = 165.
 */
TEST_F(ItemEffectsTest, AnItemHealTakesNoHealBoostWhereASkillHealDoes) {
	EFFECT_TEST_SCOPE;
	Ref<Player> drinker = player(9405, gameserver::model::PlayerClass::WARRIOR);
	skillengine::test::addStat(*drinker, StatEnum::HEAL_BOOST, 500);
	ASSERT_EQ(drinker->getGameStats()->getStat(StatEnum::HEAL_BOOST, 0)->getCurrent(), 500);
	ASSERT_EQ(drinker->getGameStats()->getStat(StatEnum::HEAL_SKILL_BOOST, 1000)->getCurrent(), 1000) << "no heal skill boost";
	const int32_t maxHp = drinker->getLifeStats()->getMaxHp();
	ASSERT_GT(maxHp, 200);
	drinker->getLifeStats()->setCurrentHp(maxHp - 200);

	Ref<Effect> potion = calculated(9889, *drinker, *drinker);
	ASSERT_TRUE(potion->isInSuccessEffects(1));
	EXPECT_EQ(reserved(*potion), 37) << "allowHpHealBoost false: the value of the template";
	Ref<Effect> healingLight = calculated(1838, *drinker, *drinker);
	ASSERT_TRUE(healingLight->isInSuccessEffects(1));
	EXPECT_EQ(reserved(*healingLight), 165) << "110 + (int) (110 * 500 / 1000f)";
}

/**
 * 8683 Dispel Acid, the material skill that heals an npc (<prochealinstant value="25000">): on a monster at 300 of 1,000 HP the cap reserves
 * 700, and the players who see the monster read the item heal's TYPE.HP.
 */
TEST_F(ItemEffectsTest, ALargeItemHealOnAMonsterIsCappedAtItsMissingHp) {
	EFFECT_TEST_SCOPE;
	Ref<Player> observer = player(9403);
	Ref<Npc> npc = monster();
	pair(*npc, *observer);
	npc->getLifeStats()->setCurrentHp(300);
	clearSent(*observer);

	Ref<Effect> effect = applied(8683, *npc, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(reserved(*effect), 700);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 1000);
	std::vector<StatusFields> statuses = statusesOf(*observer, npc->getObjectId());
	ASSERT_EQ(statuses.size(), 1u);
	EXPECT_EQ(statuses[0].value, 700);
	EXPECT_EQ(statuses[0].type, TYPE_HP);
	EXPECT_EQ(effect->getDuration(), 0) << "an instant heal only";
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(8683)) << "no position enters the controller";
}

/**
 * 620 Armor of Attrition's provoker (hittype EVERYHIT, provoke_target ME) applies 8929 (<prochealinstant value="150">) to its effector when the
 * effected is attacked (ProvokerEffect$1 -> SkillEngine.applyEffectDirectly): the Gladiator heals 150 as an item heal. docs/deviations/P5-04.md
 * listed this as the unported body an attack on a character under 620 reached (ProcHealInstantEffect.calculate).
 */
TEST_F(ItemEffectsTest, ArmorOfAttritionHealsItsEffectorThroughTheProvokedItemHeal) {
	EFFECT_TEST_SCOPE;
	Ref<Player> gladiator = player(9404, gameserver::model::PlayerClass::WARRIOR);
	Ref<Npc> npc = monster();
	Ref<Effect> armor = applied(620, *gladiator, *gladiator);
	ASSERT_TRUE(armor->isInSuccessEffects(2)) << "the provoker position";
	const int32_t maxHp = gladiator->getLifeStats()->getMaxHp();
	ASSERT_GT(maxHp, 200);
	gladiator->getLifeStats()->setCurrentHp(maxHp - 200);
	clearSent(*gladiator);

	gladiator->getObserveController()->notifyAttackedObservers(*npc, 0);
	EXPECT_EQ(gladiator->getLifeStats()->getCurrentHp(), maxHp - 50);
	std::vector<StatusFields> statuses = statusesOf(*gladiator, gladiator->getObjectId());
	ASSERT_EQ(statuses.size(), 1u);
	EXPECT_EQ(statuses[0].value, 150);
	EXPECT_EQ(statuses[0].type, TYPE_HP);
	std::vector<std::vector<uint8_t>> messages = sentTo<SM_SYSTEM_MESSAGE>(*gladiator);
	ASSERT_EQ(messages.size(), 1u);
	EXPECT_EQ(messages[0],
		cp::serialized(SM_SYSTEM_MESSAGE::STR_SKILL_PROC_EFFECT_OCCURRED(skillTemplate(8929)->getL10n()), &connection(*gladiator)));

	gladiator->getObserveController()->notifyAttackedObservers(*npc, 0);
	EXPECT_EQ(gladiator->getLifeStats()->getCurrentHp(), maxHp) << "the second heal stops at the maximum (reserve min(50, 150))";
	armor->endEffect();
}

// ---- ProcMPHealInstantEffect (ProcMPHealInstantEffect.java:15-36) ---------------------------------------------------------------------------

/**
 * 162000007 Minor Mana Potion's skill 9894: position 1 (<procmphealinstant value="59">) reserves 59 of MP, capped at the missing MP
 * (getMaxStatValue: the MAXMP stat, getCurrentStatValue: the current MP), and applyEffect restores it through increaseMp(TYPE.MP, 59, 0,
 * LOG.REGULAR): SM_ATTACK_STATUS of TYPE.MP (21, not a skill's HEAL_MP 19). The <mpheal> position ticks 59 with LOG.MPHEAL.
 */
TEST_F(ItemEffectsTest, MinorManaPotionRestoresFiftyNineMpAsAnItemHealCappedAtTheMissingMp) {
	EFFECT_TEST_SCOPE;
	Ref<Player> drinker = player(9411);
	const int32_t maxMp = drinker->getLifeStats()->getMaxMp();
	ASSERT_EQ(maxMp, drinker->getGameStats()->getMaxMp()->getCurrent());
	ASSERT_GT(maxMp, 100);
	drinker->getLifeStats()->setCurrentMp(maxMp - 100);
	clearSent(*drinker);

	Ref<Effect> potion = calculated(9894, *drinker, *drinker);
	ASSERT_NE(dynamic_cast<const ProcMPHealInstantEffect*>(potion->getEffectTemplates()[0]), nullptr)
		<< "<procmphealinstant> binds ProcMPHealInstantEffect (Effects.java:52)";
	ASSERT_TRUE(potion->isInSuccessEffects(1));
	EXPECT_EQ(reserved(*potion), 59);
	EXPECT_EQ(potion->getReserveds(1)->getType(), model::EffectReserved::ResourceType::MP) << "ResourceType.of(HealType.MP)";
	EXPECT_EQ(drinker->getLifeStats()->getCurrentHp(), drinker->getLifeStats()->getMaxHp()) << "an MP heal leaves the HP alone";

	potion->applyEffect();
	EXPECT_EQ(drinker->getLifeStats()->getCurrentMp(), maxMp - 41);
	std::vector<StatusFields> statuses = statusesOf(*drinker, drinker->getObjectId());
	ASSERT_EQ(statuses.size(), 1u);
	EXPECT_EQ(statuses[0].value, 59);
	EXPECT_EQ(statuses[0].type, TYPE_MP) << "an item heal: TYPE.MP, not TYPE.HEAL_MP (AbstractHealEffect.java:44-45)";
	EXPECT_NE(statuses[0].type, TYPE_HEAL_MP);
	EXPECT_EQ(statuses[0].log, LOG_REGULAR);
	EXPECT_EQ(statuses[0].skillId, 0);

	clearSent(*drinker);
	advance(2300);
	statuses = statusesOf(*drinker, drinker->getObjectId());
	ASSERT_EQ(statuses.size(), 1u) << "the first tick of the mp heal over time";
	EXPECT_EQ(statuses[0].value, 41) << "min(maxMp - currentMp, 59): 41 were missing";
	EXPECT_EQ(statuses[0].log, LOG_MPHEAL);
	EXPECT_EQ(statuses[0].skillId, 9894);
	potion->endEffect();

	drinker->getLifeStats()->setCurrentMp(maxMp - 20);
	EXPECT_EQ(reserved(*calculated(9894, *drinker, *drinker)), 20) << "min(maxMp - currentMp, 59)";
	drinker->getLifeStats()->setCurrentMp(maxMp);
	EXPECT_EQ(reserved(*calculated(9894, *drinker, *drinker)), 0);
}

// ---- XPBoostEffect (XPBoostEffect.java:14-20, BufEffect) ------------------------------------------------------------------------------------

/**
 * 10249 Experience Boost (the starter Lodas Amulet): calculate only adds the success; BufEffect.applyEffect puts the effect in the BOOST slot
 * and startEffect adds its four ADD changes as bonus functions, so each XP rate stat Rates.calcXpRate reads with the base 100 answers 120 for
 * duration2 = 7,200,000 ms; the end removes them.
 */
TEST_F(ItemEffectsTest, ExperienceBoostAddsTwentyToEachXpRateForTwoHours) {
	EFFECT_TEST_SCOPE;
	Ref<Player> hunter = player(9421);
	for (StatEnum stat : {StatEnum::BOOST_HUNTING_XP_RATE, StatEnum::BOOST_GROUP_HUNTING_XP_RATE, StatEnum::BOOST_GATHERING_XP_RATE,
			 StatEnum::BOOST_CRAFTING_XP_RATE})
		ASSERT_EQ(rateOf(*hunter, stat), 100);
	ASSERT_EQ(rateOf(*hunter, StatEnum::BOOST_QUEST_XP_RATE), 100);

	Ref<Effect> boost = calculated(10249, *hunter, *hunter);
	ASSERT_NE(dynamic_cast<const XPBoostEffect*>(boost->getEffectTemplates()[0]), nullptr) << "<xpboost> binds XPBoostEffect (Effects.java:83)";
	ASSERT_TRUE(boost->isInSuccessEffects(1));
	boost->applyEffect();
	EXPECT_TRUE(hunter->getEffectController()->hasAbnormalEffect(10249));
	EXPECT_EQ(boost->getDuration(), 7200000);
	for (StatEnum stat : {StatEnum::BOOST_HUNTING_XP_RATE, StatEnum::BOOST_GROUP_HUNTING_XP_RATE, StatEnum::BOOST_GATHERING_XP_RATE,
			 StatEnum::BOOST_CRAFTING_XP_RATE})
		EXPECT_EQ(rateOf(*hunter, stat), 120) << xml::enumName(stat);
	EXPECT_EQ(rateOf(*hunter, StatEnum::BOOST_QUEST_XP_RATE), 100) << "not one of its changes";

	advance(7200000);
	EXPECT_TRUE(boost->isEndedByTime());
	EXPECT_EQ(rateOf(*hunter, StatEnum::BOOST_HUNTING_XP_RATE), 100) << "the end takes the functions back";
	EXPECT_EQ(rateOf(*hunter, StatEnum::BOOST_CRAFTING_XP_RATE), 100);
}

// ---- NoDeathPenaltyEffect, NoResurrectPenaltyEffect (their .java:5-11) ----------------------------------------------------------------------

/**
 * 10350 Administrator's Boon: its three positions only add their success (noresurrectpenalty, nodeathpenalty, hipass), the effect enters the
 * BOOST slot for an hour, and the readers find it by type: Effect.isNoResurrectPenalty (PlayerReviveService.revive), Effect.isNoDeathPenalty
 * (PlayerController.onDie's exp loss) and Effect.isHiPass (TeleportService's flight price). The end removes it; nothing else changes.
 */
TEST_F(ItemEffectsTest, AdministratorsBoonCarriesItsThreeFlagsForAnHour) {
	EFFECT_TEST_SCOPE;
	Ref<Player> newcomer = player(9431);
	Ref<Effect> boon = calculated(10350, *newcomer, *newcomer);
	ASSERT_NE(dynamic_cast<const NoResurrectPenaltyEffect*>(boon->getEffectTemplates()[0]), nullptr) << "Effects.java:107";
	ASSERT_NE(dynamic_cast<const NoDeathPenaltyEffect*>(boon->getEffectTemplates()[1]), nullptr) << "Effects.java:108";
	EXPECT_TRUE(boon->isInSuccessEffects(1));
	EXPECT_TRUE(boon->isInSuccessEffects(2));
	EXPECT_TRUE(boon->isInSuccessEffects(3));

	boon->applyEffect();
	controllers::effect::EffectController& controller = *newcomer->getEffectController();
	EXPECT_TRUE(controller.hasAbnormalEffect(10350));
	EXPECT_TRUE(controller.hasAbnormalEffect([](Effect& effect) { return effect.isNoResurrectPenalty(); }));
	EXPECT_TRUE(controller.hasAbnormalEffect([](Effect& effect) { return effect.isNoDeathPenalty(); }));
	EXPECT_TRUE(controller.hasAbnormalEffect([](Effect& effect) { return effect.isHiPass(); }));
	EXPECT_EQ(boon->getDuration(), 3600000);

	advance(3600000);
	EXPECT_TRUE(boon->isEndedByTime());
	EXPECT_FALSE(controller.hasAbnormalEffect([](Effect& effect) { return effect.isNoDeathPenalty(); }));
	EXPECT_FALSE(controller.hasAbnormalEffect([](Effect& effect) { return effect.isNoResurrectPenalty(); }));
}

/**
 * The three P5-04 classes of this file override calculate with `effect.addSuccessEffect(this)` and no super.calculate, so none of
 * EffectTemplate.calculate's checks runs. 64101..64103 carry preeffect="2" without a position 2: validatePreEffects would refuse each of them
 * (the refusal the plain EffectTemplate.calculate gives the same element, asserted first), and each lands.
 */
TEST_F(ItemEffectsTest, TheFlagAndBoostEffectsLandWithoutEffectTemplateCalculatesChecks) {
	EFFECT_TEST_SCOPE;
	Ref<Player> p = player(9432);
	Ref<Effect> probe = Effect::create(*p, Ptr<Creature>(p), skillTemplate(64101), 1);
	EXPECT_FALSE(probe->getEffectTemplates()[0]->calculate(*probe, std::nullopt, std::nullopt))
		<< "EffectTemplate.calculate refuses a position whose pre-effect is not a success";

	for (int32_t skillId : {64101, 64102, 64103}) {
		Ref<Effect> effect = calculated(skillId, *p, *p);
		EXPECT_TRUE(effect->isInSuccessEffects(1)) << skillId;
		effect->applyEffect();
		EXPECT_TRUE(p->getEffectController()->hasAbnormalEffect(skillId)) << skillId;
	}
	EXPECT_TRUE(p->getEffectController()->hasAbnormalEffect([](Effect& effect) { return effect.isNoDeathPenalty(); }));
	EXPECT_TRUE(p->getEffectController()->hasAbnormalEffect([](Effect& effect) { return effect.isNoResurrectPenalty(); }));
	EXPECT_EQ(rateOf(*p, StatEnum::BOOST_HUNTING_XP_RATE), 120);
}

} // namespace
} // namespace aion::gameserver::skillengine::effect::mztest
