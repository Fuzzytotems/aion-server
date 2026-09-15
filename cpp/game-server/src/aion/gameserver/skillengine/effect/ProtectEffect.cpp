#include "aion/gameserver/skillengine/effect/ProtectEffect.h"

#include "aion/gameserver/skillengine/model/ShieldType.h"

namespace aion::gameserver::skillengine::effect {

model::ShieldType ProtectEffect::getType() const {
	return model::ShieldType::PROTECT;
}

} // namespace aion::gameserver::skillengine::effect
