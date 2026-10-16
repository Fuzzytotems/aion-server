#include "aion/gameserver/network/aion/clientpackets/CM_RECALLED_BY_OTHER_ANSWER.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/RecallService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;
using services::RecallService;

CM_RECALLED_BY_OTHER_ANSWER::CM_RECALLED_BY_OTHER_ANSWER(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_RECALLED_BY_OTHER_ANSWER.java:25-27
void CM_RECALLED_BY_OTHER_ANSWER::readImpl() {
	answer = readC();
}

// Java CM_RECALLED_BY_OTHER_ANSWER.java:30-37
void CM_RECALLED_BY_OTHER_ANSWER::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	switch (answer) {
		case 0:
			RecallService::getInstance().accept(*player);
			break;
		case 1:
			RecallService::getInstance().cancel(*player, RecallService::CancelReason::DECLINED);
			break;
		default:
			RecallService::getInstance().cancel(*player, RecallService::CancelReason::CANCELLED);
	}
}

AION_CLIENT_PACKET(CM_RECALLED_BY_OTHER_ANSWER);

} // namespace aion::gameserver::network::aion::clientpackets
