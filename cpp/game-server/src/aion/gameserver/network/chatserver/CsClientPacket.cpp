#include "aion/gameserver/network/chatserver/CsClientPacket.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/network/chatserver/ChatServerConnection.h"
#include "aion/gameserver/network/chatserver/CsServerPacket.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::chatserver {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.chatserver.CsClientPacket");

CsClientPacket::CsClientPacket(int32_t opcode) : BaseClientPacket(opcode) {
}

void CsClientPacket::run() {
	try {
		runImpl();
	} catch (...) {
		log.warnCurrentException("error handling ls (" + (getConnection() ? getConnection()->getIP() : std::string("null")) + ") message " + toString());
	}
}

void CsClientPacket::sendPacket(std::shared_ptr<CsServerPacket> msg) {
	getConnection()->sendPacket(std::move(msg));
}

std::unique_ptr<CsClientPacket> CsClientPacket::clonePacket() {
	return nullptr; // Deviation: see the declaration (no caller)
}

} // namespace aion::gameserver::network::chatserver
