#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/DynatoumHealerAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

AION_AI(DynatoumHealerAI, "dynatoum_healer");

// Java DynatoumHealerAI.java:20-23
bool DynatoumHealerAI::canThink() {
	return false;
}

// Java DynatoumHealerAI.java:25-29
void DynatoumHealerAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	startHealing();
}

// Java DynatoumHealerAI.java:31-37
void DynatoumHealerAI::startHealing() {
	runtime::Ptr<Npc> boss = getPosition()->getWorldMapInstance()->getNpc(getPosition()->getMapId() == 301230000 ? 233740 : 234686);
	if (boss != nullptr) {
		AIActions::targetCreature(*this, *boss);
		getOwner().queueSkill(21535, 1, 10000);
	}
}

// Java DynatoumHealerAI.java:39-43
void DynatoumHealerAI::onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate == nullptr) // Java: skillTemplate.getSkillId() on null
		throw runtime::NullPointerException("DynatoumHealerAI: skill template");
	if (skillTemplate->getSkillId() == 21535)
		getOwner().queueSkill(21535, 1, 10000);
}

} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk
