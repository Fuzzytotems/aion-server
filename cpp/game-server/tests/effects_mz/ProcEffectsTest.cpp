// P5-04, M5b-3 stage 1, items E-02, E-03 and E-04 (m5b3-plan.md §2.6, §5): the effect classes of the godstone procs and of the material skills
// in this chunk - ProcAtkInstantEffect (70 godstone skills, the camp fires' 8302 Flame Strike and 12 other material skills), PoisonEffect,
// SilenceEffect and ParalyzeEffect (godstone procs), and MpAttackInstantEffect (the material skill 8682 Drana's Solution; W but recommended, D7).
//
// Each case drives a real Effect through calculate -> applyEffect -> startEffect -> endEffect (EffectsMzTestSupport.h) on skill templates whose
// <effects> are copied verbatim from skill_templates.xml (line cited per template; <properties>, conditions and motions left out, no Effect reads
// them), and asserts what the Java bodies do: ProcAtkInstantEffect.java:18-45, PoisonEffect.java:23-52, SilenceEffect.java:19-41,
// ParalyzeEffect.java:19-45 and MpAttackInstantEffect.java:23-39.
//
// The golden damage follows the magical arithmetic the effects_mz DamageEffectsTest derives for 1282 Flame Bolt (AttackUtil.calculateSkillResult,
// AttackUtil.java:222-348; StatFunctions.calculateMagicalSkillDamage, StatFunctions.java:381-415): a level 1 MAGE has KNOWLEDGE 115 and no magic
// boost, the level 1 test monster mdef 100 and no elemental defense and no magical resist, so a magical hit of base b is
// (float) b * 1.15f - 100 / 10f, truncated to an int; the PvE multiplier is 1. A ProcAtkInstantEffect sets shouldUseBoostSpellAttackEffects false,
// so no BOOST_SPELL_ATTACK truncation happens between the two steps (Flame Bolt's `(int) damage`); the over-time damage of a poison uses no
// knowledge and no magic boost (calculateMagicalOverTimeSkillResult, AttackUtil.java:427-455): b - 10.

#include "EffectsMzTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_CANCEL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/effect/MpAttackInstantEffect.h"
#include "aion/gameserver/skillengine/effect/ParalyzeEffect.h"
#include "aion/gameserver/skillengine/effect/PoisonEffect.h"
#include "aion/gameserver/skillengine/effect/ProcAtkInstantEffect.h"
#include "aion/gameserver/skillengine/effect/SilenceEffect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/Effect_ForceType.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::effect::mztest {
namespace {

using network::aion::serverpackets::SM_ABNORMAL_EFFECT;
using network::aion::serverpackets::SM_ABNORMAL_STATE;
using network::aion::serverpackets::SM_ATTACK_STATUS;
using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_SKILL_CANCEL;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

// ------------------------------------------------------------------------------------------------------------------------- skill templates

/**
 * <effects> verbatim from skill_templates.xml: 8267 "Magical Earth Damage Effect" (:80570-80582, the proc of 168000116 Fx Test Earth Godstone,
 * item_templates.xml:848006; the file has one 8267 template, not two as m5b3-plan.md §2.6 (b) says: the WIND <procatk_instant> at :80566 is
 * 8266's), 8428 "Magical Fire Damage Effect" (:82626-82638), 8302 "Flame Strike" (:81174-81186, the camp fires' material skill), 8542 "Poison
 * Slash" (:84048-84060), 8538 "Silence" (:83991-84003), 8536 "Paralyze" (:83965-83977) and 8682 "Drana's Solution" (:85818-85830).
 */
constexpr const char* PROC_SKILLS_XML =
	R"(<skill_template skill_id="8267" name="Magical Earth Damage Effect" nameId="701874" stack="ITEM_SKILL_PROC_EARTH_DAMAGE" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="NONE" tslot="NONE" activation="PROVOKED" cooldown="0" duration="0" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true"><effects>)"
	R"(<procatk_instant delta="100" e="1" accmod2="100" element="EARTH" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="8428" name="Magical Fire Damage Effect" nameId="701871" stack="ITEM_SKILL_PROC_FIRE_DAMAGE_30A" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="NONE" tslot="NONE" activation="PROVOKED" cooldown="0" duration="0" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true"><effects>)"
	R"(<procatk_instant value="185" delta="1" e="1" accmod2="100" element="FIRE" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="8302" name="Flame Strike" nameId="284022" stack="MATERIAL_SKILL_PROC_BONFIRE_DAMAGE" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="NONE" tslot="NONE" activation="PROVOKED" cooldown="0" duration="0" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true"><effects>)"
	R"(<procatk_instant value="5" e="1" noresist="true" element="FIRE" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="8542" name="Poison Slash" nameId="701869" stack="ITEM_SKILL_PROC_POISON_L1_40A" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="NONE" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10" activation="PROVOKED")"
	R"( cooldown="0" duration="0" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true"><effects>)"
	R"(<poison checktime="2000" value="38" duration2="20000" effectid="82001" e="1" accmod2="100" element="FIRE" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="8538" name="Silence" nameId="701865" stack="ITEM_SKILL_PROC_SILENCE_L1_40A" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="NONE" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10" activation="PROVOKED")"
	R"( cooldown="0" duration="0" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true"><effects>)"
	R"(<silence duration2="10000" effectid="20101" e="1" accmod2="100" element="FIRE" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="8536" name="Paralyze" nameId="701863" stack="ITEM_SKILL_PROC_PARALYZE_L1_40A" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="NONE" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL" req_dispel_level="1" req_dispel_count="10" activation="PROVOKED")"
	R"( cooldown="0" duration="0" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true"><effects>)"
	R"(<paralyze duration2="5000" effectid="20002" e="1" basiclvl="10" accmod2="100" element="FIRE" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="8682" name="Drana's Solution" nameId="296289" stack="MATERIAL_SKILL_DRANAACID_1" lvl="1" skilltype="MAGICAL")"
	R"( skillsubtype="NONE" tslot="NONE" activation="PROVOKED" cooldown="0" duration="0" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true"><effects>)"
	R"(<mpattackinstant value="10" e="1" element="WATER" />)"
	R"(</effects></skill_template>)"
	// The test templates. 64111: 8428's <procatk_instant> in an ACTIVE skill (the value-only arm needs a PROVOKED one); 64112: 1120 Aether
	// Arrow's percent <mpattackinstant> (:16054) alone at position 1
	R"(<skill_template skill_id="64111" name="mz active proc" nameId="1" stack="MZ_ACTIVE_PROC" lvl="1" skilltype="MAGICAL" skillsubtype="NONE")"
	R"( tslot="NONE" activation="ACTIVE" cooldown="0" duration="0" apply_magical_skill_boost_bonus="true"><effects>)"
	R"(<procatk_instant value="185" delta="1" e="1" accmod2="100" element="FIRE" /></effects></skill_template>)"
	R"(<skill_template skill_id="64112" name="mz percent mp attack" nameId="1" stack="MZ_PERCENT_MPATTACK" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
	R"(<mpattackinstant percent="true" value="75" e="1" element="FIRE" hoptype="SKILLLV" hopb="3335" /></effects></skill_template>)";

