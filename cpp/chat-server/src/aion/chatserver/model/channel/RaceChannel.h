#pragma once

#include <optional>
#include <string>

#include "aion/chatserver/model/channel/Channel.h"

namespace aion::chatserver::model::channel {

/**
 * A channel restricted to one race of one game server.
 * <p>
 * Java: com.aionemu.chatserver.model.channel.RaceChannel
 *
 * @author ATracer, Neon
 */
class RaceChannel : public Channel {
public:
	/** @return the race, null (std::nullopt) if a staff member requested a channel of an unknown race id */
	std::optional<Race> getRace() const noexcept { return race; }

	bool matches(std::optional<ChannelType> channelType, int32_t gameServerId, std::optional<Race> race, std::string_view channelMeta) const override;

	/** @return "&lt;TYPE&gt; (&lt;race initial&gt;)", e.g. "REGION (E)" */
	std::string name() const override;

protected:
	RaceChannel(ChannelType channelType, int32_t gameServerId, std::optional<Race> race);

	/**
	 * Java: getRace().name().charAt(0)
	 * @throws commons::utils::IllegalStateException if the race is null (Java: NullPointerException)
	 */
	char raceInitial() const;

private:
	const std::optional<Race> race;
};

} // namespace aion::chatserver::model::channel
