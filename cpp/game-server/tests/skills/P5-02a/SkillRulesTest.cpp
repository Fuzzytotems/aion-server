// P5-02a, M5b-2 stage 1 part 2 (m5b2-plan.md S-03, S-04, S-07, and the two items part 1 handed over): the rules a cast is checked against,
// one class at a time - the conditions (skillengine/condition/*.java), the target properties (skillengine/properties/*.java), PenaltySkill and
// ChargeSkill (skillengine/model/*.java), MotionData.calculateAnimationTimesAfterLastHit (dataholders/MotionData.java:62-76, ported under the
// P4-09 lease) and CreatureController.useChargeSkill's call into the engine (CreatureController.java:468-493, P-04's second half).

#include "CastTestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/observer/StartMovingListener.h"
#include "aion/gameserver/dataholders/MotionData.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CASTSPELL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CASTSPELL_RESULT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/condition/Condition.h"
#include "aion/gameserver/skillengine/condition/Conditions.h"
#include "aion/gameserver/skillengine/model/ChainSkill.h"
#include "aion/gameserver/skillengine/model/ChainSkills.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/ChargeSkill.h"
#include "aion/gameserver/skillengine/model/PenaltySkill.h"
#include "aion/gameserver/skillengine/properties/Properties_CastState.h"

namespace aion::gameserver::skillengine::test {
namespace {

using namespace std::chrono_literals;
using gameserver::model::gameobjects::Creature;
using network::aion::serverpackets::SM_CASTSPELL;
using network::aion::serverpackets::SM_CASTSPELL_RESULT;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using properties::Properties_CastState;
using runtime::Ptr;
using runtime::Ref;

/** Exposes the protected KnownList::addPair, so two objects know each other without the World singleton (AttackSeamTest's KnownListPairing) */
struct KnownListPairing : world::knownlist::KnownList {
	static bool pair(gameserver::model::gameobjects::VisibleObject& a, gameserver::model::gameobjects::VisibleObject& b) { return addPair(a, b); }
};

/** A plain NpcAI (its constructor is protected): what Skill.endCast casts an npc's AI to (Skill.java:693) */
class CastTestNpcAI final : public ai::NpcAI {
public:
	explicit CastTestNpcAI(gameserver::model::gameobjects::Npc& owner) : NpcAI(owner) {}
};

class SkillRulesTest : public CastTest {
protected:
	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return cp::serialized(std::move(packet), client->con()); }

	bool sentMessage(SM_SYSTEM_MESSAGE&& packet) {
		std::vector<uint8_t> expected = message(std::move(packet));
		for (const std::vector<uint8_t>& bytes : sent())
			if (bytes == expected)
				return true;
		return false;
	}

	void setMp(int32_t value) {
		caster.player->getLifeStats()->setCurrentMp(value);
		ASSERT_EQ(currentMp(), value);
		(*client)->clearSent();
	}

	/** The index-th condition of a template's <endconditions> or <startconditions> */
	static const condition::Condition& endCondition(const model::SkillTemplate* t, size_t index) { return *t->getEndConditions()->getConditions()[index]; }
	static const condition::Condition& startCondition(const model::SkillTemplate* t, size_t index) {
		return *t->getStartconditions()->getConditions()[index];
	}
};

// ------------------------------------------------------------------------------------------------------------------------- conditions

TEST_F(SkillRulesTest, MpConditionCostsValuePlusDeltaTimesLevelAdjustedByTheBoost) {
	setMp(100);
	const condition::Condition& mp = endCondition(skillTemplate(MP_DELTA_SKILL), 0);

	// MpCondition.getCost (MpCondition.java:48-57): value + delta * skillLevel = 20 + 5 * 3
	Ref<model::Skill> level3 = model::Skill::create(skillTemplate(MP_DELTA_SKILL), *caster.player, nullptr, 3);
	EXPECT_TRUE(mp.canValidate(*level3));
	EXPECT_TRUE(mp.validate(*level3));
	EXPECT_EQ(currentMp(), 65);

	// the boost: valueWithDelta - valueWithDelta / (100 / changeMpPercent) in int arithmetic: 35 - 35 / (100 / -20) = 35 - (-7) = 42
	level3->setBoostSkillCost(-20);
	EXPECT_TRUE(mp.validate(*level3));
	EXPECT_EQ(currentMp(), 23);

	// a player who cannot pay is refused and pays nothing
	EXPECT_FALSE(mp.validate(*level3)) << "23 < 42";
	EXPECT_EQ(currentMp(), 23);
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_MP()));
}

