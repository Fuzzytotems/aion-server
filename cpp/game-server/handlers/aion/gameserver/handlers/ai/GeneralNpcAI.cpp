#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

#include "aion/gameserver/ai/handler/AggroEventHandler.h"
#include "aion/gameserver/ai/handler/AttackEventHandler.h"
#include "aion/gameserver/ai/handler/MoveEventHandler.h"
#include "aion/gameserver/ai/handler/ReturningEventHandler.h"
#include "aion/gameserver/ai/handler/TalkEventHandler.h"
#include "aion/gameserver/ai/handler/TargetEventHandler.h"
#include "aion/gameserver/ai/handler/ThinkEventHandler.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai {

AION_AI(GeneralNpcAI, "general");

void GeneralNpcAI::think() {
	ThinkEventHandler::onThink(*this);
}

void GeneralNpcAI::handleAttack(runtime::Ptr<Creature> creature) {
	AttackEventHandler::onAttack(*this, creature);
}

bool GeneralNpcAI::handleCreatureNeedsSupport(Creature& creature) {
	return AggroEventHandler::onCreatureNeedsSupport(*this, creature);
}

void GeneralNpcAI::handleCreatureNotSee(Creature& creature) {
	// Java: creature.equals(getTarget()) - AionObject.equals(null) is false (AionObject.java:57-58)
	runtime::Ptr<VisibleObject> target = getTarget();
	if (target && creature.equals(*target)) {
		getOwner().getController().abortCast();
		onGeneralEvent(AIEventType::TARGET_TOOFAR);
	}
}

void GeneralNpcAI::handleDialogStart(Player& player) {
	TalkEventHandler::onTalk(*this, player);
}

void GeneralNpcAI::handleDialogFinish(Player& creature) {
	TalkEventHandler::onFinishTalk(*this, creature);
}

void GeneralNpcAI::handleFinishAttack() {
	AttackEventHandler::onFinishAttack(*this);
}

void GeneralNpcAI::handleAttackComplete() {
	AttackEventHandler::onAttackComplete(*this);
}

void GeneralNpcAI::handleNotAtHome() {
	ReturningEventHandler::onNotAtHome(*this);
}

void GeneralNpcAI::handleBackHome() {
	ReturningEventHandler::onBackHome(*this);
}

void GeneralNpcAI::handleTargetTooFar() {
	TargetEventHandler::onTargetTooFar(*this);
}

void GeneralNpcAI::handleTargetGiveup() {
	TargetEventHandler::onTargetGiveup(*this);
}

void GeneralNpcAI::handleTargetChanged(Creature& creature) {
	NpcAI::handleTargetChanged(creature);
	TargetEventHandler::onTargetChange(*this, creature);
}

void GeneralNpcAI::handleMoveArrived() {
	NpcAI::handleMoveArrived();
	MoveEventHandler::onMoveArrived(*this);
}

void GeneralNpcAI::handleCreatureDetected(Creature& creature) {
	getOwner().getPosition()->getWorldMapInstance()->getInstanceHandler()->onCreatureDetected(getOwner(), creature);
}

bool GeneralNpcAI::canHandleEvent(AIEventType eventType) {
	switch (eventType) {
		case AIEventType::CREATURE_NEEDS_SUPPORT:
			return getState() == AIState::IDLE || getState() == AIState::WALKING;
		default:
			break;
	}
	return NpcAI::canHandleEvent(eventType);
}

AttackIntention GeneralNpcAI::chooseAttackIntention() {
	runtime::Ptr<Creature> target = runtime::as<Creature>(getTarget());
	if (!target || !getAggroList().isHating(*target)) {
		runtime::Ptr<Creature> mostHated = getAggroList().getTarget(AggroTarget::MOST_HATED);
		if (!mostHated)
			return AttackIntention::FINISH_ATTACK;
		onCreatureEvent(AIEventType::TARGET_CHANGED, *mostHated);
	}

	if (chooseSkillAttack(getOwner().getObjectTemplate()->getAttackRange() == 0))
		return AttackIntention::SKILL_ATTACK;

	return AttackIntention::SIMPLE_ATTACK;
}

bool GeneralNpcAI::chooseSkillAttack(bool alwaysRandomSkill) {
	// Java (GeneralNpcAI.java:130-138):
	//   NpcSkillEntry skill = alwaysRandomSkill ? getOwner().getSkillList().getRandomSkill() : SkillAttackManager.chooseNextSkill(this);
	//   if (skill != null) { getOwner().getGameStats().setLastSkill(skill); getOwner().removeNextQueuedSkill(skill); return true; }
	//   return false;
	// Both arms reach the skill engine of M5b-2 (NpcSkillTemplateEntry.conditionReady, SkillEngine), so M5b-1 answers "no skill" (m5b-plan.md
	// D4). AttackManager::chooseAttack then takes the SIMPLE_ATTACK arm, and SkillAttackManager - itself a shell of partials - is never asked.
	// The gate asserts this site is hit 0 times on the scripted path (m5b-plan.md §6.1 §B): neither gate monster has an npc_skills.xml entry,
	// and both have attack_range != 0, so `alwaysRandomSkill` is false there.
	AION_PARTIAL("npc skill attacks are not chosen yet (M5b-2): every attack stays a simple melee attack");
	return false;
}

} // namespace aion::gameserver::handlers::ai
