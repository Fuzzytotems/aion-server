#include "aion/gameserver/model/gameobjects/player/PortalCooldown.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::gameobjects::player {

PortalCooldown::PortalCooldown(int32_t worldIdValue, int64_t reuseTimeValue, int32_t enterCountValue)
	: worldId(worldIdValue), reuseTime(reuseTimeValue), enterCount(enterCountValue) {
}

PortalCooldown::~PortalCooldown() = default;

runtime::Ref<PortalCooldown> PortalCooldown::create(int32_t worldIdValue, int64_t reuseTimeValue, int32_t enterCountValue) {
	return runtime::makeRef<PortalCooldown>(worldIdValue, reuseTimeValue, enterCountValue);
}

void PortalCooldown::increaseEnterCount() {
	AION_UNPORTED();
}

void PortalCooldown::decreaseEnterCount(int32_t countValue) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
