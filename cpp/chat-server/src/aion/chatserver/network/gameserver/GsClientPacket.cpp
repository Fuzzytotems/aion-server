#include "aion/chatserver/network/gameserver/GsClientPacket.h"

#include <utility>

#include "aion/commons/logging/LoggerFactory.h"

namespace aion::chatserver::network::gameserver {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.network.gameserver.GsClientPacket"));
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

} // namespace aion::chatserver::network::gameserver
