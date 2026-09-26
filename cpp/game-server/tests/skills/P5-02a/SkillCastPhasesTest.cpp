// P5-02a, M5b-2 stage 1 part 2, the cast review's fixer run: the phases of Skill.useSkill / startCast / endCast that SkillCastTest.cpp does not
// pin (Skill.java:274-706), and the arithmetic of the gate path that only runs with SecurityConfig.CHECK_ANIMATIONS on (true in production,
// security.properties:36; false in the unit tests): updateHitTime's ammo flight and tolerance, the cast-speed stats, the charge duration and the
// cooldown per skill level.
//
// Each case names the Java lines it follows. The configs a case changes (CHECK_ANIMATIONS, MIN_SKILL_CAST_INTERVAL_MILLIS, the audit log) are
// restored by a ConfigOverride (CastTestSupport.h) when the case ends.

#include "CastTestSupport.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/controllers/observer/StartMovingListener.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CASTSPELL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CASTSPELL_RESULT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_CANCEL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::test {
namespace {

using namespace std::chrono_literals;
using gameserver::model::gameobjects::Creature;
using gameserver::model::stats::container::StatEnum;
using network::aion::serverpackets::SM_CASTSPELL;
using network::aion::serverpackets::SM_CASTSPELL_RESULT;
using network::aion::serverpackets::SM_SKILL_CANCEL;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using network::test::LogCapture;
using runtime::Ptr;
using runtime::Ref;

const char* const AUDIT_LOGGER = "AUDIT_LOG"; // AuditLogger.cpp:22, Java AuditLogger.java: LoggerFactory.getLogger("AUDIT_LOG")

class SkillCastPhasesTest : public CastTest {
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

	/**
	 * Skill.getDistanceTolerance counts the caster's movement since its last move update, capped at 200 ms for a caster that is not moving
	 * (Skill.java:446-455). The fixture's player was created a few milliseconds ago (CreatureMoveController's constructor stamps the time), so the
	 * case waits until the cap applies: from then on the tolerance is the same in every run.
	 */
	void waitUntilTheMovementToleranceIsCapped() {
		while (commons::utils::currentTimeMillis() - caster.player->getMoveController()->getLastMoveUpdate() <= 200)
			std::this_thread::sleep_for(1ms);
	}

	/** Casts FLAME_BOLT_SKILL at the target with the client's hit time, then cancels it and clears the hit-time boost the cancel leaves */
	int32_t flameBoltHitTime(Ptr<Creature> target, int32_t clientHitTime) {
		Ref<model::Skill> cast = skill(FLAME_BOLT_SKILL, target);
		cast->setClientHitTime(clientHitTime);
		EXPECT_TRUE(cast->useSkill());
		int32_t hitTime = cast->getHitTime();
		caster.player->getController().cancelCurrentSkill(nullptr);
		// PlayerController.cancelCurrentSkill leaves setHitTimeBoost(Long.MAX_VALUE, castSpeed) for a skill that allows the boost
		// (PlayerController.java:528-529); the next cast must not see it, or its "cast speed" uncertainty factor disappears
		caster.player->setHitTimeBoost(0, 0);
		(*client)->clearSent();
		return hitTime;
	}
};

// ------------------------------------------------------------------------------------------------------------------------- the cast phases

TEST_F(SkillCastPhasesTest, ANonInstantHitAppliesTheEffectsAtTheHitTimeNotAtTheEndOfTheCast) {
	// Skill.endCast (Skill.java:659-663): `if (isInstantSkill() || isItemSkill) applyEffect(effects); else schedule(() -> applyEffect(effects),
	// hitTime)` - Java schedules the task even for an empty effect list. isInstantSkill is `hitTime == 0 || motion.isInstantSkill()`
	// (Skill.java:1084-1086); with CHECK_ANIMATIONS off the hit time is the client's (updateHitTime's first statement, :417)
	Ref<model::Skill> cast = skill(INSTANT_SKILL);
	cast->setClientHitTime(300);
	ASSERT_EQ(executor->pendingTaskCount(), 0u);
	ASSERT_TRUE(cast->useSkill());
	EXPECT_EQ(cast->getHitTime(), 300);
	EXPECT_FALSE(cast->isInstantSkill());
	std::vector<std::vector<uint8_t>> results = packetsOf<SM_CASTSPELL_RESULT>(sent());
	ASSERT_EQ(results.size(), 1u) << "duration 0: endCast ran inside useSkill";
	EXPECT_EQ(decodeCastSpellResult(results[0]).hitTime, 300) << "the client learns the hit time from SM_CASTSPELL_RESULT";

	EXPECT_EQ(executor->pendingTaskCount(), 1u) << "the applyEffect task, due at the hit time";
	EXPECT_EQ(advance(299ms), 0u);
	EXPECT_EQ(advance(1ms), 1u) << "applyEffect(effects) runs at hitTime = 300 ms";

	// hit time 0: instant, applied inside endCast and nothing is scheduled
	ASSERT_TRUE(skill(INSTANT_SKILL)->useSkill());
	EXPECT_EQ(executor->pendingTaskCount(), 0u);
}

TEST_F(SkillCastPhasesTest, ACostThatCanNoLongerBePaidWhenTheCastEndsCancelsIt) {
	// the cast starts with enough MP (canPayCastCosts, Skill.java:175); when it ends, payCastCosts fails and endCast cancels (Skill.java:570-573)
	setMp(100);
	Ref<model::Skill> cast = skill(TIMED_SKILL);
	ASSERT_TRUE(cast->useSkill());
	advance(1000ms);
	setMp(10); // 19 MP are due

	advance(1000ms);
	EXPECT_FALSE(caster.player->isCasting()) << "cancelCurrentSkill(null, null) -> setCasting(null)";
	EXPECT_EQ(currentMp(), 10) << "nothing was paid";
	EXPECT_TRUE(packetsOf<SM_CASTSPELL_RESULT>(sent()).empty()) << "a cancelled cast sends no result";
	EXPECT_EQ(packetsOf<SM_SKILL_CANCEL>(sent()).size(), 1u) << "PlayerController.cancelCurrentSkill (PlayerController.java:532-533)";
	EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_ENOUGH_MP())) << "\"the unpaid cost already told the player what is missing\"";
	EXPECT_FALSE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_CANCELED())) << "cancelCurrentSkill(null, null): no second message";
}

