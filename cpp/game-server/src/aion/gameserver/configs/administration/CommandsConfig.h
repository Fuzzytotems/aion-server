#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::administration {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.administration.CommandsConfig
 *
 * @author Neon
 */
struct CommandsConfig {
	static inline ConfigValue<std::map<std::string, int8_t, std::less<>>> ACCESS_LEVELS;

	/** Location of chat command *.java handlers */
	static inline ConfigValue<std::vector<std::filesystem::path>> HANDLER_DIRECTORIES;

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::administration
