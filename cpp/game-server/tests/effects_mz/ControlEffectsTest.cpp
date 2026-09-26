// P5-04, M5b-2 stage 1 part 3 (m5b2-plan.md F-03, F-05, D13): the effect classes that hold a creature - RootEffect (the gate's 1328 Root),
// StunEffect, SlowEffect, SnareEffect, SanctuaryEffect and the two knock-backs of the critical proc, StumbleEffect (8218) and StaggerEffect
// (8217). Each case drives a real Effect through calculate -> applyEffect -> startEffect -> endEffect (EffectsMzTestSupport.h) and asserts
// what the Java bodies do on the way: the resistance stat calculate passes on, the abnormal state set on the effect and on the effected's
// controller (and cleared again), the observers, the stat functions, the packets and the duration. Expectations are read off the Java files
// cited per case; the abnormal ids are AbnormalState.java's.

#include "EffectsMzTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORCED_MOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_CANCEL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/model/EffectResult.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"
#include "aion/gameserver/skillengine/model/SpellStatus.h"
#include "aion/gameserver/skillengine/model/SubEffectType.h"

namespace aion::gameserver::skillengine::effect::mztest {
namespace {

using network::aion::serverpackets::SM_ABNORMAL_EFFECT;
using network::aion::serverpackets::SM_ABNORMAL_STATE;
using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_FORCED_MOVE;
using network::aion::serverpackets::SM_SKILL_CANCEL;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

class ControlEffectsTest : public EffectsMzTest {
protected:
	/**
	 * FlyController.onStopGliding on a gliding player that does not fly (glide()): the gliding states are cleared and SM_EMOTION STOP_GLIDE goes
	 * to the player and the players who see it (FlyController.java:37-49; its updateStatsAndSpeedVisually sends a speed SM_EMOTION as well)
	 */
	void expectGlideStopped(Player& p) {
		EXPECT_FALSE(p.isInGlidingState()) << "player.getFlyController().onStopGliding()";
		const std::vector<uint8_t> stopGlide = cp::serialized(SM_EMOTION(p, gameserver::model::EmotionType::STOP_GLIDE), &connection(p));
		std::vector<std::vector<uint8_t>> emotions = sentTo<SM_EMOTION>(p);
		EXPECT_EQ(std::count(emotions.begin(), emotions.end(), stopGlide), 1) << "SM_EMOTION STOP_GLIDE";
	}
};

// ---- RootEffect (RootEffect.java:26-60) -----------------------------------------------------------------------------------------------------

/**
 * 1328 Root as the gate's Mage casts it at a monster: calculate lands (the monster has no root resistance, and accmod2 500 against its magical
 * resist leaves no resist roll that can succeed); startEffect sets ROOT on the effect and on the monster's controller and adds the ATTACKED
 * observer; the players who know the monster see the effect in the DEBUFF slot for 20,000 ms (duration2); the end task ends it at exactly
 * 20,000 ms, which clears ROOT (endEffect) and takes the observer off again (Effect.endEffect -> removeObservers: cycles.toml RootEffect$1).
 */
TEST_F(ControlEffectsTest, Root1328HoldsAMonsterTwentySecondsInTheDebuffSlot) {
	EFFECT_TEST_SCOPE;
	Ref<Player> mage = player(5101);
	Ref<Npc> npc = monster();
	pair(*npc, *mage);
	ASSERT_FALSE(npc->getObserveController()->hasObservers());
	clearSent(*mage);

	Ref<Effect> effect = calculated(1328, *mage, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1)) << "RootEffect.calculate landed";
	effect->applyEffect();

	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::ROOT));
	EXPECT_EQ(effect->getAbnormals(), ROOT_ID) << "effect.setAbnormal(AbnormalState.ROOT)";
	EXPECT_EQ(effect->getDuration(), 20000);
	EXPECT_EQ(effect->getTargetSlot(), model::SkillTargetSlot::DEBUFF);
	EXPECT_TRUE(npc->getObserveController()->hasObservers()) << "the ATTACKED observer of RootEffect.startEffect";
	std::vector<std::vector<uint8_t>> announced = sentTo<SM_ABNORMAL_EFFECT>(*mage);
	ASSERT_EQ(announced.size(), 1u);
	AbnormalEffectFields shown = decodeAbnormalEffect(announced[0]);
	EXPECT_EQ(shown.effectedId, npc->getObjectId());
	EXPECT_EQ(shown.effectType, 1) << "an npc";
	EXPECT_EQ(shown.abnormals, ROOT_ID);
	EXPECT_EQ(shown.slots, DEBUFF_SLOT_ID);
	ASSERT_EQ(shown.effects.size(), 1u);
	EXPECT_EQ(shown.effects[0].skillId, 1328);
	EXPECT_EQ(shown.effects[0].level, 1);
	EXPECT_EQ(shown.effects[0].targetSlotOrdinal, DEBUFF_ORDINAL);
	EXPECT_EQ(shown.effects[0].remainingTime, 20000);
	clearSent(*mage);

	advance(19999);
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::ROOT)) << "one millisecond before duration2";
	advance(1);
	EXPECT_TRUE(effect->isEndedByTime());
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::ROOT)) << "RootEffect.endEffect: unsetAbnormal(ROOT)";
	EXPECT_FALSE(npc->getObserveController()->hasObservers()) << "the observer went with the effect";
	announced = sentTo<SM_ABNORMAL_EFFECT>(*mage);
	ASSERT_EQ(announced.size(), 1u);
	shown = decodeAbnormalEffect(announced[0]);
	EXPECT_EQ(shown.abnormals, 0);
	EXPECT_TRUE(shown.effects.empty());
}