TEST_F(SkillCastPhasesTest, AnEndTaskFindingTheCasterNoLongerCastingDoesNothingButDetachTheObservers) {
	// Skill.endCast's first lines (Skill.java:557-559): removeObservers(), then `if (!effector.isCasting() || isCancelled) return`. A caster
	// stops casting without the skill being cancelled when it dies: CreatureController.onDie calls setCasting(null) and nothing else of the skill
	// (CreatureController.java:154-156)
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	setMp(100);
	Ref<model::Skill> cast = skill(TIMED_ENEMY_SKILL, npc);
	ASSERT_TRUE(cast->useSkill());
	ASSERT_TRUE(npc->getObserveController()->hasObservers()) << "startCast attached the DeathObserver to the first target (Skill.java:528-537)";
	advance(500ms);
	caster.player->setCasting(nullptr);
	(*client)->clearSent();

	advance(1500ms);
	EXPECT_TRUE(packetsOf<SM_CASTSPELL_RESULT>(sent()).empty()) << "the end task returned at !effector.isCasting()";
	EXPECT_TRUE(packetsOf<SM_SKILL_CANCEL>(sent()).empty());
	EXPECT_FALSE(caster.player->getController().isInCombat()) << "no enterCombat of a hostile skill (Skill.java:650-651)";

	// removeObservers runs BEFORE the early return: the target's list no longer holds the DeathObserver, and the caster's move no longer reaches
	// the StartMovingListener (it was attached for one notification, ObserveController.java:31-34)
	EXPECT_FALSE(npc->getObserveController()->hasObservers()) << "the DeathObserver was removed from the first target";
	caster.player->getObserveController()->notifyMoveObservers();
	EXPECT_FALSE(cast->getMoveListener()->isEffectorMoved()) << "the move listener was removed from the caster";
}

TEST_F(SkillCastPhasesTest, CancellingACastDetachesItsObserversAtOnce) {
	// Skill.cancelCast (Skill.java:541-546) runs removeObservers itself: the observers are gone before the end task is due
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	Ref<model::Skill> cast = skill(TIMED_ENEMY_SKILL, npc);
	ASSERT_TRUE(cast->useSkill());
	ASSERT_TRUE(npc->getObserveController()->hasObservers());
	advance(500ms);

	caster.player->getController().cancelCurrentSkill(nullptr); // castingSkill.cancelCast() (PlayerController.java:526)
	EXPECT_FALSE(npc->getObserveController()->hasObservers()) << "the DeathObserver is detached by cancelCast, not by the later end task";
	caster.player->getObserveController()->notifyMoveObservers();
	EXPECT_FALSE(cast->getMoveListener()->isEffectorMoved());
	EXPECT_EQ(executor->pendingTaskCount(), 1u) << "the end task is still pending";
}

