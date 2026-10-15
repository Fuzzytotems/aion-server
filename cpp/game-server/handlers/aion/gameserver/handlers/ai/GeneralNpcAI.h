#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a non-aggressive NPC ("general"): it fights back when attacked, talks, walks its route and returns home, but never starts a fight
 * on its own (AggressiveNpcAI adds that).
 * <p>
 * Java: data/handlers/ai/GeneralNpcAI.java, @AIName("general") (the marker is in the .cpp). Every hook delegates to one of the ai/handler
 * statics, so this class is the wiring and the handler package is the behaviour.
 *
 * @author ATracer
 */
class GeneralNpcAI : public NpcAI {
public:
	explicit GeneralNpcAI(Npc& owner) : NpcAI(owner) {}

	void think() override;

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

	bool handleCreatureNeedsSupport(Creature& creature) override;

	void handleCreatureNotSee(Creature& creature) override;

	void handleDialogStart(Player& player) override;

	void handleDialogFinish(Player& creature) override;

	void handleFinishAttack() override;

	void handleAttackComplete() override;

	void handleNotAtHome() override;

	void handleBackHome() override;

	void handleTargetTooFar() override;

	void handleTargetGiveup() override;

	void handleTargetChanged(Creature& creature) override;

	void handleMoveArrived() override;

public:
	void handleCreatureDetected(Creature& creature) override;

protected:
	bool canHandleEvent(AIEventType eventType) override;

public:
	AttackIntention chooseAttackIntention() override;

protected:
	/**
	 * **M5b-1 (m5b-plan.md D4): an AION_PARTIAL that answers false**, so chooseAttackIntention never returns SKILL_ATTACK and the skill engine
	 * (M5b-2) is never reached. Java is `alwaysRandomSkill ? getSkillList().getRandomSkill() : SkillAttackManager.chooseNextSkill(this)`, and
	 * both arms need NpcSkillTemplateEntry and SkillEngine. Java declares the method `protected final`.
	 */
	bool chooseSkillAttack(bool alwaysRandomSkill);
};

} // namespace aion::gameserver::handlers::ai
