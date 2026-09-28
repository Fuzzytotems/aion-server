#pragma once

#include <atomic>
#include <cstdint>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.FallDamageConfig
 */
struct FallDamageConfig {
	/** Percentage of damage per meter. */
	static inline std::atomic<float> FALL_DAMAGE_PERCENTAGE{0.0f};

	/** Minimum fall damage range */
	static inline std::atomic<int32_t> MINIMUM_DISTANCE_DAMAGE{0};

	/** Maximum fall distance after which you will die after hitting the ground. */
	static inline std::atomic<int32_t> MAXIMUM_DISTANCE_DAMAGE{0};

	/** Maximum fall distance after which you will die in mid air. */
	static inline std::atomic<int32_t> MAXIMUM_DISTANCE_MIDAIR{0};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
