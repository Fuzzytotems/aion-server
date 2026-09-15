#include "aion/gameserver/skillengine/effect/HealOverTimeEffect.h"

#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::skillengine::effect {

bool HealOverTimeEffect::isPercent() const {
	return percent;
}

bool HealOverTimeEffect::allowHpHealBoost(model::Effect& effect) const {
	return effect.getSkillTemplate()->isApplyHealBoostBonus();
}

bool HealOverTimeEffect::allowHpHealSkillDeboost(model::Effect& effect) const {
	return effect.getSkillTemplate()->isApplyHealBoostBonus();
}

int32_t HealOverTimeEffect::calculateBaseHealValue(model::Effect& effect) const {
	return calculateBaseValue(effect);
}

} // namespace aion::gameserver::skillengine::effect