/** RootEffect.calculate passes ROOT_RESISTANCE to EffectTemplate.calculate (RootEffect.java:33): 1000 of it resists, 1000 of another does not */
TEST_F(ControlEffectsTest, RootResistanceDecidesTheRootsCalculate) {
	EFFECT_TEST_SCOPE;
	Ref<Player> mage = player(5111);
	Ref<Npc> rootResistant = monster(505, 500);
	skillengine::test::addStat(*rootResistant, StatEnum::ROOT_RESISTANCE, 1000);
	Ref<Npc> stunResistant = monster(505, 505);
	skillengine::test::addStat(*stunResistant, StatEnum::STUN_RESISTANCE, 1000);

	Ref<Effect> resisted = calculated(1328, *mage, *rootResistant);
	EXPECT_FALSE(resisted->isInSuccessEffects(1)) << "effect power 1000 - 1000: no roll of Rnd.get(1, 1000) is <= 0";
	EXPECT_EQ(resisted->getEffectResult(), model::EffectResult::RESIST);
	EXPECT_TRUE(calculated(1328, *mage, *stunResistant)->isInSuccessEffects(1));
}

/**
 * The ATTACKED observer (RootEffect.java:47-54): an attack ends the root when Rnd.chance() >= resistchance (10 for 1328) and keeps it below -
 * removeEffect(effect.getSkillId()) on the effected's controller, which ends the effect and with it the observer.
 */
TEST_F(ControlEffectsTest, AnAttackBreaksTheRootFromItsResistChanceOn) {
	EFFECT_TEST_SCOPE;
	Ref<Player> mage = player(5121);
	Ref<Npc> npc = monster();
	Ref<Effect> effect = applied(1328, *mage, *npc);
	ASSERT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::ROOT));

	seedWhereFirstChance([](float chance) { return chance < 10.0f; });
	npc->getObserveController()->notifyAttackedObservers(*mage, 0);
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::ROOT)) << "a chance below 10 keeps the root";
	EXPECT_TRUE(npc->getEffectController()->hasAbnormalEffect(1328));

	seedWhereFirstChance([](float chance) { return chance >= 10.0f && chance < 11.0f; });
	npc->getObserveController()->notifyAttackedObservers(*mage, 0);
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::ROOT)) << "a chance of 10 and more breaks it";
	EXPECT_FALSE(npc->getEffectController()->hasAbnormalEffect(1328));
	EXPECT_FALSE(effect->isEndedByTime()) << "ended by the observer, not by its end task";
	EXPECT_FALSE(npc->getObserveController()->hasObservers());
}

/**
 * A rooted player (RootEffect.java:42-45): the root stops a glide and aborts the movement (PlayerMoveController.abortMove: no longer in move),
 * and the player's own icons (SM_ABNORMAL_STATE) show the npc's root with the ROOT bit.
 */
TEST_F(ControlEffectsTest, ARootedPlayerStopsMovingAndSeesTheRootIcon) {
	EFFECT_TEST_SCOPE;
	Ref<Player> target = player(5131);
	Ref<Npc> npc = monster();
	target->getMoveController()->setInMove(true);
	glide(*target);
	clearSent(*target);

	Ref<Effect> effect = applied(1328, *npc, *target);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	expectGlideStopped(*target);
	EXPECT_FALSE(target->getMoveController()->isInMove()) << "player.getMoveController().abortMove()";
	EXPECT_TRUE(target->getEffectController()->isAbnormalSet(AbnormalState::ROOT));
	std::vector<std::vector<uint8_t>> icons = sentTo<SM_ABNORMAL_STATE>(*target);
	ASSERT_EQ(icons.size(), 1u);
	AbnormalStateFields state = decodeAbnormalState(icons[0]);
	EXPECT_EQ(state.abnormals, ROOT_ID);
	EXPECT_EQ(state.slot, DEBUFF_SLOT_ID);
	ASSERT_EQ(state.effects.size(), 1u);
	EXPECT_EQ(state.effects[0].effectorId, npc->getObjectId());
	EXPECT_EQ(state.effects[0].skillId, 1328);
	EXPECT_EQ(state.effects[0].targetSlotOrdinal, DEBUFF_ORDINAL);
	EXPECT_EQ(state.effects[0].remainingTime, 20000);
	clearSent(*target);

	effect->endEffect();
	EXPECT_FALSE(target->getEffectController()->isAbnormalSet(AbnormalState::ROOT));
	icons = sentTo<SM_ABNORMAL_STATE>(*target);
	ASSERT_EQ(icons.size(), 1u);
	state = decodeAbnormalState(icons[0]);
	EXPECT_EQ(state.abnormals, 0);
	EXPECT_TRUE(state.effects.empty());
}

