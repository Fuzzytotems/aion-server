#include "aion/gameserver/network/loginserver/LsClientPacket.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/network/loginserver/LoginServerConnection.h"
#include "aion/gameserver/network/loginserver/LsServerPacket.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::loginserver {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.loginserver.LsClientPacket");

LsClientPacket::LsClientPacket(int32_t opcode) : BaseClientPacket(opcode) {
}

void LsClientPacket::run() {
	try {
		runImpl();
	} catch (...) {
		log.errorCurrentException("Error handling LS packet from " + (getConnection() ? getConnection()->getIP() : std::string("null")) + ": " + toString());
	}
}

void LsClientPacket::sendPacket(std::shared_ptr<LsServerPacket> msg) {
	getConnection()->sendPacket(std::move(msg));
}

} // namespace aion::gameserver::network::loginserver
