#include "aion/gameserver/skillengine/effect/HealEffect.h"

#include <memory>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/HealType.h"

namespace aion::gameserver::skillengine::effect {

void HealEffect::startEffect(model::Effect& effect) const {
	HealOverTimeEffect::startEffect(effect, model::HealType::HP);
}

void HealEffect::onPeriodicAction(model::Effect& effect) const {
	HealOverTimeEffect::onPeriodicAction(effect, model::HealType::HP);
}

int32_t HealEffect::getCurrentStatValue(model::Effect& effect) const {
	return effect.getEffected()->getLifeStats()->getCurrentHp();
}

int32_t HealEffect::getMaxStatValue(model::Effect& effect) const {
	return effect.getEffected()->getGameStats()->getMaxHp()->getCurrent();
}

} // namespace aion::gameserver::skillengine::effect