// ---- StunEffect (StunEffect.java:17-45) -----------------------------------------------------------------------------------------------------

/**
 * A monster's stun ends the player's cast (cancelCurrentSkill(effector): SM_SKILL_CANCEL to the player), stops its glide, aborts its movement,
 * and holds it in STUN for duration2 3,000 ms. (Between two players out of a duel PlayerEffectController.addEffect refuses the debuff:
 * checkDuelCondition.)
 */
TEST_F(ControlEffectsTest, AStunCancelsTheCastAndHoldsThreeSeconds) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> attacker = monster(500, 500, 100);
	Ref<Player> target = player(5202, gameserver::model::PlayerClass::MAGE, 1, 503, 500, 100);
	Ref<model::Skill> cast = model::Skill::create(skillTemplate(1282), *target, Ptr<Creature>(attacker), 1);
	target->setCasting(cast);
	target->getMoveController()->setInMove(true);
	glide(*target);
	clearSent(*target);

	Ref<Effect> effect = applied(64001, *attacker, *target);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	expectGlideStopped(*target);
	EXPECT_FALSE(target->getCastingSkill()) << "effected.getController().cancelCurrentSkill(effect.getEffector())";
	EXPECT_EQ(sentTo<SM_SKILL_CANCEL>(*target).size(), 1u);
	EXPECT_FALSE(target->getMoveController()->isInMove()) << "abortMove";
	EXPECT_TRUE(target->getEffectController()->isAbnormalSet(AbnormalState::STUN));
	EXPECT_EQ(effect->getAbnormals(), STUN_ID);
	EXPECT_EQ(effect->getDuration(), 3000);
	std::vector<std::vector<uint8_t>> icons = sentTo<SM_ABNORMAL_STATE>(*target);
	ASSERT_EQ(icons.size(), 1u);
	EXPECT_EQ(decodeAbnormalState(icons[0]).abnormals, STUN_ID);

	advance(3000);
	EXPECT_TRUE(effect->isEndedByTime());
	EXPECT_FALSE(target->getEffectController()->isAbnormalSet(AbnormalState::STUN)) << "StunEffect.endEffect";
}

/**
 * A player stunning a casting monster: cancelCurrentSkill(effector) ends the monster's cast (CreatureController: SM_SKILL_CANCEL to the players
 * who know it) and tells the effector STR_SKILL_TARGET_SKILL_CANCELED - the effector StunEffect.startEffect passes in.
 */
TEST_F(ControlEffectsTest, AStunnedMonsterLosesItsCastAndTheEffectorIsTold) {
	EFFECT_TEST_SCOPE;
	Ref<Player> attacker = player(5203);
	Ref<Npc> npc = monster();
	pair(*npc, *attacker);
	npc->setCasting(model::Skill::create(skillTemplate(1282), *npc, 1, Ptr<Creature>(attacker), nullptr));
	clearSent(*attacker);

	Ref<Effect> effect = applied(64001, *attacker, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_FALSE(npc->getCastingSkill());
	EXPECT_EQ(sentTo<SM_SKILL_CANCEL>(*attacker).size(), 1u);
	std::vector<std::vector<uint8_t>> messages = sentTo<SM_SYSTEM_MESSAGE>(*attacker);
	ASSERT_EQ(messages.size(), 1u);
	EXPECT_EQ(messages[0], cp::serialized(SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_SKILL_CANCELED(), &connection(*attacker)));
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::STUN));
}

/** StunEffect.calculate passes STUN_RESISTANCE (StunEffect.java:26) */
TEST_F(ControlEffectsTest, StunResistanceDecidesTheStunsCalculate) {
	EFFECT_TEST_SCOPE;
	Ref<Player> attacker = player(5211);
	Ref<Npc> stunResistant = monster(505, 500);
	skillengine::test::addStat(*stunResistant, StatEnum::STUN_RESISTANCE, 1000);
	Ref<Npc> rootResistant = monster(505, 505);
	skillengine::test::addStat(*rootResistant, StatEnum::ROOT_RESISTANCE, 1000);

	EXPECT_FALSE(calculated(64001, *attacker, *stunResistant)->isInSuccessEffects(1));
	EXPECT_TRUE(calculated(64001, *attacker, *rootResistant)->isInSuccessEffects(1));
}

// ---- SlowEffect and SnareEffect (SlowEffect.java:15-39, SnareEffect.java:15-39): BufEffect with a state ----------------------------------

