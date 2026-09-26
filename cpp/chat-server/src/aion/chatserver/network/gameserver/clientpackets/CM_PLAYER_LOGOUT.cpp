#include "aion/chatserver/network/gameserver/clientpackets/CM_PLAYER_LOGOUT.h"

#include "aion/chatserver/service/ChatService.h"

namespace aion::chatserver::network::gameserver::clientpackets {

void CM_PLAYER_LOGOUT::readImpl() {
	playerId = readD();
}

void CM_PLAYER_LOGOUT::runImpl() {
	service::ChatService::getInstance().playerLogout(playerId);
}

} // namespace aion::chatserver::network::gameserver::clientpackets
