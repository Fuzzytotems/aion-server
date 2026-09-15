#include "aion/gameserver/skillengine/effect/DispelBuffCounterAtkEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void DispelBuffCounterAtkEffect::applyEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

bool DispelBuffCounterAtkEffect::shouldApplyAttackerMovementModifier() const {
	return false;
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
