#include "aion/gameserver/skillengine/effect/FallEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

bool FallEffect::isDodgedOrResisted(model::Effect& /*effect*/, std::optional<gameserver::model::stats::container::StatEnum> /*statEnum*/) const {
	AION_UNPORTED();
}

void FallEffect::applyEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
