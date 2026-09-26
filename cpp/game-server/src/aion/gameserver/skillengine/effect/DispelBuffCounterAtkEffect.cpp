#include "aion/gameserver/skillengine/effect/DispelBuffCounterAtkEffect.h"

#include <cstdint>

#include "aion/gameserver/controllers/attack/AttackUtil.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

namespace {

/** Java int a + b (wraps on overflow) */
constexpr int32_t addInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

/** Java int a * b (wraps on overflow) */
constexpr int32_t mulInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

} // namespace

void DispelBuffCounterAtkEffect::resolveMagicalCritical(model::Effect& /*effect*/) const {
	// this effect type deals its damage without the magical attack calculation, so it never crits and never decides the critical of other effects
}

void DispelBuffCounterAtkEffect::applyEffect(model::Effect& effect) const {
	DamageEffect::applyEffect(effect);
	effect.getEffected()->getEffectController()->dispelBuffCounterAtkEffect(effect);
}

void DispelBuffCounterAtkEffect::calculateDamage(model::Effect& effect) const {
	runtime::Ptr<gameserver::model::gameobjects::Creature> effected = effect.getEffected();
	int32_t count = calculateBaseValue(effect);
	int32_t finalPower = addInt(power, mulInt(dpower, effect.getSkillLevel()));

	int32_t dispelledEffectCount = effected->getEffectController()->calculateBuffsOrEffectorDebuffsToRemove(effect, count, dispelLevel, finalPower);
	// Java int arithmetic: hitvalue + ((hitvalue / 2) * (dispelledEffectCount - 1)) + hitdelta * skillLevel wraps; the division truncates toward 0
	int32_t valueWithDelta = dispelledEffectCount > 0 ? addInt(addInt(hitvalue, mulInt(hitvalue / 2, addInt(dispelledEffectCount, -1))),
															mulInt(hitdelta, effect.getSkillLevel()))
													  : 0;
	controllers::attack::AttackUtil::calculateSkillResult(effect, valueWithDelta, this, false);
}

bool DispelBuffCounterAtkEffect::shouldApplyAttackerMovementModifier() const {
	return false;
}

void DispelBuffCounterAtkEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->resetDesignatedDispelEffect(effect);
	DamageEffect::endEffect(effect);
}

bool DispelBuffCounterAtkEffect::shouldUseKnowledge() const {
	return false;
}

bool DispelBuffCounterAtkEffect::shouldUseBoostSpellAttackEffects() const {
	return false;
}

bool DispelBuffCounterAtkEffect::shouldUseOneTimeBoostSkillAttack() const {
	return false;
}

} // namespace aion::gameserver::skillengine::effect
