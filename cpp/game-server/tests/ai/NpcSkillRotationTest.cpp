// m5b2-plan.md N-01/N-04 (P5-05, aion_gs_ai): SkillAttackManager - the npc skill rotation - over the shipped npc_skills rows of
// NpcSkillTestSupport.h, on real Npcs in a real map instance, with the real skill engine behind the one cast that runs to its end.
//
// What each case pins, and the Java it comes from:
// - cantUseSkill's decision table (SkillAttackManager.java:105-110): a transform that forbids skills, a CANT_ATTACK_STATE, SILENCE against a
//   MAGICAL skill and BIND against a PHYSICAL one - each row next to the one that must NOT refuse.
// - chooseNextSkill's gates (:117-173) for the plain prob-25 row every start-map npc has: no skill mid-cast, none before the fight's
//   initial skill delay, none while the next-skill delay runs, and the 25 % chance itself, replayed from a seeded Rnd.
// - the chain arm with TARGET_IS_AETHERS_HOLD (855799), HELP_FRIEND's retargeting and hp_below boundary (231105), and NPC_IS_ALIVE with the
//   priority order (235763) - NpcSkillTemplateEntry.conditionReady reached the way the server reaches it, through isReady (:184-193).
// - performAttack (:35-51) entering AISubState.CAST, skillAction (:53-103) retargeting to the most hated creature and casting, and the end of
//   the cast (Skill.endCast -> afterUseSkill, :112-115) leaving CAST again; skillAction's give-up arm.
// - ShoutEventHandler.onCast with a null first target (header request m5b2-p2-8).
// - targetTooFar and getNpcSkillEntryIfNotTooFarAway (:176-182, :195-213) for a target="RANDOM" skill (855799's 17179); the queued-skill arms
//   (:124-133); a chain with two prioritised follow-ups (297192) and the shuffle of a priority group (283139).
// - skillAction's other arms: the first_target ME turn (210161's 16424), every npc_skill target attribute (FRIEND with 230745's shipped row,
//   the rest through a queued skill), the cantUseSkill arm, the scheduled skillAction of performAttack with a delay, the attack-range-0
//   TARGET_TOOFAR arms of performAttack and skillAction (206292), and the one place skillAction dereferences a skill template.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <utility>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/handler/ShoutEventHandler.h"
#include "aion/gameserver/ai/manager/SkillAttackManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTargetAttribute.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/model/SkillType.h"
#include "aion/gameserver/skillengine/model/TransformType.h"

#include "NpcSkillTestSupport.h"

namespace aion::gameserver::ai::testing {
namespace {

namespace Rnd = commons::utils::Rnd;
using manager::SkillAttackManager;
using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using model::skill::NpcSkillEntry;
using model::templates::npcskill::NpcSkillTargetAttribute;
using runtime::Ptr;
using runtime::Ref;
using skillengine::effect::AbnormalState;

class NpcSkillRotationTest : public NpcSkillWorldTest {
protected:
	static NpcAI& aiOf(Npc& npc) { return *runtime::cast<NpcAI>(npc.getAi()); }
};

// ---- cantUseSkill -------------------------------------------------------------------------------------------------------------------------------

TEST_F(NpcSkillRotationTest, CantUseSkillFollowsTheTransformAndTheAbnormalStates) {
	AI_TEST_SCOPE;
	Ref<Npc> protector = makeWorldNpc(PASHID_PROTECTOR_NPC_ID);
	Ptr<NpcSkillEntry> physical = entryOf(*protector, WIDE_POWER_ATTACK);
	Ptr<NpcSkillEntry> magical = entryOf(*protector, PROTECTIVE_SHIELD);
	ASSERT_TRUE(physical && magical);
	ASSERT_EQ(physical->getSkillTemplate()->getType(), skillengine::model::SkillType::PHYSICAL);
	ASSERT_EQ(magical->getSkillTemplate()->getType(), skillengine::model::SkillType::MAGICAL);

	EXPECT_FALSE(SkillAttackManager::cantUseSkill(*physical, *protector)) << "no state at all";
	EXPECT_FALSE(SkillAttackManager::cantUseSkill(*magical, *protector)) << "no state at all";

	struct Row {
		AbnormalState state;
		bool physicalRefused;
		bool magicalRefused;
		const char* why;
	};
	const Row rows[] = {
		{AbnormalState::ROOT, false, false, "ROOT is one of CANT_MOVE_STATE, not of CANT_ATTACK_STATE"},
		{AbnormalState::STUN, true, true, "STUN is one of CANT_ATTACK_STATE"},
		{AbnormalState::SILENCE, false, true, "SILENCE refuses only the MAGICAL skill"},
		{AbnormalState::BIND, true, false, "BIND refuses only the PHYSICAL skill"},
	};
	for (const Row& row : rows) {
		protector->getEffectController()->setAbnormal(row.state);
		EXPECT_EQ(SkillAttackManager::cantUseSkill(*physical, *protector), row.physicalRefused) << row.why;
		EXPECT_EQ(SkillAttackManager::cantUseSkill(*magical, *protector), row.magicalRefused) << row.why;
		protector->getEffectController()->unsetAbnormal(row.state);
	}

	// a transform model other than the npc's own: TransformModel::apply's cantUseSkills flag decides (the kerub's model, a real npc model)
	protector->getTransformModel().apply(STRIPED_KERUB_NPC_ID, skillengine::model::TransformType::FORM1, 0, false, false, false, false, false,
		false, false);
	ASSERT_TRUE(protector->isTransformed());
	EXPECT_FALSE(SkillAttackManager::cantUseSkill(*physical, *protector)) << "transformed, but the form allows skills";
	protector->getTransformModel().apply(STRIPED_KERUB_NPC_ID, skillengine::model::TransformType::FORM1, 0, true, false, false, false, false,
		false, false);
	EXPECT_TRUE(SkillAttackManager::cantUseSkill(*physical, *protector)) << "a form that cannot use skills";
	EXPECT_TRUE(SkillAttackManager::cantUseSkill(*magical, *protector)) << "a form that cannot use skills";
	protector->getTransformModel().apply(0);
}

// ---- chooseNextSkill ----------------------------------------------------------------------------------------------------------------------------

/**
 * The kerub's whole rotation is one prob 25 row (npc_skills.xml:2817). Each gate of chooseNextSkill is shown refusing under the seed that
 * otherwise passes the 25 % draw, so the refusal is the gate's and not the dice's.
 */
TEST_F(NpcSkillRotationTest, TheKerubChoosesBrandishOnlyWhenEveryGateAndTheChanceAllowIt) {
	AI_TEST_SCOPE;
	Ref<Npc> kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID);
	NpcAI& ai = aiOf(*kerub);
	Ptr<NpcSkillEntry> brandish = entry(*kerub, 0);
	ASSERT_EQ(brandish->getSkillId(), BRANDISH);
	// attack_speed 2100: getInitialSkillDelay draws Rnd.get(2100, 6300) before the entry's Rnd.chance() < 25
	const uint64_t passes = seedWhere(chanceDraws(2100, 25), true);
	const uint64_t fails = seedWhere(chanceDraws(2100, 25), false);

