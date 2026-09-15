#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/player/title/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author xavier
 */
class PlayerTitleListDAO {
public:
	/** @return the loaded title list, a part the caller hands to Player::setTitleList (hub-headers.md §5: a newly created part) */
	static std::unique_ptr<model::gameobjects::player::title::TitleList> loadTitleList(int32_t playerId);
	static bool storeTitles(model::gameobjects::player::Player& player, model::gameobjects::player::title::Title& entry);
	static bool removeTitle(int32_t playerId, int32_t titleId);
};

} // namespace aion::gameserver::dao
