#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "aion/chatserver/model/Race.h"
#include "aion/commons/utils/Exception.h"

namespace aion::chatserver::model {
class ChatClient;
}

namespace aion::chatserver::network::netty::handler {
class ClientChannelHandler;
}

namespace aion::chatserver::service {

/** Java: java.security.NoSuchAlgorithmException - SHA-256 is not available (OpenSSL failed) */
class NoSuchAlgorithmException : public commons::utils::Exception {
public:
	using Exception::Exception;
};

/**
 * The registered players: the game server registers them (with a token), their clients authenticate with the token, join channels and are
 * logged out by the game server. Thread safe.
 * <p>
 * Java: com.aionemu.chatserver.service.ChatService
 *
 * @author ATracer
 */
class ChatService {
public:
	static ChatService& getInstance();

	/**
	 * Registers a player for the game server (replacing a previous registration with the same id). The token is 16 random bytes followed by the
	 * SHA-256 digest of the account name's first accName.length() UTF-8 bytes (Java counts the UTF-16 length, so a name with non-ASCII
	 * characters is hashed only partly, like in Java).
	 *
	 * @throws NoSuchAlgorithmException if SHA-256 is not available
	 */
	std::shared_ptr<model::ChatClient> registerPlayer(int32_t playerId, std::string_view accName, std::string_view nick, std::optional<model::Race> race,
		int8_t accessLevel);

	/**
	 * A client authenticates: if the player was registered and token, account name (ignoring case, the client sends it in lower case) and
	 * character name match, the client gets SM_PLAYER_AUTH_RESPONSE, its connection is AUTHED and it receives the messages of its channels from now
	 * on. Otherwise a warning is logged and nothing is sent.
	 */
	void registerPlayerConnection(int32_t playerId, std::span<const uint8_t> token, std::vector<uint8_t> identifier, std::string_view name,
		std::string_view accName, const std::shared_ptr<network::netty::handler::ClientChannelHandler>& channelHandler);

	/** A client requests a channel (see ChatChannels::getOrCreate): if there is one, the client joins it and gets SM_CHANNEL_RESPONSE. */
	void registerPlayerWithChannel(const std::shared_ptr<network::netty::handler::ClientChannelHandler>& clientChannelHandler, int32_t channelRequestId,
		std::string_view identifier);

	/** The game server logged the player out: the player is removed and its client connection closed. Unknown ids are ignored. */
	void playerLogout(int32_t playerId);

	/** Sets the gag time of the player (see ChatClient::setGagTime). Unknown ids are ignored. */
	void gagPlayer(int32_t playerId, int64_t gagTimeMillis);

private:
	ChatService() = default;

	std::vector<uint8_t> generateToken(std::span<const uint8_t> accountToken) const;

	/** guards players */
	std::mutex mutex;
	std::unordered_map<int32_t, std::shared_ptr<model::ChatClient>> players;
};

} // namespace aion::chatserver::service
