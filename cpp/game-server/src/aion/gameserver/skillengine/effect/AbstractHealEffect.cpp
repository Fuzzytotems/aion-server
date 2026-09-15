#include "aion/gameserver/skillengine/effect/AbstractHealEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::skillengine::effect {

bool AbstractHealEffect::isPercent() const {
	return percent;
}

bool AbstractHealEffect::allowHpHealBoost(model::Effect& effect) const {
	return effect.getSkillTemplate()->isApplyHealBoostBonus();
}

bool AbstractHealEffect::allowHpHealSkillDeboost(model::Effect& /*effect*/) const {
	return true;
}

int32_t AbstractHealEffect::calculateBaseHealValue(model::Effect& effect) const {
	return calculateBaseValue(effect);
}

int32_t AbstractHealEffect::calculateHealValue(model::Effect& /*effect*/, model::HealType /*type*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
