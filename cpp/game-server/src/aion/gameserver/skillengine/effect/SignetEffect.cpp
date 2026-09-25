#include "aion/gameserver/skillengine/effect/SignetEffect.h"

#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

void SignetEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void SignetEffect::calculate(model::Effect& effect) const {
	effect.addSuccessEffect(this);
}

} // namespace aion::gameserver::skillengine::effect
