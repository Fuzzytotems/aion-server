#pragma once

#include <atomic>
#include <cstdint>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.PeriodicSaveConfig
 *
 * @author ATracer
 */
struct PeriodicSaveConfig {
	/** Time in seconds for saving player data */
	static inline std::atomic<int32_t> PLAYER_GENERAL{0};

	/** Time in seconds for saving player items and item stones */
	static inline std::atomic<int32_t> PLAYER_ITEMS{0};

	/** Time in seconds for saving legion wh items and item stones */
	static inline std::atomic<int32_t> LEGION_ITEMS{0};

	/** Time in seconds for updating and saving pet mood data */
	static inline std::atomic<int32_t> PLAYER_PETS{0};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
