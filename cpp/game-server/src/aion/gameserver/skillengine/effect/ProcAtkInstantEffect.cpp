#include "aion/gameserver/skillengine/effect/ProcAtkInstantEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void ProcAtkInstantEffect::applyEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

bool ProcAtkInstantEffect::shouldApplyAttackerMovementModifier() const {
	return false;
}

bool ProcAtkInstantEffect::shouldUseBoostSpellAttackEffects() const {
	return false;
}

bool ProcAtkInstantEffect::shouldUseOneTimeBoostSkillAttack() const {
	return false;
}

} // namespace aion::gameserver::skillengine::effect
