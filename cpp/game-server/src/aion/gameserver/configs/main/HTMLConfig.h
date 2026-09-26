#pragma once

#include <atomic>
#include <string>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.HTMLConfig
 *
 * @author lord_rex
 */
struct HTMLConfig {
	/** Enable HTML Welcome Message */
	static inline std::atomic<bool> ENABLE_HTML_WELCOME{false};

	/** Enable HTML Guide Message */
	static inline std::atomic<bool> ENABLE_GUIDES{false};

	/** Html files directory */
	static inline ConfigValue<std::string> HTML_ROOT;

	/** Html cache directory */
	static inline ConfigValue<std::string> HTML_CACHE_FILE;

	/** Encoding */
	static inline ConfigValue<std::string> HTML_ENCODING;

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
