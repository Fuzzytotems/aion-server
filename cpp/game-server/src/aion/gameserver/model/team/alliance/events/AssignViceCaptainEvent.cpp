#include "aion/gameserver/model/team/alliance/events/AssignViceCaptainEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::alliance::events {

using gameobjects::player::Player;
using network::aion::serverpackets::SM_ALLIANCE_INFO;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

AssignViceCaptainEvent::AssignViceCaptainEvent(PlayerAlliance& teamValue, Player& eventPlayerValue, AssignType assignTypeValue)
	: AbstractTeamPlayerEvent(teamValue, runtime::Ptr<Player>(eventPlayerValue)), assignType(assignTypeValue) {
}

bool AssignViceCaptainEvent::checkCondition() {
	return eventPlayer && eventPlayer->isOnline();
}

void AssignViceCaptainEvent::handleEvent() {
	PlayerAlliance& allianceValue = *runtime::cast<PlayerAlliance>(team);
	Player& player = *eventPlayer;
	switch (assignType) {
		case AssignType::DEMOTE:
			allianceValue.getViceCaptainIds().remove(player.getObjectId());
			break;
		case AssignType::PROMOTE:
			if (allianceValue.getViceCaptainIds().size() == 4) {
				PacketSendUtility::sendPacket(*allianceValue.getLeaderObject(), SM_SYSTEM_MESSAGE::STR_FORCE_CANNOT_PROMOTE_MANAGER());
				return;
			}
			allianceValue.getViceCaptainIds().add(player.getObjectId());
			break;
		case AssignType::DEMOTE_CAPTAIN_TO_VICECAPTAIN:
			if (allianceValue.getViceCaptainIds().size() < 3) {
				allianceValue.getViceCaptainIds().add(player.getObjectId());
			}
			break;
	}

	const AssignType type = assignType;
	allianceValue.forEach([&allianceValue, &player, type](gameobjects::AionObject& object) {
		Player& member = *runtime::cast<Player>(object);
		int32_t messageId = 0;
		switch (type) {
			case AssignType::PROMOTE:
				messageId = SM_ALLIANCE_INFO::VICECAPTAIN_PROMOTE;
				break;
			case AssignType::DEMOTE:
				messageId = SM_ALLIANCE_INFO::VICECAPTAIN_DEMOTE;
				break;
			default:
				break;
		}
		// TODO check whether same is sent to eventPlayer
		PacketSendUtility::sendPacket(member, SM_ALLIANCE_INFO(allianceValue, messageId, player.getName()));
	});

	if (allianceValue.isInLeague()) {
		// java-race: alliance → league here; league → alliance in every league event (m5g-plan.md §2.11 item 3)
		// lockdep: Java takes the league, and through it the other alliances, under this alliance's lock
		runtime::LockdepSuppression suppression("Java takes the league, and through it the other alliances, under this alliance's lock (m5g-plan.md D4)");
		allianceValue.getLeague()->broadcast();
	}
}

} // namespace aion::gameserver::model::team::alliance::events
