#include "aion/loginserver/network/gameserver/clientpackets/CM_GS_AUTH.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/loginserver/GameServerTable.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_GS_AUTH_RESPONSE.h"

namespace aion::loginserver::network::gameserver::clientpackets {

void CM_GS_AUTH::readImpl() {
	gameServerId = readC();
	password = readS();
	int8_t length = readC();
	ip = readB(length);
	port = readUH();
	minAccessLevel = readC();
	maxPlayers = readD();
}

void CM_GS_AUTH::runImpl() {
	const std::shared_ptr<GsConnection>& connection = getConnection();

	GsAuthResponse resp = GameServerTable::registerGameServer(connection, gameServerId, password, ip, port, minAccessLevel, maxPlayers);
	switch (resp) {
		case GsAuthResponse::AUTHED:
			commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.network.gameserver.clientpackets.CM_GS_AUTH")
				.info("Gameserver #" + std::to_string(gameServerId) + " is now online");
			connection->setState(GsConnection::State::AUTHED);
			connection->sendPacket(std::make_shared<serverpackets::SM_GS_AUTH_RESPONSE>(resp));
			break;
		default:
			connection->close(std::make_shared<serverpackets::SM_GS_AUTH_RESPONSE>(resp));
	}
}

} // namespace aion::loginserver::network::gameserver::clientpackets
