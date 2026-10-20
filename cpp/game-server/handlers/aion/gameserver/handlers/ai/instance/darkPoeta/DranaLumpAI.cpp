#include "aion/gameserver/handlers/ai/instance/darkPoeta/DranaLumpAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

AION_AI(DranaLumpAI, "drana_lump");

// Java DranaLumpAI.java:23-34
void DranaLumpAI::handleCreatureDetected(Creature& creature) {
	if (runtime::Ptr<Npc> npc = runtime::as<Npc>(runtime::Ptr<Creature>(creature))) {
		switch (npc->getNpcId()) {
			case 214880:
			case 215388:
			case 215389:
				checkDistance(*npc);
				break;
		}
	}
}

// Java DranaLumpAI.java:36-43 (the lambda captures the npc)
void DranaLumpAI::checkDistance(Npc& npc) {
	ThreadPoolManager::getInstance().schedule({this}, [this, npc = runtime::Ref<Npc>(npc)] {
		if (PositionUtil::getDistance(getOwner(), *npc) <= 2)
			skillengine::SkillEngine::getInstance().getSkill(getOwner(), 18536, 46, runtime::Ptr<VisibleObject>(npc))->useSkill();
		else
			checkDistance(*npc);
	}, 4000);
}

// Java DranaLumpAI.java:45-49
void DranaLumpAI::onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate == nullptr) // Java: skillTemplate.getSkillId() on null
		throw runtime::NullPointerException("DranaLumpAI: skill template");
	if (skillTemplate->getSkillId() == 18536)
		getOwner().getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
