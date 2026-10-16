#include "aion/gameserver/model/team/alliance/events/AllianceDisbandEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/events/PlayerAllianceLeavedEvent.h"

namespace aion::gameserver::model::team::alliance::events {

AllianceDisbandEvent::AllianceDisbandEvent(PlayerAlliance& allianceValue) : alliance(allianceValue) {
}

void AllianceDisbandEvent::handleEvent() {
	PlayerAlliance& allianceValue = *alliance;
	allianceValue.forEach([&allianceValue](gameobjects::AionObject& player) {
		PlayerAllianceLeavedEvent event(allianceValue, *runtime::cast<gameobjects::player::Player>(player), PlayerAllianceLeavedEvent::LeaveReson::DISBAND);
		allianceValue.onEvent(event);
	});
}

} // namespace aion::gameserver::model::team::alliance::events
