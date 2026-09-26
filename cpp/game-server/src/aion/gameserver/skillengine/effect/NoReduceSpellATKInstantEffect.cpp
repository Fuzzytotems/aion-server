#include "aion/gameserver/skillengine/effect/NoReduceSpellATKInstantEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void NoReduceSpellATKInstantEffect::resolveMagicalCritical(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void NoReduceSpellATKInstantEffect::calculateDamage(model::Effect& /*effect*/) const {
	AION_UNPORTED();
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
