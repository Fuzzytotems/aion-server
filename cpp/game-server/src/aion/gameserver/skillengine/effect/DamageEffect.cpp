#include "aion/gameserver/skillengine/effect/DamageEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::skillengine::effect {

void DamageEffect::applyEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void DamageEffect::onAttack(model::Effect& /*effect*/, network::aion::serverpackets::SM_ATTACK_STATUS_TYPE /*type*/,
	network::aion::serverpackets::SM_ATTACK_STATUS_LOG /*log*/) const {
	AION_UNPORTED();
}

void DamageEffect::resolveMagicalCritical(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void DamageEffect::calculateDamage(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

bool DamageEffect::shouldApplyAttackerMovementModifier() const {
	return true;
}

bool DamageEffect::shouldApplyMagicalSkillBoostBonus(model::Effect& effect) const {
	return effect.getSkillTemplate()->isApplyMagicalSkillBoostBonus();
}

bool DamageEffect::shouldUseKnowledge() const {
	return true;
}

bool DamageEffect::shouldUseBoostSpellAttackEffects() const {
	return true;
}

bool DamageEffect::shouldUseOneTimeBoostSkillAttack() const {
	return true;
}

} // namespace aion::gameserver::skillengine::effect
