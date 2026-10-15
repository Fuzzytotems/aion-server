#include "aion/gameserver/handlers/admincommands/Auction.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/housing/HouseType.h"
#include "aion/gameserver/services/HousingBidService.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/utils/EnumValueOf.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Auction);

Auction::Auction()
	: AdminCommand("auction", "Adds or removes houses to/from auction.", R"(<address> [starting price] - Auctions the given house.
<zone> <house type> <count> [starting price] - Auctions free houses of given type that are in the specified zone.
<asmo|ely> <house type> <count> [starting price] - Auctions free asmodian or elysean houses of given type.
end <address|zone> - Ends the auction for given house(s), transferring ownership to the highest bidder.
cancel <address|zone> - Cancels the auction for given house(s).
Zone: Zone name from zones xml files
House type: house, mansion, estate, palace
If no starting price is given, default will be taken from templates.
)") {
}

bool Auction::isDigits(std::string_view s) {
	return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

// Java Auction.java:35-142
void Auction::execute(Player& admin, std::span<const std::string> params) {
	namespace StringUtils = commons::utils::StringUtils;
	if (params.empty()) {
		sendInfo(admin);
		return;
	}

	const bool isCancel = StringUtils::equalsIgnoreCase("cancel", params[0]);
	if (StringUtils::equalsIgnoreCase("end", params[0]) || isCancel) {
		if (params.size() < 2) {
			sendInfo(admin);
			return;
		}

		const bool isHouseAddress = isDigits(params[1]);
		std::vector<runtime::Ptr<House>> houses;
		if (isHouseAddress) {
			runtime::Ptr<House> house = HousingService::getInstance().getHouseByAddress(commons::utils::parseInt(params[1]));
			if (!house) {
				sendInfo(admin, "Invalid house address.");
				return;
			}
			houses.push_back(house); // Java: Collections.singletonList(house)
		} else {
			std::optional<std::vector<runtime::Ptr<House>>> inZone = findHousesInZone(admin, params[1], [](House&) { return true; });
			if (!inZone)
				return;
			houses = std::move(*inZone);
		}

		int32_t removedHouses = 0;
		for (const runtime::Ptr<House>& house : houses) {
			if ((!isCancel && HousingBidService::getInstance().endAuction(house->getObjectId())) ||
				(isCancel && HousingBidService::getInstance().cancelAuction(*house))) {
				sendInfo(admin, std::string(isCancel ? "Canceled" : "Ended") + " auction for " +
									commons::utils::StringUtils::toLowerCase(xml::enumName(house->getHouseType())) + " " + std::to_string(house->getAddress()->getId()));
				removedHouses++;
			}
		}
		if (removedHouses == 0)
			sendInfo(admin, isHouseAddress ? "The house is not for sale." : "No houses for sale in this zone.");
	} else if (isDigits(params[0])) {
		const int32_t address = commons::utils::parseInt(params[0]);
		runtime::Ptr<House> house = HousingService::getInstance().getHouseByAddress(address);
		if (!house) {
			sendInfo(admin, "Invalid address.");
			return;
		}
		if (house->getBids()) {
			sendInfo(admin, "Address " + std::to_string(address) + " is already in auction.");
			return;
		}
		const int64_t price = params.size() < 2 ? house->getDefaultAuctionPrice() : commons::utils::parseLong(params[1]);
		if (price <= 0) {
			sendInfo(admin, "Starting price must be positive.");
			return;
		}
		HousingBidService::getInstance().auction(*house, price);
		sendInfo(admin, "Address " + std::to_string(address) + " was auctioned successfully.");
	} else if (params[0] == "add") {
		if (params.size() < 4 || params.size() > 5) {
			sendInfo(admin);
			return;
		}

		const HouseType houseType = utils::enumValueOf<HouseType>(commons::utils::StringUtils::toUpperCase(params[2]));
		const int32_t maxCount = commons::utils::parseInt(params[3]);
		if (maxCount <= 0) {
			sendInfo(admin, "Count must be positive.");
			return;
		}

		const std::function<bool(House&)> filter = [houseType](House& house) {
			return house.getHouseType() == houseType && !house.getBids() && house.getOwnerId() == 0;
		};
		std::optional<std::vector<runtime::Ptr<House>>> found;
		const std::string side = commons::utils::StringUtils::toLowerCase(params[1]);
		if (std::string_view("asmodians").starts_with(side)) {
			std::vector<runtime::Ptr<House>> matching;
			for (const runtime::Ptr<House>& house : HousingService::getInstance().getCustomHouses())
				if (filter(*house) && house->matchesLandRace(Race::ASMODIANS))
					matching.push_back(house);
			found = std::move(matching);
		} else if (std::string_view("elyos").starts_with(side)) {
			std::vector<runtime::Ptr<House>> matching;
			for (const runtime::Ptr<House>& house : HousingService::getInstance().getCustomHouses())
				if (filter(*house) && house->matchesLandRace(Race::ELYOS))
					matching.push_back(house);
			found = std::move(matching);
		} else {
			found = findHousesInZone(admin, params[1], filter);
		}
		if (!found)
			return;
		std::vector<runtime::Ptr<House>>& houses = *found;
		const std::string typeName = commons::utils::StringUtils::toLowerCase(xml::enumName(houseType));
		if (houses.empty()) {
			sendInfo(admin, "No auctionable " + typeName + "s found.");
			return;
		}
		const int64_t price = params.size() < 5 ? houses[0]->getDefaultAuctionPrice() : commons::utils::parseLong(params[4]);
		if (price <= 0) {
			sendInfo(admin, "Starting price must be positive.");
			return;
		}

		int32_t counter = 0;
		std::shuffle(houses.begin(), houses.end(), commons::utils::Rnd::generator()); // Java: Collections.shuffle
		for (const runtime::Ptr<House>& house : houses) {
			// java-bug (kept): the loop breaks only once counter exceeds maxCount, so maxCount + 1 houses go to auction (a proposed correction)
			if (HousingBidService::getInstance().auction(*house, price) && ++counter > maxCount)
				break;
		}

		sendInfo(admin, "Auctioned " + std::to_string(counter) + " " + typeName + "s for a starting price of " + std::to_string(price) + " Kinah.");
	} else {
		sendInfo(admin);
	}
}

// Java Auction.java:144-158
std::optional<std::vector<runtime::Ptr<House>>> Auction::findHousesInZone(Player& admin, std::string_view zoneName,
	const std::function<bool(House&)>& filter) {
	const world::zone::ZoneName* zone = world::zone::ZoneName::get(zoneName);
	if (zone == world::zone::ZoneName::NONE) {
		sendInfo(admin, "Invalid zone name");
		return std::nullopt;
	}
	std::vector<runtime::Ptr<House>> housesToRemove;
	for (const runtime::Ptr<House>& house : HousingService::getInstance().getCustomHouses()) {
		if (!filter(*house))
			continue;
		if (house->getPosition()->getMapRegion()->isInsideZone(zone, house->getX(), house->getY(), house->getZ()))
			housesToRemove.push_back(house);
	}
	return housesToRemove;
}

} // namespace aion::gameserver::handlers::admincommands
