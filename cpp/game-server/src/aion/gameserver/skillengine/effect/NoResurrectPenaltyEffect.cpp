#include "aion/gameserver/skillengine/effect/NoResurrectPenaltyEffect.h"

#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void NoResurrectPenaltyEffect::calculate(model::Effect& effect) const {
	// Java: no super.calculate - neither the conditions, the pre-effects nor a resist roll are asked
	effect.addSuccessEffect(this);
}

} // namespace aion::gameserver::skillengine::effect
