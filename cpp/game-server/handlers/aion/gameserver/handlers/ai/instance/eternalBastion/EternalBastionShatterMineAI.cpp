#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionShatterMineAI.h"

#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/state/CreatureSeeState.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

AION_AI(EternalBastionShatterMineAI, "eternal_bastion_shatter_mine");

// Java EternalBastionShatterMineAI.java:20-24
void EternalBastionShatterMineAI::handleSpawned() {
	EternalBastionConstructAI::handleSpawned();
	getOwner().setSeeState(model::gameobjects::state::CreatureSeeState::SEARCH2);
}

// Java EternalBastionShatterMineAI.java:26-30
void EternalBastionShatterMineAI::handleCreatureSee(Creature& creature) {
	runtime::Ptr<Player> p = runtime::as<Player>(runtime::Ptr<Creature>(creature));
	if (p != nullptr && p->isTransformed() && p->getTransformModel().getModelId() == 284320)
		SkillEngine::getInstance().getSkill(getOwner(), 21454, 1, runtime::Ptr<VisibleObject>(p))->useNoAnimationSkill();
}

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
