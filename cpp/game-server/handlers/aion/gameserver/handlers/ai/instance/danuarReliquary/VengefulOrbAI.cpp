#include "aion/gameserver/handlers/ai/instance/danuarReliquary/VengefulOrbAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::danuarReliquary {

AION_AI(VengefulOrbAI, "vengeful_orb");

// Java VengefulOrbAI.java:24-34 (the anonymous Runnable: a lambda pinned to the AI)
void VengefulOrbAI::handleSpawned() {
	NpcAI::handleSpawned();
	skillTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!getOwner().isDead())
			AIActions::useSkill(*this, 21178);
	}, 100));
}

// Java VengefulOrbAI.java:36-40
void VengefulOrbAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21178)
		getOwner().getController().delete_();
}

// Java VengefulOrbAI.java:42-48
void VengefulOrbAI::handleDespawned() {
	runtime::FutureRef task = skillTask.get();
	if (task != nullptr) {
		task->cancel(false);
	}
	NpcAI::handleDespawned();
}

} // namespace aion::gameserver::handlers::ai::instance::danuarReliquary
