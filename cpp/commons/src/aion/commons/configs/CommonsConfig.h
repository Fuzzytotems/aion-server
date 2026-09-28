#pragma once

#include <atomic>

namespace aion::commons::configuration {
class ConfigurableProcessor;
}

namespace aion::commons::configs {

struct CommonsConfig {
	/**
	 * commons.runnablestats.enable (default false)
	 * <p>
	 * std::atomic because the game server rebinds all configs at runtime (Config.load() on event start/stop and //reload), while task and packet
	 * threads read the flag (Java: a plain boolean, which is memory safe to rewrite there).
	 */
	static inline std::atomic<bool> RUNNABLESTATS_ENABLE{false};

	// Java: commons.script_compiler.caching.enable - not ported, handlers are compiled into the binary

	/** Binds the fields above to their property keys (Java: the @Property annotations). Implemented in the configuration library. */
	static void bind(configuration::ConfigurableProcessor& processor);
};

} // namespace aion::commons::configs
