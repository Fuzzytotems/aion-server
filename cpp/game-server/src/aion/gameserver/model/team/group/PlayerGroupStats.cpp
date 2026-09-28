#include "aion/gameserver/model/team/group/PlayerGroupStats.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"

namespace aion::gameserver::model::team::group {

PlayerGroupStats::PlayerGroupStats(PlayerGroup& groupValue) : OwnedPart(groupValue), group(groupValue) {
}

PlayerGroupStats::~PlayerGroupStats() = default;

void PlayerGroupStats::onAddPlayer(PlayerGroupMember& member) {
	AION_UNPORTED();
}

void PlayerGroupStats::onRemovePlayer(PlayerGroupMember& member) {
	AION_UNPORTED();
}

void PlayerGroupStats::calculateExpLevels() {
	AION_UNPORTED();
}

void PlayerGroupStats::updateMinMaxLevelPlayers() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::team::group
