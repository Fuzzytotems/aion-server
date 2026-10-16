#include "aion/gameserver/model/team/alliance/events/CheckAllianceReadyEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_READY_CHECK.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::alliance::events {

using common::events::TeamCommand;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_ALLIANCE_READY_CHECK;
using utils::PacketSendUtility;

CheckAllianceReadyEvent::CheckAllianceReadyEvent(PlayerAlliance& allianceValue, Player& playerValue, TeamCommand eventCodeValue)
	: alliance(allianceValue), player(playerValue), eventCode(eventCodeValue) {
}

void CheckAllianceReadyEvent::handleEvent() {
	PlayerAlliance& allianceValue = *alliance;
	int32_t readyStatus = allianceValue.getAllianceReadyStatus();
	switch (eventCode) {
		case TeamCommand::ALLIANCE_CHECKREADY_CANCEL:
			readyStatus = 0;
			break;
		case TeamCommand::ALLIANCE_CHECKREADY_START:
			readyStatus = static_cast<int32_t>(allianceValue.getOnlineMembers().size()) - 1;
			break;
		case TeamCommand::ALLIANCE_CHECKREADY_AUTOCANCEL:
			readyStatus = 0;
			break;
		case TeamCommand::ALLIANCE_CHECKREADY_READY:
		case TeamCommand::ALLIANCE_CHECKREADY_NOTREADY:
			readyStatus -= 1;
			break;
		default:
			break;
	}
	allianceValue.setAllianceReadyStatus(readyStatus);
	const int32_t playerObjectId = player->getObjectId();
	const TeamCommand code = eventCode;
	allianceValue.forEach([&allianceValue, playerObjectId, code](gameobjects::AionObject& object) {
		Player& member = *runtime::cast<Player>(object);
		switch (code) {
			case TeamCommand::ALLIANCE_CHECKREADY_CANCEL:
				PacketSendUtility::sendPacket(member, SM_ALLIANCE_READY_CHECK(playerObjectId, 0));
				break;
			case TeamCommand::ALLIANCE_CHECKREADY_START:
				PacketSendUtility::sendPacket(member, SM_ALLIANCE_READY_CHECK(playerObjectId, 5));
				PacketSendUtility::sendPacket(member, SM_ALLIANCE_READY_CHECK(playerObjectId, 1));
				break;
			case TeamCommand::ALLIANCE_CHECKREADY_AUTOCANCEL:
				PacketSendUtility::sendPacket(member, SM_ALLIANCE_READY_CHECK(playerObjectId, 2));
				break;
			case TeamCommand::ALLIANCE_CHECKREADY_READY:
				PacketSendUtility::sendPacket(member, SM_ALLIANCE_READY_CHECK(playerObjectId, 5));
				if (allianceValue.getAllianceReadyStatus() == 0) {
					PacketSendUtility::sendPacket(member, SM_ALLIANCE_READY_CHECK(0, 3));
				}
				break;
			case TeamCommand::ALLIANCE_CHECKREADY_NOTREADY:
				PacketSendUtility::sendPacket(member, SM_ALLIANCE_READY_CHECK(playerObjectId, 4));
				if (allianceValue.getAllianceReadyStatus() == 0) {
					PacketSendUtility::sendPacket(member, SM_ALLIANCE_READY_CHECK(0, 3));
				}
				break;
			default:
				break;
		}
	});
}

} // namespace aion::gameserver::model::team::alliance::events
