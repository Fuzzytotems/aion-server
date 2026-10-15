#include "aion/gameserver/handlers/ai/instance/sauroBase/SauroDroneAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::sauroBase {

AION_AI(SauroDroneAI, "sauro_drone");

// Java SauroDroneAI.java:19-23
void SauroDroneAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 19498)
		getOwner().getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::sauroBase
