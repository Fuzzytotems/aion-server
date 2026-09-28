#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.WorldConfig
 *
 * @author ATracer
 */
struct WorldConfig {
	/** World region size */
	static inline std::atomic<int32_t> WORLD_REGION_SIZE{0};

	static inline std::atomic<int32_t> WORLD_MAX_TWINS_USUAL{0};

	static inline std::atomic<int32_t> WORLD_MAX_TWINS_BEGINNER{0};

	static inline std::atomic<bool> WORLD_EMULATE_FASTTRACK{false};

	/** Location of zone *.java handlers */
	static inline ConfigValue<std::filesystem::path> ZONE_HANDLER_DIRECTORY;

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