/** SM_ATTACK_STATUS as SM_ATTACK_STATUS.java:120-160 writes it */
struct StatusFields {
	int32_t objectId = 0;
	int32_t value = 0;
	int32_t type = 0;
	int32_t skillId = 0;
	int32_t log = 0;
	int32_t critical = 0;
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
	f.critical = reader.C();
	EXPECT_EQ(reader.remaining(), 0u) << "SM_ATTACK_STATUS consumed exactly";
	return f;
}

// SM_ATTACK_STATUS.TYPE / .LOG values (SM_ATTACK_STATUS.java:22-89) and the critical marker (:12)
constexpr int32_t TYPE_DAMAGE = 7;
constexpr int32_t TYPE_DAMAGE_MP = 20;
constexpr int32_t TYPE_NATURAL_HP = 3;
constexpr int32_t TYPE_NATURAL_MP = 22;
constexpr int32_t LOG_POISON = 25;
constexpr int32_t LOG_PROCATKINSTANT = 93;
constexpr int32_t LOG_MPATTACK = 141;
constexpr int32_t CRITICAL_DISPLAY_CODE = 12;

/** AbnormalState ids (AbnormalState.java:9-40) of the states these bodies set */
constexpr int32_t POISON_ID = 1 << 0;
constexpr int32_t PARALYZE_ID = 1 << 2;
constexpr int32_t SILENCE_ID = 1 << 8;

/** Records the ATTACKED notifications of a creature's ObserveController */
struct AttackedRecorder final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	static Ref<AttackedRecorder> create() { return runtime::makeRef<AttackedRecorder>(); }

	void attacked(Creature& /*creature*/, int32_t skillId) override { skillIds.push_back(skillId); }

	std::vector<int32_t> skillIds;

protected:
	AttackedRecorder() : ActionObserver(controllers::observer::ObserverType::ATTACKED) {}
	~AttackedRecorder() override = default;
};

class ProcEffectsTest : public EffectsMzTest {
protected:
	void SetUp() override {
		EffectsMzTest::SetUp();
		EFFECT_TEST_SCOPE;
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the base published the lane's templates; the holder is immortal, only forgotten
		publishSkillData(effectsMzSkills() + PROC_SKILLS_XML);
		mage = player(9501);
		npc = monster();
		pair(*npc, *mage);
		skillengine::test::addStat(*npc, StatEnum::MAGICAL_CRITICAL_RESIST, 1000); // no magical critical can be rolled against it
	}

	void TearDown() override {
		mage = nullptr;
		npc = nullptr;
		EffectsMzTest::TearDown();
	}