/**
 * 873's <slow>: BufEffect.startEffect adds the ATTACK_SPEED rate function (+50 %), SlowEffect adds SLOW to the effect and the controller; the
 * end task (6,000 ms) removes both (Effect.endEffects -> CreatureGameStats.endEffect, SlowEffect.endEffect -> unsetAbnormal).
 */
TEST_F(ControlEffectsTest, ASlowAddsFiftyPercentAttackDelayForSixSeconds) {
	EFFECT_TEST_SCOPE;
	Ref<Player> mage = player(5301);
	Ref<Npc> npc = monster();
	const int32_t attackSpeed = npc->getGameStats()->getAttackSpeed()->getCurrent();
	ASSERT_EQ(attackSpeed, 2000) << "the template's attack_speed";

	Ref<Effect> effect = applied(64002, *mage, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	// StatRateFunction (bonus): 2000 * 50 / 100f = 1000 on top of the base
	EXPECT_EQ(npc->getGameStats()->getAttackSpeed()->getCurrent(), 3000);
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::SLOW));
	EXPECT_EQ(effect->getAbnormals(), SLOW_ID);
	EXPECT_EQ(effect->getDuration(), 6000);

	advance(6000);
	EXPECT_EQ(npc->getGameStats()->getAttackSpeed()->getCurrent(), 2000) << "the rate function went with the effect";
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::SLOW));
}

/** SlowEffect.calculate passes SLOW_RESISTANCE, SnareEffect.calculate SNARE_RESISTANCE (SlowEffect.java:24, SnareEffect.java:24) */
TEST_F(ControlEffectsTest, SlowAndSnareResistanceDecideTheirCalculate) {
	EFFECT_TEST_SCOPE;
	Ref<Player> mage = player(5311);
	Ref<Npc> slowResistant = monster(505, 500);
	skillengine::test::addStat(*slowResistant, StatEnum::SLOW_RESISTANCE, 1000);
	Ref<Npc> snareResistant = monster(505, 505);
	skillengine::test::addStat(*snareResistant, StatEnum::SNARE_RESISTANCE, 1000);

	EXPECT_FALSE(calculated(64002, *mage, *slowResistant)->isInSuccessEffects(1));
	EXPECT_TRUE(calculated(64002, *mage, *snareResistant)->isInSuccessEffects(1));
	EXPECT_FALSE(calculated(64003, *mage, *snareResistant)->isInSuccessEffects(1));
	EXPECT_TRUE(calculated(64003, *mage, *slowResistant)->isInSuccessEffects(1));
}

/**
 * 303's <snare>: -50 % SPEED and FLY_SPEED (StatRateFunction: base 6000 -> 3000, the run speed of a player in mm/s), SNARE on the controller and
 * the effect, and the player's icon; endEffect restores both.
 */
TEST_F(ControlEffectsTest, ASnareHalvesTheSpeedsForTenSeconds) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = monster();
	Ref<Player> target = player(5321);
	const int32_t speed = target->getGameStats()->getMovementSpeed()->getCurrent();
	const int32_t flySpeed = target->getGameStats()->getStat(StatEnum::FLY_SPEED, 9000)->getCurrent();
	clearSent(*target);

	Ref<Effect> effect = applied(64003, *npc, *target);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(target->getGameStats()->getMovementSpeed()->getCurrent(), speed - speed * 50 / 100);
	EXPECT_EQ(target->getGameStats()->getStat(StatEnum::FLY_SPEED, 9000)->getCurrent(), flySpeed - 4500);
	EXPECT_TRUE(target->getEffectController()->isAbnormalSet(AbnormalState::SNARE));
	EXPECT_EQ(effect->getAbnormals(), SNARE_ID);
	EXPECT_EQ(effect->getDuration(), 10000);
	std::vector<std::vector<uint8_t>> icons = sentTo<SM_ABNORMAL_STATE>(*target);
	ASSERT_EQ(icons.size(), 1u);
	EXPECT_EQ(decodeAbnormalState(icons[0]).abnormals, SNARE_ID);

	effect->endEffect();
	EXPECT_EQ(target->getGameStats()->getMovementSpeed()->getCurrent(), speed);
	EXPECT_FALSE(target->getEffectController()->isAbnormalSet(AbnormalState::SNARE)) << "SnareEffect.endEffect";
}

// ---- SanctuaryEffect (SanctuaryEffect.java:8-27) --------------------------------------------------------------------------------------------

/**
 * A sanctuary on oneself targets oneself (effector.equals(effected) -> setTarget(effected)); on someone else it leaves the target alone. It sets
 * SANCTUARY (1 << 31, the sign bit of the int) until it ends. 18191 Captain's Pride lasts a day (duration2 86,400,000).
 */
