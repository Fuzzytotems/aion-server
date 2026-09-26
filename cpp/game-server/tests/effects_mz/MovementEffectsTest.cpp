// P5-04, M5e stage 1, E-02 (m5e-plan.md §2.4, §15.4): the effect classes of the lane that move or hold a creature - PulledEffect (8441
// Capture, the sub effect of the Templar's 3123 Aether Leash), SimpleRootEffect (8219 Stumble, the sub effect of the Assassin's 3417 Fang
// Strike), SpinEffect (8223 Spin, the sub effect of the monsters' 16625 Redirect Attack and 17091 Powerful Wind: cast from an Npc effector
// here, the only path that reaches it in M5e), SleepEffect (285 Sleep) and RandomMoveLocEffect (the Rider's 2400 Boost, the stigma 11595
// Blind Leap).
//
// Each case drives a real Effect through calculate -> applyEffect -> startEffect -> endEffect (EffectsMzTestSupport.h) on the skill templates
// of skill_templates.xml, whose <effects> are copied verbatim below (without their <properties>, conditions and motions, which no Effect
// reads), and asserts what the Java bodies do: PulledEffect.java:24-72, SimpleRootEffect.java:24-72, SpinEffect.java:19-57,
// SleepEffect.java:19-50 and RandomMoveLocEffect.java:23-51 - the state set on the effect and on the effected's controller and cleared again,
// the resistance stat passed to EffectTemplate.calculate, the location computed from the heading, the packets and the duration.
//
// The world is the fixture's Poeta instance with an empty GeoMap (geo data off): getClosestCollision answers the requested point, canSee
// answers false only beyond 80 m (GeoMap.canSee's distance limit, GeoMap.java), and findMovementCollision leaves a creature without ground where
// it stands - except a flying player, whose search is a plain getClosestCollision (GeoService.findMovementCollision).

#include "EffectsMzTestSupport.h"

#include <bit>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORCED_MOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_POSITION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_CANCEL.h"
#include "aion/gameserver/skillengine/effect/PulledEffect.h"
#include "aion/gameserver/skillengine/effect/RandomMoveLocEffect.h"
#include "aion/gameserver/skillengine/effect/SimpleRootEffect.h"
#include "aion/gameserver/skillengine/effect/SleepEffect.h"
#include "aion/gameserver/skillengine/effect/SpinEffect.h"
#include "aion/gameserver/skillengine/model/DashStatus.h"
#include "aion/gameserver/skillengine/model/EffectResult.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"
#include "aion/gameserver/skillengine/model/SpellStatus.h"
#include "aion/gameserver/skillengine/model/SubEffectType.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::skillengine::effect::mztest {
namespace {

using gameserver::model::PlayerClass;
using network::aion::serverpackets::SM_ABNORMAL_EFFECT;
using network::aion::serverpackets::SM_ABNORMAL_STATE;
using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_FORCED_MOVE;
using network::aion::serverpackets::SM_POSITION;
using network::aion::serverpackets::SM_SKILL_CANCEL;

// ------------------------------------------------------------------------------------------------------------------------- skill templates

/**
 * The skills of the cases, their <effects> verbatim from skill_templates.xml: 8441 "Capture" and the skill that launches it, 3123 "Aether
 * Leash" (Templar, level 16); 8219 "Stumble" and its launcher 3417 "Fang Strike" (Assassin, 16); 8223 "Spin" and its launcher 16625 "Redirect
 * Attack" (the veteran tursin scouts 210183 / 210184 of Verteron); 285 "Sleep"; 2400 "Boost" (Rider, 13) and 11595 "Stigma Blind Leap I".
 */
constexpr const char* MOVEMENT_SKILLS_XML =
	R"(<skill_template skill_id="8441" name="Capture" nameId="288593" stack="PULLED" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE" tslot="DEBUFF")"
	R"( activation="ACTIVE" cooldown="0" duration="3000" cancel_rate="5" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true"><effects>)"
	R"(<pulled duration1="2000" effectid="20108" e="1" noresist="true" element="FIRE" hoptype="SKILLLV" hopb="100" hopa="100" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="3123" name="Aether Leash" nameId="2287021" cooldownId="523" group="KN_STUNNINGSNACHER" stack="KN_STUNNINGSNACHER")"
	R"( lvl="1" skilltype="PHYSICAL" skill_category="PHYSICAL_DEBUFF" skillsubtype="ATTACK" tslot="DEBUFF" dispel_category="DEBUFF_PHYSICAL")"
	R"( req_dispel_level="1" req_dispel_count="10" activation="ACTIVE" cooldown="300" duration="0" cancel_rate="10" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<skillatk e="1" accmod2="0" hoptype="SKILLLV" hopb="16793" hopa="410"><subeffect skill_id="8441" /></skillatk>)"
	R"(<snare duration2="10000" effectid="20007" e="2" element="EARTH" preeffect="1"><change stat="SPEED" func="PERCENT" value="-50" />)"
	R"(<change stat="FLY_SPEED" func="PERCENT" value="-50" /></snare>)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="8219" name="Stumble" nameId="281599" stack="NORMALATTACK_SIMPLEMOVEBACK" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="NONE" tslot="NOSHOW" activation="ACTIVE" cooldown="0" duration="3000" cancel_rate="5" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<simpleroot duration1="1000" effectid="20003" e="1" noresist="true" element="WIND" hoptype="SKILLLV" hopb="1000" hopa="100" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="3417" name="Fang Strike" nameId="2287203" cooldownId="843" group="AS_TIGERFANG" stack="AS_TIGERFANG" lvl="1")"
	R"( skilltype="PHYSICAL" skill_category="CHAIN_SKILL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="60" duration="0")"
	R"( cancel_rate="10" chain_skill_prob="100" hostile_type="DIRECT" apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<carvesignet signet="SIGNET1" signet_id="8303" signet_cap="5" value="70" e="1" hoptype="DAMAGE"><subeffect skill_id="8219" />)"
	R"(</carvesignet></effects></skill_template>)"
	R"(<skill_template skill_id="8223" name="Spin" nameId="281681" stack="NORMALATTACK_SPIN" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE")"
	R"( tslot="DEBUFF" dispel_category="STUN" activation="ACTIVE" cooldown="0" duration="3000" cancel_rate="5" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<spin duration1="2000" effectid="20012" e="1" noresist="true" element="WIND" hoptype="SKILLLV" hopb="1000" hopa="100" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="16625" name="Redirect Attack" nameId="284725" cooldownId="2" stack="GNAS_SPIN_NR" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="0" duration="3500" cancel_rate="35" hostile_type="DIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<skillatk mode="PERCENT" value="21" e="1" accmod2="0" hoptype="DAMAGE"><subeffect skill_id="8223" /></skillatk>)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="285" name="Sleep" nameId="295748" stack="Q_SLEEP_DEBUFF" lvl="1" skilltype="MAGICAL" skill_category="MENTAL_DEBUFF")"
	R"( skillsubtype="DEBUFF" tslot="DEBUFF" activation="ACTIVE" cooldown="300" duration="0" apply_magical_skill_boost_bonus="true")"
	R"( apply_magical_critical="true"><effects>)"
	R"(<sleep duration2="30000" effectid="20106" e="1" basiclvl="100" noresist="true" element="EARTH" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="2400" name="Boost" nameId="2286732" cooldownId="1703" group="RI_FORWARDDASH" stack="RI_FORWARDDASH" lvl="1")"
	R"( skilltype="PHYSICAL" skill_category="CHAIN_SKILL" skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="300" duration="0")"
	R"( cancel_rate="10" chain_skill_prob="100"><effects>)"
	R"(<randommoveloc reserved5="1" direction="0" distance="15" e="1" noresist="true" hoptype="SKILLLV" />)"
	R"(</effects></skill_template>)"
	R"(<skill_template skill_id="11595" name="Stigma Blind Leap I" nameId="289812" cooldownId="942" stack="STIGMA_DIMENSIONDOOR" lvl="1")"
	R"( skilltype="MAGICAL" skillsubtype="NONE" tslot="NONE" activation="ACTIVE" cooldown="300" duration="0" cancel_rate="20" hostile_type="INDIRECT")"
	R"( apply_magical_skill_boost_bonus="true" apply_magical_critical="true"><effects>)"
	R"(<randommoveloc direction="1" distance="10" e="1" noresist="true" hoptype="SKILLLV" />)"
	R"(</effects></skill_template>)"
	// The test templates: 64301..64304 are 8441's <pulled>, 8219's <simpleroot>, 8223's <spin> and 285's <sleep> without noresist, so the
	// resistance stat each class passes to EffectTemplate.calculate decides (accmod2 10000 against the magical resist rate, as 64023 / 64024)
	R"(<skill_template skill_id="64301" name="mz pulled" nameId="1" stack="MZ_PULLED" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE")"
	R"( tslot="DEBUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
	R"(<pulled duration1="2000" effectid="20108" e="1" accmod2="10000" element="FIRE" /></effects></skill_template>)"
	R"(<skill_template skill_id="64302" name="mz simpleroot" nameId="1" stack="MZ_SIMPLEROOT" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE")"
	R"( tslot="NOSHOW" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
	R"(<simpleroot duration1="1000" effectid="20003" e="1" accmod2="10000" element="WIND" /></effects></skill_template>)"
	R"(<skill_template skill_id="64303" name="mz spin" nameId="1" stack="MZ_SPIN" lvl="1" skilltype="PHYSICAL" skillsubtype="NONE" tslot="DEBUFF")"
	R"( activation="ACTIVE" cooldown="0" duration="0"><effects>)"
	R"(<spin duration1="2000" effectid="20012" e="1" accmod2="10000" element="WIND" /></effects></skill_template>)"
	R"(<skill_template skill_id="64304" name="mz sleep" nameId="1" stack="MZ_SLEEP" lvl="1" skilltype="MAGICAL" skillsubtype="DEBUFF")"
	R"( tslot="DEBUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
	R"(<sleep duration2="30000" effectid="20106" e="1" accmod2="10000" element="EARTH" /></effects></skill_template>)"
	// 64305: 8441 with skillsubtype DEBUFF - Effect.setShieldDefense keeps the SKILL_REFLECTOR bit only for ATTACK and DEBUFF skills
	// (Effect.java:setShieldDefense), so the NONE of the real Capture can never be reflected
	R"(<skill_template skill_id="64305" name="mz reflectable pull" nameId="1" stack="MZ_PULLED_2" lvl="1" skilltype="PHYSICAL")"
	R"( skillsubtype="DEBUFF" tslot="DEBUFF" activation="ACTIVE" cooldown="0" duration="0"><effects>)"
	R"(<pulled duration1="2000" effectid="20108" e="1" noresist="true" element="FIRE" /></effects></skill_template>)";

