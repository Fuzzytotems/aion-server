#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.AIConfig
 *
 * @author ATracer
 */
struct AIConfig {
	/** Debug (for developers) */
	static inline std::atomic<bool> MOVE_DEBUG{false};

	static inline std::atomic<bool> EVENT_DEBUG{false};

	static inline std::atomic<bool> ONCREATE_DEBUG{false};

	/** Enable NPC movement */
	static inline std::atomic<bool> ACTIVE_NPC_MOVEMENT{false};

	/** Minimum movement delay */
	static inline std::atomic<int32_t> MINIMIMUM_DELAY{0};

	/** Maximum movement delay */
	static inline std::atomic<int32_t> MAXIMUM_DELAY{0};

	/** Npc Shouts activator */
	static inline std::atomic<bool> SHOUTS_ENABLE{false};

	/** Location of AI *.java handlers */
	static inline ConfigValue<std::filesystem::path> HANDLER_DIRECTORY;

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