TEST_F(ControlEffectsTest, ASanctuaryOnOneselfTargetsOneselfAndSetsTheSanctuaryBit) {
	EFFECT_TEST_SCOPE;
	Ref<Player> self = player(5401);
	Ref<Player> other = player(5402, gameserver::model::PlayerClass::PRIEST, 1, 502, 500, 100);
	ASSERT_FALSE(self->getTarget());

	Ref<Effect> effect = applied(64022, *self, *self);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(self->getTarget(), Ptr<gameserver::model::gameobjects::VisibleObject>(self)) << "the effected targets itself";
	EXPECT_TRUE(self->getEffectController()->isAbnormalSet(AbnormalState::SANCTUARY));
	EXPECT_EQ(effect->getAbnormals(), SANCTUARY_ID);
	EXPECT_EQ(effect->getDuration(), 8000);
	advance(8000);
	EXPECT_FALSE(self->getEffectController()->isAbnormalSet(AbnormalState::SANCTUARY)) << "SanctuaryEffect.endEffect";

	Ref<Effect> onOther = applied(18191, *self, *other);
	ASSERT_TRUE(onOther->isInSuccessEffects(1));
	EXPECT_FALSE(other->getTarget()) << "another effector: the effected's target is left alone";
	EXPECT_TRUE(other->getEffectController()->isAbnormalSet(AbnormalState::SANCTUARY));
	EXPECT_EQ(onOther->getDuration(), 86400000);
	onOther->endEffect();
	EXPECT_FALSE(other->getEffectController()->isAbnormalSet(AbnormalState::SANCTUARY));
}

// ---- StumbleEffect and StaggerEffect (StumbleEffect.java:25-77, StaggerEffect.java:25-75) -----------------------------------------------

/**
 * 8218, the critical proc of POLEARM, STAFF and GREATSWORD attacks (D13): calculate lands (noresist) with SpellStatus.STUMBLE and computes the
 * knock-back - two metres from the effected, away from the effector: the heading from (500, 500) towards (505, 500) is 0, i.e. 0 degrees, so
 * (cos 0 * 2, sin 0 * 2) = (2, 0) is added, and the empty GeoMap has no collision on the way (507, 500, 100). startEffect moves the monster
 * there (World.updatePosition, keeping the monster's own heading) and sets STUMBLE for duration1 2,000 ms x skill level 1; an npc gets no
 * SM_FORCED_MOVE. The warrior is in an active region (addToRegion), so the monster still knows it after the move: its SM_ABNORMAL_EFFECT
 * (EffectController.addEffect broadcasts it after startEffect) reaches the warrior, and a forced move broadcast would too.
 */
TEST_F(ControlEffectsTest, AStumbleKnocksTheMonsterTwoMetresAwayFromTheEffector) {
	EFFECT_TEST_SCOPE;
	Ref<Player> warrior = player(5501, gameserver::model::PlayerClass::WARRIOR, 1, 500, 500, 100);
	Ref<Npc> npc = monster(505, 500, 100);
	pair(*npc, *warrior);
	addToRegion(*warrior);
	npc->getPosition()->setH(int8_t{42});
	clearSent(*warrior);

	Ref<Effect> effect = calculated(8218, *warrior, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getSpellStatus(), model::SpellStatus::STUMBLE);
	EXPECT_EQ(effect->getSubEffectType(), model::SubEffectType::NONE) << "not a sub effect";
	EXPECT_EQ(effect->getTargetX(), 507.0f);
	EXPECT_EQ(effect->getTargetY(), 500.0f);
	EXPECT_EQ(effect->getTargetZ(), 100.0f);

	effect->applyEffect();
	EXPECT_EQ(npc->getX(), 507.0f) << "World.updatePosition to the target location";
	EXPECT_EQ(npc->getY(), 500.0f);
	EXPECT_EQ(npc->getHeading(), 42) << "updatePosition(..., effected.getHeading())";
	EXPECT_TRUE(npc->getEffectController()->isAbnormalSet(AbnormalState::STUMBLE));
	EXPECT_EQ(effect->getAbnormals(), STUMBLE_ID);
	EXPECT_EQ(effect->getDuration(), 2000);
	std::vector<std::vector<uint8_t>> announced = sentTo<SM_ABNORMAL_EFFECT>(*warrior);
	ASSERT_EQ(announced.size(), 1u) << "the monster's broadcasts still reach the warrior after the move";
	EXPECT_EQ(decodeAbnormalEffect(announced[0]).abnormals, STUMBLE_ID);
	EXPECT_TRUE(sentTo<SM_FORCED_MOVE>(*warrior).empty()) << "only a stumbled player is force-moved on the clients";
	advance(2000);
	EXPECT_FALSE(npc->getEffectController()->isAbnormalSet(AbnormalState::STUMBLE)) << "StumbleEffect.endEffect";
}

/**
 * A stumbled player (StumbleEffect.java:35-46): its cast ends, its glide and its movement stop (onStopGliding, onStopMove), it is moved with
 * its own heading, and the knock-back is broadcast as SM_FORCED_MOVE to itself and the players who know it; as a sub effect it keeps
 * SubEffectType.NONE (only an npc's is STUMBLE). North of the effector: heading 30 (90 degrees), so y + 2.
 */
