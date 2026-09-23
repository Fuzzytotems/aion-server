#include "aion/gameserver/skillengine/effect/ProvokerEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void ProvokerEffect::applyEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void ProvokerEffect::startEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

bool ProvokerEffect::shouldApply(gameserver::model::gameobjects::Creature& /*effector*/, gameserver::model::gameobjects::Creature& /*target*/,
	int32_t /*attackSkillId*/) const {
	AION_UNPORTED();
}

runtime::Ptr<gameserver::model::gameobjects::Creature> ProvokerEffect::getProvokeTarget(gameserver::model::gameobjects::Creature& /*effector*/,
	gameserver::model::gameobjects::Creature& /*target*/) const {
	AION_UNPORTED();
}

void ProvokerEffect::endEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
