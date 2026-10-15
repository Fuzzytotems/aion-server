#include "aion/gameserver/network/aion/clientpackets/CM_PLAYER_STATUS_INFO.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/common/events/TeamCommand.h"
#include "aion/gameserver/model/team/common/events/TeamCommandInfo.h"
#include "aion/gameserver/model/team/common/service/PlayerTeamCommandService.h"
#include "aion/gameserver/model/team/league/LeagueService.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_PLAYER_STATUS_INFO::CM_PLAYER_STATUS_INFO(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

using model::gameobjects::player::Player;
using model::team::common::events::TeamCommand;

// Java CM_PLAYER_STATUS_INFO.java:31-36
void CM_PLAYER_STATUS_INFO::readImpl() {
	commandCode = readUC();
	selectedObjectId = readD();
	allianceGroupId = readD();
	secondObjectId = readD();
}

// Java CM_PLAYER_STATUS_INFO.java:39-55
void CM_PLAYER_STATUS_INFO::runImpl() {
	const runtime::Ptr<Player> activePlayer = getConnection()->getActivePlayer();
	TeamCommand command = model::team::common::events::getCommand(commandCode);
	switch (command) {
		case TeamCommand::GROUP_SET_LFG:
			activePlayer->setLookingForGroup(selectedObjectId == 2);
			break;
		case TeamCommand::ALLIANCE_CHANGE_GROUP:
			model::team::alliance::PlayerAllianceService::changeMemberGroup(*activePlayer, selectedObjectId, secondObjectId, allianceGroupId);
			break;
		case TeamCommand::LEAGUE_ALLIANCE_MOVE:
			model::team::league::LeagueService::moveAlliance(*activePlayer, selectedObjectId, allianceGroupId);
			break;
		default:
			model::team::common::service::PlayerTeamCommandService::executeCommand(*activePlayer, command, selectedObjectId);
	}
}

AION_CLIENT_PACKET(CM_PLAYER_STATUS_INFO);

} // namespace aion::gameserver::network::aion::clientpackets
