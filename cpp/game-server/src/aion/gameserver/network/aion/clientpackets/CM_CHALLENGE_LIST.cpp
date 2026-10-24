#include "aion/gameserver/network/aion/clientpackets/CM_CHALLENGE_LIST.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/templates/challenge/ChallengeType.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/services/ChallengeTaskService.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::templates::challenge::ChallengeType;

CM_CHALLENGE_LIST::CM_CHALLENGE_LIST(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_CHALLENGE_LIST.java:28-34
void CM_CHALLENGE_LIST::readImpl() {
	action = readUC();
	taskOwner = readD();
	ownerType = readUC();
	playerId = readD();
	dateSince = readD();
}

// Java CM_CHALLENGE_LIST.java:37-48
void CM_CHALLENGE_LIST::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (ownerType == 1) {
		if (player->getLegion() == nullptr) {
			utils::audit::AuditLogger::log(*player, "tried to receive legion challenge task without legion");
			return;
		}
		services::ChallengeTaskService::getInstance().showTaskList(*player, ChallengeType::LEGION, taskOwner);
	} else {
		services::ChallengeTaskService::getInstance().showTaskList(*player, ChallengeType::TOWN, taskOwner);
	}
}

AION_CLIENT_PACKET(CM_CHALLENGE_LIST);

} // namespace aion::gameserver::network::aion::clientpackets