/** Java AbnormalState.getId() of the states these bodies set (AbnormalState.java:14-35) */
constexpr int32_t SLEEP_ID = 1 << 3;
constexpr int32_t SPIN_ID = 1 << 19;
constexpr int32_t PULLED_ID = 1 << 22;
constexpr int32_t SIMPLE_MOVE_BACK_ID = 1 << 24;

/** Java ShieldType.SKILL_REFLECTOR.getId() (ShieldType.java:13): the shield defense bit Effect.isReflected asks */
constexpr int32_t SKILL_REFLECTOR_ID = 1 << 5;

/** Npc id of the immobile monster (run speed 0) */
constexpr int32_t MZ_IMMOBILE_MONSTER = 290002;

/**
 * mzMonsterTemplate with a run speed of 0: EffectTemplate.isImmuneToAbnormal (EffectTemplate.java:374-385) makes such an npc immune to the
 * PULLED, STAGGER and STUMBLE resistances - whatever noresist says, since the check comes before the resist rolls
 */
const gameserver::model::templates::npc::NpcTemplate* immobileMonsterTemplate() {
	static const gameserver::model::templates::npc::NpcTemplate* bound = [] {
		xml::LoadContext context;
		return xml::bindString<gameserver::model::templates::npc::NpcTemplate>(context,
			R"(<npc_template name_id="1" npc_id="290002" level="1" name="mz immobile monster" attack_speed="2000" arange="2" rating="NORMAL")"
			R"( tribe="MONSTER"><stats maxHp="1000" maxMp="100" pdef="100" mdef="100" attack="16" evasion="0" parry="0" block="0" accuracy="200")"
			R"( macc="60" pcrit="10" mcrit="20"><speeds walk="0" run="0" run_fight="0" group_walk="0" group_run_fight="0" fly="0"/></stats>)"
			R"(</npc_template>)")
			.release();
	}();
	return bound;
}

class MovementEffectsTest : public EffectsMzTest {
protected:
	void SetUp() override {
		EffectsMzTest::SetUp();
		EFFECT_TEST_SCOPE;
		dataholders::DataManager::SKILL_DATA.resetForTests(); // the base published the lane's templates; the holder is immortal, only forgotten
		publishSkillData(effectsMzSkills() + MOVEMENT_SKILLS_XML);
		canSeeEnabled = configs::main::GeoDataConfig::CANSEE_ENABLE.load();
	}

	void TearDown() override {
		configs::main::GeoDataConfig::CANSEE_ENABLE.store(canSeeEnabled);
		EffectsMzTest::TearDown();
	}

	/** Java `new Effect(effector, effected, template, 1, null, null, true, null)`: the sub effect EffectTemplate.calculateSubEffect creates */
	Ref<Effect> subEffect(int32_t skillId, Creature& effector, Creature& effected) {
		Ref<Effect> effect = Effect::create(effector, Ptr<Creature>(effected), skillTemplate(skillId), 1, std::nullopt, nullptr, true, nullptr);
		effect->initialize();
		return effect;
	}

	/** A spawned level 1 MONSTER that cannot move (immobileMonsterTemplate), placed like monster() */
	Ref<Npc> immobileMonster(float x = 505, float y = 500, float z = 100) {
		Ref<gameserver::model::templates::spawns::SpawnGroup> group =
			gameserver::model::templates::spawns::SpawnGroup::create(effecttest::POETA, MZ_IMMOBILE_MONSTER, 0, nullptr);
		gameserver::model::templates::spawns::SpawnTemplate& spawnTemplate =
			group->addSpawnTemplate(std::make_unique<effecttest::EffectTestSpawnTemplate>(*group, x, y, z));
		Ref<Npc> npc = gameserver::model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawnTemplate,
			immobileMonsterTemplate());
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		place(*npc, x, y, z);
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		return npc;
	}

	/** A monster casting 1282 Flame Bolt at the player (the cast cancelCurrentSkill ends) */
	void castAt(Npc& npc, Player& target) { npc.setCasting(model::Skill::create(skillTemplate(1282), npc, 1, Ptr<Creature>(target), nullptr)); }

	/** A player casting 1282 Flame Bolt at the creature */
	void castAt(Player& caster, Creature& target) { caster.setCasting(model::Skill::create(skillTemplate(1282), caster, Ptr<Creature>(target), 1)); }

	/** FlyController.onStopGliding on a gliding player: SM_EMOTION STOP_GLIDE to the player (FlyController.java:37-49) */
	void expectGlideStopped(Player& p) {
		EXPECT_FALSE(p.isInGlidingState()) << "player.getFlyController().onStopGliding()";
		const std::vector<uint8_t> stopGlide = cp::serialized(SM_EMOTION(p, gameserver::model::EmotionType::STOP_GLIDE), &connection(p));
		std::vector<std::vector<uint8_t>> emotions = sentTo<SM_EMOTION>(p);
		EXPECT_EQ(std::count(emotions.begin(), emotions.end(), stopGlide), 1) << "SM_EMOTION STOP_GLIDE";
	}

	bool canSeeEnabled = false;
};

