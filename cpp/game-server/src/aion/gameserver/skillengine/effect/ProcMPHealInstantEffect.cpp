#include "aion/gameserver/skillengine/effect/ProcMPHealInstantEffect.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/HealType.h"

namespace aion::gameserver::skillengine::effect {

void ProcMPHealInstantEffect::calculate(model::Effect& effect) const {
	AbstractHealEffect::calculate(effect, model::HealType::MP);
}

void ProcMPHealInstantEffect::applyEffect(model::Effect& effect) const {
	// AbstractHealEffect.applyEffect's `this instanceof ProcMPHealInstantEffect` arm: an item heal, sent as TYPE.MP (not HEAL_MP)
	AbstractHealEffect::applyEffect(effect, model::HealType::MP);
}

int32_t ProcMPHealInstantEffect::getCurrentStatValue(model::Effect& effect) const {
	return effect.getEffected()->getLifeStats()->getCurrentMp();
}

int32_t ProcMPHealInstantEffect::getMaxStatValue(model::Effect& effect) const {
	return effect.getEffected()->getGameStats()->getMaxMp()->getCurrent();
}

} // namespace aion::gameserver::skillengine::effect
