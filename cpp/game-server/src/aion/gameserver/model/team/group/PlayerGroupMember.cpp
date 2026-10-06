#include "aion/gameserver/model/team/group/PlayerGroupMember.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::model::team::group {

PlayerGroupMember::PlayerGroupMember(gameobjects::player::Player& playerValue) : PlayerTeamMember(playerValue) {
}

PlayerGroupMember::~PlayerGroupMember() = default;

runtime::Ref<PlayerGroupMember> PlayerGroupMember::create(gameobjects::player::Player& playerValue) {
	return runtime::makeRef<PlayerGroupMember>(playerValue);
}

} // namespace aion::gameserver::model::team::group
