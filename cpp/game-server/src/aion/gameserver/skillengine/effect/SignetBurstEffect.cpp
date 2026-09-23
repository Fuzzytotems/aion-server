#include "aion/gameserver/skillengine/effect/SignetBurstEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void SignetBurstEffect::calculateDamage(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void SignetBurstEffect::calculate(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

bool SignetBurstEffect::shouldUseBoostSpellAttackEffects() const {
	return false;
}

bool SignetBurstEffect::shouldUseOneTimeBoostSkillAttack() const {
	return false;
}

} // namespace aion::gameserver::skillengine::effect
