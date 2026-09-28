#pragma once

#include <atomic>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.GeoDataConfig
 */
struct GeoDataConfig {
	/** Geodata enable */
	static inline std::atomic<bool> GEO_ENABLE{false};

	/** Enable canSee checks using geodata. */
	static inline std::atomic<bool> CANSEE_ENABLE{false};

	/** Enable Fear skill using geodata. */
	static inline std::atomic<bool> FEAR_ENABLE{false};

	/** Enable Geo checks during npc movement (prevent flying mobs) */
	static inline std::atomic<bool> GEO_NPC_MOVE{false};

	/** Enable geo materials using skills */
	static inline std::atomic<bool> GEO_MATERIALS_ENABLE{false};

	/** Show collision zone name and skill id */
	static inline std::atomic<bool> GEO_MATERIALS_SHOWDETAILS{false};

	/** Enable geo shields */
	static inline std::atomic<bool> GEO_SHIELDS_ENABLE{false};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
