#include "aion/chatserver/network/gameserver/clientpackets/CM_PLAYER_GAG.h"

#include "aion/chatserver/service/ChatService.h"

namespace aion::chatserver::network::gameserver::clientpackets {

void CM_PLAYER_GAG::readImpl() {
	playerId = readD();
	gagTimeMillis = readQ();
}

void CM_PLAYER_GAG::runImpl() {
	service::ChatService::getInstance().gagPlayer(playerId, gagTimeMillis);
}

} // namespace aion::chatserver::network::gameserver::clientpackets
