#include "aion/gameserver/handlers/ai/BombAI.h"

#include "aion/gameserver/dataholders/AIData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/templates/ai/AITemplate.h"
#include "aion/gameserver/model/templates/ai/BombTemplate.h"
#include "aion/gameserver/model/templates/ai/Bombs.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai {

AION_AI(BombAI, "bomb");

// Java BombAI.java:29-34. The task (pin {this}) retains the npc until it ran or was cancelled.
void BombAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	const model::templates::ai::AITemplate* aiTemplate = DataManager::AI_DATA->getAiTemplate(getNpcId());
	if (aiTemplate == nullptr || aiTemplate->getBombs() == nullptr) // Java: the chain's NullPointerException, explicit
		throw runtime::NullPointerException("ai template " + std::to_string(getNpcId()) + " has no bombs");
	template_.set(aiTemplate->getBombs()->getBombTemplate());
	if (template_.get() == nullptr) // Java: template.getSkillId() of a null template, explicit
		throw runtime::NullPointerException("ai template " + std::to_string(getNpcId()) + " has no bomb template");
	addTask(ThreadPoolManager::getInstance().schedule({this}, [this] { useSkill(template_.get()->getSkillId()); }, template_.get()->getCd() + 2000));
}

// Java BombAI.java:36-40
void BombAI::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelTasks();
}

// Java BombAI.java:42-46
void BombAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	cancelTasks();
}

// Java BombAI.java:48-54
bool BombAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
		case AIQuestion::REWARD_LOOT:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

// Java BombAI.java:56-62
void BombAI::addTask(runtime::FutureRef task) {
	if (task == nullptr)
		return;
	SYNCHRONIZED(tasks) {
		tasks.add(task);
	}
}

// Java BombAI.java:64-71
void BombAI::cancelTasks() {
	SYNCHRONIZED(tasks) {
		for (const runtime::FutureRef& task : tasks.snapshot())
			if (task != nullptr && !task->isDone())
				task->cancel(true);
		tasks.clear();
	}
}

// Java BombAI.java:73-78. The deletion task (pin {this}) is the bomb's last one.
void BombAI::useSkill(int32_t skill) {
	AIActions::targetSelf(*this);
	AIActions::useSkill(*this, skill);
	const skillengine::model::SkillTemplate* skillTemplate = DataManager::SKILL_DATA->getSkillTemplate(skill);
	if (skillTemplate == nullptr) // Java: getSkillTemplate(skill).getDuration() of an unknown skill, explicit
		throw runtime::NullPointerException("skill template " + std::to_string(skill) + " is null");
	int32_t duration = skillTemplate->getDuration();
	addTask(ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::deleteOwner(*this); }, duration != 0 ? duration + 4000 : 0));
}

} // namespace aion::gameserver::handlers::ai
