#include "aion/gameserver/skillengine/effect/ConvertHealEffect.h"

#include "aion/gameserver/skillengine/model/ShieldType.h"

namespace aion::gameserver::skillengine::effect {

model::ShieldType ConvertHealEffect::getType() const {
	return model::ShieldType::CONVERT;
}

} // namespace aion::gameserver::skillengine::effect
