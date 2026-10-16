#include "aion/gameserver/skillengine/effect/RebirthEffect.h"

#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void RebirthEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

} // namespace aion::gameserver::skillengine::effect
