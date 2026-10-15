#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include "aion/gameserver/configs/detail/ConfigSupport.h"
#include "aion/gameserver/services/cron/CronExpression.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.AutoGroupConfig
 *
 * @author xTz
 */
struct AutoGroupConfig {
	static inline std::atomic<bool> AUTO_GROUP_ENABLE{false};

	static inline std::atomic<bool> START_TIME_ENABLE{false};

	static inline std::atomic<int64_t> DREDGION_REGISTRATION_PERIOD{0};

	static inline ConfigValue<std::vector<const services::cron::CronExpression*>> DREDGION_TIMES;

	static inline std::atomic<int64_t> KAMAR_BATTLEFIELD_REGISTRATION_PERIOD{0};

	static inline ConfigValue<std::vector<const services::cron::CronExpression*>> KAMAR_BATTLEFIELD_TIMES;

	static inline std::atomic<int64_t> ENGULFED_OPHIDAN_BRIDGE_REGISTRATION_PERIOD{0};

	static inline ConfigValue<std::vector<const services::cron::CronExpression*>> ENGULFED_OPHIDAN_BRIDGE_TIMES;

	static inline std::atomic<int64_t> IRON_WALL_WARFRONT_REGISTRATION_PERIOD{0};

	static inline ConfigValue<std::vector<const services::cron::CronExpression*>> IRON_WALL_WARFRONT_TIMES;

	static inline std::atomic<int64_t> IDGEL_DOME_REGISTRATION_PERIOD{0};

	static inline ConfigValue<std::vector<const services::cron::CronExpression*>> IDGEL_DOME_TIMES;

	static inline std::atomic<bool> ANNOUNCE_BATTLEGROUND_REGISTRATIONS{false};

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