	/** The inputs of the golden damage (see the file comment) */
	void assertInputs() {
		ASSERT_EQ(mage->getGameStats()->getKnowledge()->getCurrent(), 115);
		ASSERT_EQ(mage->getGameStats()->getMBoost()->getCurrent(), 0);
		ASSERT_EQ(npc->getGameStats()->getMDef()->getCurrent(), 100);
		ASSERT_EQ(npc->getGameStats()->getMResist()->getCurrent(), 0);
		ASSERT_EQ(npc->getGameStats()->getElementalDefenseFor(gameserver::model::SkillElement::EARTH), 0);
		ASSERT_EQ(npc->getGameStats()->getElementalDefenseFor(gameserver::model::SkillElement::FIRE), 0);
		ASSERT_EQ(npc->getLifeStats()->getCurrentHp(), 1000);
	}

	/** The SM_ATTACK_STATUS packets `to` was sent about `objectId`, without the natural regeneration */
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

	/**
	 * How many STR_SKILL_TARGET_SKILL_CANCELED the player was sent since its connection was last cleared: CreatureController.cancelCurrentSkill
	 * sends it to its lastAttacker when that is a Player (CreatureController.java:525-527)
	 */
	int64_t skillCanceledMessagesTo(Player& p) {
		const std::vector<uint8_t> canceled = cp::serialized(SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_SKILL_CANCELED(), &connection(p));
		const std::vector<std::vector<uint8_t>> messages = sentTo<SM_SYSTEM_MESSAGE>(p);
		return std::count(messages.begin(), messages.end(), canceled);
	}

	Ref<Player> mage;
	Ref<Npc> npc;
};

// ---- ProcAtkInstantEffect (ProcAtkInstantEffect.java:16-46) ---------------------------------------------------------------------------------

/**
 * 8267, the Fx Test Earth Godstone's proc: <procatk_instant delta="100"> and no value, so calculateBaseValue is value + delta * level (delta is
 * not 1): 100 at level 1, 300 at level 3, a magical EARTH hit of 100 * 1.15f - 10 = 105 and 345 - 10 = 335. applyEffect deals the reserve through
 * onAttack(effect, TYPE.DAMAGE, damage, true, LOG.PROCATKINSTANT, hopType): the players who see the monster read -105 of TYPE.DAMAGE with the
 * PROCATKINSTANT log and the skill id, and notifyAttack true tells the monster's ATTACKED observers. 8267 carries no hoptype: AggroList.addDamage
 * (AggroList.java:46-50) adds the 105 as damage and a hate of 0, which AggroInfo.addHate (AggroInfo.java:32-36) raises to its floor of 1; a
 * hopType of DAMAGE would add StatFunctions.calculateHate(mage, 1050) = 1050 * (1000 + 100) / 1000 = 1155.
 */
TEST_F(ProcEffectsTest, AGodstoneProcHitsWithDeltaTimesTheLevel) {
	EFFECT_TEST_SCOPE;
	ASSERT_NO_FATAL_FAILURE(assertInputs());
	Ref<AttackedRecorder> attacked = AttackedRecorder::create();
	npc->getObserveController()->addObserver(*attacked);
	clearSent(*mage);

	Ref<Effect> proc = calculated(8267, *mage, *npc);
	ASSERT_NE(dynamic_cast<const ProcAtkInstantEffect*>(proc->getEffectTemplates()[0]), nullptr)
		<< "<procatk_instant> binds ProcAtkInstantEffect (Effects.java:68)";
	ASSERT_TRUE(proc->isInSuccessEffects(1));
	EXPECT_FALSE(proc->isMagicalCritical(1)) << "8267's skill has no apply_magical_critical (DamageEffect.resolveMagicalCritical)";
	EXPECT_EQ(reserved(*proc), 105);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 1000) << "calculate deals nothing";

	proc->applyEffect();
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 895);
	std::vector<StatusFields> statuses = statusesOf(*mage, npc->getObjectId());
	ASSERT_EQ(statuses.size(), 1u);
	EXPECT_EQ(statuses[0].value, -105);
	EXPECT_EQ(statuses[0].type, TYPE_DAMAGE);
	EXPECT_EQ(statuses[0].log, LOG_PROCATKINSTANT);
	EXPECT_EQ(statuses[0].skillId, 8267);
	EXPECT_EQ(statuses[0].critical, 0);
	EXPECT_EQ(attacked->skillIds, std::vector<int32_t>{8267}) << "notifyAttack true";
	EXPECT_EQ(proc->getDuration(), 0) << "an instant hit";
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(8267));
	std::vector<Ptr<controllers::attack::AggroInfo>> aggro = npc->getAggroList().stream();
	ASSERT_EQ(aggro.size(), 1u);
	EXPECT_EQ(aggro[0]->getAttacker().get(), mage.get());
	EXPECT_EQ(aggro[0]->getDamage(), 105);
	EXPECT_EQ(aggro[0]->getHate(), 1)
		<< "hopType null (SKILLLV in the port): a hate of 0, which AggroInfo.addHate raises to its floor of 1; DAMAGE would add 1050 * 1.1";
	EXPECT_EQ(npc->getAggroList().getHate(*mage), 1);

	Ref<Npc> second = monster(505, 505, 100);
	EXPECT_EQ(reserved(*calculated(8267, *mage, *second, 3)), 335) << "level 3: 300 * 1.15f - 10";
}