TEST_F(ControlEffectsTest, AStumbledPlayerIsForceMovedAndLosesItsCast) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = monster(500, 500, 100);
	Ref<Player> target = player(5511, gameserver::model::PlayerClass::MAGE, 1, 500, 505, 100);
	Ref<model::Skill> cast = model::Skill::create(skillTemplate(1282), *target, Ptr<Creature>(npc), 1);
	target->setCasting(cast);
	target->getMoveController()->setInMove(true);
	glide(*target);
	target->getPosition()->setH(int8_t{42});
	clearSent(*target);

	Ref<Effect> effect = Effect::create(*npc, Ptr<Creature>(target), skillTemplate(8218), 1, std::nullopt, nullptr, true, nullptr);
	effect->initialize();
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(effect->getSubEffectType(), model::SubEffectType::NONE) << "a player effected: no sub effect type";
	EXPECT_NEAR(effect->getTargetX(), 500.0f, 0.0001f);
	EXPECT_EQ(effect->getTargetY(), 507.0f);

	effect->applyEffect();
	EXPECT_FALSE(target->getCastingSkill()) << "cancelCurrentSkill";
	expectGlideStopped(*target);
	EXPECT_FALSE(target->getMoveController()->isInMove()) << "player.getController().onStopMove()";
	EXPECT_EQ(target->getY(), 507.0f);
	EXPECT_EQ(target->getHeading(), 42) << "updatePosition(..., effected.getHeading())";
	EXPECT_EQ(sentTo<SM_FORCED_MOVE>(*target).size(), 1u) << "broadcastPacketAndReceive";
	EXPECT_TRUE(target->getEffectController()->isAbnormalSet(AbnormalState::STUMBLE));
}

/**
 * An npc stumbled as a sub effect gets SubEffectType.STUMBLE (StumbleEffect.java:62-63), a staggered one STAGGER (StaggerEffect.java:59-60);
 * each half of `effect.isSubEffect() && !(effect.getEffected() instanceof Player)` alone leaves a stagger's NONE: an npc staggered by a
 * skill of its own (not a sub effect), and a player staggered as a sub effect.
 */
TEST_F(ControlEffectsTest, AnNpcKnockedBackAsASubEffectCarriesTheSubEffectType) {
	EFFECT_TEST_SCOPE;
	Ref<Player> warrior = player(5521, gameserver::model::PlayerClass::WARRIOR);
	Ref<Npc> npc = monster();
	Ref<Effect> stumble = Effect::create(*warrior, Ptr<Creature>(npc), skillTemplate(8218), 1, std::nullopt, nullptr, true, nullptr);
	stumble->initialize();
	EXPECT_EQ(stumble->getSubEffectType(), model::SubEffectType::STUMBLE);
	Ref<Effect> stagger = Effect::create(*warrior, Ptr<Creature>(npc), skillTemplate(8217), 1, std::nullopt, nullptr, true, nullptr);
	stagger->initialize();
	EXPECT_EQ(stagger->getSubEffectType(), model::SubEffectType::STAGGER);
	EXPECT_EQ(stagger->getSpellStatus(), model::SpellStatus::STAGGER);

	Ref<Effect> ownStagger = calculated(8217, *warrior, *npc);
	ASSERT_TRUE(ownStagger->isInSuccessEffects(1));
	EXPECT_EQ(ownStagger->getSubEffectType(), model::SubEffectType::NONE) << "not a sub effect";
	Ref<Player> staggeredPlayer = player(5522, gameserver::model::PlayerClass::MAGE, 1, 510, 500, 100);
	Ref<Effect> playerStagger = Effect::create(*npc, Ptr<Creature>(staggeredPlayer), skillTemplate(8217), 1, std::nullopt, nullptr, true, nullptr);
	playerStagger->initialize();
	ASSERT_TRUE(playerStagger->isInSuccessEffects(1));
	EXPECT_EQ(playerStagger->getSubEffectType(), model::SubEffectType::NONE) << "a player effected";
}

/**
 * A creature already in one of the knock-back states takes no second one: calculate returns before EffectTemplate.calculate for OPENAERIAL,
 * PULLED, STUMBLE and STAGGER (StumbleEffect.java:53-58, StaggerEffect.java:51-55), so the effect has no success effect.
 */
