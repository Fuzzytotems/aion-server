#include "aion/gameserver/skillengine/effect/NoReduceSpellATKInstantEffect.h"

namespace aion::gameserver::skillengine::effect {

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
