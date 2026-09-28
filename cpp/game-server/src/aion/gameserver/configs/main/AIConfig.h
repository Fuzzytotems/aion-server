#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>

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

	/**
	 * C++ only (gameserver.dev.missing_ai_handlers, docs/deviations/P4-01.md): what AIEngine does with an AI name that has no compiled handler.
	 * "fail" (default, Java behaviour): AIEngine.validateScripts fails the startup and newAI throws "No AI found for name". "warn" (development
	 * while the AI handlers are not ported, m5a-plan.md D1/D2): validateScripts logs the missing names once and newAI creates a DummyAI with one
	 * warning per name. Other values count as "fail"; the value is compared ignoring case.
	 */
	static inline ConfigValue<std::string> MISSING_AI_HANDLERS;

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