TEST_F(SkillRulesTest, MpConditionRatioIsAPercentageOfMaxMp) {
	int32_t maxMp = caster.player->getLifeStats()->getMaxMp();
	ASSERT_GT(maxMp, 10);
	setMp(maxMp);
	// <mp value="10" ratio="true"/>: (maxMp * 10) / 100
	EXPECT_TRUE(endCondition(skillTemplate(MP_RATIO_SKILL), 0).validate(*skill(MP_RATIO_SKILL)));
	EXPECT_EQ(currentMp(), maxMp - maxMp * 10 / 100);
}

TEST_F(SkillRulesTest, AnNpcIsNeverBlockedByAnMpCost) {
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	Ptr<Creature>(npc)->getLifeStats()->setCurrentMp(10); // Creature's accessor: the double is no NpcLifeStats
	const condition::Condition& mp = endCondition(skillTemplate(MP_DELTA_SKILL), 0);
	Ref<model::Skill> npcSkill = model::Skill::create(skillTemplate(MP_DELTA_SKILL), *npc, 3, nullptr, nullptr);

	// MpCondition.canValidate: "npcs have no mp, so they must not be blocked by an mp cost" - only a Player is checked
	EXPECT_TRUE(mp.canValidate(*npcSkill));
	EXPECT_TRUE(mp.validate(*npcSkill));
	EXPECT_EQ(Ptr<Creature>(npc)->getLifeStats()->getCurrentMp(), 0) << "it pays what it has";
}

TEST_F(SkillRulesTest, TheFirstSkillOfAChainResetsWhenItWasUsedItsAllowedNumberOfTimes) {
	// ChainCondition.shouldReset (ChainCondition.java:50-65): the same 1TH skill again, after selfcount (1) uses, starts the chain over. The
	// skill's cooldown="100" keeps shouldReset's time-window arm (`now > lastUseTime + cooldown * 100`) closed for 10 s, so only the selfcount
	// arm can reset here - with a cooldown of 0 the window closed whenever the two casts fell into different milliseconds
	ASSERT_TRUE(skill(CHAIN_SKILL)->useSkill());
	Ptr<model::ChainSkills> chain = caster.player->getChainSkills();
	ASSERT_EQ(chain->getCurrentChainCount("T_CHAINA_1TH_1"), 1);
	ASSERT_TRUE(skill(CHAIN_SKILL)->useSkill());
	EXPECT_EQ(chain->getCurrentChainCount("T_CHAINA_1TH_1"), 1) << "reset, then counted once again - not 2";
}

TEST_F(SkillRulesTest, TheFirstSkillOfAChainResetsWhenItsActiveTimeIsOver) {
	// ChainCondition.shouldReset (ChainCondition.java:59-61): a first skill without `time` stays active for its cooldown * 100 ms after its last
	// use ("template cooldown is seconds * 10"); CHAIN_WINDOW_SKILL has selfcount="2" and cooldown="10", a 1-second window, and no `time`, so
	// ChainSkills.isChainExpired never fires (updateChain with duration 0 sets no expire time, ChainSkills.java:47)
	Ptr<model::ChainSkills> chain = caster.player->getChainSkills();
	ASSERT_TRUE(skill(CHAIN_WINDOW_SKILL)->useSkill());
	ASSERT_TRUE(skill(CHAIN_WINDOW_SKILL)->useSkill());
	EXPECT_EQ(chain->getCurrentChainCount("T_CHAINB_1TH_1"), 2) << "inside the window and below selfcount: counted on";
	ASSERT_TRUE(skill(CHAIN_WINDOW_SKILL)->useSkill());
	EXPECT_EQ(chain->getCurrentChainCount("T_CHAINB_1TH_1"), 1) << "selfcount reached: reset, then counted once";

	int64_t lastUse = chain->getCurrentChainSkill()->getLastUseTime();
	while (commons::utils::currentTimeMillis() <= lastUse + 1000)
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	ASSERT_TRUE(skill(CHAIN_WINDOW_SKILL)->useSkill());
	EXPECT_EQ(chain->getCurrentChainCount("T_CHAINB_1TH_1"), 1) << "the window is over: reset, then counted once - not 2";
}

