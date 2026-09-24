#include "aion/gameserver/skillengine/effect/HealInstantEffect.h"

#include <memory>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/HealType.h"

namespace aion::gameserver::skillengine::effect {

void HealInstantEffect::applyEffect(model::Effect& effect) const {
	AbstractHealEffect::applyEffect(effect, model::HealType::HP);
}

void HealInstantEffect::calculate(model::Effect& effect) const {
	AbstractHealEffect::calculate(effect, model::HealType::HP);
}

int32_t HealInstantEffect::getCurrentStatValue(model::Effect& effect) const {
	return effect.getEffected()->getLifeStats()->getCurrentHp();
}

int32_t HealInstantEffect::getMaxStatValue(model::Effect& effect) const {
	return effect.getEffected()->getGameStats()->getMaxHp()->getCurrent();
}

} // namespace aion::gameserver::skillengine::effect
