#pragma once

#include <atomic>
#include <cstdint>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.ThreadConfig
 *
 * @author lord_rex, Neon
 */
struct ThreadConfig {
	static inline std::atomic<int32_t> BASE_THREAD_POOL_SIZE{0};

	static inline std::atomic<int32_t> SCHEDULED_THREAD_POOL_SIZE{0};

	static inline std::atomic<int64_t> MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING{0};

	/** For instant pool set priority to 7, in Linux you must be root and use extra switches */
	static inline std::atomic<bool> USE_PRIORITIES{false};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
