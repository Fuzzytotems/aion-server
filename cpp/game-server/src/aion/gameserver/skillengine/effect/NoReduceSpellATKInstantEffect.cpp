#include "aion/gameserver/skillengine/effect/NoReduceSpellATKInstantEffect.h"

#include <cstdint>

#include "aion/gameserver/controllers/attack/AttackUtil.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void NoReduceSpellATKInstantEffect::resolveMagicalCritical(model::Effect& effect) const {
	effect.reuseMagicalCritical(position); // shows an already rolled critical, but never multiplies the damage
}

void NoReduceSpellATKInstantEffect::calculateDamage(model::Effect& effect) const {
	int32_t valueWithDelta = calculateBaseValue(effect);
	if (percent) {
		float percentToCount = static_cast<float>(valueWithDelta) / 100.0f;
		valueWithDelta = gameserver::model::templates::detail::floatToInt(static_cast<float>(effect.getEffected()->getLifeStats()->getMaxHp()) * percentToCount);
	}
	if (max_damage > 0)
		valueWithDelta = valueWithDelta > max_damage ? max_damage : valueWithDelta;
	controllers::attack::AttackUtil::calculateSkillResult(effect, valueWithDelta, this, false);
}

bool NoReduceSpellATKInstantEffect::shouldApplyAttackerMovementModifier() const {
	return false;
}

bool NoReduceSpellATKInstantEffect::shouldApplyMagicalSkillBoostBonus(model::Effect& /*effect*/) const {
	return false;
}

bool NoReduceSpellATKInstantEffect::shouldUseKnowledge() const {
	return false;
}

bool NoReduceSpellATKInstantEffect::shouldUseBoostSpellAttackEffects() const {
	return false;
}

bool NoReduceSpellATKInstantEffect::shouldUseOneTimeBoostSkillAttack() const {
	return false;
}

} // namespace aion::gameserver::skillengine::effect