// ---- PulledEffect (PulledEffect.java:22-73) -------------------------------------------------------------------------------------------------

/**
 * 8441 Capture as 3123 Aether Leash launches it (a sub effect, EffectTemplate.calculateSubEffect): calculate lands (noresist, no PULLED /
 * STUMBLE / OPENAERIAL state, the monster in sight), marks the npc's pull as SubEffectType.PULL_NPC and computes the pull target 1.5 m in
 * front of the effector towards the effected: the heading from the templar (500, 500) to the monster (505, 500) is 0, i.e. 0 degrees, so
 * ((float) cos 0 * 1.5f, (float) sin 0 * 1.5f) = (1.5, 0) is added to the effector's x, y and its z is taken: (501.5, 500, 100), which the empty
 * GeoMap does not shorten. startEffect cancels the monster's cast, moves it there with its own heading and sets PULLED for duration1 2,000 ms x
 * skill level 1; an npc gets no SM_FORCED_MOVE. The end task clears PULLED again.
 */
TEST_F(MovementEffectsTest, CaptureDrawsTheMonsterToOneAndAHalfMetresBeforeTheTemplar) {
	EFFECT_TEST_SCOPE;
	Ref<Player> templar = player(6101, PlayerClass::WARRIOR, 1, 500, 500, 100);
	Ref<Npc> npc = monster(505, 500, 100);
	pair(*npc, *templar);
	addToRegion(*templar);
	npc->getPosition()->setH(int8_t{42});
	castAt(*npc, *templar);
	clearSent(*templar);

	Ref<Effect> effect = subEffect(8441, *templar, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	ASSERT_NE(dynamic_cast<const PulledEffect*>(effect->getEffectTemplates()[0]), nullptr);
	EXPECT_EQ(effect->getSubEffectType(), model::SubEffectType::PULL_NPC) << "a sub effect on an npc";
	EXPECT_EQ(effect->getTargetX(), 501.5f);
	EXPECT_EQ(effect->getTargetY(), 500.0f);
	EXPECT_EQ(effect->getTargetZ(), 100.0f) << "the effector's z";
	EXPECT_EQ(npc->getX(), 505.0f) << "calculate moves nothing";

	effect->applyEffect();
	EXPECT_FALSE(npc->getCastingSkill()) << "cancelCurrentSkill(effector)";
	EXPECT_EQ(npc->getX(), 501.5f) << "World.updatePosition to the target location";
	EXPECT_EQ(npc->getY(), 500.0f);
	EXPECT_EQ(npc->getHeading(), 42) << "updatePosition(..., effected.getHeading())";
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::PULLED));
	EXPECT_EQ(effect->getAbnormals(), PULLED_ID) << "effect.setAbnormal(AbnormalState.PULLED)";
	EXPECT_EQ(effect->getDuration(), 2000);
	std::vector<std::vector<uint8_t>> announced = sentTo<SM_ABNORMAL_EFFECT>(*templar);
	ASSERT_EQ(announced.size(), 1u) << "the monster's broadcasts still reach the templar after the move";
	EXPECT_EQ(decodeAbnormalEffect(announced[0]).abnormals, PULLED_ID);
	EXPECT_TRUE(sentTo<SM_FORCED_MOVE>(*templar).empty()) << "only a pulled player is force-moved on the clients";

	advance(1999);
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::PULLED));
	advance(1);
	EXPECT_TRUE(effect->isEndedByTime());
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::PULLED)) << "PulledEffect.endEffect: unsetAbnormal(PULLED)";
}

/**
 * A pulled player (PulledEffect.java:51-68): SubEffectType.PULL, its cast cancelled, its glide and movement stopped (onStopGliding,
 * onStopMove), moved with its own heading and force-moved on the clients: SM_FORCED_MOVE (effector, target, location) to itself and the players
 * who see it. North of the effector: heading 30, 90 degrees, so y + 1.5 (x + (float) cos 90° * 1.5f, a float of about 1e-16, leaves 500).
 */
TEST_F(MovementEffectsTest, APulledPlayerIsForceMovedWithPullAsItsSubEffectType) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = monster(500, 500, 100);
	Ref<Player> target = player(6111, PlayerClass::MAGE, 1, 500, 505, 100);
	castAt(*target, *npc);
	target->getMoveController()->setInMove(true);
	glide(*target);
	target->getPosition()->setH(int8_t{42});
	clearSent(*target);

	Ref<Effect> effect = subEffect(8441, *npc, *target);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getSubEffectType(), model::SubEffectType::PULL) << "a sub effect on a player";
	EXPECT_EQ(effect->getTargetX(), 500.0f);
	EXPECT_EQ(effect->getTargetY(), 501.5f);

	effect->applyEffect();
	EXPECT_FALSE(target->getCastingSkill()) << "cancelCurrentSkill(effector)";
	EXPECT_EQ(sentTo<SM_SKILL_CANCEL>(*target).size(), 1u);
	expectGlideStopped(*target);
	EXPECT_FALSE(target->getMoveController()->isInMove()) << "player.getController().onStopMove()";
	EXPECT_EQ(target->getY(), 501.5f);
	EXPECT_EQ(target->getHeading(), 42);
	std::vector<std::vector<uint8_t>> moves = sentTo<SM_FORCED_MOVE>(*target);
	ASSERT_EQ(moves.size(), 1u) << "broadcastPacketAndReceive";
	EXPECT_EQ(moves[0], cp::serialized(SM_FORCED_MOVE(*npc, target->getObjectId(), 500.0f, 501.5f, 100.0f), &connection(*target)));
	std::vector<std::vector<uint8_t>> icons = sentTo<SM_ABNORMAL_STATE>(*target);
	ASSERT_EQ(icons.size(), 1u);
	EXPECT_EQ(decodeAbnormalState(icons[0]).abnormals, PULLED_ID);
	EXPECT_TRUE(target->getEffectController()->isAbnormalSet(AbnormalState::PULLED));

	// not as a sub effect: no SubEffectType (effect.isSubEffect() is false)
	Ref<Player> other = player(6112, PlayerClass::MAGE, 1, 500, 510, 100);
	Ref<Effect> direct = calculated(8441, *npc, *other);
	ASSERT_TRUE(direct->isInSuccessEffects(1));
	EXPECT_EQ(direct->getSubEffectType(), model::SubEffectType::NONE);
}

/**
 * A reflected pull (Effect.isReflected: SKILL_REFLECTOR in the shield defense) pulls the caster: getEffected() is then the effector, and the
 * effector of the location is getOriginalEffected() (PulledEffect.java:40). The real 8441 cannot be reflected (skillsubtype NONE: the bit is
 * dropped), so 64305 is 8441 as a DEBUFF. The templar (500, 500) is pulled towards the monster (505, 500): the heading from the monster to the
 * templar is 60, 180 degrees, so (505 - 1.5, 500 + (float) sin 180° * 1.5f) = (503.5, 500). startEffect does not cancel the templar's cast nor
 * stop its glide (`if (!effect.isReflected())`) and names the monster as the mover in SM_FORCED_MOVE.
 */
