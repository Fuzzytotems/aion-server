#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "aion/gameserver/custom/instance/fwd.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Jo, Estrayl
 */
class CustomInstanceDAO {
public:
	/** @return the rank, null if the player has none (CustomInstanceService tests it) */
	static std::optional<custom::instance::CustomInstanceRank> loadPlayerRankObject(int32_t playerId);
	static bool storePlayer(custom::instance::CustomInstanceRank& rankObj);
	static std::vector<custom::instance::CustomInstanceRankedPlayer> loadTop10(model::Race race);
};

} // namespace aion::gameserver::dao
