#include "aion/gameserver/skillengine/effect/MPShieldEffect.h"

#include "aion/gameserver/skillengine/model/ShieldType.h"

namespace aion::gameserver::skillengine::effect {

model::ShieldType MPShieldEffect::getType() const {
	return model::ShieldType::MPSHIELD;
}

} // namespace aion::gameserver::skillengine::effect
