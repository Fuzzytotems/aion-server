#include "aion/gameserver/model/gameobjects/player/PortalCooldown.h"

namespace aion::gameserver::model::gameobjects::player {

PortalCooldown::PortalCooldown(int32_t worldIdValue, int64_t reuseTimeValue, int32_t enterCountValue)
	: worldId(worldIdValue), reuseTime(reuseTimeValue), enterCount(enterCountValue) {
}

PortalCooldown::~PortalCooldown() = default;

runtime::Ref<PortalCooldown> PortalCooldown::create(int32_t worldIdValue, int64_t reuseTimeValue, int32_t enterCountValue) {
	return runtime::makeRef<PortalCooldown>(worldIdValue, reuseTimeValue, enterCountValue);
}

void PortalCooldown::increaseEnterCount() {
	enterCount++; // java-race: unsynchronized increment (lost updates as in Java)
}

void PortalCooldown::decreaseEnterCount(int32_t countValue) {
	enterCount -= countValue;
}

} // namespace aion::gameserver::model::gameobjects::player