TEST_F(SkillCastPhasesTest, ACompletedCastDetachesItsMoveListener) {
	// Skill.endCast's first statement is removeObservers() (Skill.java:557) on the path that goes on to the result too: the caster's first move
	// after a completed cast no longer reaches the StartMovingListener useSkill attached (Skill.java:299). This is the check m5b2-plan.md G-07's
	// `live StartMovingListener == live Skill` row cannot make: the listener is attached for one notification (ObserveController.java:31-34),
	// so one that removeObservers forgot is dropped at the caster's next move and never shows up as a live count at the shutdown
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	Ref<model::Skill> cast = skill(FLAME_BOLT_SKILL, npc);
	ASSERT_TRUE(cast->useSkill());
	advance(2000ms);
	ASSERT_EQ(packetsOf<SM_CASTSPELL_RESULT>(sent()).size(), 1u) << "the cast completed";
	EXPECT_FALSE(caster.player->isCasting());
	EXPECT_FALSE(cast->getMoveListener()->isEffectorMoved()) << "nothing moved during the cast";

	caster.player->getObserveController()->notifyMoveObservers();
	EXPECT_FALSE(cast->getMoveListener()->isEffectorMoved()) << "endCast's removeObservers took the move listener out of the caster's controller";
}

TEST_F(SkillCastPhasesTest, MovingDuringACastWithMoveCastingDisallowedCancelsItWhenItEnds) {
	// Skill.useSkill attaches the StartMovingListener to the caster (Skill.java:299); a move of the caster reaches it through the observe
	// controller (here the real PlayerMoveController.updateFalling, PlayerMoveController.java:70-80), and 1282's
	// <useconditions><move_casting allow="false"/></useconditions> fails in endCast's preUsageCheck (PlayerMovedCondition.java:28-30,
	// Skill.java:562-564), which cancels with STR_SKILL_CANCELED
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	{
		SCOPED_TRACE("standing still");
		ASSERT_TRUE(skill(FLAME_BOLT_SKILL, npc)->useSkill());
		advance(2000ms);
		EXPECT_EQ(packetsOf<SM_CASTSPELL_RESULT>(sent()).size(), 1u);
		(*client)->clearSent();
	}
	{
		SCOPED_TRACE("falling during the cast");
		Ref<model::Skill> cast = skill(FLAME_BOLT_SKILL, npc);
		ASSERT_TRUE(cast->useSkill());
		advance(1000ms);
		caster.player->getMoveController()->updateFalling(49.0f);
		EXPECT_TRUE(cast->getMoveListener()->isEffectorMoved()) << "the listener useSkill attached saw the move";
		EXPECT_TRUE(caster.player->isCasting()) << "a fall does not cancel by itself (PlayerController.onStartMove would)";
		advance(1000ms);
		EXPECT_FALSE(caster.player->isCasting());
		EXPECT_TRUE(packetsOf<SM_CASTSPELL_RESULT>(sent()).empty());
		EXPECT_EQ(packetsOf<SM_SKILL_CANCEL>(sent()).size(), 1u);
		EXPECT_TRUE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_CANCELED())) << "cancelCurrentSkill(null) (Skill.java:563)";
	}
}

TEST_F(SkillCastPhasesTest, UseWithoutPropSkillSkipsCanUseSkill) {
	// Skill.useWithoutPropSkill is useSkill(false, false) and `if (checkproperties && !canUseSkill(CAST_START))` skips every start check
	// (Skill.java:270-279): the path of post-spawn npc skills and PenaltySkill.useSkill. 5 MP cannot pay TIMED_SKILL's 19
	setMp(5);
	EXPECT_FALSE(skill(TIMED_SKILL, caster.player)->useSkill()) << "canUseSkill -> canPayCastCosts refuses";
	EXPECT_TRUE(packetsOf<SM_CASTSPELL>(sent()).empty());
	(*client)->clearSent();

	Ref<model::Skill> cast = skill(TIMED_SKILL, caster.player);
	EXPECT_TRUE(cast->useWithoutPropSkill()) << "no start check";
	EXPECT_EQ(packetsOf<SM_CASTSPELL>(sent()).size(), 1u);
	EXPECT_TRUE(caster.player->isCasting());
	advance(2000ms);
	EXPECT_TRUE(packetsOf<SM_CASTSPELL_RESULT>(sent()).empty()) << "the end conditions are still checked when the cast ends";
	EXPECT_EQ(currentMp(), 5);
}

