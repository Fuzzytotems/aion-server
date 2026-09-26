#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "aion/chatserver/model/ChannelType.h"
#include "aion/chatserver/model/Race.h"

namespace aion::chatserver::model::channel {
class Channel;
}

namespace aion::chatserver::network::netty::handler {
class ClientChannelHandler;
}

namespace aion::chatserver::model {

/**
 * A player registered by the game server (CM_PLAYER_AUTH) and, once its client connected and authenticated with the token, the client's
 * connection and channels.
 * <p>
 * <b>Threads.</b> The game server's packets, the client's own packets and the broadcasts of other clients use a ChatClient concurrently, so
 * the mutable state (identifier, channel handler, channels, last message times) is guarded by a mutex and the gag time is atomic (Java: plain
 * fields and unsynchronized lists inside a ConcurrentHashMap).
 * <p>
 * <b>Ownership.</b> ChatService, BroadcastService and the client's ClientChannelHandler hold the ChatClient; the ChatClient holds the handler
 * (Java: a reference cycle the garbage collector resolves). The handler drops its reference when it is disconnected, which breaks the cycle.
 * <p>
 * Java: com.aionemu.chatserver.model.ChatClient
 *
 * @author ATracer, Neon
 */
class ChatClient {
public:
	/**
	 * @param race
	 *          null (std::nullopt) if the game server sent an unknown race id
	 */
	ChatClient(int32_t clientId, std::vector<uint8_t> token, std::string accName, std::string nick, std::optional<Race> race, int8_t accessLevel);
	~ChatClient();

	ChatClient(const ChatClient&) = delete;
	ChatClient& operator=(const ChatClient&) = delete;

	int32_t getClientId() const noexcept { return clientId; }

	const std::vector<uint8_t>& getToken() const noexcept { return token; }

	const std::string& getAccountName() const noexcept { return accName; }

	const std::string& getName() const noexcept { return name; }

	std::optional<Race> getRace() const noexcept { return race; }

	int8_t getAccessLevel() const noexcept { return accessLevel; }

	/** @return the "name@identifier" bytes (UTF-16LE) the client authenticated with, null (std::nullopt) before that */
	std::optional<std::vector<uint8_t>> getIdentifier() const;

	void setIdentifier(std::vector<uint8_t> identifier);

	/** @return the client's connection, nullptr before the client authenticated */
	std::shared_ptr<network::netty::handler::ClientChannelHandler> getChannelHandler() const;

	void setChannelHandler(std::shared_ptr<network::netty::handler::ClientChannelHandler> channelHandler);

	/**
	 * Adds the channel to the client's channels. A client is in one channel per type, except for ChannelType::JOB: starting classes join both
	 * main class channels, so up to 2 job channels are kept (a third one replaces both).
	 */
	void addChannel(std::shared_ptr<channel::Channel> channel);

	/** @return true if the client was in the channel (compared by id); false for a null channel */
	bool removeChannel(const std::shared_ptr<channel::Channel>& channel);

	bool isInChannel(const channel::Channel& channel) const;

	/** @return time of the last message sent in a channel of this type (wall clock, ms), 0 if none */
	int64_t getLastMessageTime(ChannelType ct) const;

	void updateLastMessageTime(ChannelType ct);

	/** @return seconds until the client may chat again in a channel of this type (flood protection, implemented same as on client-side), 0 if now */
	int32_t nextMessageTimeSec(ChannelType ct) const;

	/** @return true if a gag time is set and did not pass yet (the gag time is compared as a point in time, see setGagTime) */
	bool isGagged() const;

	int64_t getGagTime() const noexcept { return gagTime.load(); }

	/** @param gagTime the time (wall clock, ms) until which the player may not chat, as sent by the game server (CM_PLAYER_GAG) */
	void setGagTime(int64_t gagTime) noexcept;

	/** @return "Player [name=&lt;name&gt;, id=&lt;id&gt;, race=&lt;race&gt;]" */
	std::string toString() const;

private:
	const int32_t clientId;
	const std::vector<uint8_t> token;
	const std::string accName;
	const std::string name;
	const std::optional<Race> race;
	const int8_t accessLevel;

	/** guards identifier, channelHandler, channels and lastMessageTime */
	mutable std::mutex mutex;
	std::optional<std::vector<uint8_t>> identifier;
	std::shared_ptr<network::netty::handler::ClientChannelHandler> channelHandler;
	std::atomic<int64_t> gagTime = 0;

	/**
	 * Map with all connected channels<br>
	 * Support for 2 channels of ChannelType.JOB, since starting classes join both main class channels
	 */
	std::map<ChannelType, std::vector<std::shared_ptr<channel::Channel>>> channels;
	std::map<ChannelType, int64_t> lastMessageTime;
};

inline std::string format_as(const ChatClient& client) {
	return client.toString();
}

} // namespace aion::chatserver::model
