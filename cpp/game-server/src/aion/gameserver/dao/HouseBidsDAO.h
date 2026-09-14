#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/house/HouseBids.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::dao {

/**
 * @author Rolandas
 */
class HouseBidsDAO {
public:
	static std::unordered_set<int32_t> loadBids(const std::unordered_map<int32_t, runtime::Ptr<model::house::HouseBids>>& bidsById);
	static bool addBid(model::house::HouseBids::Bid& bid);
	static bool deleteOrDisableBids(int32_t playerObjectId, const std::vector<runtime::Ptr<model::house::HouseBids::Bid>>& bidsToDelete);
	static bool deleteHouseBids(int32_t houseId);
};

} // namespace aion::gameserver::dao
