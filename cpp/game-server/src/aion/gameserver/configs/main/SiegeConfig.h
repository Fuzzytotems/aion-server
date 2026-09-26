#pragma once

#include <atomic>
#include <cstdint>

#include "aion/gameserver/configs/detail/ConfigSupport.h"
#include "aion/gameserver/services/cron/CronExpression.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.SiegeConfig
 *
 * @author Sarynth, xTz, Source
 */
struct SiegeConfig {
	/** Siege Enabled */
	static inline std::atomic<bool> SIEGE_ENABLED{false};

	/** Balaur Assaults Enabled */
	static inline std::atomic<bool> BALAUR_AUTO_ASSAULT{false};

	static inline std::atomic<float> BALAUR_ASSAULT_RATE{0.0f};

	/** Berserker Sunayaka spawn time */
	static inline std::atomic<const services::cron::CronExpression*> MOLTENUS_SPAWN_SCHEDULE{nullptr};

	static inline std::atomic<float> FORTRESS_PROTECTOR_HEALTH_MULTIPLIER{0.0f};

	static inline std::atomic<float> ARTIFACT_PROTECTOR_HEALTH_MULTIPLIER{0.0f};

	static inline std::atomic<float> BASE_PROTECTOR_HEALTH_MULTIPLIER{0.0f};

	static inline std::atomic<float> SIEGE_DIFFICULTY_MULTIPLIER{0.0f};

	static inline std::atomic<int32_t> PANESTERRA_MAX_PLAYERS_PER_TEAM{0};

	static inline std::atomic<int32_t> AHSERION_MAX_PLAYERS_PER_TEAM{0};

	static inline std::atomic<const services::cron::CronExpression*> AHSERION_START_SCHEDULE{nullptr};

	static inline std::atomic<int32_t> LEGION_GP_CAP_PER_MEMBER{0};

	static inline std::atomic<double> DOOR_REPAIR_HEAL_PERCENT{0.0};

	static inline std::atomic<bool> SIEGE_REWARD_BALAUR_VICTORY{false};

	static inline std::atomic<bool> IGNORE_STAFF_ON_LOCATION_CLEAR{false};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