	Rnd::seedCurrentThreadForTests(passes);
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), brandish.get()) << "the 25 % draw passed";
	Rnd::seedCurrentThreadForTests(fails);
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), nullptr) << "the 25 % draw failed";

	ASSERT_TRUE(ai.setSubStateIfNot(AISubState::CAST));
	Rnd::seedCurrentThreadForTests(passes);
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), nullptr) << "no next skill while the npc casts";
	ai.setSubStateIfNot(AISubState::NONE);

	kerub->getGameStats()->setFightStartingTime();
	Rnd::seedCurrentThreadForTests(passes);
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), nullptr) << "the fight began less than Rnd.get(2100, 6300) ms ago";
	kerub->getGameStats()->resetFightStats();

	kerub->getGameStats()->renewLastSkillTime();
	kerub->getGameStats()->setNextSkillDelay(5000);
	Rnd::seedCurrentThreadForTests(passes);
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), nullptr) << "the last skill was used less than 5000 ms ago";
	kerub->getGameStats()->resetFightStats();

	Rnd::seedCurrentThreadForTests(passes);
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), brandish.get()) << "every gate open again";
}

/**
 * 855799's chain (npc_skills.xml:63170-63175): 17179 Aion's Judgment opens chain 1 and throws the target into the air; 17182 Fall follows it
 * only while the target is in Aether's Hold (OPENAERIAL). The state after the opener is set as Skill.useSkill and NpcController::useSkill
 * leave it: the opener is the last skill, used just now (its 15 s cooldown runs), next_skill_time 0.
 */
TEST_F(NpcSkillRotationTest, TheLanmarkChainFollowsUpOnlyIntoAnAethersHoldAndOnlyInsideTheChainWindow) {
	AI_TEST_SCOPE;
	Ref<Npc> lanmark = makeWorldNpc(LANMARK_NPC_ID, 500, 500, 100);
	Ref<Npc> victim = makeWorldNpc(TAMED_PAGATI_NPC_ID, 502, 500, 100);
	NpcAI& ai = aiOf(*lanmark);
	Ptr<NpcSkillEntry> judgment = entry(*lanmark, 0);
	Ptr<NpcSkillEntry> fall = entry(*lanmark, 1);
	ASSERT_EQ(judgment->getSkillId(), AIONS_JUDGMENT);
	ASSERT_EQ(fall->getSkillId(), FALL);
	lanmark->setTarget(victim);
	lanmark->getGameStats()->setLastSkill(judgment);
	judgment->setLastTimeUsed();
	lanmark->getGameStats()->setNextSkillDelay(0);
	lanmark->getGameStats()->renewLastSkillTime();

	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), nullptr)
		<< "the target is not in Aether's Hold, so Fall is not ready, and the opener is on its cooldown";
	victim->getEffectController()->setAbnormal(AbnormalState::OPENAERIAL);
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), fall.get()) << "Aether's Hold: the chain goes on (prob 100)";

	lanmark->getGameStats()->resetFightStats(); // the last skill time is 0: max_chain_time (15000 ms) has run out
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), nullptr) << "outside the chain window a chain skill is never chosen";
	victim->getEffectController()->unsetAbnormal(AbnormalState::OPENAERIAL);
}

