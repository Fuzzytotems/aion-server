#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"

#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::gameobjects::player {

Cooldowns::Cooldowns() : runtime::ConcurrentHashMap<int32_t, int64_t>(AION_LOCK_CLASS(Cooldowns#stripe)) {
}

Cooldowns::~Cooldowns() = default;

runtime::Ref<Cooldowns> Cooldowns::create() {
	return runtime::makeRef<Cooldowns>();
}

std::optional<int64_t> Cooldowns::get(int32_t cooldownId) {
	std::optional<int64_t> cd = runtime::ConcurrentHashMap<int32_t, int64_t>::get(cooldownId);
	if (cd && *cd <= commons::utils::currentTimeMillis()) {
		if (remove(cooldownId, *cd))
			return std::nullopt;
		cd = get(cooldownId); // get again, because the retrieved cd isn't up to date
	}
	return cd;
}

std::optional<int64_t> Cooldowns::put(int32_t cooldownId, int64_t reuseTimeMillis) {
	// Java: a null reuse time throws IllegalArgumentException("No cooldown given for ID " + cooldownId); a C++ int64_t is never null
	return reuseTimeMillis <= commons::utils::currentTimeMillis() ? remove(cooldownId)
																	: runtime::ConcurrentHashMap<int32_t, int64_t>::put(cooldownId, reuseTimeMillis);
}

bool Cooldowns::hasCooldown(int32_t cooldownId) {
	// Java: containsKey(cooldownId), which ConcurrentHashMap implements as get(key) != null, so the overridden get removes expired cooldowns
	return get(cooldownId).has_value();
}

int32_t Cooldowns::remainingSeconds(int32_t cooldownId) {
	std::optional<int64_t> cd = get(cooldownId);
	return !cd ? 0 : static_cast<int32_t>((*cd - commons::utils::currentTimeMillis()) / 1000);
}

} // namespace aion::gameserver::model::gameobjects::player
