#include "aion/gameserver/skillengine/effect/CaseHealEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void CaseHealEffect::applyEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

int32_t CaseHealEffect::getCurrentStatValue(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

int32_t CaseHealEffect::getMaxStatValue(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

bool CaseHealEffect::allowHpHealBoost(model::Effect& /*effect*/) const {
	return false;
}

bool CaseHealEffect::allowHpHealSkillDeboost(model::Effect& /*effect*/) const {
	return false;
}

} // namespace aion::gameserver::skillengine::effect
