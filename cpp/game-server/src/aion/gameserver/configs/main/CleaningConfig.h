#pragma once

#include <atomic>
#include <cstdint>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.CleaningConfig
 *
 * @author nrg
 */
struct CleaningConfig {
	/** Enable Database Cleaning */
	static inline std::atomic<bool> CLEANING_ENABLE{false};

	/**
	 * Minimum account inactivity in days, after which chars get deleted<br>
	 * Cleaning will only be executed with a value greater than 30
	 */
	static inline std::atomic<int32_t> MIN_ACCOUNT_INACTIVITY_DAYS{0};

	/** Maximum level of characters that will be deleted on each account */
	static inline std::atomic<int32_t> MAX_DELETABLE_CHAR_LEVEL{0};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