TEST_F(MovementEffectsTest, AReflectedPullDrawsTheCasterToTheCreatureItWasReflectedBy) {
	EFFECT_TEST_SCOPE;
	Ref<Player> templar = player(6121, PlayerClass::WARRIOR, 1, 500, 500, 100);
	Ref<Npc> npc = monster(505, 500, 100);
	castAt(*templar, *npc);
	glide(*templar);
	clearSent(*templar);

	Ref<Effect> capture = Effect::create(*templar, Ptr<Creature>(npc), skillTemplate(8441), 1, std::nullopt, nullptr, true, nullptr);
	capture->setShieldDefense(SKILL_REFLECTOR_ID);
	ASSERT_FALSE(capture->isReflected()) << "8441 is no ATTACK or DEBUFF skill";

	Ref<Effect> effect = Effect::create(*templar, Ptr<Creature>(npc), skillTemplate(64305), 1, std::nullopt, nullptr, true, nullptr);
	effect->setShieldDefense(SKILL_REFLECTOR_ID);
	ASSERT_TRUE(effect->isReflected());
	ASSERT_EQ(effect->getEffected().get(), templar.get()) << "a reflected effect's effected is its effector";
	effect->initialize();
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getSubEffectType(), model::SubEffectType::PULL) << "the effected, the templar, is a player";
	EXPECT_EQ(effect->getTargetX(), 503.5f);
	EXPECT_NEAR(effect->getTargetY(), 500.0f, 0.0001f);

	effect->applyEffect();
	EXPECT_TRUE(templar->getCastingSkill()) << "a reflected pull cancels no cast";
	EXPECT_TRUE(templar->isInGlidingState()) << "and stops no glide";
	EXPECT_EQ(templar->getX(), 503.5f);
	EXPECT_EQ(npc->getX(), 505.0f) << "the monster stays";
	std::vector<std::vector<uint8_t>> moves = sentTo<SM_FORCED_MOVE>(*templar);
	ASSERT_EQ(moves.size(), 1u);
	EXPECT_EQ(moves[0],
		cp::serialized(SM_FORCED_MOVE(*npc, templar->getObjectId(), effect->getTargetX(), effect->getTargetY(), 100.0f), &connection(*templar)))
		<< "the original effected is the one who pulls";
	EXPECT_TRUE(templar->getEffectController()->isAbnormalSet(AbnormalState::PULLED));
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::PULLED));
}

/**
 * A creature already PULLED, STUMBLEd or in OPENAERIAL is not pulled again: calculate returns before EffectTemplate.calculate
 * (PulledEffect.java:31-33); a STAGGERed one is.
 */
TEST_F(MovementEffectsTest, APulledStumbledOrAirborneCreatureIsNotPulledAgain) {
	EFFECT_TEST_SCOPE;
	Ref<Player> templar = player(6131, PlayerClass::WARRIOR);
	for (AbnormalState state : {AbnormalState::PULLED, AbnormalState::STUMBLE, AbnormalState::OPENAERIAL}) {
		Ref<Npc> npc = monster();
		npc->getEffectController()->setAbnormal(state);
		Ref<Effect> effect = subEffect(8441, *templar, *npc);
		EXPECT_FALSE(effect->isInSuccessEffects(1)) << xml::enumName(state);
		EXPECT_EQ(effect->getSubEffectType(), model::SubEffectType::NONE) << xml::enumName(state);
	}
	Ref<Npc> staggered = monster();
	staggered->getEffectController()->setAbnormal(AbnormalState::STAGGER);
	EXPECT_TRUE(subEffect(8441, *templar, *staggered)->isInSuccessEffects(1));
}

/**
 * GeoService.canSee(effected, effector) must answer true (PulledEffect.java:34-36): with the can-see check on, a monster 85 m away is out of
 * sight (GeoMap.canSee refuses anything beyond 80 m) and is not pulled, one 5 m away is.
 */
TEST_F(MovementEffectsTest, ACreatureOutOfSightIsNotPulled) {
	EFFECT_TEST_SCOPE;
	Ref<Player> templar = player(6141, PlayerClass::WARRIOR, 1, 500, 500, 100);
	Ref<Npc> far = monster(585, 500, 100);
	Ref<Npc> near = monster(505, 500, 100);
	configs::main::GeoDataConfig::CANSEE_ENABLE.store(true);
	ASSERT_FALSE(world::geo::GeoService::getInstance().canSee(*far, *templar));
	Ref<Effect> refused = subEffect(8441, *templar, *far);
	EXPECT_FALSE(refused->isInSuccessEffects(1));
	EXPECT_EQ(refused->getSubEffectType(), model::SubEffectType::NONE);
	EXPECT_TRUE(subEffect(8441, *templar, *near)->isInSuccessEffects(1));
}

/**
 * PulledEffect passes PULLED_RESISTANCE (PulledEffect.java:37): 64301 (8441 without noresist) is resisted by 1,000 of it and not by 1,000
 * STUN_RESISTANCE. The stat also decides EffectTemplate.isImmuneToAbnormal, which comes before the noresist check: a monster that cannot move is
 * immune to the real 8441, noresist as it is.
 */
TEST_F(MovementEffectsTest, PulledResistanceDecidesThePullAndAnImmobileMonsterIsImmune) {
	EFFECT_TEST_SCOPE;
	Ref<Player> templar = player(6151, PlayerClass::WARRIOR);
	Ref<Npc> pullResistant = monster(505, 500);
	skillengine::test::addStat(*pullResistant, StatEnum::PULLED_RESISTANCE, 1000);
	Ref<Npc> stunResistant = monster(505, 505);
	skillengine::test::addStat(*stunResistant, StatEnum::STUN_RESISTANCE, 1000);
	Ref<Effect> resisted = calculated(64301, *templar, *pullResistant);
	EXPECT_FALSE(resisted->isInSuccessEffects(1)) << "effect power 1000 - 1000: no roll of Rnd.get(1, 1000) is <= 0";
	EXPECT_EQ(resisted->getEffectResult(), model::EffectResult::RESIST);
	EXPECT_TRUE(calculated(64301, *templar, *stunResistant)->isInSuccessEffects(1));

	Ref<Npc> immobile = immobileMonster(505, 510);
	EXPECT_FALSE(subEffect(8441, *templar, *immobile)->isInSuccessEffects(1)) << "run speed 0: immune to PULLED_RESISTANCE effects";
	EXPECT_TRUE(subEffect(8441, *templar, *monster(505, 515))->isInSuccessEffects(1));
}

/**
 * 3123 Aether Leash end to end: its <skillatk> launches 8441 (EffectTemplate.calculateSubEffect, chance 100) once the physical hit lands, and
 * Effect.applyEffect starts the sub effect (startSubEffect): the monster is pulled before the templar. The hit's rolls draw from the seeded
 * random number generator; the first seed whose hit lands and launches is taken.
 */
TEST_F(MovementEffectsTest, AetherLeashPullsTheMonsterThroughItsSubEffect) {
	EFFECT_TEST_SCOPE;
	Ref<Player> templar = player(6161, PlayerClass::WARRIOR, 1, 500, 500, 100);
	for (uint64_t seed = 1; seed < 100; ++seed) {
		Rnd::seedCurrentThreadForTests(seed);
		Ref<Npc> npc = monster(505, 500, 100);
		Ref<Effect> effect = calculated(3123, *templar, *npc);
		if (!effect->isInSuccessEffects(1) || !effect->getSubEffect())
			continue;
		Ptr<Effect> pull = effect->getSubEffect();
		ASSERT_TRUE(pull->isInSuccessEffects(1));
		EXPECT_EQ(pull->getSkillId(), 8441);
		EXPECT_TRUE(pull->isSubEffect());
		EXPECT_EQ(effect->getSubEffectType(), model::SubEffectType::PULL_NPC) << "calculateSubEffect copies the sub effect's type";
		EXPECT_EQ(effect->getTargetX(), 501.5f) << "and its target location";
		effect->applyEffect();
		EXPECT_EQ(npc->getX(), 501.5f);
		EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::PULLED));
		EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(8441));
		return;
	}
	FAIL() << "no seed landed the hit";
}

/**
 * The pull target takes the effector's height (PulledEffect.java:48, `float z = effector.getZ()`), not the pulled creature's: a monster 3 m above
 * the templar is drawn down to (501.5, 500, 100), and World.updatePosition puts it there.
 */
