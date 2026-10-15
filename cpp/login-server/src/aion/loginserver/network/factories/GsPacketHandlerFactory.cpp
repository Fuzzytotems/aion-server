#include "aion/loginserver/network/factories/GsPacketHandlerFactory.h"

#include <fmt/format.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/loginserver/network/gameserver/clientpackets/CM_ACCOUNT_AUTH.h"
#include "aion/loginserver/network/gameserver/clientpackets/CM_ACCOUNT_CONNECTION_INFO.h"
#include "aion/loginserver/network/gameserver/clientpackets/CM_ACCOUNT_DISCONNECTED.h"
#include "aion/loginserver/network/gameserver/clientpackets/CM_ACCOUNT_LIST.h"
#include "aion/loginserver/network/gameserver/clientpackets/CM_ACCOUNT_RECONNECT_KEY.h"
#include "aion/loginserver/network/gameserver/clientpackets/CM_BAN.h"
#include "aion/loginserver/network/gameserver/clientpackets/CM_CHANGE_ALLOWED_HDD_SERIAL.h"
#include "aion/loginserver/network/gameserver/clientpackets/CM_GS_AUTH.h"
#include "aion/loginserver/network/gameserver/clientpackets/CM_GS_CHARACTER.h"
#include "aion/loginserver/network/gameserver/clientpackets/CM_GS_PONG.h"
#include "aion/loginserver/network/gameserver/clientpackets/CM_HDDBAN_CONTROL.h"
#include "aion/loginserver/network/gameserver/clientpackets/CM_LS_CONTROL.h"
#include "aion/loginserver/network/gameserver/clientpackets/CM_MACBAN_CONTROL.h"
#include "aion/loginserver/network/gameserver/clientpackets/CM_PTRANSFER_CONTROL.h"

namespace aion::loginserver::network::factories::GsPacketHandlerFactory {

using gameserver::GsClientPacket;
using gameserver::GsConnection;
using State = gameserver::GsConnection::State;
using namespace gameserver::clientpackets;

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.network.factories.GsPacketHandlerFactory"));
	return *logger;
}

std::unique_ptr<GsClientPacket> create(State state, int32_t id) {
	switch (state) {
		case State::CONNECTED:
			switch (id) {
				case 0:
					return std::make_unique<CM_GS_AUTH>();
				default:
					return nullptr;
			}
		case State::AUTHED:
			switch (id) {
				case 1:
					return std::make_unique<CM_ACCOUNT_AUTH>();
				case 2:
					return std::make_unique<CM_ACCOUNT_RECONNECT_KEY>();
				case 3:
					return std::make_unique<CM_ACCOUNT_DISCONNECTED>();
				case 4:
					return std::make_unique<CM_ACCOUNT_LIST>();
				case 5:
					return std::make_unique<CM_LS_CONTROL>();
				case 6:
					return std::make_unique<CM_BAN>();
				case 7:
					return std::make_unique<CM_ACCOUNT_CONNECTION_INFO>();
				case 8:
					return std::make_unique<CM_GS_CHARACTER>();
				case 9:
					return std::make_unique<CM_MACBAN_CONTROL>();
				case 10:
					return std::make_unique<CM_HDDBAN_CONTROL>();
				case 11:
					return std::make_unique<CM_CHANGE_ALLOWED_HDD_SERIAL>();
				case 12:
					return std::make_unique<CM_GS_PONG>();
				case 13:
					return std::make_unique<CM_PTRANSFER_CONTROL>();
				default:
					return nullptr;
			}
	}
	return nullptr;
}

} // namespace

std::unique_ptr<GsClientPacket> handle(commons::utils::ByteBuffer& data, std::shared_ptr<GsConnection> client) {
	State state = client->getState();
	int32_t id = data.get() & 0xff;

	std::unique_ptr<GsClientPacket> msg = create(state, id);

	if (!msg) {
		log().warn(fmt::format("Unknown packet received from Game Server: 0x{:02X} state={}", id, gameserver::toString(state)));
	} else {
		msg->setConnection(std::move(client));
		msg->setBuffer(data);
	}

	return msg;
}

} // namespace aion::loginserver::network::factories::GsPacketHandlerFactory
