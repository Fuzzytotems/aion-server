#include "aion/gameserver/skillengine/effect/ReflectorEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/ShieldType.h"

namespace aion::gameserver::skillengine::effect {

void ReflectorEffect::startEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void ReflectorEffect::endEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

model::ShieldType ReflectorEffect::getType() const {
	return reflectType == 1 ? model::ShieldType::SKILL_REFLECTOR : model::ShieldType::REFLECTOR;
}

} // namespace aion::gameserver::skillengine::effect
