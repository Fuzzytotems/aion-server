#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "aion/chatserver/model/channel/RaceChannel.h"

namespace aion::chatserver::model::channel {

/**
 * The trade channel of a map (client identifier "trade").
 * <p>
 * Java: com.aionemu.chatserver.model.channel.TradeChannel
 *
 * @author ATracer, Neon
 */
class TradeChannel : public RaceChannel {
public:
	TradeChannel(int32_t gameServerId, std::optional<Race> race, std::string_view mapIdentifier);

	const std::string& getMapIdentifier() const noexcept { return mapIdentifier; }

	bool matches(std::optional<ChannelType> channelType, int32_t gameServerId, std::optional<Race> race, std::string_view mapIdentifier) const override;

private:
	const std::string mapIdentifier;
};

} // namespace aion::chatserver::model::channel