TEST_F(MovementEffectsTest, APullDrawsTheCreatureDownToTheEffectorsHeight) {
	EFFECT_TEST_SCOPE;
	Ref<Player> templar = player(6171, PlayerClass::WARRIOR, 1, 500, 500, 100);
	Ref<Npc> npc = monster(505, 500, 103);
	Ref<Effect> effect = subEffect(8441, *templar, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getTargetX(), 501.5f);
	EXPECT_EQ(effect->getTargetY(), 500.0f);
	EXPECT_EQ(effect->getTargetZ(), 100.0f) << "effector.getZ(), not the effected's 103";
	effect->applyEffect();
	EXPECT_EQ(npc->getX(), 501.5f);
	EXPECT_EQ(npc->getZ(), 100.0f);
}

/**
 * A pull at 9 degrees pins the float arithmetic of PulledEffect.java:49-50, `(float) Math.cos(radian) * 1.5f`: the cast binds to the cosine, so
 * the cosine is narrowed first and the product is a float product - two roundings. The templar stands at (0, 0) and the monster at
 * (5, 0.9375): atan2(0.9375, 5) is 10.62 degrees, heading 3 (convertAngleToHeading truncates 10.62 / 3), which convertHeadingToAngle turns
 * into 9 degrees. Math.toRadians(9.0) = 0.15707963267948966, Math.cos of it 0.9876883405951378 and Math.sin 0.15643446504023087, narrowed to
 * 0.98768836f and 0.15643446f; times 1.5f they are 1.48153257f (0x3FBDA2DC) and 0.234651685f (0x3E704888). One rounding of the double
 * products, (float) (Math.cos(radian) * 1.5f), would give 1.48153245f (0x3FBDA2DB) and 0.2346517f (0x3E704889): one ulp off, which the
 * headings of 0, 90 and 180 degrees above cannot tell apart. (A cosine or sine one double ulp away gives the same floats.) Added to the
 * templar's 0 the products stay exact, and the empty GeoMap returns them.
 */
TEST_F(MovementEffectsTest, ANineDegreePullAddsTheFloatProductsOfTheNarrowedCosineAndSine) {
	EFFECT_TEST_SCOPE;
	Ref<Player> templar = player(6181, PlayerClass::WARRIOR, 1, 0, 0, 100);
	Ref<Npc> npc = monster(5, 0.9375f, 100);
	ASSERT_EQ(static_cast<int32_t>(gameserver::utils::PositionUtil::getHeadingTowards(*templar, *npc)), 3);
	Ref<Effect> effect = subEffect(8441, *templar, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(std::bit_cast<uint32_t>(effect->getTargetX()), 0x3FBDA2DCu) << "(float) Math.cos(radian) * 1.5f = 1.48153257f";
	EXPECT_EQ(std::bit_cast<uint32_t>(effect->getTargetY()), 0x3E704888u) << "(float) Math.sin(radian) * 1.5f = 0.234651685f";
	EXPECT_EQ(effect->getTargetZ(), 100.0f);
	effect->applyEffect();
	EXPECT_EQ(std::bit_cast<uint32_t>(npc->getX()), 0x3FBDA2DCu);
	EXPECT_EQ(std::bit_cast<uint32_t>(npc->getY()), 0x3E704888u);
}

// ---- SimpleRootEffect (SimpleRootEffect.java:22-73) -----------------------------------------------------------------------------------------

/**
 * 8219 Stumble as 3417 Fang Strike launches it (a sub effect): calculate lands (noresist) with SubEffectType.SIMPLE_MOVE_BACK and sets the
 * effected 0.7 m back, away from the effector: heading 0 (0 degrees) from the assassin (500, 500) to the monster (505, 500), so
 * (float) (cos 0 * 0.7f) = 0.7f is added to the monster's own x - (505.7, 500, 100). startEffect resets the spell status, moves the monster there
 * without a known-list update and, for an npc, broadcasts SM_POSITION; SIMPLE_MOVE_BACK lasts duration1 1,000 ms.
 */
TEST_F(MovementEffectsTest, TheStumbleOfFangStrikeSetsTheMonsterBackSeventyCentimetres) {
	EFFECT_TEST_SCOPE;
	Ref<Player> assassin = player(6201, PlayerClass::SCOUT, 1, 500, 500, 100);
	Ref<Npc> npc = monster(505, 500, 100);
	pair(*npc, *assassin);
	addToRegion(*assassin);
	npc->getPosition()->setH(int8_t{42});
	clearSent(*assassin);

	Ref<Effect> effect = subEffect(8219, *assassin, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	ASSERT_NE(dynamic_cast<const SimpleRootEffect*>(effect->getEffectTemplates()[0]), nullptr);
	EXPECT_EQ(effect->getSubEffectType(), model::SubEffectType::SIMPLE_MOVE_BACK);
	EXPECT_EQ(effect->getTargetX(), 505.0f + 0.7f) << "the effected's x + (float) (cos 0 * 0.7f)";
	EXPECT_EQ(effect->getTargetY(), 500.0f);
	EXPECT_EQ(effect->getTargetZ(), 100.0f) << "the effected's z";
	effect->setSpellStatus(model::SpellStatus::STUMBLE); // what a previous position of the launching skill may have set

	effect->applyEffect();
	EXPECT_EQ(effect->getSpellStatus(), model::SpellStatus::NONE) << "effect.setSpellStatus(SpellStatus.NONE)";
	EXPECT_EQ(npc->getX(), 505.0f + 0.7f);
	EXPECT_EQ(npc->getHeading(), 42);
	std::vector<std::vector<uint8_t>> positions = sentTo<SM_POSITION>(*assassin);
	ASSERT_EQ(positions.size(), 1u) << "an npc effected broadcasts its new position";
	EXPECT_EQ(positions[0], cp::serialized(SM_POSITION(*npc), &connection(*assassin)));
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::SIMPLE_MOVE_BACK));
	EXPECT_EQ(effect->getAbnormals(), SIMPLE_MOVE_BACK_ID);
	EXPECT_EQ(effect->getDuration(), 1000);
	advance(1000);
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::SIMPLE_MOVE_BACK)) << "SimpleRootEffect.endEffect";
}

/**
 * Not as a sub effect (SimpleRootEffect.java:34 and :53): no SubEffectType, no target location, no move and no SM_POSITION; the state is set
 * all the same, and a player effected stops moving (onStopMove). A player set back as a sub effect is moved, without an SM_POSITION.
 */
TEST_F(MovementEffectsTest, AStumbleThatIsNoSubEffectLeavesTheCreatureWhereItIs) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = monster(500, 500, 100);
	Ref<Player> target = player(6211, PlayerClass::MAGE, 1, 505, 500, 100);
	pair(*npc, *target);
	target->getMoveController()->setInMove(true);
	clearSent(*target);

	Ref<Effect> effect = calculated(8219, *npc, *target);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getSubEffectType(), model::SubEffectType::NONE);
	EXPECT_EQ(effect->getTargetX(), 0.0f) << "no target location";
	effect->applyEffect();
	EXPECT_FALSE(target->getMoveController()->isInMove()) << "player.getController().onStopMove()";
	EXPECT_EQ(target->getX(), 505.0f);
	EXPECT_TRUE(sentTo<SM_POSITION>(*target).empty());
	EXPECT_TRUE(target->getEffectController()->isAbnormalSet(AbnormalState::SIMPLE_MOVE_BACK));

	// a player set back as a sub effect is moved, but broadcasts no SM_POSITION (only a non-player does)
	Ref<Player> other = player(6212, PlayerClass::MAGE, 1, 505, 505, 100);
	pair(*npc, *other);
	clearSent(*other);
	Ref<Effect> back = subEffect(8219, *npc, *other);
	ASSERT_TRUE(back->isInSuccessEffects(1));
	back->applyEffect();
	EXPECT_EQ(other->getX(), back->getTargetX());
	EXPECT_EQ(other->getY(), back->getTargetY());
	EXPECT_NE(other->getY(), 505.0f) << "moved away from the npc";
	EXPECT_TRUE(sentTo<SM_POSITION>(*other).empty());
}

