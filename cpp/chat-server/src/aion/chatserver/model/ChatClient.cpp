#include "aion/chatserver/model/ChatClient.h"

#include <algorithm>
#include <utility>

#include "aion/chatserver/model/channel/Channel.h"
#include "aion/chatserver/network/netty/handler/ClientChannelHandler.h"
#include "aion/commons/utils/TimeUtils.h"

namespace aion::chatserver::model {

using commons::utils::currentTimeMillis;

ChatClient::ChatClient(int32_t clientId, std::vector<uint8_t> token, std::string accName, std::string nick, std::optional<Race> race, int8_t accessLevel)
	: clientId(clientId), token(std::move(token)), accName(std::move(accName)), name(std::move(nick)), race(race), accessLevel(accessLevel) {
}

ChatClient::~ChatClient() = default;

std::optional<std::vector<uint8_t>> ChatClient::getIdentifier() const {
	std::lock_guard lock(mutex);
	return identifier;
}

void ChatClient::setIdentifier(std::vector<uint8_t> value) {
	std::lock_guard lock(mutex);
	identifier = std::move(value);
}

std::shared_ptr<network::netty::handler::ClientChannelHandler> ChatClient::getChannelHandler() const {
	std::lock_guard lock(mutex);
	return channelHandler;
}

void ChatClient::setChannelHandler(std::shared_ptr<network::netty::handler::ClientChannelHandler> value) {
	std::lock_guard lock(mutex);
	channelHandler = std::move(value);
}

void ChatClient::addChannel(std::shared_ptr<channel::Channel> channel) {
	std::lock_guard lock(mutex);
	auto channelsOfType = channels.find(channel->getChannelType());
	if (channelsOfType == channels.end()) {
		channelsOfType = channels.emplace(channel->getChannelType(), std::vector<std::shared_ptr<channel::Channel>>()).first;
	} else if (channel->getChannelType() != ChannelType::JOB || channelsOfType->second.size() == 2) {
		channelsOfType->second.clear();
	}
	channelsOfType->second.push_back(std::move(channel));
}

bool ChatClient::removeChannel(const std::shared_ptr<channel::Channel>& channel) {
	if (channel) {
		std::lock_guard lock(mutex);
		auto channelsOfType = channels.find(channel->getChannelType());
		if (channelsOfType != channels.end())
			return std::erase_if(channelsOfType->second, [&](const auto& ch) { return ch->getChannelId() == channel->getChannelId(); }) > 0;
	}
	return false;
}

bool ChatClient::isInChannel(const channel::Channel& channel) const {
	std::lock_guard lock(mutex);
	auto channelsOfType = channels.find(channel.getChannelType());
	return channelsOfType != channels.end() &&
				 std::ranges::any_of(channelsOfType->second, [&](const auto& ch) { return ch->getChannelId() == channel.getChannelId(); });
}

int64_t ChatClient::getLastMessageTime(ChannelType ct) const {
	std::lock_guard lock(mutex);
	auto time = lastMessageTime.find(ct);
	return time == lastMessageTime.end() ? 0 : time->second;
}

void ChatClient::updateLastMessageTime(ChannelType ct) {
	std::lock_guard lock(mutex);
	lastMessageTime[ct] = currentTimeMillis();
}

int32_t ChatClient::nextMessageTimeSec(ChannelType ct) const {
	int32_t delay = ct == ChannelType::LFG || ct == ChannelType::TRADE ? 30000 : 1000; // implemented same as on client-side
	int64_t floodProtectionTime = delay - (currentTimeMillis() - getLastMessageTime(ct));
	return floodProtectionTime <= 0 ? 0 : std::max(1, static_cast<int32_t>(floodProtectionTime / 1000));
}

bool ChatClient::isGagged() const {
	int64_t time = gagTime.load();
	return time > 0 && currentTimeMillis() < time;
}

void ChatClient::setGagTime(int64_t value) noexcept {
	gagTime.store(value);
}

std::string ChatClient::toString() const {
	return "Player [name=" + name + ", id=" + std::to_string(clientId) + ", race=" + model::toString(race) + "]";
}

} // namespace aion::chatserver::model