TEST_F(SkillRulesTest, AFollowUpNeedsItsPreCategorySkillPreCountTimes) {
	// ChainCondition.validate (ChainCondition.java:39-44): `if (currentSkill.getUseCount() < preCount) return false` - the follow-up with
	// precount="2" is refused after one activation of its precategory skill and allowed after the second
	Ptr<model::ChainSkills> chain = caster.player->getChainSkills();
	ASSERT_TRUE(skill(CHAIN_WINDOW_SKILL)->useSkill());
	EXPECT_FALSE(skill(CHAIN_PRECOUNT_SKILL)->useSkill()) << "1 < precount 2";
	EXPECT_EQ(chain->getCurrentChainSkill()->getCategory(), "T_CHAINB_1TH_1") << "a refused follow-up leaves the chain as it was";
	EXPECT_EQ(chain->getCurrentChainCount("T_CHAINB_1TH_1"), 1);

	ASSERT_TRUE(skill(CHAIN_WINDOW_SKILL)->useSkill());
	ASSERT_EQ(chain->getCurrentChainCount("T_CHAINB_1TH_1"), 2);
	EXPECT_TRUE(skill(CHAIN_PRECOUNT_SKILL)->useSkill()) << "2 activations: precount met";
	EXPECT_EQ(chain->getCurrentChainSkill()->getCategory(), "T_CHAINB_2TH_1");
	EXPECT_EQ(chain->getPreviousChainSkill()->getCategory(), "T_CHAINB_1TH_1");
}

TEST_F(SkillRulesTest, TargetConditionWantsAnNpcAsTheFirstTarget) {
	const condition::Condition& target = startCondition(skillTemplate(CONDITIONS_SKILL), 1);
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	EXPECT_TRUE(target.validate(*skill(CONDITIONS_SKILL, npc)));

	cp::PlayerFixture other = cp::makePlayer(410003, 9403, "Other");
	(*client)->clearSent();
	EXPECT_FALSE(target.validate(*skill(CONDITIONS_SKILL, other.player))) << "TargetCondition.java: value NPC, a Player target";
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID()));
}

TEST_F(SkillRulesTest, WeaponConditionChecksAPlayersMainHandOnly) {
	const condition::Condition& weapon = startCondition(skillTemplate(CONDITIONS_SKILL), 0);
	// WeaponCondition.isValidWeapon: a Player without a main-hand weapon matches none of SWORD MACE
	EXPECT_FALSE(weapon.validate(*skill(CONDITIONS_SKILL)));

	// "for npcs we don't validate weapon, though in templates they are present"
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	EXPECT_TRUE(weapon.validate(*model::Skill::create(skillTemplate(CONDITIONS_SKILL), *npc, 1, nullptr, nullptr)));

	// a skill that is not CAST (the PROVOKED method) is not checked at all
	learn({PROVOKED_SKILL, CONDITIONS_SKILL});
	Ref<model::Skill> provoked = SkillEngine::getInstance().getSkill(*caster.player, PROVOKED_SKILL, 1, nullptr);
	ASSERT_EQ(provoked->getSkillMethod(), model::Skill::SkillMethod::PROVOKED);
	EXPECT_TRUE(weapon.validate(*provoked));
}

TEST_F(SkillRulesTest, CombatCheckAndMoveCastingConditions) {
	const model::SkillTemplate* t = skillTemplate(CONDITIONS_SKILL);
	Ref<model::Skill> cast = skill(CONDITIONS_SKILL);

	// CombatCheckCondition: a Player in combat may not use the skill
	EXPECT_TRUE(startCondition(t, 2).validate(*cast));
	caster.player->getController().enterCombat(true);
	EXPECT_FALSE(startCondition(t, 2).validate(*cast));

	// PlayerMovedCondition allow="false": valid while the move listener has not seen a move
	EXPECT_TRUE(startCondition(t, 3).validate(*cast));
	cast->getMoveListener()->moved();
	EXPECT_FALSE(startCondition(t, 3).validate(*cast));
}

