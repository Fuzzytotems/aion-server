#include "aion/gameserver/model/house/HouseBids.h"

#include <algorithm>
#include <limits>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::model::house {

namespace {

/** Java: (long) a - NaN 0, saturating */
int64_t javaLongCast(double a) noexcept {
	if (a != a)
		return 0;
	if (a >= 9223372036854775808.0)
		return std::numeric_limits<int64_t>::max();
	if (a <= -9223372036854775808.0)
		return std::numeric_limits<int64_t>::min();
	return static_cast<int64_t>(a);
}

} // namespace

HouseBids::Bid::Bid(HouseBids& outer, int32_t playerObjectIdValue, int64_t kinahValue, int64_t timeValue)
	: playerObjectId(playerObjectIdValue), kinah(kinahValue), time(timeValue), listIndex(outer.listIndex), houseObjectId(outer.houseObjectId),
	  registrationFee(outer.registrationFee) {
}

HouseBids::Bid::~Bid() = default;

runtime::Ref<HouseBids::Bid> HouseBids::Bid::create(HouseBids& outer, int32_t playerObjectIdValue, int64_t kinahValue, int64_t timeValue) {
	return runtime::makeRef<Bid>(outer, playerObjectIdValue, kinahValue, timeValue);
}

int64_t HouseBids::Bid::calculateSalesCommission() {
	// Java: (long) (kinah * HousingConfig.AUCTION_SALES_COMMISION_PERCENT), a float multiplication
	return javaLongCast(static_cast<float>(kinah) * configs::main::HousingConfig::AUCTION_SALES_COMMISION_PERCENT.load());
}

int64_t HouseBids::Bid::calculateSaleRewardKinah() {
	return kinah - calculateSalesCommission() + registrationFee;
}

HouseBids::HouseBids(int32_t houseObjectIdValue, int64_t initialPrice)
	: HouseBids(houseObjectIdValue, initialPrice, commons::utils::currentTimeMillis()) {
}

HouseBids::HouseBids(int32_t houseObjectIdValue, int64_t initialPrice, int64_t timeValue)
	: listIndex(counter.incrementAndGet()), houseObjectId(houseObjectIdValue),
	  // Java: (long) (initialPrice * HousingConfig.AUCTION_REGISTRATION_FEE_PERCENT), a float multiplication
	  registrationFee(javaLongCast(static_cast<float>(initialPrice) * configs::main::HousingConfig::AUCTION_REGISTRATION_FEE_PERCENT.load())) {
	bids.add(Bid::create(*this, 0, initialPrice, timeValue));
}

HouseBids::~HouseBids() = default;

runtime::Ref<HouseBids> HouseBids::create(int32_t houseObjectIdValue, int64_t initialPrice) {
	return runtime::makeRef<HouseBids>(houseObjectIdValue, initialPrice);
}

runtime::Ref<HouseBids> HouseBids::create(int32_t houseObjectIdValue, int64_t initialPrice, int64_t timeValue) {
	return runtime::makeRef<HouseBids>(houseObjectIdValue, initialPrice, timeValue);
}

runtime::Ptr<HouseBids::Bid> HouseBids::bid(gameobjects::player::Player& player, int64_t bidKinah) {
	return bid(player.getObjectId(), bidKinah, commons::utils::currentTimeMillis());
}

runtime::Ptr<HouseBids::Bid> HouseBids::bid(int32_t playerObjectIdValue, int64_t bidKinah, int64_t timeValue) {
	SYNCHRONIZED(*this) {
		runtime::Ptr<Bid> highestBid = getHighestBid();
		if (highestBid->getKinah() < bidKinah || (highestBid == getInitialOffer() && highestBid->getKinah() == bidKinah)) {
			runtime::Ref<Bid> newBid = Bid::create(*this, playerObjectIdValue, bidKinah, timeValue);
			bids.add(newBid);
			return runtime::Ptr<Bid>(newBid);
		}
		return nullptr;
	}
}

bool HouseBids::isHighestBidder(gameobjects::player::Player& player) {
	return getHighestBid()->getPlayerObjectId() == player.getObjectId();
}

runtime::Ptr<HouseBids::Bid> HouseBids::getHighestBid() {
	SYNCHRONIZED(*this) {
		return bids.get(bids.size() - 1);
	}
}

runtime::Ptr<HouseBids::Bid> HouseBids::getLatestBid(gameobjects::player::Player& player) {
	SYNCHRONIZED(*this) {
		for (int32_t i = bids.size() - 1; i >= 0; i--) {
			runtime::Ptr<Bid> candidate = bids.get(i);
			if (candidate->getPlayerObjectId() == player.getObjectId())
				return candidate;
		}
		return nullptr;
	}
}

runtime::Ptr<HouseBids::Bid> HouseBids::getInitialOffer() {
	SYNCHRONIZED(*this) {
		return bids.get(0);
	}
}

int32_t HouseBids::getBidCount() {
	SYNCHRONIZED(*this) {
		return bids.size() - 1; // first bid is initialPrice
	}
}

std::vector<runtime::Ref<HouseBids::Bid>> HouseBids::deleteOrDisableBids(int32_t playerObjectIdValue) {
	SYNCHRONIZED(*this) {
		std::vector<runtime::Ref<Bid>> bidsToDelete;
		for (int32_t i = 1, indexOfHighestBid = bids.size() - 1; i <= indexOfHighestBid; i++) {
			runtime::Ptr<Bid> candidate = bids.get(i);
			if (candidate->getPlayerObjectId() == playerObjectIdValue) {
				if (i == 1 || i < indexOfHighestBid)
					bidsToDelete.emplace_back(candidate);
				else
					candidate->playerObjectId.set(0);
			}
		}
		// Java: bids.removeAll(bidsToDelete) (Bid has identity equality)
		for (const runtime::Ref<Bid>& deleted : bidsToDelete)
			bids.remove(runtime::Ptr<Bid>(deleted));
		return bidsToDelete;
	}
}

} // namespace aion::gameserver::model::house
