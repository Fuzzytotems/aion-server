#include "aion/gameserver/skillengine/effect/XPBoostEffect.h"

#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void XPBoostEffect::calculate(model::Effect& effect) const {
	// Java: no super.calculate - neither the conditions, the pre-effects nor a resist roll are asked (EffectTemplate.calculate's comment names
	// xpboosteffect among the exceptions that only add their success)
	effect.addSuccessEffect(this);
}

} // namespace aion::gameserver::skillengine::effect
