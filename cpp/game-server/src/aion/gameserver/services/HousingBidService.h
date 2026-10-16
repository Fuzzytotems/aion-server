#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/HouseBids.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 * HouseBids.h is included for the nested HouseBids::Bid (hub-headers.md §9.3).
 *
 * @author Rolandas, Neon
 */
class HousingBidService : public runtime::Immortal {
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::house::HouseBids>> bids{AION_LOCK_CLASS(HousingBidService::bids#stripe)}; // Java: = new ConcurrentHashMap<>()
	HousingBidService();
	~HousingBidService();
	void setBidInfoToHouses();
public:
	static HousingBidService& getInstance(); // Java singleton
	bool isRegisteringAllowed();
	bool auction(model::house::House& house, int64_t initialPrice);
private:
	bool isAuctionOpen(int32_t houseObjectId);
	bool isBiddingTime(int32_t houseObjectId);
public:
	runtime::Ptr<model::house::HouseBids> getBidInfo(model::house::House& house);
	std::vector<runtime::Ptr<model::house::HouseBids>> getBidInfo(model::Race race);
	runtime::Ptr<model::house::HouseBids::Bid> bid(model::gameobjects::player::Player& player, int32_t listIndex, int64_t bidOffer);
private:
	bool isAllowedToBid(model::gameobjects::player::Player& player, runtime::Ptr<model::house::HouseBids> houseBids, int64_t bidOffer);
public:
	void endAuctions();
	bool endAuction(int32_t houseObjectId);
private:
	void impoundAndAuctionOldPlayerHouses();
	int32_t getMinBidLevel(model::house::House& house);
public:
	void disableBids(int32_t playerObjId);
	runtime::Ptr<model::house::HouseBids::Bid> findLastBid(model::gameobjects::player::Player& player);
	runtime::Ptr<model::house::HouseBids> findBidsForRegisteredHouse(model::gameobjects::player::Player& player);
	bool cancelAuction(model::house::House& house);
	/** Notify once about new auction results, based on system mail checks and login time */
	void onPlayerLogin(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::services
