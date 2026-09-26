#include "aion/gameserver/skillengine/effect/NoFlyEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void NoFlyEffect::calculate(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void NoFlyEffect::applyEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

bool NoFlyEffect::isDodgedOrResisted(model::Effect& /*effect*/, std::optional<gameserver::model::stats::container::StatEnum> /*statEnum*/) const {
	AION_UNPORTED();
}

void NoFlyEffect::startEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void NoFlyEffect::endEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
