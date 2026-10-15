#include "aion/gameserver/handlers/ai/instance/danuarReliquary/FinishThemAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::danuarReliquary {

AION_AI(FinishThemAI, "finish_them");

// Java FinishThemAI.java:20-24
void FinishThemAI::handleSpawned() {
	NpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::useSkill(*this, 21199); }, 1000);
}

// Java FinishThemAI.java:26-30
void FinishThemAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21199)
		getOwner().getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::danuarReliquary
