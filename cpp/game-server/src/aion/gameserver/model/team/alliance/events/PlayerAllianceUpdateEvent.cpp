#include "aion/gameserver/model/team/alliance/events/PlayerAllianceUpdateEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceMember.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_MEMBER_INFO.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/collections/Predicates.h"

namespace aion::gameserver::model::team::alliance::events {

using common::legacy::PlayerAllianceEvent;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_ALLIANCE_MEMBER_INFO;

PlayerAllianceUpdateEvent::PlayerAllianceUpdateEvent(PlayerAlliance& allianceValue, Player& playerValue, PlayerAllianceEvent allianceEventValue,
	int32_t slotValue)
	: alliance(allianceValue), player(playerValue), allianceEvent(allianceEventValue), updateMember(allianceValue.getMember(playerValue.getObjectId())),
	  slot(slotValue) {
}

PlayerAllianceUpdateEvent::PlayerAllianceUpdateEvent(PlayerAlliance& allianceValue, Player& playerValue, PlayerAllianceEvent allianceEventValue)
	: PlayerAllianceUpdateEvent(allianceValue, playerValue, allianceEventValue, 0) {
}

void PlayerAllianceUpdateEvent::handleEvent() {
	switch (allianceEvent) {
		case PlayerAllianceEvent::MOVEMENT:
		case PlayerAllianceEvent::UPDATE:
		case PlayerAllianceEvent::UPDATE_EFFECTS: {
			if (!updateMember)
				throw runtime::NullPointerException("PlayerAllianceUpdateEvent.updateMember");
			const auto allExcept = utils::collections::Predicates::Players::allExcept(*player);
			SM_ALLIANCE_MEMBER_INFO packet(*updateMember, allianceEvent, slot);
			alliance->sendPacket([&allExcept](gameobjects::AionObject& object) { return allExcept(*runtime::cast<Player>(object)); }, {packet});
			break;
		}
		default:
			// Unsupported
			break;
	}
}

} // namespace aion::gameserver::model::team::alliance::events
