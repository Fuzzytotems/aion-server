#include "aion/gameserver/skillengine/effect/BuffStunEffect.h"

#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void BuffStunEffect::calculate(model::Effect& effect) const {
	effect.addSuccessEffect(this);
}

} // namespace aion::gameserver::skillengine::effect
