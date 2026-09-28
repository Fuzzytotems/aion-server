#include "aion/gameserver/skillengine/effect/AbstractDispelEffect.h"

#include <cstdint>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void AbstractDispelEffect::applyEffect(model::Effect& effect, model::DispelCategoryType type, model::SkillTargetSlot slot) const {
	int32_t count = calculateBaseValue(effect);
	// Java int arithmetic: power + dpower * skillLevel wraps
	int32_t finalPower =
		static_cast<int32_t>(static_cast<uint32_t>(power) + static_cast<uint32_t>(dpower) * static_cast<uint32_t>(effect.getSkillLevel()));

	effect.getEffected()->getEffectController()->removeEffectByDispelCat(type, slot, count, dispelLevel, finalPower);
}

} // namespace aion::gameserver::skillengine::effect