/**
 * 231105 (Eternal_Bastion.xml:105-111): prob 100 everywhere, so no dice. Protective Shield is prio 1 with HELP_FRIEND hp_below 50 (range: the
 * template default 10); Wide Power Attack is the prio 0 fallback; Midnight Robe's prob 0 is never ready.
 */
TEST_F(NpcSkillRotationTest, HelpFriendTakesAFriendAtHalfItsHpAsTheTarget) {
	AI_TEST_SCOPE;
	Ref<Npc> protector = makeWorldNpc(PASHID_PROTECTOR_NPC_ID, 500, 500, 100);
	Ref<Npc> wounded = makeWorldNpc(PASHID_PROTECTOR_NPC_ID, 505, 500, 100); // the same tribe: TribeRelationService.isSupport
	Ref<Npc> enemy = makeWorldNpc(TAMED_PAGATI_NPC_ID, 502, 500, 100);
	know(*protector, *wounded);
	know(*protector, *enemy);
	NpcAI& ai = aiOf(*protector);
	Ptr<NpcSkillEntry> shield = entryOf(*protector, PROTECTIVE_SHIELD);
	Ptr<NpcSkillEntry> wideAttack = entryOf(*protector, WIDE_POWER_ATTACK);
	ASSERT_TRUE(shield && wideAttack);
	protector->setTarget(enemy);

	setHpPercentage(*wounded, 51);
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), wideAttack.get()) << "51 % is above hp_below 50: the prio 0 attack";
	EXPECT_EQ(protector->getTarget().get(), enemy.get()) << "the target stays";

	setHpPercentage(*wounded, 50);
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), shield.get()) << "50 % is at hp_below: the prio 1 shield comes first";
	EXPECT_EQ(protector->getTarget().get(), wounded.get()) << "HELP_FRIEND made the wounded friend the target";
}

/**
 * 235763 (npc_skills.xml:63745-63753): Dark Shield is prio 1 with NPC_IS_ALIVE 856059, the four others prio 0 (Earthly Grudge prob 100, so
 * the prio 0 group always has a ready skill). The same seed passes Dark Shield's 70 % draw in all three rows, so only the watched npc
 * differs; and the prio 0 group can only lose to Dark Shield if the priorities are walked from the highest down.
 */
TEST_F(NpcSkillRotationTest, NpcIsAliveRanksThePrioOneSkillFirstOnlyWhileTheWatchedNpcLives) {
	AI_TEST_SCOPE;
	Ref<Npc> leader = makeWorldNpc(HIRAKIKI_LEADER_NPC_ID, 500, 500, 100);
	Ref<Npc> enemy = makeWorldNpc(TAMED_PAGATI_NPC_ID, 502, 500, 100);
	NpcAI& ai = aiOf(*leader);
	Ptr<NpcSkillEntry> darkShield = entryOf(*leader, DARK_SHIELD);
	ASSERT_TRUE(darkShield);
	leader->setTarget(enemy); // Absorb Vitality's target="RANDOM" asks targetTooFar for a living, visible target
	// attack_speed 2040: Rnd.get(2040, 6120), then prio 1 holds one entry (no shuffle) and Dark Shield draws Rnd.chance() < 70
	const uint64_t passes = seedWhere(chanceDraws(2040, 70), true);

	Rnd::seedCurrentThreadForTests(passes);
	Ptr<NpcSkillEntry> chosen = SkillAttackManager::chooseNextSkill(ai);
	EXPECT_NE(chosen.get(), darkShield.get()) << "no npc 856059 in the instance";

	Ref<Npc> watched = makeWorldNpc(HIRAKIKI_WATCHED_NPC_ID, 520, 500, 100);
	mapInstance->addObject(*watched);
	Rnd::seedCurrentThreadForTests(passes);
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), darkShield.get()) << "856059 lives: prio 1 before prio 0";

	watched->getLifeStats()->setCurrentHp(0);
	ASSERT_TRUE(watched->isDead());
	Rnd::seedCurrentThreadForTests(passes);
	chosen = SkillAttackManager::chooseNextSkill(ai);
	EXPECT_NE(chosen.get(), darkShield.get()) << "a dead 856059 does not count";
	mapInstance->removeObject(*watched);
}

/**
 * targetTooFar (SkillAttackManager.java:195-213) for 855799's 17179 Aion's Judgment: target="RANDOM" (npc_skills.xml:63171) and a TARGET
 * first target of range 4, target_type ONLYONE (skill_templates.xml:129768-129785). Such a skill asks the npc's current target: a living,
 * visible creature within first_target_range plus both bound radii (isInRange(..., false)); otherwise getNpcSkillEntryIfNotTooFarAway skips the
 * skill and makes the next one wait 5000 ms (:176-182). One seed passes 17179's 60 % draw in every row, so only the target differs.
 */
