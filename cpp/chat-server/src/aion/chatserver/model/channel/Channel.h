#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aion/chatserver/model/ChannelType.h"
#include "aion/chatserver/model/Race.h"

namespace aion::chatserver::model::channel {

/**
 * A chat channel. Channels are created by ChatChannels, shared through std::shared_ptr (by ChatChannels, the clients in them and the messages
 * sent to them) and never removed, like in Java. They are immutable, so any thread may use them.
 * <p>
 * Java: com.aionemu.chatserver.model.channel.Channel
 *
 * @author ATracer, Neon
 */
class Channel {
public:
	virtual ~Channel() = default;

	Channel(const Channel&) = delete;
	Channel& operator=(const Channel&) = delete;

	ChannelType getChannelType() const noexcept { return channelType; }

	int32_t getGameServerId() const noexcept { return gameServerId; }

	/** @return The unique id of this channel. */
	int32_t getChannelId() const noexcept { return channelId; }

	/**
	 * @param channelType
	 *          null (std::nullopt) for an unknown type identifier, which matches no channel
	 * @param race
	 *          null (std::nullopt) for an unknown race id
	 * @return True, if the channel matches the specified criteria. Used to determine if a clients request matches an existing channel or we need
	 *         to create a new one.
	 */
	virtual bool matches(std::optional<ChannelType> channelType, int32_t gameServerId, std::optional<Race> race, std::string_view channelMeta) const = 0;

	/**
	 * @return The name of this channel (mainly for logging purposes).
	 * @throws commons::utils::IllegalStateException if the name contains the race initial and the race is null (Java: NullPointerException)
	 */
	virtual std::string name() const = 0;

	/**
	 * Java: Object.toString() - "&lt;class name&gt;@&lt;hex&gt;", used in log messages. The identity hash code is replaced by the channel id
	 * (hexadecimal).
	 */
	std::string toString() const;

protected:
	/** Takes the next id from IdFactory (also when the constructor of the subclass throws afterwards, like Java). */
	Channel(ChannelType channelType, int32_t gameServerId);

private:
	const ChannelType channelType;
	const int32_t gameServerId;
	const int32_t channelId;
};

} // namespace aion::chatserver::model::channel
