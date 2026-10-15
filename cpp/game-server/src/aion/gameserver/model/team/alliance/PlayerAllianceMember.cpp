#include "aion/gameserver/model/team/alliance/PlayerAllianceMember.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceGroup.h"

namespace aion::gameserver::model::team::alliance {

PlayerAllianceMember::PlayerAllianceMember(gameobjects::player::Player& playerValue) : PlayerTeamMember(playerValue) {
}

PlayerAllianceMember::~PlayerAllianceMember() = default;

runtime::Ref<PlayerAllianceMember> PlayerAllianceMember::create(gameobjects::player::Player& playerValue) {
	return runtime::makeRef<PlayerAllianceMember>(playerValue);
}

runtime::Ptr<PlayerAllianceGroup> PlayerAllianceMember::getPlayerAllianceGroup() {
	return getPlayer().getPlayerAllianceGroup();
}

void PlayerAllianceMember::setPlayerAllianceGroup(runtime::Ptr<PlayerAllianceGroup> playerAllianceGroup) {
	getPlayer().setPlayerAllianceGroup(playerAllianceGroup);
}

} // namespace aion::gameserver::model::team::alliance