TEST_F(SkillCastPhasesTest, TheNextSkillUseIsTheMinimumCastIntervalUnlessTheAnimationIsLonger) {
	// Skill.startCast: setNextSkillUse(now + GSConfig.MIN_SKILL_CAST_INTERVAL_MILLIS) (Skill.java:521-522); endCast: setNextSkillUse(Math.max(
	// nextSkillUse, now + animation.lastHitMillis())) "because nextSkillUse set from startCast() must not be undercut" (Skill.java:678-679).
	// The interval is 0 in the unit tests, which hides both lines; 5,000 ms is longer than testmotion's 500 ms last hit.
	ConfigOverride<int32_t> interval(configs::main::GSConfig::MIN_SKILL_CAST_INTERVAL_MILLIS, 5000);
	int64_t before = commons::utils::currentTimeMillis();
	ASSERT_TRUE(skill(MOTION_SKILL)->useSkill());
	int64_t after = commons::utils::currentTimeMillis();
	EXPECT_GE(caster.player->getNextSkillUse(), before + 5000);
	EXPECT_LE(caster.player->getNextSkillUse(), after + 5000);
}

TEST_F(SkillCastPhasesTest, AnAnimationThatAllowsTheCastSpeedBoostSetsTheHitTimeBoost) {
	// Skill.endCast (Skill.java:668-677): with an animation and apply_casting_time_bonus, setHitTimeBoost(now + fullDurationMillis + 50,
	// castSpeed). +300 BOOST_CASTING_TIME makes the cast speed 1 - 300 / 1000f = 0.7 (updateCastDurationAndSpeed, Skill.java:347), which
	// MotionData.calculateCastSpeedRate turns into 0.7 + 0.3 / 2 = 0.85: full duration (int) (1.0 * 1000 * 0.85) = 850, last hit 425
	// (MotionData.java:62-81)
	addStat(*caster.player, StatEnum::BOOST_CASTING_TIME, 300);
	int64_t before = commons::utils::currentTimeMillis();
	Ref<model::Skill> cast = skill(BOOSTED_MOTION_SKILL);
	ASSERT_TRUE(cast->useSkill());
	int64_t after = commons::utils::currentTimeMillis();
	EXPECT_FLOAT_EQ(cast->getCastSpeedForAnimationBoostAndChargeSkills(), 0.7f);
	EXPECT_FLOAT_EQ(caster.player->getHitTimeBoostCastSpeed(), 0.7f);
	EXPECT_TRUE(caster.player->isHitTimeBoosted(before + 900)) << "boosted until now + 850 + 50";
	EXPECT_FALSE(caster.player->isHitTimeBoosted(after + 901));
	EXPECT_GE(caster.player->getNextSkillUse(), before + 425);
	EXPECT_LE(caster.player->getNextSkillUse(), after + 425);

	// without apply_casting_time_bonus the boost is cleared: setHitTimeBoost(0, 0) (Skill.java:675-676)
	ASSERT_TRUE(skill(MOTION_SKILL)->useSkill());
	EXPECT_FALSE(caster.player->isHitTimeBoosted());
	EXPECT_FLOAT_EQ(caster.player->getHitTimeBoostCastSpeed(), 0.0f);
}

// ------------------------------------------------------------------------------------------------------------------------- the gate path

