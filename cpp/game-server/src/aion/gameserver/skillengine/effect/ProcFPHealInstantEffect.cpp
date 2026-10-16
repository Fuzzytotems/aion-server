#include "aion/gameserver/skillengine/effect/ProcFPHealInstantEffect.h"

#include <cstdint>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/HealType.h"

namespace aion::gameserver::skillengine::effect {

void ProcFPHealInstantEffect::calculate(model::Effect& effect) const {
	AbstractHealEffect::calculate(effect, model::HealType::FP);
}

void ProcFPHealInstantEffect::applyEffect(model::Effect& effect) const {
	// AbstractHealEffect.applyEffect's FP arm: the FP heal of a proc (an item's or a skill's proc), sent as TYPE.FP_RINGS
	AbstractHealEffect::applyEffect(effect, model::HealType::FP);
}

int32_t ProcFPHealInstantEffect::getCurrentStatValue(model::Effect& effect) const {
	return effect.getEffected()->getLifeStats()->getCurrentFp();
}

int32_t ProcFPHealInstantEffect::getMaxStatValue(model::Effect& effect) const {
	return effect.getEffected()->getLifeStats()->getMaxFp();
}

} // namespace aion::gameserver::skillengine::effect