TEST_F(ControlEffectsTest, AKnockedBackCreatureTakesNoSecondKnockBack) {
	EFFECT_TEST_SCOPE;
	Ref<Player> warrior = player(5531, gameserver::model::PlayerClass::WARRIOR);
	for (AbnormalState state : {AbnormalState::OPENAERIAL, AbnormalState::PULLED, AbnormalState::STUMBLE, AbnormalState::STAGGER}) {
		Ref<Npc> npc = monster();
		npc->getEffectController()->setAbnormal(state);
		EXPECT_FALSE(calculated(8218, *warrior, *npc)->isInSuccessEffects(1)) << xml::enumName(state);
		EXPECT_FALSE(calculated(8217, *warrior, *npc)->isInSuccessEffects(1)) << xml::enumName(state);
	}
	Ref<Npc> unaffected = monster();
	EXPECT_TRUE(calculated(8218, *warrior, *unaffected)->isInSuccessEffects(1));
	EXPECT_TRUE(calculated(8217, *warrior, *unaffected)->isInSuccessEffects(1));
}

/**
 * Without noresist the resistance stat decides: StumbleEffect passes STUMBLE_RESISTANCE, StaggerEffect STAGGER_RESISTANCE (StumbleEffect.java:60,
 * StaggerEffect.java:57). 64023 and 64024 are 8218's and 8217's effects without noresist.
 */
TEST_F(ControlEffectsTest, StumbleAndStaggerResistanceDecideTheirCalculate) {
	EFFECT_TEST_SCOPE;
	Ref<Player> warrior = player(5561, gameserver::model::PlayerClass::WARRIOR);
	Ref<Npc> stumbleResistant = monster(505, 500);
	skillengine::test::addStat(*stumbleResistant, StatEnum::STUMBLE_RESISTANCE, 1000);
	Ref<Npc> staggerResistant = monster(505, 505);
	skillengine::test::addStat(*staggerResistant, StatEnum::STAGGER_RESISTANCE, 1000);

	EXPECT_FALSE(calculated(64023, *warrior, *stumbleResistant)->isInSuccessEffects(1));
	EXPECT_TRUE(calculated(64023, *warrior, *staggerResistant)->isInSuccessEffects(1));
	EXPECT_FALSE(calculated(64024, *warrior, *staggerResistant)->isInSuccessEffects(1));
	EXPECT_TRUE(calculated(64024, *warrior, *stumbleResistant)->isInSuccessEffects(1));
}

/**
 * A stumble ends the effected's stuns (removeStunEffects, StumbleEffect.java:37); a stagger does not (StaggerEffect.java:36 removes only the
 * paralyze effects). Both end the paralyze effects (removeParalyzeEffects): 64029, a root whose skill carries a <paralyze> and no <stun>, so
 * only that call can end it. Both set their own state and move the effected; 8217 staggers for duration1 2,000 ms.
 */
TEST_F(ControlEffectsTest, AStumbleEndsTheStunAndAStaggerKeepsIt) {
	EFFECT_TEST_SCOPE;
	Ref<Player> warrior = player(5541, gameserver::model::PlayerClass::WARRIOR, 1, 500, 500, 100);
	Ref<Npc> stumbled = monster(505, 500, 100);
	Ref<Effect> firstStun = applied(64001, *warrior, *stumbled);
	ASSERT_TRUE(stumbled->getEffectController()->isAbnormalSet(AbnormalState::STUN));
	rootedWithParalyze(*warrior, *stumbled);
	ASSERT_TRUE(stumbled->getEffectController()->hasAbnormalEffect(64029));
	applied(8218, *warrior, *stumbled);
	EXPECT_FALSE(stumbled->getEffectController()->isAbnormalSet(AbnormalState::STUN)) << "the stun was removed";
	EXPECT_FALSE(stumbled->getEffectController()->hasAbnormalEffect(64001));
	EXPECT_FALSE(stumbled->getEffectController()->hasAbnormalEffect(64029)) << "removeParalyzeEffects";
	EXPECT_TRUE(stumbled->getEffectController()->isAbnormalSet(AbnormalState::STUMBLE));

	Ref<Npc> staggered = monster(500, 505, 100);
	applied(64001, *warrior, *staggered);
	rootedWithParalyze(*warrior, *staggered);
	ASSERT_TRUE(staggered->getEffectController()->hasAbnormalEffect(64029));
	Ref<Effect> stagger = applied(8217, *warrior, *staggered);
	ASSERT_TRUE(stagger->isInSuccessEffects(1));
	EXPECT_TRUE(staggered->getEffectController()->isAbnormalSet(AbnormalState::STUN)) << "a stagger does not remove stuns";
	EXPECT_FALSE(staggered->getEffectController()->hasAbnormalEffect(64029)) << "removeParalyzeEffects";
	EXPECT_TRUE(staggered->getEffectController()->isAbnormalSet(AbnormalState::STAGGER));
	EXPECT_EQ(stagger->getAbnormals(), STAGGER_ID);
	EXPECT_EQ(stagger->getDuration(), 2000);
	EXPECT_EQ(staggered->getY(), 507.0f) << "north of the effector: y + 2";
	advance(2000);
	EXPECT_FALSE(staggered->getEffectController()->isAbnormalSet(AbnormalState::STAGGER)) << "StaggerEffect.endEffect";
}