TEST_F(SkillCastPhasesTest, WithAnimationChecksTheServerHitTimeAddsTheAmmoFlightAndAuditsAnEarlierClientHitTime) {
	ConfigOverride<bool> checkAnimations(configs::main::SecurityConfig::CHECK_ANIMATIONS, true);
	ConfigOverride<bool> noPunishment(configs::main::PunishmentConfig::PUNISHMENT_ENABLE, false);
	ConfigOverride<bool> auditLog(configs::main::LoggingConfig::LOG_AUDIT, true);
	LogCapture audit({AUDIT_LOGGER});
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f); // 10 m
	waitUntilTheMovementToleranceIsCapped();

	// Skill.updateHitTime (Skill.java:416-444), 1282's data:
	//   animationTimeUntilFirstHit  pointfire's min 0.4 * motion speed 100 * 10 * attack speed rate 1 = 400 (MotionData.java:49-60)
	//   ammo                        distance 10 / ammospeed 30 * 1000 = 333.33 -> 733.33
	//   tolerance                   1 + ceil(getDistanceTolerance / 30 * 1000); the caster stands, so its movement is capped at 200 ms at
	//                               6 m/s (6000 thousandths): 1.2 m -> 1 + ceil(40.000004) = 42 ms
	//   serverHitTime               motion delay 0 + Math.round(733.33) = 733
	Ref<model::Skill> cast = skill(FLAME_BOLT_SKILL, npc);
	cast->setClientHitTime(500);
	ASSERT_TRUE(cast->useSkill());
	EXPECT_EQ(cast->getHitTime(), 733) << "serverHitTime > clientHitTime: the server's replaces the client's (Skill.java:435-436)";
	EXPECT_EQ(audit.count("modified hit time for skill 60022 (client < server: 500/733). Uncertainty factors: cast speed, movement (calculated "
						  "tolerance: 42 ms)"),
		1)
		<< audit.dump();

	advance(2000ms);
	std::vector<std::vector<uint8_t>> results = packetsOf<SM_CASTSPELL_RESULT>(sent());
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(decodeCastSpellResult(results[0]).hitTime, 733);
	EXPECT_EQ(executor->pendingTaskCount(), 1u) << "applyEffect waits for the server's hit time";
}

TEST_F(SkillCastPhasesTest, TheToleranceDecidesWhetherAClientHitTimeIsSuspicious) {
	ConfigOverride<bool> checkAnimations(configs::main::SecurityConfig::CHECK_ANIMATIONS, true);
	ConfigOverride<bool> noPunishment(configs::main::PunishmentConfig::PUNISHMENT_ENABLE, false);
	ConfigOverride<bool> auditLog(configs::main::LoggingConfig::LOG_AUDIT, true);
	LogCapture audit({AUDIT_LOGGER});
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	waitUntilTheMovementToleranceIsCapped();

	// Skill.isSuspiciousClientHitTime (Skill.java:457-465): `clientHitTime >= serverHitTime - tolerance` is not suspicious; 733 - 42 = 691
	EXPECT_EQ(flameBoltHitTime(npc, 691), 733);
	EXPECT_FALSE(audit.contains("modified hit time")) << audit.dump();
	EXPECT_EQ(flameBoltHitTime(npc, 690), 733);
	EXPECT_EQ(audit.count("(client < server: 690/733)"), 1) << audit.dump();

	// a client hit time at or above the server's is kept, and never audited (Skill.java:435)
	EXPECT_EQ(flameBoltHitTime(npc, 800), 800);
	EXPECT_EQ(flameBoltHitTime(npc, 733), 733);
	EXPECT_EQ(audit.count("modified hit time"), 1) << audit.dump();

	// 0 from the client is suspicious for a skill whose motion is not instant and which is no item skill (Skill.java:460-461)
	EXPECT_EQ(flameBoltHitTime(npc, 0), 733);
	EXPECT_EQ(audit.count("(client < server: 0/733)"), 1) << audit.dump();
}

TEST_F(SkillCastPhasesTest, AMovingTargetShortensTheAmmoFlightByTheCastersRunUntilTheShot) {
	ConfigOverride<bool> checkAnimations(configs::main::SecurityConfig::CHECK_ANIMATIONS, true);
	ConfigOverride<bool> noPunishment(configs::main::PunishmentConfig::PUNISHMENT_ENABLE, false);
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	npc->getMoveController()->setInMove(true);

	// Skill.java:425-426: distance -= calculateMaxCoveredDistance(player, Math.round(400)) = 6000 * 400 / 1e6 = 2.4 m, so the ammo flies
	// 7.6 / 30 * 1000 = 253.33 ms and the server hit time is Math.round(400 + 253.33) = 653 (the tolerance, which grows with the target's
	// movement time, only decides the audit)
	EXPECT_EQ(flameBoltHitTime(npc, 0), 653);
}

