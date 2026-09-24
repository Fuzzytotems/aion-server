#include "aion/chatserver/network/aion/AbstractClientPacket.h"

#include "aion/chatserver/network/netty/handler/ClientChannelHandler.h"
#include "aion/commons/utils/Exception.h"

namespace aion::chatserver::network::aion {

std::shared_ptr<model::ChatClient> AbstractClientPacket::getChatClient() const {
	std::shared_ptr<model::ChatClient> chatClient = clientChannelHandler->getChatClient();
	if (!chatClient)
		throw commons::utils::IllegalStateException(
			"Cannot invoke a method of the ChatClient because the return value of \"ClientChannelHandler.getChatClient()\" is null");
	return chatClient;
}

} // namespace aion::chatserver::network::aion