TEST_F(NpcSkillRotationTest, TargetTooFarSkipsARandomTargetSkillWhoseTargetIsOutOfReachAndDelaysTheNextSkill) {
	AI_TEST_SCOPE;
	Ref<Npc> lanmark = makeWorldNpc(LANMARK_NPC_ID, 500, 500, 100);
	Ref<Npc> nearVictim = makeWorldNpc(TAMED_PAGATI_NPC_ID, 502, 500, 100);
	Ref<Npc> farVictim = makeWorldNpc(TAMED_PAGATI_NPC_ID, 510, 500, 100); // 10 m, beyond 4 + 0.6825 + 1.75
	NpcAI& ai = aiOf(*lanmark);
	Ptr<NpcSkillEntry> judgment = entry(*lanmark, 0);
	ASSERT_EQ(judgment->getSkillId(), AIONS_JUDGMENT);
	// attack_speed 1900: Rnd.get(1900, 5700); the shuffle of the prio 0 group [17179, 17182], whose chain skill 17182 is never asked there
	// (a shuffle of two draws the same whatever it shuffles); then 17179's Rnd.chance() < 60
	const uint64_t passes = seedWhere(
		[] {
			Rnd::get(1900, 5700);
			std::vector<int> group{0, 1};
			std::shuffle(group.begin(), group.end(), Rnd::generator());
			return Rnd::chance() < 60.0f;
		},
		true);
	lanmark->getGameStats()->renewLastSkillTime(); // the next-skill delay is 0, so canUseNextSkill holds now; a delay of 5000 would not

	lanmark->setTarget(nearVictim);
	Rnd::seedCurrentThreadForTests(passes);
	ASSERT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), judgment.get()) << "precondition: the seed passes 17179's draw, target in reach";
	ASSERT_TRUE(lanmark->getGameStats()->canUseNextSkill());

	lanmark->setTarget(farVictim);
	Rnd::seedCurrentThreadForTests(passes);
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), nullptr) << "10 m away: out of first_target_range";
	EXPECT_FALSE(lanmark->getGameStats()->canUseNextSkill()) << "getNpcSkillEntryIfNotTooFarAway made the next skill wait 5000 ms";

	lanmark->getGameStats()->setNextSkillDelay(0);
	nearVictim->getLifeStats()->setCurrentHp(0);
	ASSERT_TRUE(nearVictim->isDead());
	lanmark->setTarget(nearVictim);
	Rnd::seedCurrentThreadForTests(passes);
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), nullptr) << "a dead target";

	lanmark->getGameStats()->setNextSkillDelay(0);
	lanmark->setTarget(nullptr);
	Rnd::seedCurrentThreadForTests(passes);
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), nullptr) << "no target at all";
}

/**
 * A queued skill comes before the rotation (SkillAttackManager.java:124-133): one with next_skill_time 0 even before the fight's initial skill
 * delay, any other once the gates are open. The queued entries are the ones Npc.queueSkill builds (prob 100) around the kerub's own Brandish,
 * so only the object tells a queued entry from the kerub's list entry.
 */
TEST_F(NpcSkillRotationTest, AQueuedSkillComesFirstAndANextSkillTimeOfZeroSkipsTheInitialDelay) {
	AI_TEST_SCOPE;
	Ref<Npc> kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID);
	NpcAI& ai = aiOf(*kerub);
	const uint64_t listFails = seedWhere(chanceDraws(2100, 25), false); // the kerub's own prob 25 Brandish would not be chosen

	kerub->getGameStats()->setFightStartingTime(); // the initial skill delay, Rnd.get(2100, 6300) ms, has not passed
	Ptr<NpcSkillEntry> immediate = queueSkill(*kerub, BRANDISH, 1, 0, NpcSkillTargetAttribute::MOST_HATED);
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), immediate.get()) << "next_skill_time 0: ahead of the initial skill delay";
	kerub->clearQueuedSkills();

	Ptr<NpcSkillEntry> waiting = queueSkill(*kerub, BRANDISH, 1, -1, NpcSkillTargetAttribute::MOST_HATED); // Npc.queueSkill(id, level)
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), nullptr) << "next_skill_time -1: the queued skill waits for the gates";

	kerub->getGameStats()->resetFightStats(); // the fight began long ago: the gates are open
	Rnd::seedCurrentThreadForTests(listFails);
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), waiting.get()) << "the gates are open: the queued skill comes before the list";
}

/**
 * 297192's chain 1 (400030000_Transidium_Annex.xml:339-345): 21287 Agrint Curse opens it, and 16989 Cold Attack (prio 24, max_hp 35) and
 * 17335 Flame Bolt (prio 23) both follow it, prob 100. With more than one follow-up and a priority among them chooseNextSkill sorts the
 * follow-ups highest priority first (SkillAttackManager.java:143-149) and takes the first ready one. The opener's state is what the cast of it
 * leaves: the last skill, used just now (the chain window, max_chain_time 15000 ms, is open).
 */
