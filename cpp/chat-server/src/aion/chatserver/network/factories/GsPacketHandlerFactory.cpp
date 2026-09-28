#include "aion/chatserver/network/factories/GsPacketHandlerFactory.h"

#include <fmt/format.h>

#include "aion/chatserver/network/gameserver/clientpackets/CM_CS_AUTH.h"
#include "aion/chatserver/network/gameserver/clientpackets/CM_PLAYER_AUTH.h"
#include "aion/chatserver/network/gameserver/clientpackets/CM_PLAYER_GAG.h"
#include "aion/chatserver/network/gameserver/clientpackets/CM_PLAYER_LOGOUT.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::chatserver::network::factories::GsPacketHandlerFactory {

using gameserver::GsClientPacket;
using gameserver::GsConnection;
using GameServerConnectionState = GsConnection::GameServerConnectionState;
using namespace gameserver::clientpackets;

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.network.factories.GsPacketHandlerFactory"));
	return *logger;
}

void logUnknownPacket(int32_t id, GameServerConnectionState state) {
	log().warn("Unknown packet received from Game Server: {} state {}", fmt::format("0x{:02X}", id), gameserver::toString(state));
}

} // namespace

std::unique_ptr<GsClientPacket> handle(commons::utils::ByteBuffer& data, std::shared_ptr<GsConnection> client) {
	std::unique_ptr<GsClientPacket> msg;
	GameServerConnectionState state = client->getState();
	int32_t id = data.get() & 0xff;

	switch (state) {
		case GameServerConnectionState::CONNECTED:
			if (id == 0x00)
				msg = std::make_unique<CM_CS_AUTH>();
			else
				logUnknownPacket(id, state);
			break;
		case GameServerConnectionState::AUTHED:
			switch (id) {
				case 0x01:
					msg = std::make_unique<CM_PLAYER_AUTH>();
					break;
				case 0x02:
					msg = std::make_unique<CM_PLAYER_LOGOUT>();
					break;
				case 0x03:
					msg = std::make_unique<CM_PLAYER_GAG>();
					break;
				default:
					logUnknownPacket(id, state);
			}
			break;
	}

	if (msg) {
		msg->setConnection(std::move(client));
		msg->setBuffer(data);
	}
	return msg;
}

} // namespace aion::chatserver::network::factories::GsPacketHandlerFactory
