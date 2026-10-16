#include "aion/gameserver/handlers/ai/instance/danuarReliquary/ModorsPositionIndicatorAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::danuarReliquary {

AION_AI(ModorsPositionIndicatorAI, "modors_position_indicator");

// Java ModorsPositionIndicatorAI.java:19-24
void ModorsPositionIndicatorAI::handleSpawned() {
	NpcAI::handleSpawned();
	SkillEngine::getInstance().getSkill(getOwner(), 21166, 1, runtime::Ptr<VisibleObject>(getOwner()))->useWithoutPropSkill();
}

// Java ModorsPositionIndicatorAI.java:26-30
void ModorsPositionIndicatorAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21166)
		getOwner().getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::danuarReliquary
