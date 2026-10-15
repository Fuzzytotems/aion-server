#include "aion/gameserver/network/chatserver/clientpackets/CM_CS_PLAYER_AUTH_RESPONSE.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHAT_INIT.h"
#include "aion/gameserver/network/chatserver/ChatServer.h"
#include "aion/gameserver/services/ban/ChatBanService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::network::chatserver::clientpackets {

CM_CS_PLAYER_AUTH_RESPONSE::CM_CS_PLAYER_AUTH_RESPONSE(int32_t opcode) : CsClientPacket(opcode) {
}

void CM_CS_PLAYER_AUTH_RESPONSE::readImpl() {
	playerId = readD();
	int32_t tokenLenght = readUC();
	token = readB(tokenLenght);
}

void CM_CS_PLAYER_AUTH_RESPONSE::runImpl() {
	if (ChatServer::getInstance().isUp()) {
		runtime::Ptr<model::gameobjects::player::Player> player = world::World::getInstance().getPlayer(playerId);
		if (player) {
			utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_CHAT_INIT(token));
			if (services::ban::ChatBanService::isBanned(*player))
				ChatServer::getInstance().sendPlayerGagPacket(player->getObjectId(), services::ban::ChatBanService::getBanMinutes(*player) * 60000LL);
		}
	}
}

} // namespace aion::gameserver::network::chatserver::clientpackets
