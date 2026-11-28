#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

namespace aion::chatserver::model {
class ChatClient;
}

namespace aion::chatserver::model::channel {

class Channel;

/**
 * All channels, by id. Channels are created on the first request and never removed. Thread safe.
 * <p>
 * Java: com.aionemu.chatserver.model.channel.ChatChannels
 *
 * @author ATracer
 */
namespace ChatChannels {

/**
 * @param channelId
 *          the channelId of the requesting Channel
 * @return Channel with this channelId or null if no channel with this id exists (logged if LoggingConfig::LOG_CHANNEL_INVALID).
 */
std::shared_ptr<Channel> getChannelById(int32_t channelId);

/**
 * Parses a channel request of a client, e.g. "@&lt;U+0001&gt;trade_Housing_barrack&lt;U+0001&gt;1.0.AION.KOR": the channel type ("trade") and the
 * channel meta ("Housing_barrack", the map, class or language), the game server id (1) and the race id (0). A client may only request channels
 * of its own race, unless it has an access level.
 * <p>
 * Deviation: finding and creating the channel is one atomic step. In Java two clients requesting a new channel at the same time can create two
 * channels for it, each client then chatting in its own one.
 *
 * @param client
 *          the requesting client; like in Java it is only used (and must not be null) once the request was split into its three parts
 * @param identifier
 *          - the identifier of the requested channel, e.g. @trade_Housing_barrack1.0.AION.KOR
 * @return Channel with this identifier or creates and returns a new channel if no channel with such identifier exists.<br>
 *         Null if the identifier does not have three parts, has no channel name, an unknown type, a name longer than 64 characters, malformed
 *         restrictions, the game server 99 or another one than the registered game server, an unknown race, if the client requested a
 *         channel of another race, or if a new channel would exceed 256 channels (upstream b0317f967; all but the first are logged).
 * @throws commons::utils::IllegalStateException if client is null (Java: NullPointerException)
 */
std::shared_ptr<Channel> getOrCreate(const std::shared_ptr<ChatClient>& client, std::string_view identifier);

} // namespace ChatChannels

} // namespace aion::chatserver::model::channel