TEST_F(NpcSkillRotationTest, AChainTakesItsHighestPriorityFollowUpThatIsReady) {
	AI_TEST_SCOPE;
	Ref<Npc> sorcerer = makeWorldNpc(TROOPERS_SORCERER_NPC_ID);
	NpcAI& ai = aiOf(*sorcerer);
	Ptr<NpcSkillEntry> coldAttack = entryOf(*sorcerer, COLD_ATTACK);
	Ptr<NpcSkillEntry> flameBolt = entryOf(*sorcerer, FLAME_BOLT);
	Ptr<NpcSkillEntry> curse = entryOf(*sorcerer, AGRINT_CURSE);
	ASSERT_TRUE(coldAttack && flameBolt && curse);
	sorcerer->getGameStats()->setLastSkill(curse);
	sorcerer->getGameStats()->renewLastSkillTime();
	sorcerer->getGameStats()->setNextSkillDelay(0);

	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), flameBolt.get()) << "all HP: Cold Attack's max_hp 35 is not met, Flame Bolt follows";
	setHpPercentage(*sorcerer, 35);
	EXPECT_EQ(SkillAttackManager::chooseNextSkill(ai).get(), coldAttack.get()) << "35 % HP: both are ready, prio 24 before prio 23";
}

/**
 * 283139 (300520000_Dragon_Lords_Refuge.xml:44-47): 20985 Smash Casting and 20986 Summon Rock, both prob 100 and priority 0, so every
 * decision takes the skill asked first - and chooseNextSkill shuffles a priority group of more than one skill before it asks
 * (SkillAttackManager.java:160-162). Over 32 fixed seeds the second skill of the row is taken at least once; unshuffled it never would be.
 */
TEST_F(NpcSkillRotationTest, APriorityGroupIsAskedInARandomOrder) {
	AI_TEST_SCOPE;
	Ref<Npc> creation = makeWorldNpc(DIVISIVE_CREATION_NPC_ID);
	NpcAI& ai = aiOf(*creation);
	Ptr<NpcSkillEntry> smash = entry(*creation, 0);
	Ptr<NpcSkillEntry> rock = entry(*creation, 1);
	ASSERT_EQ(smash->getSkillId(), SMASH_CASTING);
	ASSERT_EQ(rock->getSkillId(), SUMMON_ROCK);

	int32_t rockTaken = 0;
	for (uint64_t seed = 20260924; seed < 20260924 + 32; ++seed) {
		Rnd::seedCurrentThreadForTests(seed);
		Ptr<NpcSkillEntry> chosen = SkillAttackManager::chooseNextSkill(ai);
		ASSERT_TRUE(chosen.get() == smash.get() || chosen.get() == rock.get()) << "precondition: prob 100, every decision takes one of the two";
		if (chosen.get() == rock.get())
			++rockTaken;
	}
	EXPECT_GT(rockTaken, 0) << "20986, second in the row, was asked first under some seed";
}

// ---- performAttack and skillAction --------------------------------------------------------------------------------------------------------------

/**
 * The kerub's Brandish end to end: performAttack enters CAST and skillAction turns the npc to its most hated creature (the npc_skill's target
 * attribute, MOST_HATED by default) and casts; Brandish is a 2,500 ms cast, and Skill.endCast hands the npc back to afterUseSkill, which leaves
 * CAST. The pagati's tribe is hostile to MONSTER in the shipped relations, which is what Brandish's ENEMY target relation needs.
 */
TEST_F(NpcSkillRotationTest, PerformAttackEntersTheCastSubStateAndTheEndOfTheCastLeavesIt) {
	AI_TEST_SCOPE;
	Ref<Npc> kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
	Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501.5f, 500, 100);
	Ref<Npc> bystander = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501, 501, 100);
	know(*kerub, *pagati);
	know(*kerub, *bystander);
	ASSERT_TRUE(kerub->isEnemy(*pagati)) << "HOSTILEONLYMONSTER: <hostile>MONSTER</hostile>";
	NpcAI& ai = aiOf(*kerub);
	ai.setStateIfNot(AIState::FIGHT);
	kerub->getAggroList().addHate(*pagati, 100);
	kerub->setTarget(bystander); // not the most hated one
	Ptr<NpcSkillEntry> brandish = entry(*kerub, 0);
	kerub->getGameStats()->setLastSkill(brandish); // what GeneralNpcAI::chooseSkillAttack stores before AttackManager asks for the attack
	const int32_t pagatiMaxHp = pagati->getLifeStats()->getMaxHp();
	ASSERT_EQ(pagati->getLifeStats()->getCurrentHp(), pagatiMaxHp);
	Rnd::seedCurrentThreadForTests(20260924); // the hit, dodge and damage rolls of Effect.initialize: one fixed run

	SkillAttackManager::performAttack(ai, 0);
	EXPECT_TRUE(ai.isInSubState(AISubState::CAST));
	EXPECT_EQ(kerub->getTarget().get(), pagati.get()) << "skillAction turned the npc to its most hated creature";
	EXPECT_TRUE(kerub->isCasting()) << "skillAction cast Brandish";

	executor->advance(std::chrono::milliseconds(2400));
	EXPECT_TRUE(ai.isInSubState(AISubState::CAST)) << "the npc stays in CAST while the 2,500 ms cast bar runs";
	executor->advance(std::chrono::milliseconds(200));
	EXPECT_TRUE(ai.isInSubState(AISubState::NONE)) << "Skill.endCast -> SkillAttackManager::afterUseSkill";
	EXPECT_LT(pagati->getLifeStats()->getCurrentHp(), pagatiMaxHp)
		<< "the most hated creature took Brandish's <skillatk> (SkillAttackInstantEffect -> DamageEffect -> CreatureController::onAttack)";
	EXPECT_EQ(bystander->getLifeStats()->getCurrentHp(), bystander->getLifeStats()->getMaxHp()) << "and the former target did not";
}

