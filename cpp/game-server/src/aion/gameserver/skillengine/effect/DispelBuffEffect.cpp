#include "aion/gameserver/skillengine/effect/DispelBuffEffect.h"

#include "aion/gameserver/skillengine/model/DispelCategoryType.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"

namespace aion::gameserver::skillengine::effect {

void DispelBuffEffect::applyEffect(model::Effect& effect) const {
	AbstractDispelEffect::applyEffect(effect, model::DispelCategoryType::BUFF, model::SkillTargetSlot::BUFF);
}

} // namespace aion::gameserver::skillengine::effect
