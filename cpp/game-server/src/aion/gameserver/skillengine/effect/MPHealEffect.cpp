#include "aion/gameserver/skillengine/effect/MPHealEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void MPHealEffect::startEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void MPHealEffect::onPeriodicAction(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

int32_t MPHealEffect::getCurrentStatValue(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

int32_t MPHealEffect::getMaxStatValue(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
