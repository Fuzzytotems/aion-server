#include "aion/gameserver/skillengine/effect/ConvertHealEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/ShieldType.h"

namespace aion::gameserver::skillengine::effect {

void ConvertHealEffect::startEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

void ConvertHealEffect::endEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

model::ShieldType ConvertHealEffect::getType() const {
	return model::ShieldType::CONVERT;
}

} // namespace aion::gameserver::skillengine::effect
