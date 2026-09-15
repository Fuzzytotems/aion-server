#include "aion/gameserver/skillengine/effect/ShieldEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/ShieldType.h"

namespace aion::gameserver::skillengine::effect {

void ShieldEffect::applyEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void ShieldEffect::startEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

model::ShieldType ShieldEffect::getType() const {
	return model::ShieldType::NORMAL;
}

} // namespace aion::gameserver::skillengine::effect
