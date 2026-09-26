#include "aion/gameserver/skillengine/effect/DPHealEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void DPHealEffect::startEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void DPHealEffect::onPeriodicAction(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

int32_t DPHealEffect::getCurrentStatValue(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

int32_t DPHealEffect::getMaxStatValue(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
