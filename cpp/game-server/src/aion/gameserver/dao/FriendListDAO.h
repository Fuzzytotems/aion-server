#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Ben
 */
class FriendListDAO {
public:
	static runtime::Ref<model::gameobjects::player::FriendList> load(model::gameobjects::player::Player& player);
	static bool addFriends(model::gameobjects::player::Player& player, model::gameobjects::player::Player& friend_);
	static bool delFriends(int32_t playerOid, int32_t friendOid);
	static bool setFriendMemo(int32_t playerOid, int32_t friendOid, std::string_view memo);
};

} // namespace aion::gameserver::dao
