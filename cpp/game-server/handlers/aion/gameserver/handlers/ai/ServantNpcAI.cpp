#include "aion/gameserver/handlers/ai/ServantNpcAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/manager/SkillAttackManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/NpcObjectType.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::handlers::ai {

AION_AI(ServantNpcAI, "servant");

// Java ServantNpcAI.java:29-32
void ServantNpcAI::think() {
	// servants are not thinking
}

// Java ServantNpcAI.java:34-37
bool ServantNpcAI::canThink() {
	return false;
}

// Java ServantNpcAI.java:39-50. The lambda (fieldmap: a task, pin {this}) is pinned on this AI, a part of its npc, so the pin retains the npc
// until the task has run.
void ServantNpcAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	if (getCreator()) {
		ThreadPoolManager::getInstance().schedule({this}, [this] {
			if (getOwner().getNpcObjectType() != NpcObjectType::TOTEM) {
				// Java: AIActions.targetCreature(this, (Creature) getCreator().getTarget()) - a null target is set as null
				runtime::Ptr<Creature> target = runtime::cast<Creature>(getCreator()->getTarget());
				if (target)
					AIActions::targetCreature(*this, *target);
				else
					getOwner().setTarget(nullptr);
			} else
				AIActions::targetSelf(*this);
			healOrAttack();
		}, 200);
	}
}

// Java ServantNpcAI.java:52-76. The fixed-rate lambda (fieldmap: pin {this, &target, &skill}) holds the target and the skill entry as Refs
// (the target may be null, which a pin list cannot name).
void ServantNpcAI::healOrAttack() {
	runtime::Ptr<NpcSkillEntry> skill = getSkillList()->getRandomSkill();
	if (!skill)
		return;
	getOwner().getGameStats()->setLastSkill(skill);
	int32_t duration = getOwner().getNpcObjectType() == NpcObjectType::TOTEM ? 3000 : 5000;
	int32_t startDelay = 1000;
	switch (getOwner().getNpcId()) {
		// Taunting Spirit
		case 833403:
		case 833404:
		case 833478:
		case 833479:
		case 833480:
		case 833481:
			duration = 5000;
			break;
		// Battle Banner
		case 833077:
		case 833078:
		case 833452:
		case 833453:
		case 833454:
		case 833455:
			duration = 3000;
			startDelay = 100;
			break;
		default:
			break;
	}
	const runtime::Ref<Creature> target(runtime::cast<Creature>(getOwner().getTarget()));
	const runtime::Ref<NpcSkillEntry> entry(skill);
	skillTask = ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this, target, entry] {
		if (!target || target->isDead()) {
			AIActions::deleteOwner(*this);
			cancelTask();
		} else if (!SkillAttackManager::cantUseSkill(*entry, getOwner())) {
			SkillEngine::getInstance().getSkill(getOwner(), entry->getSkillId(), entry->getSkillLevel(), getOwner().getTarget())->useSkill();
		}
	}, startDelay, duration);
	getOwner().getController().addTask(TaskId::SKILL_USE, skillTask.get());
}

// Java ServantNpcAI.java:78-81
bool ServantNpcAI::isMoveSupported() {
	return false;
}

// Java ServantNpcAI.java:83-86
void ServantNpcAI::cancelTask() {
	runtime::Ptr<runtime::Future> task = skillTask.get();
	if (task && !task->isDone())
		task->cancel(true);
}

// Java ServantNpcAI.java:88-94
bool ServantNpcAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return GeneralNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai
