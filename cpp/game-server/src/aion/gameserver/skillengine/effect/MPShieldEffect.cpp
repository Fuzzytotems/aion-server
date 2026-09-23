#include "aion/gameserver/skillengine/effect/MPShieldEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/ShieldType.h"

namespace aion::gameserver::skillengine::effect {

void MPShieldEffect::startEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void MPShieldEffect::endEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

model::ShieldType MPShieldEffect::getType() const {
	return model::ShieldType::MPSHIELD;
}

} // namespace aion::gameserver::skillengine::effect
