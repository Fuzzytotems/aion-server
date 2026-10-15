#include "aion/gameserver/model/team/group/events/PlayerGroupUpdateEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_MEMBER_INFO.h"
#include "aion/gameserver/utils/collections/Predicates.h"

namespace aion::gameserver::model::team::group::events {

using gameobjects::player::Player;

PlayerGroupUpdateEvent::PlayerGroupUpdateEvent(PlayerGroup& groupValue, Player& playerValue, common::legacy::GroupEvent groupEventValue,
	int32_t slotValue)
	: group(groupValue), player(playerValue), groupEvent(groupEventValue), slot(slotValue) {
}

PlayerGroupUpdateEvent::PlayerGroupUpdateEvent(PlayerGroup& groupValue, Player& playerValue, common::legacy::GroupEvent groupEventValue)
	: PlayerGroupUpdateEvent(groupValue, playerValue, groupEventValue, 0) {
}

void PlayerGroupUpdateEvent::handleEvent() {
	const auto allExcept = utils::collections::Predicates::Players::allExcept(*player);
	network::aion::serverpackets::SM_GROUP_MEMBER_INFO packet(*group, *player, groupEvent, slot);
	group->sendPacket([&allExcept](gameobjects::AionObject& object) { return allExcept(*runtime::cast<Player>(object)); }, {packet});
}

} // namespace aion::gameserver::model::team::group::events
