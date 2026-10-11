#include "aion/gameserver/taskmanager/tasks/housing/AuctionAutoFillTask.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseBids.h"
#include "aion/gameserver/model/templates/housing/HouseType.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/HousingBidService.h"
#include "aion/gameserver/services/HousingService.h"

namespace aion::gameserver::taskmanager::tasks::housing {

using model::house::House;
using runtime::Ptr;

static_assert(static_cast<int32_t>(configs::detail::HouseType::PALACE) == static_cast<int32_t>(model::templates::housing::HouseType::PALACE) &&
		static_cast<int32_t>(configs::detail::HouseType::HOUSE) == static_cast<int32_t>(model::templates::housing::HouseType::HOUSE),
	"the config placeholder HouseType has HouseType.java's constant order");

AuctionAutoFillTask::AuctionAutoFillTask()
	: AbstractCronTask(configs::main::HousingConfig::AUCTION_AUTO_FILL_TIME.load(), "com.aionemu.gameserver.taskmanager.tasks.housing.AuctionAutoFillTask") {
}

AuctionAutoFillTask::~AuctionAutoFillTask() = default;

AuctionAutoFillTask& AuctionAutoFillTask::getInstance() {
	// Java: private static final AuctionAutoFillTask instance = new AuctionAutoFillTask() - one Ref that is never released
	static const runtime::Ref<AuctionAutoFillTask>* const instance = [] {
		auto* created = new runtime::Ref<AuctionAutoFillTask>(runtime::makeRef<AuctionAutoFillTask>());
		(*created)->postConstruct();
		return created;
	}();
	return **instance;
}

void AuctionAutoFillTask::executeTask() {
	if (configs::main::HousingConfig::ENABLE_HOUSE_AUCTIONS.load()) {
		autoFillAuction(model::Race::ELYOS);
		autoFillAuction(model::Race::ASMODIANS);
	}
}

// Java AuctionAutoFillTask.java:47-62
void AuctionAutoFillTask::autoFillAuction(model::Race race) {
	std::vector<Ptr<House>> auctionedHouses = findAuctionedHouses(race);
	std::vector<Ptr<House>> auctionableHouses = findAuctionableHouses(race, auctionedHouses);
	std::shuffle(auctionableHouses.begin(), auctionableHouses.end(), commons::utils::Rnd::generator()); // Java: Collections.shuffle
	// Java: groupingBy(House::getHouseType, counting())
	std::map<model::templates::housing::HouseType, int64_t> auctionedHouseCounts;
	for (const Ptr<House>& house : auctionedHouses)
		auctionedHouseCounts[house->getHouseType()]++;
	int32_t added = 0;
	auto limits = configs::main::HousingConfig::AUCTION_AUTO_FILL_LIMITS.get();
	if (!limits) // Java: NullPointerException on the null map
		throw runtime::NullPointerException("HousingConfig.AUCTION_AUTO_FILL_LIMITS is null");
	// Java: HousingConfig.AUCTION_AUTO_FILL_LIMITS.forEach - the config map's order (here the HouseType order of std::map)
	for (const auto& [configHouseType, limit] : *limits) {
		// the config's placeholder enum (configs/detail/ConfigEnums.h) has the constants of HouseType.java in the same order
		const auto houseType = static_cast<model::templates::housing::HouseType>(static_cast<int32_t>(configHouseType));
		const auto counted = auctionedHouseCounts.find(houseType);
		for (int64_t auctioned = counted == auctionedHouseCounts.end() ? 0 : counted->second; auctioned < limit; auctioned++) {
			Ptr<House> house = findAndRemoveHouse(auctionableHouses, houseType);
			if (!house || !services::HousingBidService::getInstance().auction(*house, house->getDefaultAuctionPrice()))
				break;
			added++;
		}
	}
	log.info("[" + std::string(xml::enumName(race)) + "] Added " + std::to_string(added) + " new houses automatically to auction.");
}

std::vector<runtime::Ptr<model::house::House>> AuctionAutoFillTask::findAuctionedHouses(model::Race race) {
	std::vector<Ptr<House>> houses;
	for (const Ptr<model::house::HouseBids>& houseBids : services::HousingBidService::getInstance().getBidInfo(race)) {
		Ptr<House> house = toHouse(*houseBids);
		if (std::find(houses.begin(), houses.end(), house) == houses.end())
			houses.push_back(house);
	}
	return houses;
}

runtime::Ptr<model::house::House> AuctionAutoFillTask::toHouse(model::house::HouseBids& houseBids) {
	return services::HousingService::getInstance().findHouse(houseBids.getHouseObjectId());
}

std::vector<runtime::Ptr<model::house::House>> AuctionAutoFillTask::findAuctionableHouses(model::Race race,
	const std::vector<runtime::Ptr<model::house::House>>& auctionedHouses) {
	std::vector<Ptr<House>> houses = services::HousingService::getInstance().getCustomHouses();
	std::erase_if(houses, [race, &auctionedHouses](const Ptr<House>& house) {
		return house->getOwnerId() != 0 || std::find(auctionedHouses.begin(), auctionedHouses.end(), house) != auctionedHouses.end() ||
			!house->matchesLandRace(race);
	});
	return houses;
}

runtime::Ptr<model::house::House> AuctionAutoFillTask::findAndRemoveHouse(std::vector<runtime::Ptr<model::house::House>>& houses,
	model::templates::housing::HouseType houseType) {
	for (auto iterator = houses.begin(); iterator != houses.end(); ++iterator) {
		Ptr<House> house = *iterator;
		if (house->getHouseType() == houseType) {
			houses.erase(iterator);
			return house;
		}
	}
	return nullptr;
}

} // namespace aion::gameserver::taskmanager::tasks::housing