/**
 * A creature in any state of CANT_MOVE_STATE (AbnormalState.java:49: SPIN, ROOT, SLEEP, STUMBLE, STUN, STAGGER, OPENAERIAL, PARALYZE, PULLED,
 * SANCTUARY) is not set back (SimpleRootEffect.java:31-32); a SNAREd one is. SimpleRootEffect passes STAGGER_RESISTANCE (:33): it resists 64302
 * and makes an immobile monster immune to the real 8219.
 */
TEST_F(MovementEffectsTest, ACreatureThatCannotMoveIsNotSetBackAndStaggerResistanceDecides) {
	EFFECT_TEST_SCOPE;
	Ref<Player> assassin = player(6221, PlayerClass::SCOUT);
	for (AbnormalState state : {AbnormalState::SPIN, AbnormalState::ROOT, AbnormalState::SLEEP, AbnormalState::STUMBLE, AbnormalState::STUN,
			 AbnormalState::STAGGER, AbnormalState::OPENAERIAL, AbnormalState::PARALYZE, AbnormalState::PULLED, AbnormalState::SANCTUARY}) {
		Ref<Npc> npc = monster();
		npc->getEffectController()->setAbnormal(state);
		EXPECT_FALSE(subEffect(8219, *assassin, *npc)->isInSuccessEffects(1)) << xml::enumName(state);
	}
	Ref<Npc> snared = monster();
	snared->getEffectController()->setAbnormal(AbnormalState::SNARE);
	EXPECT_TRUE(subEffect(8219, *assassin, *snared)->isInSuccessEffects(1));

	Ref<Npc> staggerResistant = monster(505, 500);
	skillengine::test::addStat(*staggerResistant, StatEnum::STAGGER_RESISTANCE, 1000);
	Ref<Npc> stumbleResistant = monster(505, 505);
	skillengine::test::addStat(*stumbleResistant, StatEnum::STUMBLE_RESISTANCE, 1000);
	EXPECT_FALSE(calculated(64302, *assassin, *staggerResistant)->isInSuccessEffects(1));
	EXPECT_TRUE(calculated(64302, *assassin, *stumbleResistant)->isInSuccessEffects(1));
	EXPECT_FALSE(subEffect(8219, *assassin, *immobileMonster(505, 510))->isInSuccessEffects(1)) << "immune to STAGGER_RESISTANCE effects";
}

/**
 * 3417 Fang Strike launches 8219 after its <carvesignet> hit lands (calculateSubEffect copies the sub effect's type and location), and the sub
 * effect, applied as startSubEffect applies it, sets the monster back.
 */
TEST_F(MovementEffectsTest, FangStrikeLaunchesTheSetBack) {
	EFFECT_TEST_SCOPE;
	Ref<Player> assassin = player(6231, PlayerClass::SCOUT, 1, 500, 500, 100);
	for (uint64_t seed = 1; seed < 100; ++seed) {
		Rnd::seedCurrentThreadForTests(seed);
		Ref<Npc> npc = monster(505, 500, 100);
		Ref<Effect> effect = calculated(3417, *assassin, *npc);
		if (!effect->isInSuccessEffects(1) || !effect->getSubEffect())
			continue;
		Ptr<Effect> setBack = effect->getSubEffect();
		ASSERT_TRUE(setBack->isInSuccessEffects(1));
		EXPECT_EQ(setBack->getSkillId(), 8219);
		EXPECT_EQ(effect->getSubEffectType(), model::SubEffectType::SIMPLE_MOVE_BACK);
		EXPECT_EQ(effect->getTargetX(), 505.0f + 0.7f);
		setBack->applyEffect();
		EXPECT_EQ(npc->getX(), 505.0f + 0.7f);
		EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::SIMPLE_MOVE_BACK));
		return;
	}
	FAIL() << "no seed landed the hit";
}

/**
 * The set-back keeps the creature's own height (SimpleRootEffect.java:43 passes effected.getZ()), not the effector's: a monster 3 m above the
 * assassin is set back to (505.7, 500, 103).
 */
TEST_F(MovementEffectsTest, ASetBackKeepsTheCreaturesOwnHeight) {
	EFFECT_TEST_SCOPE;
	Ref<Player> assassin = player(6241, PlayerClass::SCOUT, 1, 500, 500, 100);
	Ref<Npc> npc = monster(505, 500, 103);
	Ref<Effect> effect = subEffect(8219, *assassin, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getTargetX(), 505.0f + 0.7f);
	EXPECT_EQ(effect->getTargetY(), 500.0f);
	EXPECT_EQ(effect->getTargetZ(), 103.0f) << "effected.getZ(), not the effector's 100";
	effect->applyEffect();
	EXPECT_EQ(npc->getX(), 505.0f + 0.7f);
	EXPECT_EQ(npc->getZ(), 103.0f);
}

/**
 * A set-back at 9 degrees pins SimpleRootEffect.java:41-42, `(float) (Math.cos(radian) * 0.7f)`: here the cast narrows the double product of the
 * cosine and 0.7f (0.699999988079071), one rounding. The assassin stands at (0, 0) and the monster at (0.04296875, 0.0078125): atan2 gives
 * 10.30 degrees, heading 3, 9 degrees again. 0.9876883405951378 * 0.699999988079071 narrows to 0.6913818f (0x3F30FE66) and
 * 0.15643446504023087 * 0.699999988079071 to 0.109504126f (0x3DE043B3); the float products of the narrowed cosine and sine,
 * (float) Math.cos(radian) * 0.7f, would be 0.6913819f (0x3F30FE67) and 0.109504119f (0x3DE043B2). The monster's small coordinates keep the sums
 * exact in their binades, so the target is (0.734350562f (0x3F3BFE66), 0.117316626f (0x3DF043B3), 100), where the other order would land one
 * ulp away (0x3F3BFE67, 0x3DF043B2).
 */
