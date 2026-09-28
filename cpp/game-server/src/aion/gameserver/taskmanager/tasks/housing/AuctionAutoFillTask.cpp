#include "aion/gameserver/taskmanager/tasks/housing/AuctionAutoFillTask.h"

#include <algorithm>

#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseBids.h"
#include "aion/gameserver/model/templates/housing/HouseType.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/HousingBidService.h"
#include "aion/gameserver/services/HousingService.h"

namespace aion::gameserver::taskmanager::tasks::housing {

using model::house::House;
using runtime::Ptr;

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
		// Java: autoFillAuction(Race.ELYOS); autoFillAuction(Race.ASMODIANS) - HousingBidService.auction is not ported yet
		AION_PARTIAL("automatic house auction registration is not ported yet (HousingBidService.auction, M5a E2-05)");
	}
}

void AuctionAutoFillTask::autoFillAuction(model::Race race) {
	AION_UNPORTED();
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
