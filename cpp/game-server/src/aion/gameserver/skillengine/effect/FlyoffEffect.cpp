#include "aion/gameserver/skillengine/effect/FlyoffEffect.h"

namespace aion::gameserver::skillengine::effect {

void FlyoffEffect::applyEffect(model::Effect& /*effect*/) const {
	// TODO Distance is Z, value probably contains angle or width (Java FlyoffEffect.java: an empty body, the effect does nothing yet)
}

} // namespace aion::gameserver::skillengine::effect
