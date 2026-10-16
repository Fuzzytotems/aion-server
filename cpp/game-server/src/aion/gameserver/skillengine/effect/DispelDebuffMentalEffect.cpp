#include "aion/gameserver/skillengine/effect/DispelDebuffMentalEffect.h"

#include "aion/gameserver/skillengine/model/DispelCategoryType.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"

namespace aion::gameserver::skillengine::effect {

void DispelDebuffMentalEffect::applyEffect(model::Effect& effect) const {
	AbstractDispelEffect::applyEffect(effect, model::DispelCategoryType::DEBUFF_MENTAL, model::SkillTargetSlot::DEBUFF);
}

} // namespace aion::gameserver::skillengine::effect