// ------------------------------------------------------------------------------------------------------------------------- properties

TEST_F(SkillRulesTest, AnAreaSkillKeepsTheTargetMaxCountCreaturesNearestToTheFirstTarget) {
	Ref<CastTestNpc> first = spawnMonster(110.0f, 100.0f, 50.0f);
	Ref<CastTestNpc> two = spawnMonster(112.0f, 100.0f, 50.0f);
	Ref<CastTestNpc> five = spawnMonster(115.0f, 100.0f, 50.0f);
	Ref<CastTestNpc> eight = spawnMonster(118.0f, 100.0f, 50.0f);
	Ref<CastTestNpc> fifteen = spawnMonster(125.0f, 100.0f, 50.0f); // beyond effective_range="10" of the first target
	for (const Ref<CastTestNpc>& other : {two, five, eight, fifteen})
		ASSERT_TRUE(KnownListPairing::pair(*first, *other));

	// TargetRangeProperty AREA: the first target's known creatures within effective_range of it (TargetRangeProperty.java:80-100), then
	// MaxCountProperty: the target_maxcount nearest to the first target, the list itself filtered in place (MaxCountProperty.java:16-45)
	Ref<model::Skill> cast = skill(AREA_SKILL, first);
	ASSERT_TRUE(cast->canUseSkill(Properties_CastState::CAST_START));
	std::vector<Ptr<Creature>> effected = cast->getEffectedList().snapshot();
	ASSERT_EQ(effected.size(), 2u) << "target_maxcount=\"2\" of the four in range";
	EXPECT_EQ(effected[0], Ptr<Creature>(first)) << "distance 0";
	EXPECT_EQ(effected[1], Ptr<Creature>(two)) << "distance 2";

	// without a target_maxcount (0): MaxCountProperty keeps them all - the first target, then the known creatures in range, in known-list order
	Ref<model::Skill> all = skill(AREA_ALL_SKILL, first);
	ASSERT_TRUE(all->canUseSkill(Properties_CastState::CAST_START));
	std::vector<Ptr<Creature>> everyone = all->getEffectedList().snapshot();
	ASSERT_EQ(everyone.size(), 4u) << "the one at 15 m is beyond effective_range=\"10\" (TargetRangeProperty.checkRange)";
	EXPECT_EQ(everyone[0], Ptr<Creature>(first));
	for (const Ref<CastTestNpc>& inRange : {two, five, eight})
		EXPECT_NE(std::find(everyone.begin(), everyone.end(), Ptr<Creature>(inRange)), everyone.end());
	EXPECT_EQ(std::find(everyone.begin(), everyone.end(), Ptr<Creature>(fifteen)), everyone.end());
}

TEST_F(SkillRulesTest, TargetOrMeFallsBackToTheCasterForAnEnemyOrNoTarget) {
	// FirstTargetProperty TARGETORME, relation FRIEND (FirstTargetProperty.java:26-61): an enemy target changes to the caster, with a message
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	Ref<model::Skill> onEnemy = skill(HEAL_SKILL, npc);
	ASSERT_TRUE(onEnemy->canUseSkill(Properties_CastState::CAST_START));
	EXPECT_EQ(onEnemy->getFirstTarget(), Ptr<Creature>(caster.player));
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_AUTO_CHANGE_TARGET_TO_MY()));
	(*client)->clearSent();

	// no target: the caster, silently
	Ref<model::Skill> onNobody = skill(HEAL_SKILL);
	ASSERT_TRUE(onNobody->canUseSkill(Properties_CastState::CAST_START));
	EXPECT_EQ(onNobody->getFirstTarget(), Ptr<Creature>(caster.player));
	EXPECT_FALSE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_AUTO_CHANGE_TARGET_TO_MY()));

	// a friendly player in range stays the target
	cp::PlayerFixture friendly = cp::makePlayer(410004, 9404, "Friendly");
	place(*friendly.player, 105.0f, 100.0f, 50.0f);
	Ref<model::Skill> onFriend = skill(HEAL_SKILL, friendly.player);
	ASSERT_TRUE(onFriend->canUseSkill(Properties_CastState::CAST_START));
	EXPECT_EQ(onFriend->getFirstTarget(), Ptr<Creature>(friendly.player));
}

