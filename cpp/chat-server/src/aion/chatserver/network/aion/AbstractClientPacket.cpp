#include "aion/chatserver/network/aion/AbstractClientPacket.h"

#include <utility>

#include "aion/chatserver/network/aion/AionConnection.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"

namespace aion::chatserver::network::aion {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.network.aion.AbstractClientPacket"));
	return *logger;
}

} // namespace

AbstractClientPacket::AbstractClientPacket(commons::utils::ByteBuffer buf, std::shared_ptr<AionConnection> connection, int32_t opCode)
	: BaseClientPacket(std::move(buf), opCode) {
	setConnection(std::move(connection));
}

void AbstractClientPacket::run() {
	try {
		runImpl();
	} catch (...) {
		try {
			log().errorCurrentException("Could not execute " + toString() + " from " + connectionToString());
		} catch (...) {
		}
	}
}

std::shared_ptr<model::ChatClient> AbstractClientPacket::getChatClient() const {
	std::shared_ptr<model::ChatClient> chatClient = getConnection()->getChatClient();
	if (!chatClient)
		throw commons::utils::IllegalStateException(
			"Cannot invoke a method of the ChatClient because the return value of \"AionConnection.getChatClient()\" is null");
	return chatClient;
}

} // namespace aion::chatserver::network::aion
