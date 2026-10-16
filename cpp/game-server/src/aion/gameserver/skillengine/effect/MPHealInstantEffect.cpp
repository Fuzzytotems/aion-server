#include "aion/gameserver/skillengine/effect/MPHealInstantEffect.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/HealType.h"

namespace aion::gameserver::skillengine::effect {

void MPHealInstantEffect::calculate(model::Effect& effect) const {
	AbstractHealEffect::calculate(effect, model::HealType::MP);
}

void MPHealInstantEffect::applyEffect(model::Effect& effect) const {
	// AbstractHealEffect.applyEffect's MP arm: not a ProcMPHealInstantEffect, so a skill heal, sent as TYPE.HEAL_MP
	AbstractHealEffect::applyEffect(effect, model::HealType::MP);
}

int32_t MPHealInstantEffect::getCurrentStatValue(model::Effect& effect) const {
	return effect.getEffected()->getLifeStats()->getCurrentMp();
}

int32_t MPHealInstantEffect::getMaxStatValue(model::Effect& effect) const {
	return effect.getEffected()->getGameStats()->getMaxMp()->getCurrent();
}

} // namespace aion::gameserver::skillengine::effect