/**
 * calculateBaseValue's own arm: `delta == 1 && effect.getSkillTemplate().isProvoked()` answers the value alone. 8428 (PROVOKED, value 185,
 * delta 1) at level 5 hits with 185 * 1.15f - 10 = 202.75 -> 202, where value + delta * level = 190 would give 208; the same element in an ACTIVE
 * skill (64111) takes EffectTemplate.calculateBaseValue's 190 -> 208.
 */
TEST_F(ProcEffectsTest, AProvokedProcOfDeltaOneTakesItsValueAlone) {
	EFFECT_TEST_SCOPE;
	ASSERT_NO_FATAL_FAILURE(assertInputs());
	Ref<Effect> provoked = calculated(8428, *mage, *npc, 5);
	ASSERT_TRUE(provoked->isInSuccessEffects(1));
	EXPECT_EQ(reserved(*provoked), 202);
	provoked->applyEffect();
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 798);

	Ref<Npc> second = monster(505, 505, 100);
	Ref<Effect> active = calculated(64111, *mage, *second, 5);
	ASSERT_TRUE(active->isInSuccessEffects(1));
	EXPECT_EQ(reserved(*active), 208) << "not provoked: 185 + 1 * 5";
}

/**
 * The camp fire (m5b3-plan.md §2.6): the material task applies 8302 Flame Strike to the creature that touches the fire, as its own effector,
 * through SkillEngine.applyEffectDirectly(8302, 1, creature, creature, null, MATERIAL_SKILL) (AbstractMaterialSkillActor.MaterialSkillTask). The
 * forced effect skips the resist roll (noresist as well), ProcAtkInstantEffect.applyEffect takes the reserve from the character and it reads its
 * own SM_ATTACK_STATUS of TYPE.DAMAGE / LOG.PROCATKINSTANT with 8302. The value goes through the magical arithmetic of a character hitting
 * itself (m5b2-plan.md D8: an invariant, not a golden number): the fire burns (a positive reserve), and what is reserved is exactly what is
 * taken and shown.
 */
TEST_F(ProcEffectsTest, TheCampFireBurnsTheCharacterStandingInItThroughItsMaterialSkill) {
	EFFECT_TEST_SCOPE;
	Ref<Player> camper = player(9511, gameserver::model::PlayerClass::WARRIOR);
	const int32_t hp = camper->getLifeStats()->getCurrentHp();
	clearSent(*camper);

	Ref<Effect> fire = SkillEngine::getInstance().applyEffectDirectly(8302, 1, *camper, *camper, std::nullopt, model::Effect_ForceType::MATERIAL_SKILL);
	ASSERT_TRUE(fire);
	ASSERT_TRUE(fire->isInSuccessEffects(1));
	ASSERT_NE(dynamic_cast<const ProcAtkInstantEffect*>(fire->getEffectTemplates()[0]), nullptr);
	const int32_t damage = reserved(*fire);
	EXPECT_GT(damage, 0) << "the fire burns";
	EXPECT_EQ(camper->getLifeStats()->getCurrentHp(), hp - damage);
	std::vector<StatusFields> statuses = statusesOf(*camper, camper->getObjectId());
	ASSERT_EQ(statuses.size(), 1u);
	EXPECT_EQ(statuses[0].value, -damage);
	EXPECT_EQ(statuses[0].type, TYPE_DAMAGE);
	EXPECT_EQ(statuses[0].log, LOG_PROCATKINSTANT);
	EXPECT_EQ(statuses[0].skillId, 8302);
}

// ---- PoisonEffect (PoisonEffect.java:21-53) -------------------------------------------------------------------------------------------------

/**
 * 8542 Poison Slash, a godstone proc: calculate passes POISON_RESISTANCE; startEffect reserves the over-time damage of value 38 (no knowledge, no
 * magic boost: 38 - 10 = 28), sets POISON on the effect and on the monster's controller and schedules the ticks of AbstractOverTimeEffect: every
 * checktime 2,000 ms from 2,300 ms on, within duration2 + 1,000 = 21,000 ms - ten ticks of 28. Each tick is onAttack(effect, TYPE.DAMAGE, 28,
 * false, LOG.POISON, ...): -28 of TYPE.DAMAGE with the POISON log and the skill id, and no ATTACKED notification. The end clears POISON.
 */
