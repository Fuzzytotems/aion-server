#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/dao/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Estrayl, Neon
 */
class BonusPackDAO {
public:
	static int32_t loadReceivingPlayer(int32_t accountId);
	static bool storeReceivingPlayer(int32_t accountId, int32_t playerId);
};

} // namespace aion::gameserver::dao
