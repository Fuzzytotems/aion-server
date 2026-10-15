#pragma once

#include <atomic>
#include <cstdint>

#include "aion/gameserver/configs/detail/ConfigSupport.h"
#include "aion/gameserver/services/cron/CronExpression.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.ShutdownConfig
 *
 * @author lord_rex
 */
struct ShutdownConfig {
	/** Shutdown Hook delay in seconds. */
	static inline std::atomic<int32_t> DELAY{0};

	/** Shutdown restart schedule. */
	static inline std::atomic<const services::cron::CronExpression*> RESTART_SCHEDULE{nullptr};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
