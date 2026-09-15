#include "aion/gameserver/skillengine/effect/SignetBurstEffect.h"

namespace aion::gameserver::skillengine::effect {

bool SignetBurstEffect::shouldUseBoostSpellAttackEffects() const {
	return false;
}

bool SignetBurstEffect::shouldUseOneTimeBoostSkillAttack() const {
	return false;
}

} // namespace aion::gameserver::skillengine::effect