// ------------------------------------------------------------------------------------------------------------------------- an npc casts

TEST_F(SkillRulesTest, AnNpcCastsWithItsTemplatesCastSpeedAndResumesItsAiAfterwards) {
	// the npc path of Skill.useSkill / endCast (a post-spawn skill: SkillEngine.getSkill(npc, id, level, target).useWithoutPropSkill(),
	// SpawnEventHandler.java:20-22). Skill.endCast casts the npc's AI to NpcAI (Skill.java:693), as a spawned npc's is.
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	npc->replaceAi(std::make_unique<CastTestNpcAI>(*npc));
	Ref<model::Skill> cast = SkillEngine::getInstance().getSkill(*npc, TIMED_SKILL, 1, npc);
	ASSERT_TRUE(cast);
	ASSERT_TRUE(cast->useWithoutPropSkill());

	// updateCastDurationAndSpeed, npc arm: Math.round(baseCastDuration * (castSpeed / 1000f)) = round(2000 * 0.75) (Skill.java:339-341)
	EXPECT_TRUE(npc->isCasting());
	EXPECT_EQ(npc->getAi().getSubState(), ai::AISubState::CAST) << "useSkill: setSubStateIfNot(CAST) for an npc (Skill.java:295-296)";
	advance(1499ms);
	EXPECT_TRUE(npc->isCasting());
	advance(1ms);
	EXPECT_FALSE(npc->isCasting()) << "ended after 1,500 ms, not the template's 2,000";
	EXPECT_EQ(npc->getAi().getSubState(), ai::AISubState::NONE) << "SkillAttackManager.afterUseSkill leaves CAST (Skill.java:693)";
	EXPECT_EQ(Ptr<Creature>(npc)->getLifeStats()->getCurrentMp(), 81) << "the 19 MP end condition, paid by the npc";
}

// ------------------------------------------------------------------------------------------------------------------------- critical proc guard

TEST_F(SkillRulesTest, ATargetUnderANormalShieldIsNeverStumbled) {
	// SkillEngine.createCriticalProcEffect's first statement (SkillEngine.java:194-195), which M5b-1 had skipped (m5b-plan.md D14): with a NORMAL
	// shield up (ShieldType.NORMAL, bit 2 of Effect.shieldDefense) the answer is null before anything else is read - here, before the
	// unknown skill id 99999 is looked up, which without the shield dereferences a null template (SkillEngine.java:197-198)
	cp::PlayerFixture target = cp::makePlayer(410005, 9405, "Shielded");
	place(*target.player, 105.0f, 100.0f, 50.0f);
	EXPECT_THROW(SkillEngine::getInstance().createCriticalProcEffect(*caster.player, *target.player, 99999), runtime::NullPointerException)
		<< "no shield: the skill id filter runs";

	Ref<model::Effect> shield = model::Effect::create(*target.player, target.player, skillTemplate(HEAL_SKILL), 1);
	shield->setShieldDefense(2);
	target.player->getEffectController()->addEffect(*shield);
	ASSERT_TRUE(target.player->getEffectController()->isUnderNormalShield());
	EXPECT_FALSE(SkillEngine::getInstance().createCriticalProcEffect(*caster.player, *target.player, 99999));
	target.player->getEffectController()->removeAllEffects();
}

// ------------------------------------------------------------------------------------------------------------------------- penalty, charge, motion

TEST_F(SkillRulesTest, APenaltySkillIsCastWithoutPropertiesAndWithThePenaltyMethod) {
	// SkillEngine.getPenaltySkill -> new PenaltySkill(template, effector, level): the effector is its own first target and
	// initializeSkillMethod is PenaltySkill's (PENALTY), which a C++ base constructor cannot dispatch to (PenaltySkill.cpp)
	Ref<model::PenaltySkill> penalty = SkillEngine::getInstance().getPenaltySkill(*caster.player, INSTANT_SKILL, 1);
	ASSERT_TRUE(penalty);
	EXPECT_EQ(penalty->getSkillMethod(), model::Skill::SkillMethod::PENALTY);
	EXPECT_EQ(penalty->getFirstTarget(), Ptr<Creature>(caster.player));
	EXPECT_FALSE(SkillEngine::getInstance().getPenaltySkill(*caster.player, 99999, 1));

	// PenaltySkill.useSkill: useWithoutPropSkill - no SM_CASTSPELL (startCast is for CAST and ITEM), but the result (Skill.java:665-667)
	EXPECT_TRUE(penalty->useSkill());
	EXPECT_TRUE(packetsOf<SM_CASTSPELL>(sent()).empty());
	EXPECT_EQ(packetsOf<SM_CASTSPELL_RESULT>(sent()).size(), 1u);
}