/**
 * A monster knocked back while it casts loses the cast (cancelCurrentSkill(effector), StumbleEffect.java:35, StaggerEffect.java:35), and the
 * effector is told STR_SKILL_TARGET_SKILL_CANCELED. For an npc nothing else of startEffect ends a cast (a player's onStopMove may).
 */
TEST_F(ControlEffectsTest, AKnockedBackMonsterLosesItsCast) {
	EFFECT_TEST_SCOPE;
	Ref<Player> warrior = player(5571, gameserver::model::PlayerClass::WARRIOR);
	for (int32_t skillId : {8218, 8217}) {
		Ref<Npc> npc = monster();
		npc->setCasting(model::Skill::create(skillTemplate(1282), *npc, 1, Ptr<Creature>(warrior), nullptr));
		clearSent(*warrior);
		Ref<Effect> effect = applied(skillId, *warrior, *npc);
		ASSERT_TRUE(effect->isInSuccessEffects(1)) << skillId;
		EXPECT_FALSE(npc->getCastingSkill()) << skillId;
		EXPECT_EQ(sentTo<SM_SYSTEM_MESSAGE>(*warrior).size(), 1u) << skillId;
	}
}

/**
 * A staggered player (StaggerEffect.java:33-47): cast cancelled, glide and movement stopped, moved with its own heading and force-moved on the
 * clients (SM_FORCED_MOVE with the effector, the target and the location), STAGGER in its icons.
 */
TEST_F(ControlEffectsTest, AStaggeredPlayerIsForceMovedAndLosesItsCast) {
	EFFECT_TEST_SCOPE;
	Ref<Npc> npc = monster(500, 500, 100);
	Ref<Player> target = player(5551, gameserver::model::PlayerClass::MAGE, 1, 505, 500, 100);
	Ref<model::Skill> cast = model::Skill::create(skillTemplate(1282), *target, Ptr<Creature>(npc), 1);
	target->setCasting(cast);
	target->getMoveController()->setInMove(true);
	glide(*target);
	target->getPosition()->setH(int8_t{42});
	clearSent(*target);

	Ref<Effect> effect = applied(8217, *npc, *target);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_FALSE(target->getCastingSkill());
	expectGlideStopped(*target);
	EXPECT_FALSE(target->getMoveController()->isInMove()) << "player.getController().onStopMove()";
	EXPECT_EQ(target->getX(), 507.0f);
	EXPECT_EQ(target->getHeading(), 42) << "updatePosition(..., effected.getHeading())";
	std::vector<std::vector<uint8_t>> moves = sentTo<SM_FORCED_MOVE>(*target);
	ASSERT_EQ(moves.size(), 1u);
	EXPECT_EQ(moves[0], cp::serialized(SM_FORCED_MOVE(*npc, target->getObjectId(), 507.0f, 500.0f, 100.0f), &connection(*target)));
	std::vector<std::vector<uint8_t>> icons = sentTo<SM_ABNORMAL_STATE>(*target);
	ASSERT_EQ(icons.size(), 1u);
	EXPECT_EQ(decodeAbnormalState(icons[0]).abnormals, STAGGER_ID);
}

/**
 * A staggered monster (StaggerEffect.java:40-43): moved two metres away from the effector with its own heading, and no SM_FORCED_MOVE, which
 * only a player effected broadcasts. As in AStumbleKnocksTheMonsterTwoMetresAwayFromTheEffector the warrior is in an active region, so the
 * monster's SM_ABNORMAL_EFFECT after the move shows that its broadcasts still reach it.
 */
TEST_F(ControlEffectsTest, AStaggeredMonsterIsMovedWithoutAForcedMove) {
	EFFECT_TEST_SCOPE;
	Ref<Player> warrior = player(5581, gameserver::model::PlayerClass::WARRIOR, 1, 500, 500, 100);
	Ref<Npc> npc = monster(500, 505, 100);
	pair(*npc, *warrior);
	addToRegion(*warrior);
	npc->getPosition()->setH(int8_t{42});
	clearSent(*warrior);

	Ref<Effect> effect = applied(8217, *warrior, *npc);
	ASSERT_TRUE(effect->isInSuccessEffects(1));
	EXPECT_EQ(npc->getY(), 507.0f) << "north of the effector: y + 2";
	EXPECT_EQ(npc->getHeading(), 42) << "updatePosition(..., effected.getHeading())";
	std::vector<std::vector<uint8_t>> announced = sentTo<SM_ABNORMAL_EFFECT>(*warrior);
	ASSERT_EQ(announced.size(), 1u) << "the monster's broadcasts still reach the warrior after the move";
	EXPECT_EQ(decodeAbnormalEffect(announced[0]).abnormals, STAGGER_ID);
	EXPECT_TRUE(sentTo<SM_FORCED_MOVE>(*warrior).empty()) << "only a staggered player is force-moved on the clients";
}

} // namespace
} // namespace aion::gameserver::skillengine::effect::mztest