TEST_F(ProcEffectsTest, PoisonSlashPoisonsForTwentyOneSecondsTickingItsReserve) {
	EFFECT_TEST_SCOPE;
	ASSERT_NO_FATAL_FAILURE(assertInputs());
	Ref<AttackedRecorder> attacked = AttackedRecorder::create();
	npc->getObserveController()->addObserver(*attacked);
	clearSent(*mage);

	Ref<Effect> poison = applied(8542, *mage, *npc);
	ASSERT_NE(dynamic_cast<const PoisonEffect*>(poison->getEffectTemplates()[0]), nullptr) << "<poison> binds PoisonEffect (Effects.java:34)";
	ASSERT_TRUE(poison->isInSuccessEffects(1));
	EXPECT_FALSE(poison->isMagicalCritical(1));
	EXPECT_EQ(reserved(*poison), 28);
	EXPECT_TRUE(poison->getReserveds(1)->isDamage());
	EXPECT_EQ(poison->getEffectedHp(), -1) << "setReserveds(reserved, true): an over-time reserve";
	std::vector<Ref<model::EffectReserved>> toSend = poison->getReservedEffectsToSend();
	ASSERT_EQ(toSend.size(), 1u);
	EXPECT_EQ(toSend[0]->getPosition(), 0)
		<< "the reserve is made with send false: Effect.getReservedEffectsToSend leaves it out and answers its attack status placeholder";
	EXPECT_EQ(toSend[0]->getValue(), 0);
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::POISON));
	EXPECT_EQ(poison->getAbnormals(), POISON_ID);
	EXPECT_EQ(poison->getDuration(), 21000);
	std::vector<std::vector<uint8_t>> shown = sentTo<SM_ABNORMAL_EFFECT>(*mage);
	ASSERT_EQ(shown.size(), 1u);
	EXPECT_EQ(decodeAbnormalEffect(shown[0]).abnormals, POISON_ID);
	clearSent(*mage);

	advance(2299);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 1000);
	advance(1);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 972);
	std::vector<StatusFields> ticks = statusesOf(*mage, npc->getObjectId());
	ASSERT_EQ(ticks.size(), 1u);
	EXPECT_EQ(ticks[0].value, -28);
	EXPECT_EQ(ticks[0].type, TYPE_DAMAGE);
	EXPECT_EQ(ticks[0].log, LOG_POISON);
	EXPECT_EQ(ticks[0].skillId, 8542);
	EXPECT_EQ(ticks[0].critical, 0);
	advance(18000);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 720) << "ten ticks: 2,300 to 20,300 ms";
	advance(700);
	EXPECT_TRUE(poison->isEndedByTime());
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::POISON)) << "PoisonEffect.endEffect";
	advance(4000);
	EXPECT_EQ(npc->getLifeStats()->getCurrentHp(), 720) << "the ticks ended with the effect";
	EXPECT_TRUE(attacked->skillIds.empty()) << "notifyAttack false: the ticks notify no ATTACKED observer";
}

/** PoisonEffect.calculate passes POISON_RESISTANCE (PoisonEffect.java:30), not another resistance */
TEST_F(ProcEffectsTest, PoisonResistanceDecidesThePoisonsCalculate) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> poisonResistant = monster(505, 505, 100);
	skillengine::test::addStat(*poisonResistant, StatEnum::POISON_RESISTANCE, 1000);
	Ref<Npc> bleedResistant = monster(505, 495, 100);
	skillengine::test::addStat(*bleedResistant, StatEnum::BLEED_RESISTANCE, 1000);
	skillengine::test::addStat(*bleedResistant, StatEnum::MAGICAL_CRITICAL_RESIST, 1000);

	EXPECT_FALSE(calculated(8542, *mage, *poisonResistant)->isInSuccessEffects(1));
	EXPECT_TRUE(calculated(8542, *mage, *bleedResistant)->isInSuccessEffects(1));
}

/**
 * PoisonEffect.resolveMagicalCritical rolls the critical although 8542's skill has no apply_magical_critical ("periodic damage ignores the flag"):
 * against a monster without critical resist the chance is the Mage's 50 per mille, the first draw of the calculation. A critical reserves
 * 28 * 1.5f = 42 (AttackUtil.calculateWeaponCritical, magical), and each tick passes isMagicalCritical(position) on: the critical marker.
 */
TEST_F(ProcEffectsTest, APoisonRollsItsMagicalCriticalAndTicksItAsACriticalHit) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> target = monster(505, 505, 100);
	pair(*target, *mage);
	ASSERT_EQ(mage->getGameStats()->getMCritical()->getCurrent() - target->getGameStats()->getMCR()->getCurrent(), 50);
	uint64_t critical = 1;
	for (; critical < 100000; ++critical) {
		Rnd::seedCurrentThreadForTests(critical);
		if (Rnd::nextInt(1000) < 50)
			break;
	}
	Rnd::seedCurrentThreadForTests(critical);
	Ref<Effect> crit = applied(8542, *mage, *target);
	ASSERT_TRUE(crit->isInSuccessEffects(1));
	EXPECT_TRUE(crit->isMagicalCritical(1));
	EXPECT_EQ(reserved(*crit), 42);
	clearSent(*mage);
	advance(2300);
	std::vector<StatusFields> ticks = statusesOf(*mage, target->getObjectId());
	ASSERT_EQ(ticks.size(), 1u);
	EXPECT_EQ(ticks[0].value, -42);
	EXPECT_EQ(ticks[0].critical, CRITICAL_DISPLAY_CODE);
	crit->endEffect();
}

