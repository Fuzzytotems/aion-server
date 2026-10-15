#include "aion/gameserver/handlers/ai/instance/idgelDome/UnstableIdeEnergyAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::idgelDome {

AION_AI(UnstableIdeEnergyAI, "unstable_id_energy");

// Java UnstableIdeEnergyAI.java:27-31
void UnstableIdeEnergyAI::handleSpawned() {
	NpcAI::handleSpawned();
	scheduleSkill(commons::utils::Rnd::get(10000, 30000));
}

// Java UnstableIdeEnergyAI.java:33-36
float UnstableIdeEnergyAI::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
	return 0;
}

// Java UnstableIdeEnergyAI.java:38-40
void UnstableIdeEnergyAI::scheduleSkill(int32_t delay) {
	skillTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::useSkill(*this, 21559); }, delay));
}

// Java UnstableIdeEnergyAI.java:42-49
void UnstableIdeEnergyAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	switch (skillTemplate->getSkillId()) {
		case 21559:
			scheduleSkill(commons::utils::Rnd::get(10000, 30000));
			break;
	}
}

// Java UnstableIdeEnergyAI.java:51-55 (a despawn before the spawn finds skillTask null: Java's NullPointerException)
void UnstableIdeEnergyAI::handleDespawned() {
	runtime::FutureRef task = skillTask.get();
	if (task == nullptr)
		throw runtime::NullPointerException("UnstableIdeEnergyAI.skillTask");
	task->cancel(true);
	NpcAI::handleDespawned();
}

} // namespace aion::gameserver::handlers::ai::instance::idgelDome
