#include "aion/gameserver/skillengine/effect/HealEffectTemplate.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

int32_t HealEffectTemplate::calculateSnapshotHealValue(model::Effect& /*effect*/, model::HealType /*type*/) const {
	AION_UNPORTED();
}

int32_t HealEffectTemplate::applyHealDeboost(model::Effect& /*effect*/, int32_t /*healValue*/) const {
	AION_UNPORTED();
}

int32_t HealEffectTemplate::calculateHealValue(model::Effect& /*effect*/, model::HealType /*type*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
