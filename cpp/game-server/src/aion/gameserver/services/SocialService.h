#pragma once

#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * Handles activities related to social groups ingame such as the buddy list, block list, etc
 *
 * @author Ben, Neon
 */
class SocialService {
public:
	static bool addBlockedUser(model::gameobjects::player::Player& player, model::gameobjects::player::PlayerCommonData& blockedPlayer,
		std::string_view reason);
	static bool deleteBlockedUser(model::gameobjects::player::Player& player, model::gameobjects::player::BlockedPlayer& target);
	/** Sets the reason for blocking a user */
	static bool setBlockedReason(model::gameobjects::player::Player& player, model::gameobjects::player::BlockedPlayer& target,
		std::string_view reason);
	static bool setFriendMemo(model::gameobjects::player::Player& player, model::gameobjects::player::Friend& target, std::string_view memo);
	/** Adds two players to each others friend lists, and updates the database */
	static bool makeFriends(model::gameobjects::player::Player& friend1, model::gameobjects::player::Player& friend2);
	/** Deletes two players from eachother's friend lists, and updates the database */
	static bool deleteFriend(model::gameobjects::player::Player& deleter, model::gameobjects::player::Friend& friend_);
};

} // namespace aion::gameserver::services
