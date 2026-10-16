#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <vector>

#include "aion/gameserver/configs/detail/ConfigEnums.h"
#include "aion/gameserver/configs/detail/ConfigSupport.h"
#include "aion/gameserver/services/cron/CronExpression.h"

namespace aion::gameserver::configs::main {

/**
 * Threads: rebound at runtime by Config::load (see Config.h), so scalar fields are std::atomic and all others ConfigValue.
 * <p>
 * Java: com.aionemu.gameserver.configs.main.HousingConfig
 *
 * @author Rolandas
 */
struct HousingConfig {
	/** Distance Visibility */
	static inline std::atomic<float> VISIBILITY_DISTANCE{200.0f};

	static inline std::atomic<bool> ENABLE_HOUSE_AUCTIONS{false};

	static inline std::atomic<bool> ENABLE_HOUSE_PAY{false};

	static inline std::atomic<const services::cron::CronExpression*> HOUSE_AUCTION_END_TIME{nullptr};

	static inline ConfigValue<std::vector<int32_t>> HOUSE_AUCTION_REGISTER_DAYS;

	static inline std::atomic<const services::cron::CronExpression*> HOUSE_MAINTENANCE_TIME{nullptr};

	/** Auction default bid prices */
	static inline std::atomic<int32_t> HOUSE_MIN_BID{0};

	static inline std::atomic<int32_t> MANSION_MIN_BID{0};

	static inline std::atomic<int32_t> ESTATE_MIN_BID{0};

	static inline std::atomic<int32_t> PALACE_MIN_BID{0};

	/** Auction minimal level required for bidding */
	static inline std::atomic<int32_t> HOUSE_MIN_BID_LEVEL{0};

	static inline std::atomic<int32_t> MANSION_MIN_BID_LEVEL{0};

	static inline std::atomic<int32_t> ESTATE_MIN_BID_LEVEL{0};

	static inline std::atomic<int32_t> PALACE_MIN_BID_LEVEL{0};

	static inline std::atomic<float> AUCTION_REGISTRATION_FEE_PERCENT{0.0f};

	static inline std::atomic<float> AUCTION_SALES_COMMISION_PERCENT{0.0f};

	static inline std::atomic<float> AUCTION_GRACE_END_REFUND_PERCENT{0.0f};

	static inline std::atomic<float> AUCTION_BID_STEP_LIMIT{0.0f};

	static inline std::atomic<const services::cron::CronExpression*> AUCTION_AUTO_FILL_TIME{nullptr};

	static inline ConfigValue<std::map<detail::HouseType, int32_t>> AUCTION_AUTO_FILL_LIMITS;

	/** Binds the fields above to their property keys (Java: the @Property/@Properties annotations). */
	static void bind(commons::configuration::ConfigurableProcessor& processor);
};

} // namespace aion::gameserver::configs::main
