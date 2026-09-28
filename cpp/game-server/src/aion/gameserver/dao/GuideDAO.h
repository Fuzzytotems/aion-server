#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/guide/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author xTz
 */
class GuideDAO {
public:
	static bool deleteGuide(int32_t guide_id);
	static std::vector<model::guide::Guide> loadGuides(int32_t playerId);
	/** @return the guide, null if absent (HTMLService.java:114 tests it) */
	static std::optional<model::guide::Guide> loadGuide(int32_t player_id, int32_t guide_id);
	static void saveGuide(int32_t guide_id, model::gameobjects::player::Player& player, std::string_view title);
	static std::vector<int32_t> getUsedIDs();
};

} // namespace aion::gameserver::dao
