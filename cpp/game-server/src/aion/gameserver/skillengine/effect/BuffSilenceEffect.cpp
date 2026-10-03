#include "aion/gameserver/skillengine/effect/BuffSilenceEffect.h"

#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void BuffSilenceEffect::calculate(model::Effect& effect) const {
	effect.addSuccessEffect(this);
}

} // namespace aion::gameserver::skillengine::effect
