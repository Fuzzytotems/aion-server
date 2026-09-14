#pragma once

#include <atomic>
#include <cstdint>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.CraftConfig
 */
struct CraftConfig {
	static inline std::atomic<bool> DELETE_EXCESS_CRAFT_ENABLE{false};

	/** Maximum number of expert skills a player can have */
	static inline std::atomic<int32_t> MAX_EXPERT_CRAFTING_SKILLS{0};

	/** Maximum number of master skills a player can have */
	static inline std::atomic<int32_t> MAX_MASTER_CRAFTING_SKILLS{0};

	/** Enable leveling of aether and essence tapping skills above 499 points */
	static inline std::atomic<bool> DISABLE_AETHER_AND_ESSENCE_TAPPING_CAP{false};

	static inline std::atomic<int32_t> MAX_CRAFT_FAILURE_CHANCE{0};

	static inline std::atomic<int32_t> MAX_GATHER_FAILURE_CHANCE{0};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
