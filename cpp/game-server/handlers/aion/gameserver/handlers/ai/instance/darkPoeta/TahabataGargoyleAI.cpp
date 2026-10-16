#include "aion/gameserver/handlers/ai/instance/darkPoeta/TahabataGargoyleAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

AION_AI(TahabataGargoyleAI, "tahabata_gargoyle");

// Java TahabataGargoyleAI.java:19-23
void TahabataGargoyleAI::onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate == nullptr) // Java: skillTemplate.getSkillId() on null
		throw runtime::NullPointerException("TahabataGargoyleAI: skill template");
	if (skillTemplate->getSkillId() == 18219)
		getOwner().getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
