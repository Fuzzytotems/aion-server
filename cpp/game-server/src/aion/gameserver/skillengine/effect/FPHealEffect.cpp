#include "aion/gameserver/skillengine/effect/FPHealEffect.h"

#include <cstdint>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/HealType.h"

namespace aion::gameserver::skillengine::effect {

void FPHealEffect::startEffect(model::Effect& effect) const {
	HealOverTimeEffect::startEffect(effect, model::HealType::FP);
}

void FPHealEffect::onPeriodicAction(model::Effect& effect) const {
	HealOverTimeEffect::onPeriodicAction(effect, model::HealType::FP);
}

int32_t FPHealEffect::getCurrentStatValue(model::Effect& effect) const {
	return effect.getEffected()->getLifeStats()->getCurrentFp();
}

int32_t FPHealEffect::getMaxStatValue(model::Effect& effect) const {
	return effect.getEffected()->getLifeStats()->getMaxFp();
}

} // namespace aion::gameserver::skillengine::effect