TEST_F(NpcSkillRotationTest, SkillActionGivesUpWithoutATargetOrWithoutASkill) {
	AI_TEST_SCOPE;
	Ref<Npc> kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
	Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501.5f, 500, 100);
	NpcAI& ai = aiOf(*kerub);
	ai.setStateIfNot(AIState::FIGHT);

	ASSERT_TRUE(ai.setSubStateIfNot(AISubState::CAST));
	kerub->getGameStats()->setLastSkill(entry(*kerub, 0));
	SkillAttackManager::skillAction(ai);
	EXPECT_TRUE(ai.isInSubState(AISubState::NONE)) << "no target";

	ai.setSubStateIfNot(AISubState::NONE); // each row starts outside CAST, whatever the row before left
	kerub->setTarget(pagati);
	ASSERT_TRUE(ai.setSubStateIfNot(AISubState::CAST));
	kerub->getGameStats()->setLastSkill(nullptr);
	SkillAttackManager::skillAction(ai);
	EXPECT_TRUE(ai.isInSubState(AISubState::NONE)) << "no skill";
}

/**
 * 210161's 16424 Shout (npc_skills.xml:2878) has first_target ME (skill_templates.xml:118331): skillAction turns the npc to itself before it
 * casts, whatever it was fighting (SkillAttackManager.java:79-80). 9 of the 29 start-map npc skills are such first_target ME skills.
 */
TEST_F(NpcSkillRotationTest, AFirstTargetMeSkillTurnsTheNpcToItself) {
	AI_TEST_SCOPE;
	Ref<Npc> loudmouth = makeWorldNpc(TURSIN_LOUDMOUTH_NPC_ID, 500, 500, 100);
	Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501.5f, 500, 100);
	know(*loudmouth, *pagati);
	ASSERT_TRUE(loudmouth->isEnemy(*pagati)) << "HOSTILEONLYMONSTER is hostile to KRALL's base MONSTER";
	NpcAI& ai = aiOf(*loudmouth);
	ai.setStateIfNot(AIState::FIGHT);
	loudmouth->getAggroList().addHate(*pagati, 100);
	loudmouth->setTarget(pagati);
	Ptr<NpcSkillEntry> shout = entryOf(*loudmouth, SHOUT);
	ASSERT_TRUE(shout);
	loudmouth->getGameStats()->setLastSkill(shout);

	SkillAttackManager::performAttack(ai, 0);
	EXPECT_EQ(loudmouth->getTarget().get(), loudmouth.get()) << "first_target ME: the npc targets itself for Shout";
	loudmouth->getController().abortCast();
}

/**
 * skillAction's switch over the npc_skill's target attribute (SkillAttackManager.java:82-96) for the attributes no small shipped row carries
 * (NpcSkillTestSupport.h): each row is a fresh kerub that hates three pagatis 300/200/100, targets a fourth one it does not hate and attacks
 * with a queued Brandish of that attribute. MOST_HATED is PerformAttackEntersTheCastSubStateAndTheEndOfTheCastLeavesIt's row, FRIEND the next
 * case's.
 */
TEST_F(NpcSkillRotationTest, SkillActionTurnsTheNpcToTheTargetItsNpcSkillNames) {
	AI_TEST_SCOPE;
	Ref<Npc> mostHated = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501, 500, 100);
	Ref<Npc> second = makeWorldNpc(TAMED_PAGATI_NPC_ID, 500, 501, 100);
	Ref<Npc> third = makeWorldNpc(TAMED_PAGATI_NPC_ID, 499, 500, 100);
	Ref<Npc> bystander = makeWorldNpc(TAMED_PAGATI_NPC_ID, 500, 499, 100);
	auto attackWith = [&](NpcSkillTargetAttribute attribute, Npc& initialTarget) {
		Ref<Npc> kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
		for (const Ref<Npc>* other : {&mostHated, &second, &third, &bystander})
			know(*kerub, **other);
		kerub->getAggroList().addHate(*mostHated, 300);
		kerub->getAggroList().addHate(*second, 200);
		kerub->getAggroList().addHate(*third, 100);
		kerub->setTarget(Ptr<VisibleObject>(initialTarget));
		kerub->getGameStats()->setLastSkill(queueSkill(*kerub, BRANDISH, 1, 0, attribute));
		SkillAttackManager::performAttack(aiOf(*kerub), 0);
		Ptr<VisibleObject> target = kerub->getTarget();
		kerub->getController().abortCast(); // the Brandish cast itself is not the point
		return std::make_pair(kerub, target);
	};

	EXPECT_EQ(attackWith(NpcSkillTargetAttribute::SECOND_MOST_HATED, *bystander).second.get(), second.get()) << "SECOND_MOST_HATED";
	EXPECT_EQ(attackWith(NpcSkillTargetAttribute::THIRD_MOST_HATED, *bystander).second.get(), third.get()) << "THIRD_MOST_HATED";
	const auto me = attackWith(NpcSkillTargetAttribute::ME, *bystander);
	EXPECT_EQ(me.second.get(), me.first.get()) << "ME: the npc itself";
	EXPECT_EQ(attackWith(NpcSkillTargetAttribute::NONE, *bystander).second.get(), bystander.get()) << "NONE: the target stays";
	const Ptr<VisibleObject> random = attackWith(NpcSkillTargetAttribute::RANDOM, *bystander).second;
	EXPECT_TRUE(random.get() == mostHated.get() || random.get() == second.get() || random.get() == third.get())
		<< "RANDOM: one of the hated creatures within Brandish's first_target_range";
	const Ptr<VisibleObject> exceptCurrent = attackWith(NpcSkillTargetAttribute::RANDOM_EXCEPT_CURRENT_TARGET, *mostHated).second;
	EXPECT_TRUE(exceptCurrent.get() == second.get() || exceptCurrent.get() == third.get())
		<< "RANDOM_EXCEPT_CURRENT_TARGET: a hated creature other than the current target";
}

