#include "aion/gameserver/skillengine/effect/ProcHealInstantEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void ProcHealInstantEffect::calculate(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void ProcHealInstantEffect::applyEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

int32_t ProcHealInstantEffect::getCurrentStatValue(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

int32_t ProcHealInstantEffect::getMaxStatValue(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

bool ProcHealInstantEffect::allowHpHealBoost(model::Effect& /*effect*/) const {
	return false;
}

} // namespace aion::gameserver::skillengine::effect
