#include "aion/gameserver/skillengine/effect/DispelNpcBuffEffect.h"

#include "aion/gameserver/skillengine/model/DispelCategoryType.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"

namespace aion::gameserver::skillengine::effect {

void DispelNpcBuffEffect::applyEffect(model::Effect& effect) const {
	AbstractDispelEffect::applyEffect(effect, model::DispelCategoryType::NPC_BUFF, model::SkillTargetSlot::BUFF);
}

} // namespace aion::gameserver::skillengine::effect
