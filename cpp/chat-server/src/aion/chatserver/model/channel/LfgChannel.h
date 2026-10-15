#pragma once

#include <optional>

#include "aion/chatserver/model/channel/RaceChannel.h"

namespace aion::chatserver::model::channel {

/**
 * The looking-for-group channel (client identifier "partyFind"): one per race and game server, the channel meta is ignored.
 * <p>
 * Java: com.aionemu.chatserver.model.channel.LfgChannel
 *
 * @author ATracer, Neon
 */
class LfgChannel : public RaceChannel {
public:
	LfgChannel(int32_t gameServerId, std::optional<Race> race) : RaceChannel(ChannelType::LFG, gameServerId, race) {}
};

} // namespace aion::chatserver::model::channel
