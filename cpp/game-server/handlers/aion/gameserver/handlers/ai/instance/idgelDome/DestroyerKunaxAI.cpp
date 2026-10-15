#include "aion/gameserver/handlers/ai/instance/idgelDome/DestroyerKunaxAI.h"

#include "aion/gameserver/model/templates/BoundRadius.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::handlers::ai::instance::idgelDome {

AION_AI(DestroyerKunaxAI, "destroyer_kunax");

// Java DestroyerKunaxAI.java:21-33
AttackIntention DestroyerKunaxAI::chooseAttackIntention() {
	double dist = 0;
	if (getTarget() != nullptr) {
		dist = PositionUtil::getDistance(getOwner(), *getTarget()) - getObjectTemplate()->getBoundRadius()->getMaxOfFrontAndSide()
			- getTarget()->getObjectTemplate()->getBoundRadius()->getMaxOfFrontAndSide();
	}
	if (dist > 3 && dist <= 30) {
		SkillEngine::getInstance().getSkill(getOwner(), 21550, 56, getTarget())->useSkill();
		return AttackIntention::SKILL_ATTACK;
	}
	return AggressiveNpcAI::chooseAttackIntention();
}

} // namespace aion::gameserver::handlers::ai::instance::idgelDome