/**
 * 230745's 20556 Protective Shield has target="FRIEND" (300540000_Eternal_Bastion.xml:525): skillAction turns the npc to a known, visible,
 * living npc that is no enemy of it, within the skill's first_target_range (SkillAttackManager.java:85-86) - the friend 231105, not the enemy
 * 209556 it was fighting.
 */
TEST_F(NpcSkillRotationTest, AFriendTargetSkillTurnsTheNpcToAnNpcThatIsNoEnemy) {
	AI_TEST_SCOPE;
	Ref<Npc> tribuni = makeWorldNpc(TRIBUNI_PROTECTOR_NPC_ID, 500, 500, 100);
	Ref<Npc> friendNpc = makeWorldNpc(PASHID_PROTECTOR_NPC_ID, 502, 500, 100);
	Ref<Npc> guard = makeWorldNpc(GRANIRS_DISCIPLE_NPC_ID, 498, 500, 100);
	know(*tribuni, *friendNpc);
	know(*tribuni, *guard);
	ASSERT_TRUE(tribuni->isEnemy(*guard)) << "IDF5_TD_GUARD_DARK aggroes VRITRASUPPORT";
	ASSERT_FALSE(tribuni->isEnemy(*friendNpc)) << "IDF5_TD_ASSULT is a <friend> of VRITRASUPPORT";
	NpcAI& ai = aiOf(*tribuni);
	Ptr<NpcSkillEntry> shield = entry(*tribuni, 1);
	ASSERT_EQ(shield->getSkillId(), PROTECTIVE_SHIELD);
	ASSERT_EQ(shield->getTemplate()->getTarget(), NpcSkillTargetAttribute::FRIEND);
	tribuni->setTarget(guard);
	tribuni->getGameStats()->setLastSkill(shield);

	SkillAttackManager::performAttack(ai, 0);
	EXPECT_EQ(tribuni->getTarget().get(), friendNpc.get()) << "FRIEND: the npc that is no enemy";
	tribuni->getController().abortCast();
}

/** skillAction's cantUseSkill arm (SkillAttackManager.java:76-77): a skill the npc cannot use now (STUN is a CANT_ATTACK_STATE) leaves CAST */
TEST_F(NpcSkillRotationTest, ASkillTheNpcCannotUseNowLeavesTheCastSubStateAtOnce) {
	AI_TEST_SCOPE;
	Ref<Npc> kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
	Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501.5f, 500, 100);
	know(*kerub, *pagati);
	NpcAI& ai = aiOf(*kerub);
	ai.setStateIfNot(AIState::FIGHT);
	kerub->getAggroList().addHate(*pagati, 100);
	kerub->setTarget(pagati);
	kerub->getGameStats()->setLastSkill(entry(*kerub, 0));
	ASSERT_TRUE(ai.setSubStateIfNot(AISubState::CAST)); // the skill attack was decided before the stun
	kerub->getEffectController()->setAbnormal(AbnormalState::STUN);

	SkillAttackManager::skillAction(ai);
	EXPECT_TRUE(ai.isInSubState(AISubState::NONE)) << "cantUseSkill -> afterUseSkill";
	kerub->getEffectController()->unsetAbnormal(AbnormalState::STUN);
}

/**
 * skillAction reads the entry's skill template where Java does and nowhere else (SkillAttackManager.java:72-83): `skill.getSkillTemplate()`
 * is null for a queued skill of an id SKILL_DATA does not know (99999 is in no skill_templates.xml row), cantUseSkill never dereferences it
 * while the npc is in a CANT_ATTACK_STATE, and the cast does - `template.getProperties()`, a NullPointerException.
 */
