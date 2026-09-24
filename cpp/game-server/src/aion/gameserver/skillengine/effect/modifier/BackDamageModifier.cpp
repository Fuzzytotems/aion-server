#include "aion/gameserver/skillengine/effect/modifier/BackDamageModifier.h"

#include <cstdint>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::skillengine::effect::modifier {

int32_t BackDamageModifier::analyze(model::Effect& effect) const {
	// Java: value + effect.getSkillLevel() * delta - int arithmetic, wrapping on overflow
	return static_cast<int32_t>(static_cast<uint32_t>(value) + static_cast<uint32_t>(effect.getSkillLevel()) * static_cast<uint32_t>(delta));
}

bool BackDamageModifier::check(model::Effect& effect) const {
	return utils::PositionUtil::isBehind(*effect.getEffector(), *effect.getEffected());
}

} // namespace aion::gameserver::skillengine::effect::modifier
