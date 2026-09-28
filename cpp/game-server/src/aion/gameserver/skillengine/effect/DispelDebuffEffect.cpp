#include "aion/gameserver/skillengine/effect/DispelDebuffEffect.h"

#include "aion/gameserver/skillengine/model/DispelCategoryType.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"

namespace aion::gameserver::skillengine::effect {

void DispelDebuffEffect::applyEffect(model::Effect& effect) const {
	AbstractDispelEffect::applyEffect(effect, model::DispelCategoryType::ALL, model::SkillTargetSlot::DEBUFF);
}

} // namespace aion::gameserver::skillengine::effect
