#include "aion/gameserver/model/house/HouseBids.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::house {

HouseBids::Bid::Bid(HouseBids& outer, int32_t playerObjectIdValue, int64_t kinahValue, int64_t timeValue)
	: playerObjectId(playerObjectIdValue), kinah(kinahValue), time(timeValue), listIndex(outer.listIndex), houseObjectId(outer.houseObjectId),
	  registrationFee(outer.registrationFee) {
}

HouseBids::Bid::~Bid() = default;

runtime::Ref<HouseBids::Bid> HouseBids::Bid::create(HouseBids& outer, int32_t playerObjectIdValue, int64_t kinahValue, int64_t timeValue) {
	return runtime::makeRef<Bid>(outer, playerObjectIdValue, kinahValue, timeValue);
}

int64_t HouseBids::Bid::calculateSalesCommission() {
	AION_UNPORTED();
}

int64_t HouseBids::Bid::calculateSaleRewardKinah() {
	AION_UNPORTED();
}

HouseBids::HouseBids(int32_t houseObjectIdValue, int64_t initialPrice)
	: HouseBids(houseObjectIdValue, initialPrice, commons::utils::currentTimeMillis()) {
}

HouseBids::HouseBids(int32_t houseObjectIdValue, int64_t initialPrice, int64_t timeValue)
	: listIndex(counter.incrementAndGet()), houseObjectId(houseObjectIdValue), registrationFee(0) {
	// Java: registrationFee = (long) (initialPrice * HousingConfig.AUCTION_REGISTRATION_FEE_PERCENT); bids.add(new Bid(0, initialPrice, time))
	static_cast<void>(initialPrice);
	static_cast<void>(timeValue);
	AION_UNPORTED();
}

HouseBids::~HouseBids() = default;

runtime::Ref<HouseBids> HouseBids::create(int32_t houseObjectIdValue, int64_t initialPrice) {
	return runtime::makeRef<HouseBids>(houseObjectIdValue, initialPrice);
}

runtime::Ref<HouseBids> HouseBids::create(int32_t houseObjectIdValue, int64_t initialPrice, int64_t timeValue) {
	return runtime::makeRef<HouseBids>(houseObjectIdValue, initialPrice, timeValue);
}

runtime::Ptr<HouseBids::Bid> HouseBids::bid(gameobjects::player::Player& player, int64_t bidKinah) {
	AION_UNPORTED();
}

runtime::Ptr<HouseBids::Bid> HouseBids::bid(int32_t playerObjectId, int64_t bidKinah, int64_t timeValue) {
	AION_UNPORTED();
}

bool HouseBids::isHighestBidder(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

runtime::Ptr<HouseBids::Bid> HouseBids::getHighestBid() {
	AION_UNPORTED();
}

runtime::Ptr<HouseBids::Bid> HouseBids::getLatestBid(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

runtime::Ptr<HouseBids::Bid> HouseBids::getInitialOffer() {
	AION_UNPORTED();
}

int32_t HouseBids::getBidCount() {
	AION_UNPORTED();
}

std::vector<runtime::Ref<HouseBids::Bid>> HouseBids::deleteOrDisableBids(int32_t playerObjectId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::house
