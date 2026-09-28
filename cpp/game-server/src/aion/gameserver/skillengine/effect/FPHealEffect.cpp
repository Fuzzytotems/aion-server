#include "aion/gameserver/skillengine/effect/FPHealEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void FPHealEffect::startEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void FPHealEffect::onPeriodicAction(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

int32_t FPHealEffect::getCurrentStatValue(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

int32_t FPHealEffect::getMaxStatValue(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
