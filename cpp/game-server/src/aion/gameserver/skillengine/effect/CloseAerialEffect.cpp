#include "aion/gameserver/skillengine/effect/CloseAerialEffect.h"

#include <optional>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SpellStatus.h"

namespace aion::gameserver::skillengine::effect {

void CloseAerialEffect::applyEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->removeEffect(8224);
}

void CloseAerialEffect::calculate(model::Effect& effect) const {
	EffectTemplate::calculate(effect, std::nullopt, model::SpellStatus::CLOSEAERIAL);
}

} // namespace aion::gameserver::skillengine::effect
