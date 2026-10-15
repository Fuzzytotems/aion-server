#include "aion/gameserver/skillengine/effect/FPHealInstantEffect.h"

#include <cstdint>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/HealType.h"

namespace aion::gameserver::skillengine::effect {

void FPHealInstantEffect::calculate(model::Effect& effect) const {
	AbstractHealEffect::calculate(effect, model::HealType::FP);
}

void FPHealInstantEffect::applyEffect(model::Effect& effect) const {
	AbstractHealEffect::applyEffect(effect, model::HealType::FP);
}

int32_t FPHealInstantEffect::getCurrentStatValue(model::Effect& effect) const {
	return effect.getEffected()->getLifeStats()->getCurrentFp();
}

int32_t FPHealInstantEffect::getMaxStatValue(model::Effect& effect) const {
	return effect.getEffected()->getLifeStats()->getMaxFp();
}

} // namespace aion::gameserver::skillengine::effect
