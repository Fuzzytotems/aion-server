#include "aion/gameserver/handlers/ai/instance/tallocsHollow/KinquidDebuffAI.h"

#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::tallocsHollow {

AION_AI(KinquidDebuffAI, "kinquid_debuff");

// Java KinquidDebuffAI.java:20-26
void KinquidDebuffAI::handleCreatureMoved(Creature& creature) {
	AggressiveNpcAI::handleCreatureMoved(creature);
	runtime::Ptr<Npc> npc = runtime::as<Npc>(runtime::Ptr<Creature>(creature));
	if (npc != nullptr && npc->getNpcId() == 215467 && isInRange(creature, 2)) { // kinquid within debuff range
		SkillEngine::getInstance().getSkill(getOwner(), getNpcId() == 282008 ? 19235 : 19236, 46, runtime::Ptr<VisibleObject>(getOwner()))
			->useNoAnimationSkill();
	}
}

} // namespace aion::gameserver::handlers::ai::instance::tallocsHollow
