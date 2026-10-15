#include "aion/loginserver/network/gameserver/GsClientPacket.h"

#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/loginserver/GameServerInfo.h"

namespace aion::loginserver::network::gameserver {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.network.gameserver.GsClientPacket"));
	return *logger;
}

} // namespace

void GsClientPacket::run() {
	try {
		runImpl();
	} catch (...) {
		try {
			log().warnCurrentException("error handling gs (" + getConnection()->getIP() + ") message " + toString());
		} catch (...) {
			// logging must not throw out of the packet processor
		}
	}
}

void GsClientPacket::sendPacket(std::shared_ptr<GsServerPacket> msg) const {
	getConnection()->sendPacket(std::move(msg));
}

std::shared_ptr<GameServerInfo> GsClientPacket::getGameServerInfo() const {
	std::shared_ptr<GameServerInfo> gsi = getConnection()->getGameServerInfo();
	if (!gsi)
		throw commons::utils::IllegalStateException(getConnection()->toString() + " has no GameServerInfo");
	return gsi;
}

} // namespace aion::loginserver::network::gameserver