/**
 * Each poison tick notifies the effected's DOT_ATTACKED observers with the effector (PoisonEffect.java:51): an effect that ends on damage (a 10 s
 * snare with setCancelOnDmg) ends at the first tick.
 */
TEST_F(ProcEffectsTest, PoisonTicksNotifyTheDotAttackedObservers) {
	EFFECT_TEST_SCOPE;
	Ref<Effect> fragile = calculated(64003, *mage, *npc);
	fragile->setCancelOnDmg(true);
	fragile->applyEffect();
	ASSERT_TRUE(npc->getEffectController()->hasAbnormalEffect(64003));

	applied(8542, *mage, *npc);
	advance(2299);
	EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(64003));
	advance(1);
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(64003)) << "the DOT_ATTACKED observer ended it";
}

// ---- SilenceEffect (SilenceEffect.java:17-42) -----------------------------------------------------------------------------------------------

/**
 * 8538 Silence, a godstone proc: startEffect sets SILENCE on the effect and on the monster's controller, and cancels the monster's cast only if
 * that is a MAGICAL skill (1282 Flame Bolt: cancelled, SM_SKILL_CANCEL to the players who know it and STR_SKILL_TARGET_SKILL_CANCELED to the
 * effector, cancelCurrentSkill's lastAttacker; 2864 Ferocious Strike, PHYSICAL: kept). It holds duration2 = 10,000 ms, and the end clears SILENCE.
 */
TEST_F(ProcEffectsTest, SilenceHoldsTenSecondsAndCancelsOnlyAMagicalCast) {
	EFFECT_TEST_SCOPE;
	npc->setCasting(model::Skill::create(skillTemplate(1282), *npc, 1, Ptr<Creature>(mage), nullptr));
	Ref<Npc> fighter = monster(505, 505, 100);
	pair(*fighter, *mage);
	fighter->setCasting(model::Skill::create(skillTemplate(2864), *fighter, 1, Ptr<Creature>(mage), nullptr));
	clearSent(*mage);

	Ref<Effect> silence = applied(8538, *mage, *npc);
	ASSERT_NE(dynamic_cast<const SilenceEffect*>(silence->getEffectTemplates()[0]), nullptr) << "<silence> binds SilenceEffect (Effects.java:56)";
	ASSERT_TRUE(silence->isInSuccessEffects(1));
	EXPECT_FALSE(npc->getCastingSkill()) << "a MAGICAL cast: cancelCurrentSkill(effect.getEffector())";
	EXPECT_EQ(sentTo<SM_SKILL_CANCEL>(*mage).size(), 1u);
	EXPECT_EQ(skillCanceledMessagesTo(*mage), 1) << "the effector, a Player, is the lastAttacker told";
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::SILENCE));
	EXPECT_EQ(silence->getAbnormals(), SILENCE_ID);
	EXPECT_EQ(silence->getDuration(), 10000);

	clearSent(*mage);
	Ref<Effect> kept = applied(8538, *mage, *fighter);
	ASSERT_TRUE(kept->isInSuccessEffects(1));
	EXPECT_TRUE(fighter->getCastingSkill()) << "a PHYSICAL cast is not cancelled";
	EXPECT_TRUE(sentTo<SM_SKILL_CANCEL>(*mage).empty());
	EXPECT_EQ(skillCanceledMessagesTo(*mage), 0);
	EXPECT_TRUE(fighter->getEffectController()->isAbnormalSet(AbnormalState::SILENCE));
	fighter->setCasting(nullptr); // the cast holds its caster (Skill.effector)

	advance(10000);
	EXPECT_TRUE(silence->isEndedByTime());
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::SILENCE)) << "SilenceEffect.endEffect";
}

/** SilenceEffect.calculate passes SILENCE_RESISTANCE (SilenceEffect.java:26) */
TEST_F(ProcEffectsTest, SilenceResistanceDecidesTheSilencesCalculate) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> silenceResistant = monster(505, 505, 100);
	skillengine::test::addStat(*silenceResistant, StatEnum::SILENCE_RESISTANCE, 1000);
	Ref<Npc> paralyzeResistant = monster(505, 495, 100);
	skillengine::test::addStat(*paralyzeResistant, StatEnum::PARALYZE_RESISTANCE, 1000);

	EXPECT_FALSE(calculated(8538, *mage, *silenceResistant)->isInSuccessEffects(1));
	EXPECT_TRUE(calculated(8538, *mage, *paralyzeResistant)->isInSuccessEffects(1));
}

// ---- ParalyzeEffect (ParalyzeEffect.java:17-46) ---------------------------------------------------------------------------------------------

/**
 * 8536 Paralyze on a gliding, moving, casting player (an npc's proc): startEffect cancels the cast, stops the glide and the move, then sets
 * PARALYZE on the effect and on the controller; the player sees the icon in the DEBUFF slot for duration2 = 5,000 ms, and the end clears it.
 */
