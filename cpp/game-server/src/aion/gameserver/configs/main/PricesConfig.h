#pragma once

#include <atomic>
#include <cstdint>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.PricesConfig
 *
 * @author Sarynth
 */
struct PricesConfig {
	/** Controls the "Prices:" value in influence tab. */
	static inline std::atomic<int32_t> DEFAULT_PRICES{0};

	/** Hidden modifier for all prices. */
	static inline std::atomic<int32_t> DEFAULT_MODIFIER{0};

	/** Taxes: value = 100 + tax % */
	static inline std::atomic<int32_t> DEFAULT_TAXES{0};

	static inline std::atomic<int32_t> VENDOR_BUY_MODIFIER{0};

	static inline std::atomic<int32_t> VENDOR_SELL_MODIFIER{0};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
