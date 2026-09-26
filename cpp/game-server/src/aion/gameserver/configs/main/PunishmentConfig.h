#pragma once

#include <atomic>
#include <cstdint>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.PunishmentConfig
 *
 * @author synchro2
 */
struct PunishmentConfig {
	static inline std::atomic<bool> PUNISHMENT_ENABLE{false};

	static inline std::atomic<int32_t> PUNISHMENT_TYPE{0};

	static inline std::atomic<int32_t> PUNISHMENT_TIME{0};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
