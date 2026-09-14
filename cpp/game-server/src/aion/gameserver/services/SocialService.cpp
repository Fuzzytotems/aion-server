#include "aion/gameserver/services/SocialService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

bool SocialService::addBlockedUser(model::gameobjects::player::Player& player, model::gameobjects::player::PlayerCommonData& blockedPlayer,
	std::string_view reason) {
	AION_UNPORTED();
}

bool SocialService::deleteBlockedUser(model::gameobjects::player::Player& player, model::gameobjects::player::BlockedPlayer& target) {
	AION_UNPORTED();
}

bool SocialService::setBlockedReason(model::gameobjects::player::Player& player, model::gameobjects::player::BlockedPlayer& target,
	std::string_view reason) {
	AION_UNPORTED();
}

bool SocialService::setFriendMemo(model::gameobjects::player::Player& player, model::gameobjects::player::Friend& target, std::string_view memo) {
	AION_UNPORTED();
}

bool SocialService::makeFriends(model::gameobjects::player::Player& friend1, model::gameobjects::player::Player& friend2) {
	AION_UNPORTED();
}

bool SocialService::deleteFriend(model::gameobjects::player::Player& deleter, model::gameobjects::player::Friend& friend_) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
