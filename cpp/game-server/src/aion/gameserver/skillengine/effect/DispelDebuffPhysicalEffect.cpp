#include "aion/gameserver/skillengine/effect/DispelDebuffPhysicalEffect.h"

#include "aion/gameserver/skillengine/model/DispelCategoryType.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"

namespace aion::gameserver::skillengine::effect {

void DispelDebuffPhysicalEffect::applyEffect(model::Effect& effect) const {
	AbstractDispelEffect::applyEffect(effect, model::DispelCategoryType::DEBUFF_PHYSICAL, model::SkillTargetSlot::DEBUFF);
}

} // namespace aion::gameserver::skillengine::effect
