#include "aion/gameserver/skillengine/effect/DispelNpcDebuffEffect.h"

#include "aion/gameserver/skillengine/model/DispelCategoryType.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"

namespace aion::gameserver::skillengine::effect {

void DispelNpcDebuffEffect::applyEffect(model::Effect& effect) const {
	AbstractDispelEffect::applyEffect(effect, model::DispelCategoryType::NPC_DEBUFF_PHYSICAL, model::SkillTargetSlot::DEBUFF);
}

} // namespace aion::gameserver::skillengine::effect