TEST_F(ProcEffectsTest, AParalyzedPlayerLosesItsCastItsGlideAndItsMoveForFiveSeconds) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> attacker = monster(500, 500, 100);
	Ref<Player> target = player(9521, gameserver::model::PlayerClass::MAGE, 1, 503, 500, 100);
	Ref<model::Skill> cast = model::Skill::create(skillTemplate(1282), *target, Ptr<Creature>(attacker), 1);
	target->setCasting(cast);
	target->getMoveController()->setInMove(true);
	target->setFlyState(gameserver::model::gameobjects::state::FlyState::GLIDING);
	target->setState(gameserver::model::gameobjects::state::CreatureState::GLIDING);
	clearSent(*target);

	Ref<Effect> paralyze = applied(8536, *attacker, *target);
	ASSERT_NE(dynamic_cast<const ParalyzeEffect*>(paralyze->getEffectTemplates()[0]), nullptr) << "<paralyze> binds ParalyzeEffect (Effects.java:58)";
	ASSERT_TRUE(paralyze->isInSuccessEffects(1));
	EXPECT_FALSE(target->getCastingSkill()) << "effected.getController().cancelCurrentSkill(effect.getEffector())";
	EXPECT_EQ(sentTo<SM_SKILL_CANCEL>(*target).size(), 1u);
	EXPECT_EQ(skillCanceledMessagesTo(*target), 0) << "the effector is an npc: no lastAttacker message";
	EXPECT_FALSE(target->isInGlidingState()) << "player.getFlyController().onStopGliding()";
	const std::vector<uint8_t> stopGlide = cp::serialized(SM_EMOTION(*target, gameserver::model::EmotionType::STOP_GLIDE), &connection(*target));
	const std::vector<std::vector<uint8_t>> emotions = sentTo<SM_EMOTION>(*target);
	EXPECT_EQ(std::count(emotions.begin(), emotions.end(), stopGlide), 1) << "SM_EMOTION STOP_GLIDE";
	EXPECT_FALSE(target->getMoveController()->isInMove()) << "abortMove";
	EXPECT_TRUE(target->getEffectController()->isAbnormalSet(AbnormalState::PARALYZE));
	EXPECT_EQ(paralyze->getAbnormals(), PARALYZE_ID);
	EXPECT_EQ(paralyze->getDuration(), 5000);
	std::vector<std::vector<uint8_t>> icons = sentTo<SM_ABNORMAL_STATE>(*target);
	ASSERT_EQ(icons.size(), 1u);
	EXPECT_EQ(decodeAbnormalState(icons[0]).abnormals, PARALYZE_ID);
	ASSERT_EQ(decodeAbnormalState(icons[0]).effects.size(), 1u);
	EXPECT_EQ(decodeAbnormalState(icons[0]).effects[0].skillId, 8536);
	EXPECT_EQ(decodeAbnormalState(icons[0]).effects[0].targetSlotOrdinal, DEBUFF_ORDINAL);

	advance(5000);
	EXPECT_TRUE(paralyze->isEndedByTime());
	EXPECT_FALSE(target->getEffectController()->isAbnormalSet(AbnormalState::PARALYZE)) << "ParalyzeEffect.endEffect";
}

/**
 * A paralyzed monster loses its cast too (the npc arm: no fly or move controller call; the effector, a player, is told
 * STR_SKILL_TARGET_SKILL_CANCELED), and PARALYZE_RESISTANCE decides calculate
 */
TEST_F(ProcEffectsTest, AParalyzedMonsterLosesItsCastAndParalyzeResistanceDecides) {
	EFFECT_TEST_SCOPE;
	npc->setCasting(model::Skill::create(skillTemplate(2864), *npc, 1, Ptr<Creature>(mage), nullptr));
	clearSent(*mage);
	Ref<Effect> paralyze = applied(8536, *mage, *npc);
	ASSERT_TRUE(paralyze->isInSuccessEffects(1));
	EXPECT_FALSE(npc->getCastingSkill()) << "any cast, physical too";
	EXPECT_EQ(sentTo<SM_SKILL_CANCEL>(*mage).size(), 1u);
	EXPECT_EQ(skillCanceledMessagesTo(*mage), 1) << "cancelCurrentSkill(effect.getEffector()): the mage is the lastAttacker told";
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::PARALYZE));

	Ref<Npc> paralyzeResistant = monster(505, 505, 100);
	skillengine::test::addStat(*paralyzeResistant, StatEnum::PARALYZE_RESISTANCE, 1000);
	Ref<Npc> silenceResistant = monster(505, 495, 100);
	skillengine::test::addStat(*silenceResistant, StatEnum::SILENCE_RESISTANCE, 1000);
	EXPECT_FALSE(calculated(8536, *mage, *paralyzeResistant)->isInSuccessEffects(1));
	EXPECT_TRUE(calculated(8536, *mage, *silenceResistant)->isInSuccessEffects(1));
}

// ---- MpAttackInstantEffect (MpAttackInstantEffect.java:18-40) -------------------------------------------------------------------------------

