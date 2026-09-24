#include "aion/gameserver/skillengine/effect/ProcHealInstantEffect.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/HealType.h"

namespace aion::gameserver::skillengine::effect {

void ProcHealInstantEffect::calculate(model::Effect& effect) const {
	AbstractHealEffect::calculate(effect, model::HealType::HP);
}

void ProcHealInstantEffect::applyEffect(model::Effect& effect) const {
	// AbstractHealEffect.applyEffect's `this instanceof ProcHealInstantEffect` arm: an item heal, sent as TYPE.HP (not REGULAR)
	AbstractHealEffect::applyEffect(effect, model::HealType::HP);
}

int32_t ProcHealInstantEffect::getCurrentStatValue(model::Effect& effect) const {
	return effect.getEffected()->getLifeStats()->getCurrentHp();
}

int32_t ProcHealInstantEffect::getMaxStatValue(model::Effect& effect) const {
	return effect.getEffected()->getGameStats()->getMaxHp()->getCurrent();
}

bool ProcHealInstantEffect::allowHpHealBoost(model::Effect& /*effect*/) const {
	return false;
}

} // namespace aion::gameserver::skillengine::effect
