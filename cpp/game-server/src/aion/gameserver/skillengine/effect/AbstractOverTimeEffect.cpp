#include "aion/gameserver/skillengine/effect/AbstractOverTimeEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void AbstractOverTimeEffect::applyEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void AbstractOverTimeEffect::startEffect(model::Effect& effect) const {
	startEffect(effect, std::nullopt);
}

void AbstractOverTimeEffect::startEffect(model::Effect& /*effect*/, std::optional<AbnormalState> /*abnormal*/) const {
	AION_UNPORTED();
}

void AbstractOverTimeEffect::endEffect(model::Effect& /*effect*/, std::optional<AbnormalState> /*abnormal*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
