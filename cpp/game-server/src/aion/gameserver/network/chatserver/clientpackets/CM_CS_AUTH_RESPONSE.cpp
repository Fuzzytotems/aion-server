#include "aion/gameserver/network/chatserver/clientpackets/CM_CS_AUTH_RESPONSE.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/network/chatserver/ChatServer.h"
#include "aion/gameserver/network/chatserver/ChatServerConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::chatserver::clientpackets {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.chatserver.clientpackets.CM_CS_AUTH_RESPONSE");

CM_CS_AUTH_RESPONSE::CM_CS_AUTH_RESPONSE(int32_t opcode) : CsClientPacket(opcode) {
}

void CM_CS_AUTH_RESPONSE::readImpl() {
	response = readC();
	if (response == 0) {
		int32_t length = readUC();
		ip = readB(length);
		port = readUH();
	}
}

void CM_CS_AUTH_RESPONSE::runImpl() {
	switch (response) {
		case 0: { // Authed
			// java-race: the state becomes AUTHED before the public address is set, so isUp() readers can see the old (empty) address
			getConnection()->setState(ChatServerConnection::State::AUTHED);
			runtime::Ref<runtime::Array<int8_t>> address = runtime::Array<int8_t>::make(static_cast<int32_t>(ip.size()));
			for (size_t i = 0; i < ip.size(); i++)
				(*address)[static_cast<int32_t>(i)] = static_cast<int8_t>(ip[i]);
			ChatServer::getInstance().setPublicAddress(address, port);
		} break;
		case 1: // Not authed
			log.warn("GameServer is not authenticated at ChatServer side!");
			getConnection()->close();
			break;
		case 2: // Already registered
			log.warn("GameServer is already registered at ChatServer side!");
			getConnection()->close();
			break;
	}
}

} // namespace aion::gameserver::network::chatserver::clientpackets