/**
 * 8682 Drana's Solution, a material skill for players: calculate reserves value 10 as a damage of MP (ResourceType.MP, isDamage true) and then
 * runs EffectTemplate.calculate(effect, null, null, element); applyEffect takes it through reduceMp(TYPE.DAMAGE_MP, 10, skillId,
 * LOG.MPATTACK), which announces previousMp - newMp = 10 (CreatureLifeStats.java:139-140), written as -10 for TYPE.DAMAGE_MP
 * (SM_ATTACK_STATUS.java:136-139): the character reads -10 of TYPE.DAMAGE_MP with the MPATTACK log and 8682.
 */
TEST_F(ProcEffectsTest, DranasSolutionTakesTenMpFromTheCharacter) {
	EFFECT_TEST_SCOPE;
	Ref<Player> target = player(9531, gameserver::model::PlayerClass::MAGE, 1, 503, 500, 100);
	ASSERT_LE(target->getGameStats()->getMResist()->getCurrent() - npc->getGameStats()->getMAccuracy()->getCurrent(), 0)
		<< "no magical resist rate: no roll can resist it";
	const int32_t maxMp = target->getLifeStats()->getMaxMp();
	ASSERT_EQ(target->getLifeStats()->getCurrentMp(), maxMp);
	clearSent(*target);

	Ref<Effect> acid = calculated(8682, *npc, *target);
	ASSERT_NE(dynamic_cast<const MpAttackInstantEffect*>(acid->getEffectTemplates()[0]), nullptr)
		<< "<mpattackinstant> binds MpAttackInstantEffect (Effects.java:87)";
	ASSERT_TRUE(acid->isInSuccessEffects(1));
	EXPECT_EQ(reserved(*acid), 10);
	EXPECT_EQ(acid->getReserveds(1)->getType(), model::EffectReserved::ResourceType::MP);
	EXPECT_TRUE(acid->getReserveds(1)->isDamage());

	acid->applyEffect();
	EXPECT_EQ(target->getLifeStats()->getCurrentMp(), maxMp - 10);
	std::vector<StatusFields> statuses = statusesOf(*target, target->getObjectId());
	ASSERT_EQ(statuses.size(), 1u);
	EXPECT_EQ(statuses[0].value, -10) << "writeD(-value) for DAMAGE_MP";
	EXPECT_EQ(statuses[0].type, TYPE_DAMAGE_MP);
	EXPECT_EQ(statuses[0].log, LOG_MPATTACK);
	EXPECT_EQ(statuses[0].skillId, 8682);
	EXPECT_EQ(target->getLifeStats()->getCurrentHp(), target->getLifeStats()->getMaxHp()) << "an MP attack leaves the HP alone";
}

/**
 * percent="true" (1120 Aether Arrow's second position, 64112): the reserve is (maxMp * 75) / 100 of the effected's maximum MP, an int product
 * truncated by the division; applyEffect takes it.
 */
TEST_F(ProcEffectsTest, APercentMpAttackTakesItsShareOfTheMaximumMp) {
	EFFECT_TEST_SCOPE;
	Ref<Player> target = player(9532, gameserver::model::PlayerClass::MAGE, 1, 503, 500, 100);
	const int32_t maxMp = target->getLifeStats()->getMaxMp();
	ASSERT_NE(maxMp * 75 % 100, 0) << "a maximum whose share is truncated";
	Ref<Effect> drain = calculated(64112, *npc, *target);
	ASSERT_TRUE(drain->isInSuccessEffects(1));
	EXPECT_EQ(reserved(*drain), maxMp * 75 / 100);
	drain->applyEffect();
	EXPECT_EQ(target->getLifeStats()->getCurrentMp(), maxMp - maxMp * 75 / 100);
}

/**
 * MpAttackInstantEffect.calculate reserves before it calls EffectTemplate.calculate and ignores its answer: a resisted Drana's Solution (a
 * character with 100,000 MAGICAL_RESIST: the resist rate is capped at 900, and a first Rnd.get(1, 1000) of at most 900 resists) is no success,
 * but its reserve of 10 is there.
 */
TEST_F(ProcEffectsTest, AResistedMpAttackKeepsTheReserveItSetFirst) {
	EFFECT_TEST_SCOPE;
	Ref<Player> target = player(9533, gameserver::model::PlayerClass::MAGE, 1, 503, 500, 100);
	skillengine::test::addStat(*target, StatEnum::MAGICAL_RESIST, 100000);
	uint64_t seed = 1;
	for (; seed < 1000; ++seed) {
		Rnd::seedCurrentThreadForTests(seed);
		if (Rnd::get(1, 1000) <= 900)
			break;
	}
	Rnd::seedCurrentThreadForTests(seed);
	Ref<Effect> resisted = calculated(8682, *npc, *target);
	EXPECT_FALSE(resisted->isInSuccessEffects(1));
	ASSERT_TRUE(resisted->getReserveds(1));
	EXPECT_EQ(reserved(*resisted), 10);
}

} // namespace
} // namespace aion::gameserver::skillengine::effect::mztest
