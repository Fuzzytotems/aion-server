#include "aion/chatserver/service/ChatService.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include <openssl/evp.h>

#include "aion/chatserver/model/ChatClient.h"
#include "aion/chatserver/model/channel/Channel.h"
#include "aion/chatserver/model/channel/ChatChannels.h"
#include "aion/chatserver/network/aion/serverpackets/SM_CHANNEL_RESPONSE.h"
#include "aion/chatserver/network/aion/serverpackets/SM_PLAYER_AUTH_RESPONSE.h"
#include "aion/chatserver/network/netty/handler/ClientChannelHandler.h"
#include "aion/chatserver/service/BroadcastService.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::chatserver::service {

using model::ChatClient;
using network::netty::handler::ClientChannelHandler;
using ClientChannelHandlerState = ClientChannelHandler::ClientChannelHandlerState;

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.service.ChatService"));
	return *logger;
}

/** Java: MessageDigest.getInstance("SHA-256").digest() of the given bytes */
std::vector<uint8_t> sha256(std::string_view data) {
	std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
	unsigned int digestLength = 0;
	if (EVP_Digest(data.data(), data.size(), digest.data(), &digestLength, EVP_sha256(), nullptr) != 1)
		throw NoSuchAlgorithmException("SHA-256 MessageDigest not available");
	return std::vector<uint8_t>(digest.begin(), digest.begin() + digestLength);
}

/** Java: clientChannelHandler.getChatClient(), which must not be null here */
std::shared_ptr<ChatClient> requireChatClient(const ClientChannelHandler& clientChannelHandler) {
	std::shared_ptr<ChatClient> chatClient = clientChannelHandler.getChatClient();
	if (!chatClient)
		throw commons::utils::IllegalStateException(
			"Cannot invoke \"com.aionemu.chatserver.model.ChatClient.addChannel(...)\" because the return value of \"getChatClient()\" is null");
	return chatClient;
}

} // namespace

ChatService& ChatService::getInstance() {
	static auto* instance = new ChatService(); // leaked: packets may run while static objects are destroyed
	return *instance;
}

std::shared_ptr<ChatClient> ChatService::registerPlayer(int32_t playerId, std::string_view accName, std::string_view nick, std::optional<model::Race> race,
	int8_t accessLevel) {
	// Java: md.update(accName.getBytes(StandardCharsets.UTF_8), 0, accName.length()) - the UTF-16 length used as a byte count
	size_t hashedBytes = std::min(accName.size(), static_cast<size_t>(commons::utils::StringUtils::utf16Length(accName)));
	std::vector<uint8_t> accountToken = sha256(accName.substr(0, hashedBytes));
	std::vector<uint8_t> token = generateToken(accountToken);
	auto chatClient = std::make_shared<ChatClient>(playerId, std::move(token), std::string(accName), std::string(nick), race, accessLevel);
	std::lock_guard lock(mutex);
	players.insert_or_assign(playerId, chatClient);
	return chatClient;
}

std::vector<uint8_t> ChatService::generateToken(std::span<const uint8_t> accountToken) const {
	std::vector<uint8_t> dynamicToken(16);
	commons::utils::Rnd::nextBytes(dynamicToken);
	std::vector<uint8_t> token(48);
	for (size_t i = 0; i < token.size(); i++) {
		if (i < 16)
			token[i] = dynamicToken[i];
		else
			token[i] = accountToken[i - 16];
	}
	return token;
}

void ChatService::registerPlayerConnection(int32_t playerId, std::span<const uint8_t> token, std::vector<uint8_t> identifier, std::string_view name,
	std::string_view accName, const std::shared_ptr<ClientChannelHandler>& channelHandler) {
	std::shared_ptr<ChatClient> chatClient;
	{
		std::lock_guard lock(mutex);
		auto found = players.find(playerId);
		if (found != players.end())
			chatClient = found->second;
	}
	if (!chatClient)
		log().warn("Client tried to connect but was not yet registered from game server side");
	else if (!std::ranges::equal(chatClient->getToken(), token))
		log().warn("Client tried to connect but given token doesn't match");
	else if (!commons::utils::StringUtils::equalsIgnoreCase(chatClient->getAccountName(), accName)) // client sends accName lowercase
		log().warn("Client tried to connect with account name: {} (expected: {})", accName, chatClient->getAccountName());
	else if (chatClient->getName() != name)
		log().warn("Client tried to connect with character name: {} (expected: {})", name, chatClient->getName());
	else {
		chatClient->setIdentifier(std::move(identifier));
		chatClient->setChannelHandler(channelHandler);
		channelHandler->sendPacket(network::aion::serverpackets::SM_PLAYER_AUTH_RESPONSE());
		channelHandler->setState(ClientChannelHandlerState::AUTHED);
		channelHandler->setChatClient(chatClient);
		BroadcastService::getInstance().addClient(chatClient);
	}
}

void ChatService::registerPlayerWithChannel(const std::shared_ptr<ClientChannelHandler>& clientChannelHandler, int32_t channelRequestId,
	std::string_view identifier) {
	std::shared_ptr<model::channel::Channel> channel = model::channel::ChatChannels::getOrCreate(clientChannelHandler->getChatClient(), identifier);
	if (channel) {
		requireChatClient(*clientChannelHandler)->addChannel(channel);
		clientChannelHandler->sendPacket(network::aion::serverpackets::SM_CHANNEL_RESPONSE(*channel, channelRequestId));
	}
}

void ChatService::playerLogout(int32_t playerId) {
	std::shared_ptr<ChatClient> chatClient;
	{
		std::lock_guard lock(mutex);
		auto found = players.find(playerId);
		if (found != players.end()) {
			chatClient = std::move(found->second);
			players.erase(found);
		}
	}
	if (chatClient) {
		BroadcastService::getInstance().removeClient(*chatClient);
		log().info("Player[id={}] logged out ", playerId);
		if (std::shared_ptr<ClientChannelHandler> channelHandler = chatClient->getChannelHandler())
			channelHandler->close();
		else
			log().warn("Received logout event without client authentication for player {}", playerId);
	}
}

void ChatService::gagPlayer(int32_t playerId, int64_t gagTimeMillis) {
	std::shared_ptr<ChatClient> client;
	{
		std::lock_guard lock(mutex);
		auto found = players.find(playerId);
		if (found != players.end())
			client = found->second;
	}
	if (client) {
		client->setGagTime(gagTimeMillis);
		log().info("Player[id={}] was gagged for {} minutes", playerId, gagTimeMillis / 60000);
	}
}

} // namespace aion::chatserver::service
