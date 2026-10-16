#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/abyss/fwd.h"

namespace aion::gameserver::services::abyss {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author ATracer
 */
class AbyssService {
private:
	static constexpr std::array<int32_t, 14> killAnnounceMaps{210050000, 210070000, 220070000, 220080000, 400010000, 400020000, 400030000,
		400040000, 400050000, 400060000, 600010000, 600070000, 600090000, 600100000};
	static bool shouldAnnounceHighRankedDeath(model::gameobjects::player::Player& victim);
public:
	static void announceHighRankedDeath(model::gameobjects::player::Player& victim);
	static void announceAbyssSkillUsage(model::gameobjects::player::Player& player, std::string_view skillL10n);
};

} // namespace aion::gameserver::services::abyss
