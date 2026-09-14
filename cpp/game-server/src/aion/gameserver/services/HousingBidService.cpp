#include "aion/gameserver/services/HousingBidService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/house/HouseBids.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("HOUSE_AUCTION_LOG");

HousingBidService::HousingBidService() {
	AION_UNPORTED();
}

HousingBidService::~HousingBidService() = default;

HousingBidService& HousingBidService::getInstance() {
	static HousingBidService instance; // Java SingletonHolder
	return instance;
}

void HousingBidService::setBidInfoToHouses() {
	AION_UNPORTED();
}

bool HousingBidService::isRegisteringAllowed() {
	AION_UNPORTED();
}

bool HousingBidService::auction(model::house::House& house, int64_t initialPrice) {
	AION_UNPORTED();
}

bool HousingBidService::isAuctionOpen(int32_t houseObjectId) {
	AION_UNPORTED();
}

bool HousingBidService::isBiddingTime(int32_t houseObjectId) {
	AION_UNPORTED();
}

runtime::Ptr<model::house::HouseBids> HousingBidService::getBidInfo(model::house::House& house) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::house::HouseBids>> HousingBidService::getBidInfo(model::Race race) {
	AION_UNPORTED();
}

runtime::Ptr<model::house::HouseBids::Bid> HousingBidService::bid(model::gameobjects::player::Player& player, int32_t listIndex, int64_t bidOffer) {
	AION_UNPORTED();
}

bool HousingBidService::isAllowedToBid(model::gameobjects::player::Player& player, runtime::Ptr<model::house::HouseBids> houseBids, int64_t bidOffer) {
	AION_UNPORTED();
}

void HousingBidService::endAuctions() {
	AION_UNPORTED();
}

bool HousingBidService::endAuction(int32_t houseObjectId) {
	AION_UNPORTED();
}

void HousingBidService::impoundAndAuctionOldPlayerHouses() {
	AION_UNPORTED();
}

int32_t HousingBidService::getMinBidLevel(model::house::House& house) {
	AION_UNPORTED();
}

void HousingBidService::disableBids(int32_t playerObjId) {
	AION_UNPORTED();
}

runtime::Ptr<model::house::HouseBids::Bid> HousingBidService::findLastBid(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

runtime::Ptr<model::house::HouseBids> HousingBidService::findBidsForRegisteredHouse(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool HousingBidService::cancelAuction(model::house::House& house) {
	AION_UNPORTED();
}

void HousingBidService::onPlayerLogin(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
