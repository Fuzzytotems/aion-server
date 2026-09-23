#include "aion/gameserver/skillengine/effect/AbstractDispelEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void AbstractDispelEffect::applyEffect(model::Effect& /*effect*/, model::DispelCategoryType /*type*/, model::SkillTargetSlot /*slot*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
