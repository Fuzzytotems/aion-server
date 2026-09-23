#include "aion/gameserver/skillengine/effect/HealEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void HealEffect::startEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void HealEffect::onPeriodicAction(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

int32_t HealEffect::getCurrentStatValue(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

int32_t HealEffect::getMaxStatValue(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
