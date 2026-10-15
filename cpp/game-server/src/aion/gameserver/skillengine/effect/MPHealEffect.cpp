#include "aion/gameserver/skillengine/effect/MPHealEffect.h"

#include <cstdint>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/HealType.h"

namespace aion::gameserver::skillengine::effect {

void MPHealEffect::startEffect(model::Effect& effect) const {
	HealOverTimeEffect::startEffect(effect, model::HealType::MP);
}

void MPHealEffect::onPeriodicAction(model::Effect& effect) const {
	HealOverTimeEffect::onPeriodicAction(effect, model::HealType::MP);
}

int32_t MPHealEffect::getCurrentStatValue(model::Effect& effect) const {
	return effect.getEffected()->getLifeStats()->getCurrentMp();
}

int32_t MPHealEffect::getMaxStatValue(model::Effect& effect) const {
	return effect.getEffected()->getGameStats()->getMaxMp()->getCurrent();
}

} // namespace aion::gameserver::skillengine::effect
