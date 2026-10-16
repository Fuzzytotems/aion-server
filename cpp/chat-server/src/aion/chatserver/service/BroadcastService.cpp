#include "aion/chatserver/service/BroadcastService.h"

#include <utility>
#include <vector>

#include "aion/chatserver/model/ChatClient.h"
#include "aion/chatserver/model/channel/Channel.h"
#include "aion/chatserver/model/message/Message.h"
#include "aion/chatserver/network/aion/serverpackets/SM_CHANNEL_MESSAGE.h"
#include "aion/chatserver/network/netty/handler/ClientChannelHandler.h"
#include "aion/commons/utils/Exception.h"

namespace aion::chatserver::service {

BroadcastService& BroadcastService::getInstance() {
	static auto* instance = new BroadcastService(); // leaked: packets may run while static objects are destroyed
	return *instance;
}

void BroadcastService::addClient(std::shared_ptr<model::ChatClient> client) {
	std::lock_guard lock(mutex);
	int32_t clientId = client->getClientId();
	clients.insert_or_assign(clientId, std::move(client));
}

void BroadcastService::removeClient(const model::ChatClient& client) {
	std::lock_guard lock(mutex);
	clients.erase(client.getClientId());
}

void BroadcastService::broadcastMessage(const model::message::Message& message) {
	std::vector<std::shared_ptr<model::ChatClient>> recipients;
	{
		std::lock_guard lock(mutex);
		recipients.reserve(clients.size());
		for (const auto& [id, client] : clients)
			recipients.push_back(client);
	}
	for (const std::shared_ptr<model::ChatClient>& client : recipients) {
		if (client->isInChannel(*message.getChannel()))
			sendMessage(*client, message);
	}
}

void BroadcastService::sendMessage(const model::ChatClient& chatClient, const model::message::Message& message) {
	std::shared_ptr<network::netty::handler::ClientChannelHandler> channelHandler = chatClient.getChannelHandler();
	if (!channelHandler)
		throw commons::utils::IllegalStateException("Cannot invoke \"ClientChannelHandler.sendPacket(...)\" because the return value of "
																								"\"com.aionemu.chatserver.model.ChatClient.getChannelHandler()\" is null");
	channelHandler->sendPacket(network::aion::serverpackets::SM_CHANNEL_MESSAGE(message));
}

} // namespace aion::chatserver::service