TEST_F(MovementEffectsTest, ANineDegreeSetBackAddsTheNarrowedDoubleProducts) {
	EFFECT_TEST_SCOPE;
	Ref<Player> assassin = player(6251, PlayerClass::SCOUT, 1, 0, 0, 100);
	Ref<Npc> npc = monster(0.04296875f, 0.0078125f, 100);
	ASSERT_EQ(static_cast<int32_t>(gameserver::utils::PositionUtil::getHeadingTowards(*assassin, *npc)), 3);
	Ref<Effect> effect = subEffect(8219, *assassin, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(std::bit_cast<uint32_t>(effect->getTargetX()), 0x3F3BFE66u) << "0.04296875f + (float) (Math.cos(radian) * 0.7f)";
	EXPECT_EQ(std::bit_cast<uint32_t>(effect->getTargetY()), 0x3DF043B3u) << "0.0078125f + (float) (Math.sin(radian) * 0.7f)";
	EXPECT_EQ(effect->getTargetZ(), 100.0f);
	effect->applyEffect();
	EXPECT_EQ(std::bit_cast<uint32_t>(npc->getX()), 0x3F3BFE66u);
	EXPECT_EQ(std::bit_cast<uint32_t>(npc->getY()), 0x3DF043B3u);
}

// ---- SpinEffect (SpinEffect.java:17-58) -----------------------------------------------------------------------------------------------------

/**
 * 16625 Redirect Attack of the veteran tursin scouts (210183 / 210184, Verteron): once its <skillatk> hits, it launches 8223 Spin, which a
 * player takes as a sub effect - calculate passes SPIN_RESISTANCE and SpellStatus.SPIN (the launching effect takes the status over, since it is
 * neither DODGE nor RESIST), and startEffect cancels the player's cast, stops its glide, aborts its movement and sets SPIN for duration1
 * 2,000 ms. Monsters only: the effector is an Npc.
 */
TEST_F(MovementEffectsTest, RedirectAttackSpinsThePlayerThroughItsSubEffect) {
	EFFECT_TEST_SCOPE;
	for (uint64_t seed = 1; seed < 100; ++seed) {
		Rnd::seedCurrentThreadForTests(seed);
		Ref<Npc> scout = monster(500, 500, 100);
		Ref<Player> target = player(static_cast<int32_t>(6300 + seed), PlayerClass::MAGE, 1, 505, 500, 100);
		castAt(*target, *scout);
		target->getMoveController()->setInMove(true);
		glide(*target);
		clearSent(*target);
		Ref<Effect> effect = calculated(16625, *scout, *target);
		if (!effect->isInSuccessEffects(1) || !effect->getSubEffect())
			continue;
		Ptr<Effect> spin = effect->getSubEffect();
		ASSERT_TRUE(spin->isInSuccessEffects(1));
		ASSERT_NE(dynamic_cast<const SpinEffect*>(spin->getEffectTemplates()[0]), nullptr);
		EXPECT_EQ(spin->getSpellStatus(), model::SpellStatus::SPIN);
		EXPECT_EQ(effect->getSpellStatus(), model::SpellStatus::SPIN) << "calculateSubEffect takes the sub effect's status over";
		effect->applyEffect();
		EXPECT_FALSE(target->getCastingSkill()) << "cancelCurrentSkill(effector)";
		expectGlideStopped(*target);
		EXPECT_FALSE(target->getMoveController()->isInMove()) << "player.getMoveController().abortMove()";
		EXPECT_TRUE(target->getEffectController()->isAbnormalSet(AbnormalState::SPIN));
		EXPECT_EQ(spin->getAbnormals(), SPIN_ID);
		EXPECT_EQ(spin->getDuration(), 2000);
		advance(2000);
		EXPECT_FALSE(target->getEffectController()->isAbnormalSet(AbnormalState::SPIN)) << "SpinEffect.endEffect";
		return;
	}
	FAIL() << "no seed landed the hit";
}

/**
 * A spin ends the effected's paralyze effects (removeParalyzeEffects, SpinEffect.java:45): 64029, a root whose skill carries a <paralyze>. The
 * player sees SPIN in its icons for the 2,000 ms, and a casting monster spun by another monster loses its cast.
 */
TEST_F(MovementEffectsTest, ASpinEndsTheParalysisAndTheCast) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> scout = monster(500, 500, 100);
	Ref<Player> target = player(6401, PlayerClass::MAGE, 1, 505, 500, 100);
	rootedWithParalyze(*scout, *target);
	ASSERT_TRUE(target->getEffectController()->hasAbnormalEffect(64029));
	clearSent(*target);

	Ref<Effect> spin = subEffect(8223, *scout, *target);
	ASSERT_TRUE(spin->isInSuccessEffects(1));
	spin->applyEffect();
	EXPECT_FALSE(target->getEffectController()->hasAbnormalEffect(64029)) << "removeParalyzeEffects";
	EXPECT_FALSE(target->getEffectController()->isAbnormalSet(AbnormalState::ROOT));
	std::vector<std::vector<uint8_t>> icons = sentTo<SM_ABNORMAL_STATE>(*target);
	ASSERT_FALSE(icons.empty());
	AbnormalStateFields state = decodeAbnormalState(icons.back());
	EXPECT_EQ(state.abnormals, SPIN_ID);
	ASSERT_EQ(state.effects.size(), 1u);
	EXPECT_EQ(state.effects[0].skillId, 8223);
	EXPECT_EQ(state.effects[0].remainingTime, 2000);

	Ref<Npc> caster = monster(505, 505, 100);
	castAt(*caster, *target);
	ASSERT_TRUE(caster->getCastingSkill());
	ASSERT_TRUE(subEffect(8223, *scout, *caster)->isInSuccessEffects(1));
	Ref<Effect> spun = subEffect(8223, *scout, *caster);
	spun->applyEffect();
	EXPECT_FALSE(caster->getCastingSkill());
	EXPECT_TRUE(caster->getEffectController()->isAbnormalSet(AbnormalState::SPIN));
}

/**
 * A creature already PULLED, SPUN, in OPENAERIAL, STAGGERed or STUMBLEd is not spun (SpinEffect.java:27-32); a ROOTed one is. SpinEffect
 * passes SPIN_RESISTANCE (:33): it resists 64303 where STUN_RESISTANCE does not, and an immobile monster is not immune to it.
 */
TEST_F(MovementEffectsTest, AKnockedAboutCreatureIsNotSpunAndSpinResistanceDecides) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> scout = monster(500, 500, 100);
	for (AbnormalState state :
		{AbnormalState::PULLED, AbnormalState::SPIN, AbnormalState::OPENAERIAL, AbnormalState::STAGGER, AbnormalState::STUMBLE}) {
		Ref<Player> target = player(static_cast<int32_t>(6410 + static_cast<int32_t>(state)), PlayerClass::MAGE, 1, 505, 500, 100);
		target->getEffectController()->setAbnormal(state);
		EXPECT_FALSE(subEffect(8223, *scout, *target)->isInSuccessEffects(1)) << xml::enumName(state);
	}
	Ref<Player> rooted = player(6461, PlayerClass::MAGE, 1, 505, 500, 100);
	rooted->getEffectController()->setAbnormal(AbnormalState::ROOT);
	EXPECT_TRUE(subEffect(8223, *scout, *rooted)->isInSuccessEffects(1));

	Ref<Npc> spinResistant = monster(505, 500);
	skillengine::test::addStat(*spinResistant, StatEnum::SPIN_RESISTANCE, 1000);
	Ref<Npc> stunResistant = monster(505, 505);
	skillengine::test::addStat(*stunResistant, StatEnum::STUN_RESISTANCE, 1000);
	EXPECT_FALSE(calculated(64303, *scout, *spinResistant)->isInSuccessEffects(1));
	EXPECT_TRUE(calculated(64303, *scout, *stunResistant)->isInSuccessEffects(1));
	EXPECT_TRUE(subEffect(8223, *scout, *immobileMonster(505, 510))->isInSuccessEffects(1)) << "SPIN_RESISTANCE is no immunity of the immobile";
}

// ---- SleepEffect (SleepEffect.java:17-51) ---------------------------------------------------------------------------------------------------

/**
 * 285 Sleep cast by a monster at a player: calculate lands (noresist), startEffect cancels the cast, stops the glide, aborts the movement and
 * sets SLEEP for duration2 30,000 ms, and marks the effect cancel-on-damage (setCancelOnDmg), so Effect.startEffect adds the ATTACKED and
 * DOT_ATTACKED observers of addCancelOnDmgObserver: the first attack wakes the sleeper and clears SLEEP (endEffect).
 */
TEST_F(MovementEffectsTest, SleepHoldsThirtySecondsUntilAnAttackWakesTheSleeper) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = monster(500, 500, 100);
	Ref<Player> target = player(6501, PlayerClass::MAGE, 1, 505, 500, 100);
	castAt(*target, *npc);
	target->getMoveController()->setInMove(true);
	glide(*target);
	ASSERT_FALSE(target->getObserveController()->hasObservers());
	clearSent(*target);

	Ref<Effect> effect = calculated(285, *npc, *target);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	ASSERT_NE(dynamic_cast<const SleepEffect*>(effect->getEffectTemplates()[0]), nullptr);
	EXPECT_FALSE(effect->isCancelOnDmg()) << "set by startEffect";
	effect->applyEffect();
	EXPECT_FALSE(target->getCastingSkill());
	expectGlideStopped(*target);
	EXPECT_FALSE(target->getMoveController()->isInMove()) << "player.getMoveController().abortMove()";
	EXPECT_TRUE(target->getEffectController()->isAbnormalSet(AbnormalState::SLEEP));
	EXPECT_EQ(effect->getAbnormals(), SLEEP_ID);
	EXPECT_TRUE(effect->isCancelOnDmg()) << "effect.setCancelOnDmg(true)";
	EXPECT_EQ(effect->getDuration(), 30000);
	EXPECT_TRUE(target->getObserveController()->hasObservers()) << "the cancel-on-damage observers";
	std::vector<std::vector<uint8_t>> icons = sentTo<SM_ABNORMAL_STATE>(*target);
	ASSERT_EQ(icons.size(), 1u);
	EXPECT_EQ(decodeAbnormalState(icons[0]).abnormals, SLEEP_ID);

	advance(10000);
	target->getObserveController()->notifyAttackedObservers(*npc, 0);
	EXPECT_FALSE(target->getEffectController()->isAbnormalSet(AbnormalState::SLEEP)) << "woken by the attack";
	EXPECT_FALSE(target->getEffectController()->hasAbnormalEffect(285));
	EXPECT_FALSE(effect->isEndedByTime());
	EXPECT_FALSE(target->getObserveController()->hasObservers());
}

