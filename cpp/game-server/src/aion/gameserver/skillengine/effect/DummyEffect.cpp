#include "aion/gameserver/skillengine/effect/DummyEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void DummyEffect::applyEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