TEST_F(SkillCastPhasesTest, WithoutAnimationChecksTheClientHitTimeIsKept) {
	// updateHitTime returns after `hitTime = clientHitTime` for !checkAnimation (Skill.java:417-419): useSkill() passes CHECK_ANIMATIONS,
	// useNoAnimationSkill() passes false
	ConfigOverride<bool> checkAnimations(configs::main::SecurityConfig::CHECK_ANIMATIONS, true);
	Ref<CastTestNpc> npc = spawnMonster(110.0f, 100.0f, 50.0f);
	Ref<model::Skill> cast = skill(FLAME_BOLT_SKILL, npc);
	cast->setClientHitTime(500);
	ASSERT_TRUE(cast->useNoAnimationSkill());
	EXPECT_EQ(cast->getHitTime(), 500);
}

TEST_F(SkillCastPhasesTest, CastSpeedStatsShortenAMagicalCastDownToAQuarterOfItsDuration) {
	// Skill.calculateMagicalCastDuration (Skill.java:383-399) for BOOSTED_ATTACK_SKILL's 2,000 ms:
	//   baseDurationCap   Math.round(2000 * 0.25f) = 500
	//   castDuration      max(reverse BOOST_CASTING_TIME of 2000, 500): +300 -> 1700
	//   boostValue        reverse BOOST_CASTING_TIME_SKILL of 2000: +100 -> 1900; then the ATTACK stat of 1900: +100 -> 1800
	//   buffDelta         2000 - 1800 = 200 -> castDuration 1500, max(1500, 500)
	// and the cast speed of the animation boost: 1 - getStat(BOOST_CASTING_TIME, 1000).getBonus() / 1000f = 0.7 (Skill.java:347)
	addStat(*caster.player, StatEnum::BOOST_CASTING_TIME, 300);
	addStat(*caster.player, StatEnum::BOOST_CASTING_TIME_SKILL, 100);
	addStat(*caster.player, StatEnum::BOOST_CASTING_TIME_ATTACK, 100);
	addStat(*caster.player, StatEnum::BOOST_CASTING_TIME_HEAL, 900); // not this skill's sub type
	(*client)->clearSent();
	Ref<model::Skill> cast = skill(BOOSTED_ATTACK_SKILL);
	ASSERT_TRUE(cast->useSkill());
	std::vector<std::vector<uint8_t>> started = packetsOf<SM_CASTSPELL>(sent());
	ASSERT_EQ(started.size(), 1u);
	EXPECT_EQ(decodeCastSpell(started[0]).castDuration, 1500);
	EXPECT_FLOAT_EQ(cast->getCastSpeedForAnimationBoostAndChargeSkills(), 0.7f);
	advance(1499ms);
	EXPECT_TRUE(caster.player->isCasting());
	advance(1ms);
	EXPECT_FALSE(caster.player->isCasting()) << "the end task is scheduled at the reduced duration";

	// +900 more BOOST_CASTING_TIME: reverse 800, above the cap -> 800
	addStat(*caster.player, StatEnum::BOOST_CASTING_TIME, 900);
	(*client)->clearSent();
	ASSERT_TRUE(skill(BOOSTED_ATTACK_SKILL)->useSkill());
	EXPECT_EQ(decodeCastSpell(packetsOf<SM_CASTSPELL>(sent()).at(0)).castDuration, 600) << "max(800, 500) - buffDelta 200";
	caster.player->getController().cancelCurrentSkill(nullptr);

	// +900 more: reverse -100 -> getPositiveReverseStat 0 -> the cap: "casting time stats cap 75%"
	addStat(*caster.player, StatEnum::BOOST_CASTING_TIME, 900);
	(*client)->clearSent();
	ASSERT_TRUE(skill(BOOSTED_ATTACK_SKILL)->useSkill());
	EXPECT_EQ(decodeCastSpell(packetsOf<SM_CASTSPELL>(sent()).at(0)).castDuration, 500) << "max(max(0, 500) - 200, 500)";
	caster.player->getController().cancelCurrentSkill(nullptr);

	// a template without apply_casting_time_bonus keeps its duration (calculateCastDuration, Skill.java:378-379)
	(*client)->clearSent();
	ASSERT_TRUE(skill(TIMED_ENEMY_SKILL, spawnMonster(110.0f, 100.0f, 50.0f))->useSkill());
	EXPECT_EQ(decodeCastSpell(packetsOf<SM_CASTSPELL>(sent()).at(0)).castDuration, 2000);
}

