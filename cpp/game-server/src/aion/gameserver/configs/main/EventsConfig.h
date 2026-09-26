#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_set>

#include "aion/gameserver/configs/detail/ConfigSupport.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.EventsConfig
 *
 * @author Rolandas
 */
struct EventsConfig {
	static inline ConfigValue<std::unordered_set<std::string>> DISABLED_EVENTS;

	/** Event Upgrade Arcade */
	static inline std::atomic<bool> ENABLE_EVENT_ARCADE{false};

	static inline std::atomic<int32_t> ARCADE_RESUME_TOKEN{0};

	/** World Raid */
	static inline std::atomic<bool> ENABLE_WORLDRAID{false};

	static inline std::atomic<bool> WORLDRAID_ENABLE_SPAWNMSG{false};

	/** Headhunting */
	static inline std::atomic<bool> ENABLE_HEADHUNTING{false};

	static inline ConfigValue<std::unordered_set<int32_t>> HEADHUNTING_MAPS;

	static inline std::atomic<int32_t> HEADHUNTING_CONSOLATION_PRIZE_KILLS{0};

	/** Advent Calendar */
	static inline std::atomic<bool> ENABLE_ADVENT_CALENDAR{false};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
