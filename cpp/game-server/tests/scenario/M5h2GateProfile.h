#pragma once

// M5h-2 H2-4 (m5h-plan.md 14.3): the housing keys of the land-house gate gs.scenario.m5h2, merged into its gateKeys like M5h's legion keys. The
// auction and the maintenance are weekly crons in Java (HousingConfig.java:28, :34, :67); the gate needs them inside its run, at fixed seconds of
// every minute:
// - AUCTION_AUTO_FILL_TIME at second 10 with the limits HOUSE 1, the other types 0 (AuctionAutoFillTask: one free HOUSE per race on sale),
// - HOUSE_AUCTION_END_TIME at second 20 (AuctionEndTask: HousingBidService.endAuctions; an unsold house leaves the list until the next fill),
// - HOUSE_MAINTENANCE_TIME at second 40 (MaintenanceTask: the first free period, the overdue notices, the impound),
// - the initial offer of a HOUSE 100,000 kinah (HOUSE_MIN_BID) and its bidding level 1 (HOUSE_MIN_BID_LEVEL; the lands' sale level is 50),
// - registration allowed every day (HousingBidService.isRegisteringAllowed: from 1 to 7),
// - the pay-rent and auction switches on, the fees Java's defaults written out.

#include <map>
#include <string>

namespace aion::gameserver::scenario {

inline std::map<std::string, std::string> m5h2HousingKeys() {
	return {
		{"gameserver.housing.auction.enable", "true"},
		{"gameserver.housing.pay.enable", "true"},
		{"gameserver.housing.auction.auto_fill.time", "10 * * ? * *"},
		{"gameserver.housing.auction.auto_fill.limit.HOUSE", "1"},
		{"gameserver.housing.auction.auto_fill.limit.MANSION", "0"},
		{"gameserver.housing.auction.auto_fill.limit.ESTATE", "0"},
		{"gameserver.housing.auction.auto_fill.limit.PALACE", "0"},
		{"gameserver.housing.auction.end_time", "20 * * ? * *"},
		{"gameserver.housing.maintain.time", "40 * * ? * *"},
		{"gameserver.housing.auction.default_bid.house", "100000"},
		{"gameserver.housing.auction.bidding.min_level.house", "1"},
		{"gameserver.housing.auction.register_days", "1, 7"},
		{"gameserver.housing.auction.registration_fee", "0.3"},
		{"gameserver.housing.auction.sales_commission", "0.1"},
		{"gameserver.housing.auction.steplimit", "100"},
	};
}

} // namespace aion::gameserver::scenario
