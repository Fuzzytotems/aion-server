#include "aion/gameserver/skillengine/effect/BuffBindEffect.h"

#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void BuffBindEffect::calculate(model::Effect& effect) const {
	effect.addSuccessEffect(this);
}

} // namespace aion::gameserver::skillengine::effect