TEST_F(SkillCastPhasesTest, TheCooldownGrowsOrShrinksWithTheSkillLevel) {
	// Skill.setCooldowns / getCooldown (Skill.java:321-336): cooldown + cooldown_delta_lv * skillLevel = 100 - 6 * 3 = 82 tenths of a second
	int64_t before = commons::utils::currentTimeMillis();
	Ref<model::Skill> cast = model::Skill::create(skillTemplate(COOLDOWN_DELTA_SKILL), *caster.player, nullptr, 3);
	EXPECT_EQ(cast->getCooldown(), 82);
	ASSERT_TRUE(cast->useSkill());
	int64_t after = commons::utils::currentTimeMillis();
	std::vector<std::vector<uint8_t>> results = packetsOf<SM_CASTSPELL_RESULT>(sent());
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(decodeCastSpellResult(results[0]).cooldown, 82) << "SM_CASTSPELL_RESULT writes skill.getCooldown()";
	EXPECT_GE(caster.player->getSkillCoolDown(524), before + 8200) << "cooldown * 100 + now";
	EXPECT_LE(caster.player->getSkillCoolDown(524), after + 8200);
}

TEST_F(SkillCastPhasesTest, AChargeCastLastsTheChargeStepsScaledByHalfTheCastSpeedAndIsCancelledWhenItRunsOut) {
	// Skill.calculateChargeCastDuration (Skill.java:351-370) for charge 1 (MAGICAL, two 1,600 ms steps) with +800 BOOST_CASTING_TIME:
	// baseCastDuration = 3200; speedRatio = calculateMagicalCastDuration() / 3200 = max(3200 - 800, 800) / 3200 = 0.75; "charge skills are only
	// affected by half of the speed bonus": (int) (3200 * (1 - 0.25 / 2)) = 2800. castSpeedForAnimationBoostAndChargeSkills = 2800 / 3200f
	addStat(*caster.player, StatEnum::BOOST_CASTING_TIME, 800);
	(*client)->clearSent();
	Ref<model::Skill> start = skill(CHARGE_BOOSTED_SKILL);
	ASSERT_TRUE(start->useSkill());
	EXPECT_EQ(decodeCastSpell(packetsOf<SM_CASTSPELL>(sent()).at(0)).castDuration, 2800);
	EXPECT_FLOAT_EQ(start->getCastSpeedForAnimationBoostAndChargeSkills(), 0.875f);

	// CreatureController.useChargeSkill (CreatureController.java:468-493) scales every step by that cast speed: 1600 * 0.875 = 1400 < 1500, so a
	// 1,500 ms charge reaches the second step (at cast speed 1 it would stop at the first); the minimum is 400 * 0.875 = 350 ms
	(*client)->clearSent();
	EXPECT_TRUE(caster.player->getController().useChargeSkill(*start, 1500));
	std::vector<std::vector<uint8_t>> results = packetsOf<SM_CASTSPELL_RESULT>(sent());
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(decodeCastSpellResult(results[0]).skillId, CHARGED_SKILL_2);
	(*client)->clearSent();
	advance(2800ms);
	EXPECT_TRUE(packetsOf<SM_SKILL_CANCEL>(sent()).empty()) << "the released start skill was cancelled: cancelCurrentSkillCast does nothing";

	// Skill.useSkill schedules cancelCurrentSkillCast, not endCast, for a charge skill (Skill.java:311-312): a charge held past its duration is
	// cancelled (Skill.java:548-551) without a result
	Ref<model::Skill> held = skill(CHARGE_BOOSTED_SKILL);
	ASSERT_TRUE(held->useSkill());
	(*client)->clearSent();
	advance(2799ms);
	EXPECT_TRUE(caster.player->isCasting());
	advance(1ms);
	EXPECT_FALSE(caster.player->isCasting());
	EXPECT_EQ(packetsOf<SM_SKILL_CANCEL>(sent()).size(), 1u);
	EXPECT_TRUE(packetsOf<SM_CASTSPELL_RESULT>(sent()).empty());
	EXPECT_FALSE(sentMessage(SM_SYSTEM_MESSAGE::STR_SKILL_CANCELED())) << "cancelCurrentSkill(null, null)";
}

} // namespace
} // namespace aion::gameserver::skillengine::test