TEST_F(NpcSkillRotationTest, SkillActionDereferencesTheSkillTemplateOnlyWhereJavaDoes) {
	AI_TEST_SCOPE;
	Ref<Npc> kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
	Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501.5f, 500, 100);
	know(*kerub, *pagati);
	NpcAI& ai = aiOf(*kerub);
	ai.setStateIfNot(AIState::FIGHT);
	kerub->getAggroList().addHate(*pagati, 100);
	kerub->setTarget(pagati);
	Ptr<NpcSkillEntry> unknown = queueSkill(*kerub, 99999, 1, 0, NpcSkillTargetAttribute::MOST_HATED);
	ASSERT_EQ(unknown->getSkillTemplate(), nullptr);
	kerub->getGameStats()->setLastSkill(unknown);

	kerub->getEffectController()->setAbnormal(AbnormalState::STUN);
	ASSERT_TRUE(ai.setSubStateIfNot(AISubState::CAST));
	EXPECT_NO_THROW(SkillAttackManager::skillAction(ai)) << "a stunned npc: cantUseSkill answers without the template";
	EXPECT_TRUE(ai.isInSubState(AISubState::NONE)) << "cantUseSkill -> afterUseSkill";
	kerub->getEffectController()->unsetAbnormal(AbnormalState::STUN);

	ASSERT_TRUE(ai.setSubStateIfNot(AISubState::CAST));
	EXPECT_THROW(SkillAttackManager::skillAction(ai), runtime::NullPointerException) << "the cast: template.getProperties() on null";
}

/**
 * performAttack with a delay (SkillAttackManager.java:44-47) is the production path: AttackManager.scheduleNextAttack passes
 * getNextAttackInterval, 750 ms for the first decision in attack range and what is left of the attack speed after that. The npc enters CAST at
 * once and casts when the scheduled skillAction runs.
 */
TEST_F(NpcSkillRotationTest, ADelayedSkillAttackEntersCastAtOnceAndCastsWhenItsTaskRuns) {
	AI_TEST_SCOPE;
	Ref<Npc> kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID, 500, 500, 100);
	Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 501.5f, 500, 100);
	know(*kerub, *pagati);
	NpcAI& ai = aiOf(*kerub);
	ai.setStateIfNot(AIState::FIGHT);
	kerub->getAggroList().addHate(*pagati, 100);
	kerub->setTarget(pagati);
	kerub->getGameStats()->setLastSkill(entry(*kerub, 0));

	SkillAttackManager::performAttack(ai, 750);
	EXPECT_TRUE(ai.isInSubState(AISubState::CAST)) << "CAST at once";
	EXPECT_FALSE(kerub->isCasting()) << "the skillAction task has not run yet";
	executor->advance(std::chrono::milliseconds(750));
	EXPECT_TRUE(kerub->isCasting()) << "the task ran skillAction, which cast Brandish";
	executor->advance(std::chrono::milliseconds(2600)); // to the end of the cast
}

/**
 * An npc without attack range casts only at a target inside its aggro range (SkillAttackManager.java:36-43 and :67-71; 206292 has no arange
 * and srange 10). Outside it performAttack gives up before CAST, and skillAction aborts the cast it was about to begin; both hand the AI
 * TARGET_TOOFAR, and abortCast drops the npc's last skill (CreatureController.java:495-506).
 */
TEST_F(NpcSkillRotationTest, AnNpcWithoutAttackRangeGivesUpItsSkillOnATargetOutsideItsAggroRange) {
	AI_TEST_SCOPE;
	Ref<Npc> caster = makeWorldNpc(RANGELESS_CASTER_NPC_ID, 500, 500, 100);
	Ref<Npc> pagati = makeWorldNpc(TAMED_PAGATI_NPC_ID, 512, 500, 100); // 12 m: outside srange 10, inside Flame Blaze's first_target_range 15
	know(*caster, *pagati);
	ASSERT_TRUE(caster->isEnemy(*pagati));
	ASSERT_EQ(caster->getObjectTemplate()->getAttackRange(), 0);
	ASSERT_EQ(caster->getAggroRange(), 10);
	NpcAI& ai = aiOf(*caster);
	ai.setStateIfNot(AIState::FIGHT);
	caster->getAggroList().addHate(*pagati, 100);
	caster->setTarget(pagati);
	Ptr<NpcSkillEntry> flameBlaze = entry(*caster, 0);

	caster->getGameStats()->setLastSkill(flameBlaze);
	SkillAttackManager::performAttack(ai, 750);
	EXPECT_FALSE(ai.isInSubState(AISubState::CAST)) << "performAttack gave up at once: no CAST, no scheduled skillAction";

	// the skillAction of a skill attack decided while the target was still inside the aggro range
	caster->getGameStats()->setLastSkill(flameBlaze);
	ASSERT_TRUE(ai.setSubStateIfNot(AISubState::CAST));
	SkillAttackManager::skillAction(ai);
	EXPECT_EQ(caster->getGameStats()->getLastSkill().get(), nullptr) << "skillAction aborted: abortCast dropped the last skill";
	EXPECT_FALSE(caster->isCasting()) << "and Flame Blaze was not cast";
}

// ---- ShoutEventHandler.onCast -------------------------------------------------------------------------------------------------------------------

TEST_F(NpcSkillRotationTest, OnCastIgnoresANullFirstTarget) {
	AI_TEST_SCOPE;
	Ref<Npc> kerub = makeWorldNpc(STRIPED_KERUB_NPC_ID);
	// Java: `firstTarget instanceof Player && ...` is false for null (ShoutEventHandler.java:123-126); Skill.startCast passes the nullable first
	// target of every npc cast of target type 0 (Skill.java:504-506)
	EXPECT_NO_THROW(handler::ShoutEventHandler::onCast(aiOf(*kerub), nullptr));
}

} // namespace
} // namespace aion::gameserver::ai::testing
