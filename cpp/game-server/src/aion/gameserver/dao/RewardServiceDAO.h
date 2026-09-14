#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/templates/rewards/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author KID, Neon
 */
class RewardServiceDAO {
public:
	static std::vector<runtime::Ref<model::templates::rewards::RewardEntryItem>> loadUnreceived(int32_t playerId);
	static void storeReceived(const std::vector<int32_t>& ids, int64_t timeReceived);
};

} // namespace aion::gameserver::dao
