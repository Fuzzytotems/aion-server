#include "aion/gameserver/skillengine/effect/HealInstantEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void HealInstantEffect::applyEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

int32_t HealInstantEffect::getCurrentStatValue(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

int32_t HealInstantEffect::getMaxStatValue(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
