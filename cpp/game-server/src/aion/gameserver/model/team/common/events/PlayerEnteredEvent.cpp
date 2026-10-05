#include "aion/gameserver/model/team/common/events/PlayerEnteredEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/services/event/EventService.h"

namespace aion::gameserver::model::team::common::events {

PlayerEnteredEvent::PlayerEnteredEvent(TemporaryPlayerTeam& teamValue, gameobjects::player::Player& playerValue)
	: team(teamValue), player(playerValue) {
}

bool PlayerEnteredEvent::checkCondition() {
	return !team->hasMember(player->getObjectId());
}

void PlayerEnteredEvent::handleEvent() {
	services::event::EventService::getInstance().onEnteredTeam(*player, *team);
}

} // namespace aion::gameserver::model::team::common::events
