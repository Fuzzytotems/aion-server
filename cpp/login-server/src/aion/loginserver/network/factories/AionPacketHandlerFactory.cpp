#include "aion/loginserver/network/factories/AionPacketHandlerFactory.h"

#include <fmt/format.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/loginserver/network/aion/clientpackets/CM_AUTH_GG.h"
#include "aion/loginserver/network/aion/clientpackets/CM_LOGIN.h"
#include "aion/loginserver/network/aion/clientpackets/CM_PLAY.h"
#include "aion/loginserver/network/aion/clientpackets/CM_SERVER_LIST.h"
#include "aion/loginserver/network/aion/clientpackets/CM_UPDATE_SESSION.h"

namespace aion::loginserver::network::factories::AionPacketHandlerFactory {

using aion::AionClientPacket;
using aion::LoginConnection;
using State = aion::LoginConnection::State;
using namespace aion::clientpackets;

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.network.factories.AionPacketHandlerFactory"));
	return *logger;
}

/** Logs an unknown packet */
void unknownPacket(int32_t opCode, State state, commons::utils::ByteBuffer& buf) {
	int32_t length = buf.remaining();
	std::string sb;
	sb.reserve(static_cast<size_t>(length) * 3);
	while (buf.hasRemaining())
		fmt::format_to(std::back_inserter(sb), "{:02X} ", static_cast<uint8_t>(buf.get()));
	while (!sb.empty() && sb.back() == ' ')
		sb.pop_back();
	log().warn(fmt::format("Unknown packet received from client: opCode=0x{:02X} state={} length={} data=[{}]", opCode, aion::toString(state), length, sb));
}

} // namespace

std::unique_ptr<AionClientPacket> handle(commons::utils::ByteBuffer& data, std::shared_ptr<LoginConnection> client) {
	std::unique_ptr<AionClientPacket> msg;
	State state = client->getState();
	int32_t opCode = data.get() & 0xFF;

	switch (state) {
		case State::CONNECTED:
			switch (opCode) {
				case 0x07:
					msg = std::make_unique<CM_AUTH_GG>(data, std::move(client), opCode);
					break;
				case 0x08:
					msg = std::make_unique<CM_UPDATE_SESSION>(data, std::move(client), opCode);
					break;
				default:
					unknownPacket(opCode, state, data);
			}
			break;
		case State::AUTHED_GG:
			switch (opCode) {
				case 0x00:
					msg = std::make_unique<CM_LOGIN>(data, std::move(client), opCode);
					break;
				default:
					unknownPacket(opCode, state, data);
			}
			break;
		case State::AUTHED_LOGIN:
			switch (opCode) {
				case 0x05:
					msg = std::make_unique<CM_SERVER_LIST>(data, std::move(client), opCode);
					break;
				case 0x02:
					msg = std::make_unique<CM_PLAY>(data, std::move(client), opCode);
					break;
				default:
					unknownPacket(opCode, state, data);
			}
			break;
	}

	return msg;
}

} // namespace aion::loginserver::network::factories::AionPacketHandlerFactory
