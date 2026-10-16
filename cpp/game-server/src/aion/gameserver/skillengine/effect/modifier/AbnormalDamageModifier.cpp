#include "aion/gameserver/skillengine/effect/modifier/AbnormalDamageModifier.h"

#include <cstdint>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect::modifier {

int32_t AbnormalDamageModifier::analyze(model::Effect& effect) const {
	// Java: (value + effect.getSkillLevel() * delta) - int arithmetic, wrapping on overflow
	return static_cast<int32_t>(static_cast<uint32_t>(value) + static_cast<uint32_t>(effect.getSkillLevel()) * static_cast<uint32_t>(delta));
}

bool AbnormalDamageModifier::check(model::Effect& effect) const {
	return effect.getEffected()->getEffectController()->isAbnormalSet(state);
}

} // namespace aion::gameserver::skillengine::effect::modifier
