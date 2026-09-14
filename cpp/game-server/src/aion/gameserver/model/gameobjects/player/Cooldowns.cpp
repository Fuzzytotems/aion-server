#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::gameobjects::player {

Cooldowns::Cooldowns() : runtime::ConcurrentHashMap<int32_t, int64_t>(AION_LOCK_CLASS(Cooldowns#stripe)) {
}

Cooldowns::~Cooldowns() = default;

runtime::Ref<Cooldowns> Cooldowns::create() {
	return runtime::makeRef<Cooldowns>();
}

std::optional<int64_t> Cooldowns::get(int32_t cooldownId) {
	AION_UNPORTED();
}

std::optional<int64_t> Cooldowns::put(int32_t cooldownId, int64_t reuseTimeMillis) {
	AION_UNPORTED();
}

bool Cooldowns::hasCooldown(int32_t cooldownId) {
	AION_UNPORTED();
}

int32_t Cooldowns::remainingSeconds(int32_t cooldownId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