TEST_F(SkillRulesTest, TheAnimationTimesAfterTheLastHitComeFromTheMotionTable) {
	const dataholders::MotionData& motions = *dataholders::DataManager::MOTION_DATA;
	// MotionData.java:62-76: motion id max(1, multiCastCount) = 1 for a plain skill; speed 100 -> motionSpeed 1000; no cast speed boost
	// (apply_casting_time_bonus is false), and the attack speed rate of a player without a weapon is 1500 / 1500 = 1
	std::optional<dataholders::MotionData::AnimationTimes> plain = motions.calculateAnimationTimesAfterLastHit(*caster.player, *skill(MOTION_SKILL));
	ASSERT_TRUE(plain.has_value());
	EXPECT_EQ(plain->lastHitMillis, 500) << "(int) (max 0.5 * 1000 * 1)";
	EXPECT_EQ(plain->fullDurationMillis, 1000) << "(int) (animation_length 1.0 * 1000 * 1)";

	// a ChargeSkill reads the times of its own motion id
	Ref<model::Skill> start = skill(CHARGE_START_SKILL);
	Ref<model::ChargeSkill> charged = SkillEngine::getInstance().getChargeSkill(*caster.player, CHARGED_SKILL_2, 1, 2, *start);
	ASSERT_TRUE(charged);
	EXPECT_EQ(charged->getMotionId(), 2);
	std::optional<dataholders::MotionData::AnimationTimes> second = motions.calculateAnimationTimesAfterLastHit(*caster.player, *charged);
	ASSERT_TRUE(second.has_value());
	EXPECT_EQ(second->lastHitMillis, 800);
	EXPECT_EQ(second->fullDurationMillis, 1500);

	// no <motion>: Java null
	EXPECT_FALSE(motions.calculateAnimationTimesAfterLastHit(*caster.player, *skill(INSTANT_SKILL)).has_value());
}

TEST_F(SkillRulesTest, APlayerCastWaitsForTheLastHitOfItsAnimation) {
	// Skill.endCast (Skill.java:665-680): after SM_CASTSPELL_RESULT, nextSkillUse = max(nextSkillUse, now + animation.lastHitMillis())
	int64_t before = commons::utils::currentTimeMillis();
	ASSERT_TRUE(skill(MOTION_SKILL)->useSkill());
	int64_t after = commons::utils::currentTimeMillis();
	EXPECT_GE(caster.player->getNextSkillUse(), before + 500);
	EXPECT_LE(caster.player->getNextSkillUse(), after + 500);
	EXPECT_FALSE(caster.player->isHitTimeBoosted()) << "no cast speed boost: setHitTimeBoost(0, 0)";
}

TEST_F(SkillRulesTest, UseChargeSkillCastsTheChargedStepThroughTheEngine) {
	// CreatureController.useChargeSkill (CreatureController.java:468-493): the step whose summed time reaches the charge time, capped at the last
	// but one index's successor; then SkillEngine.getChargeSkill(owner, id, level, index + 1, startSkill).useSkill(), and the start skill is
	// cancelled in the finally block. The start skill never cast, so its cast speed is 0 and every step counts 0 ms: the loop stops at index 1.
	Ref<model::Skill> start = skill(CHARGE_START_SKILL);
	EXPECT_TRUE(caster.player->getController().useChargeSkill(*start, 2000));
	std::vector<std::vector<uint8_t>> results = packetsOf<SM_CASTSPELL_RESULT>(sent());
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(decodeCastSpellResult(results[0]).skillId, CHARGED_SKILL_2);
	EXPECT_EQ(runtime::unportedHitCount(), 0u) << "the chargeSkillGetAndUse stand-in is gone";
}

} // namespace
} // namespace aion::gameserver::skillengine::test
