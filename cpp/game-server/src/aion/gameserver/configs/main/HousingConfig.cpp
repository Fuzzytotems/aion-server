#include "aion/gameserver/configs/main/HousingConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"
#include "aion/gameserver/services/cron/CronService.h"

namespace aion::gameserver::configs::main {

void HousingConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.housing.visibility.distance", VISIBILITY_DISTANCE, "200");
	AION_BIND(p, "gameserver.housing.auction.enable", ENABLE_HOUSE_AUCTIONS, "true");
	AION_BIND(p, "gameserver.housing.pay.enable", ENABLE_HOUSE_PAY, "true");
	AION_BIND(p, "gameserver.housing.auction.end_time", HOUSE_AUCTION_END_TIME, "0 0 12 ? * SUN");
	AION_BIND(p, "gameserver.housing.auction.register_days", HOUSE_AUCTION_REGISTER_DAYS, "1, 5");
	AION_BIND(p, "gameserver.housing.maintain.time", HOUSE_MAINTENANCE_TIME, "0 0 0 ? * MON");
	AION_BIND(p, "gameserver.housing.auction.default_bid.house", HOUSE_MIN_BID, "0");
	AION_BIND(p, "gameserver.housing.auction.default_bid.mansion", MANSION_MIN_BID, "0");
	AION_BIND(p, "gameserver.housing.auction.default_bid.estate", ESTATE_MIN_BID, "0");
	AION_BIND(p, "gameserver.housing.auction.default_bid.palace", PALACE_MIN_BID, "0");
	AION_BIND(p, "gameserver.housing.auction.bidding.min_level.house", HOUSE_MIN_BID_LEVEL, "0");
	AION_BIND(p, "gameserver.housing.auction.bidding.min_level.mansion", MANSION_MIN_BID_LEVEL, "0");
	AION_BIND(p, "gameserver.housing.auction.bidding.min_level.estate", ESTATE_MIN_BID_LEVEL, "0");
	AION_BIND(p, "gameserver.housing.auction.bidding.min_level.palace", PALACE_MIN_BID_LEVEL, "0");
	AION_BIND(p, "gameserver.housing.auction.registration_fee", AUCTION_REGISTRATION_FEE_PERCENT, "0.3");
	AION_BIND(p, "gameserver.housing.auction.sales_commission", AUCTION_SALES_COMMISION_PERCENT, "0.1");
	AION_BIND(p, "gameserver.housing.auction.grace_end_refund", AUCTION_GRACE_END_REFUND_PERCENT, "0.5");
	AION_BIND(p, "gameserver.housing.auction.steplimit", AUCTION_BID_STEP_LIMIT, "100");
	AION_BIND(p, "gameserver.housing.auction.auto_fill.time", AUCTION_AUTO_FILL_TIME, "0 0 0 ? * MON");
	AION_BIND_PATTERN(p, "^gameserver\\.housing\\.auction\\.auto_fill\\.limit\\.(.+)", AUCTION_AUTO_FILL_LIMITS);
}

} // namespace aion::gameserver::configs::main
