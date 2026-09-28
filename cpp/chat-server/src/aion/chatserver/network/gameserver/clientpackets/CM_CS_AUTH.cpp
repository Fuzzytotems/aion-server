#include "aion/chatserver/network/gameserver/clientpackets/CM_CS_AUTH.h"

#include "aion/chatserver/network/gameserver/serverpackets/SM_GS_AUTH_RESPONSE.h"
#include "aion/chatserver/service/GameServerService.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::chatserver::network::gameserver::clientpackets {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.network.gameserver.clientpackets.CM_CS_AUTH"));
	return *logger;
}

} // namespace

void CM_CS_AUTH::readImpl() {
	gameServerId = readC();
	password = readS();
}

void CM_CS_AUTH::runImpl() {
	GsAuthResponse resp = service::GameServerService::getInstance().registerGameServer(gameServerId, password);
	switch (resp) {
		case GsAuthResponse::AUTHED:
			getConnection()->setState(GsConnection::GameServerConnectionState::AUTHED);
			log().info("Gameserver #{} is now online", gameServerId);
			break;
		case GsAuthResponse::NOT_AUTHED:
			log().warn("Gameserver #{} (IP: {}) tried to register with an invalid password", gameServerId, getConnection()->getIP());
			break;
		case GsAuthResponse::ALREADY_REGISTERED:
			log().info("Gameserver #{} is already registered", gameServerId);
			break;
	}
	sendPacket(std::make_shared<serverpackets::SM_GS_AUTH_RESPONSE>(resp));
}

} // namespace aion::chatserver::network::gameserver::clientpackets