/** A sleeping monster sleeps the whole duration2 when nothing hits it; the players who see it read SLEEP in SM_ABNORMAL_EFFECT */
TEST_F(MovementEffectsTest, AnUndisturbedMonsterSleepsItsThirtySeconds) {
	EFFECT_TEST_SCOPE;
	Ref<Player> sorcerer = player(6511, PlayerClass::MAGE);
	Ref<Npc> npc = monster();
	pair(*npc, *sorcerer);
	castAt(*npc, *sorcerer);
	clearSent(*sorcerer);
	Ref<Effect> effect = applied(285, *sorcerer, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_FALSE(npc->getCastingSkill());
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::SLEEP));
	std::vector<std::vector<uint8_t>> announced = sentTo<SM_ABNORMAL_EFFECT>(*sorcerer);
	ASSERT_EQ(announced.size(), 1u);
	EXPECT_EQ(decodeAbnormalEffect(announced[0]).abnormals, SLEEP_ID);
	advance(29999);
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::SLEEP));
	advance(1);
	EXPECT_TRUE(effect->isEndedByTime());
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::SLEEP));
}

/** SleepEffect passes SLEEP_RESISTANCE (SleepEffect.java:31): 1,000 of it resists 64304 (285 without noresist), 1,000 STUN_RESISTANCE does not */
TEST_F(MovementEffectsTest, SleepResistanceDecidesTheSleepsCalculate) {
	EFFECT_TEST_SCOPE;
	Ref<Player> sorcerer = player(6521, PlayerClass::MAGE);
	Ref<Npc> sleepResistant = monster(505, 500);
	skillengine::test::addStat(*sleepResistant, StatEnum::SLEEP_RESISTANCE, 1000);
	Ref<Npc> stunResistant = monster(505, 505);
	skillengine::test::addStat(*stunResistant, StatEnum::STUN_RESISTANCE, 1000);
	EXPECT_FALSE(calculated(64304, *sorcerer, *sleepResistant)->isInSuccessEffects(1));
	EXPECT_TRUE(calculated(64304, *sorcerer, *stunResistant)->isInSuccessEffects(1));
}

// ---- RandomMoveLocEffect (RandomMoveLocEffect.java:21-52) -----------------------------------------------------------------------------------

/**
 * 2400 Boost (reserved5 1: DashStatus.RANDOMMOVELOC_NEW, direction 0: forwards, 15 m) from a flying rider facing heading 0: calculate adds the
 * success without any check and sets the skill's target position to the end of the move - (515, 500, 100) with the rider's heading on the
 * empty GeoMap. applyEffect moves the rider there (World.updatePosition with the skill's position and heading) and tells its
 * PlayerMoveController (setHasMovedByRandomMoveLocEffect).
 */
TEST_F(MovementEffectsTest, BoostCarriesAFlyingRiderFifteenMetresForward) {
	EFFECT_TEST_SCOPE;
	Ref<Player> rider = player(6601, PlayerClass::ENGINEER, 1, 500, 500, 100);
	rider->setFlyState(gameserver::model::gameobjects::state::FlyState::FLYING);
	rider->getPosition()->setH(int8_t{0});
	Ref<model::Skill> skill = model::Skill::create(skillTemplate(2400), *rider, Ptr<Creature>(rider), 1);
	Ref<Effect> effect = Effect::create(*skill, Ptr<Creature>(rider));
	effect->initialize();
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	ASSERT_NE(dynamic_cast<const RandomMoveLocEffect*>(effect->getEffectTemplates()[0]), nullptr);
	EXPECT_EQ(effect->getDashStatus(), model::DashStatus::RANDOMMOVELOC_NEW) << "reserved5 == 1";
	EXPECT_EQ(skill->getX(), 515.0f);
	EXPECT_EQ(skill->getY(), 500.0f);
	EXPECT_EQ(skill->getZ(), 100.0f);
	EXPECT_EQ(skill->getH(), 0);
	EXPECT_EQ(rider->getX(), 500.0f) << "calculate moves nothing";
	EXPECT_FALSE(rider->getMoveController()->hasMovedByRandomMoveLocEffect());

	effect->applyEffect();
	EXPECT_EQ(rider->getX(), 515.0f) << "World.updatePosition to the skill's position";
	EXPECT_EQ(rider->getY(), 500.0f);
	EXPECT_TRUE(rider->getMoveController()->hasMovedByRandomMoveLocEffect());
}

/**
 * 11595 Stigma Blind Leap I (reserved5 0: DashStatus.RANDOMMOVELOC, direction 1: backwards, 10 m): the direction is the heading's angle + 180,
 * so a flying sorcerer facing heading 30 (90 degrees) leaps to 270 degrees, (500, 490); it keeps its heading.
 */
TEST_F(MovementEffectsTest, BlindLeapCarriesAFlyingSorcererTenMetresBack) {
	EFFECT_TEST_SCOPE;
	Ref<Player> sorcerer = player(6611, PlayerClass::MAGE, 1, 500, 500, 100);
	sorcerer->setFlyState(gameserver::model::gameobjects::state::FlyState::FLYING);
	sorcerer->getPosition()->setH(int8_t{30});
	Ref<model::Skill> skill = model::Skill::create(skillTemplate(11595), *sorcerer, Ptr<Creature>(sorcerer), 1);
	Ref<Effect> effect = Effect::create(*skill, Ptr<Creature>(sorcerer));
	effect->initialize();
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getDashStatus(), model::DashStatus::RANDOMMOVELOC) << "reserved5 == 0";
	EXPECT_NEAR(skill->getX(), 500.0f, 0.0001f);
	EXPECT_NEAR(skill->getY(), 490.0f, 0.0001f);
	EXPECT_EQ(skill->getH(), 30);
	effect->applyEffect();
	EXPECT_NEAR(sorcerer->getY(), 490.0f, 0.0001f);
	EXPECT_EQ(sorcerer->getHeading(), 30);
}

/**
 * On ground without geo data there is nothing to stand on (GeoMap.findMovementCollision finds no ground 1 m ahead), so the leap ends where it
 * started: the skill's target position is the leaper's own, and applyEffect moves it nowhere (but still marks the move).
 */
TEST_F(MovementEffectsTest, WithoutGroundTheLeapEndsWhereItStarted) {
	EFFECT_TEST_SCOPE;
	Ref<Player> rider = player(6621, PlayerClass::ENGINEER, 1, 500, 500, 100);
	Ref<model::Skill> skill = model::Skill::create(skillTemplate(2400), *rider, Ptr<Creature>(rider), 1);
	Ref<Effect> effect = Effect::create(*skill, Ptr<Creature>(rider));
	effect->initialize();
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(skill->getX(), 500.0f);
	EXPECT_EQ(skill->getY(), 500.0f);
	EXPECT_EQ(skill->getZ(), 100.0f);
	effect->applyEffect();
	EXPECT_EQ(rider->getX(), 500.0f);
	EXPECT_TRUE(rider->getMoveController()->hasMovedByRandomMoveLocEffect());
}

} // namespace
} // namespace aion::gameserver::skillengine::effect::mztest
