#include "aion/gameserver/skillengine/effect/ProtectEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/ShieldType.h"

namespace aion::gameserver::skillengine::effect {

void ProtectEffect::startEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void ProtectEffect::endEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

model::ShieldType ProtectEffect::getType() const {
	return model::ShieldType::PROTECT;
}

} // namespace aion::gameserver::skillengine::effect
