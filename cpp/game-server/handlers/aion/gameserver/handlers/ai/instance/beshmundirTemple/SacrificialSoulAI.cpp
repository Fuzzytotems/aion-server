#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/SacrificialSoulAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

AION_AI(SacrificialSoulAI, "templeSoul");

// Java SacrificialSoulAI.java:24-34
void SacrificialSoulAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	AIActions::useSkill(*this, 18901);
	this->setStateIfNot(gameserver::ai::AIState::FOLLOWING);
	boss.set(getPosition()->getWorldMapInstance()->getNpc(216263));
	if (runtime::Ptr<Npc> b = boss.get(); b != nullptr && !b->isDead()) {
		AIActions::targetCreature(*this, *b);
		getMoveController()->moveToTargetObject();
	}
}

// Java SacrificialSoulAI.java:36-43
void SacrificialSoulAI::handleAttack(runtime::Ptr<Creature> creature) {
	GeneralNpcAI::handleAttack(creature);
	if (creature->getEffectController()->hasAbnormalEffect(18959)) {
		getMoveController()->abortMove();
		AIActions::deleteOwner(*this);
	}
}

// Java SacrificialSoulAI.java:45-51
void SacrificialSoulAI::handleMoveArrived() {
	if (runtime::Ptr<Npc> b = boss.get(); b != nullptr && !b->isDead()) {
		SkillEngine::getInstance().getSkill(getOwner(), 18960, 55, runtime::Ptr<VisibleObject>(b))->useNoAnimationSkill();
		AIActions::deleteOwner(*this);
	}
}

// Java SacrificialSoulAI.java:53-56
bool SacrificialSoulAI::canThink() {
	return false;
}

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
