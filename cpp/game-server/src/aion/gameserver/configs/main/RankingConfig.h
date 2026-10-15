#pragma once

#include <atomic>
#include <cstdint>
#include <map>

#include "aion/gameserver/configs/detail/ConfigEnums.h"
#include "aion/gameserver/configs/detail/ConfigSupport.h"
#include "aion/gameserver/services/cron/CronExpression.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.RankingConfig
 *
 * @author Sarynth
 */
struct RankingConfig {
	static inline std::atomic<const services::cron::CronExpression*> TOP_RANKING_UPDATE_RULE{nullptr};

	static inline std::atomic<const services::cron::CronExpression*> TOP_RANKING_DAILY_GP_LOSS_TIME{nullptr};

	static inline std::atomic<int32_t> RANKING_LIST_LEGION_LIMIT{0};

	static inline std::atomic<int32_t> TOP_RANKING_MAX_OFFLINE_DAYS{0};

	static inline std::atomic<detail::AbyssRankEnum> XFORM_MIN_RANK{detail::AbyssRankEnum::GRADE9_SOLDIER};

	static inline ConfigValue<std::map<detail::AbyssRankEnum, int32_t>> TOP_RANKING_QUOTA;

	static inline ConfigValue<std::map<detail::AbyssRankEnum, int32_t>> TOP_RANKING_GP_LOSS;

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
