#include "aion/chatserver/model/channel/RaceChannel.h"

#include "aion/commons/utils/Exception.h"

namespace aion::chatserver::model::channel {

RaceChannel::RaceChannel(ChannelType channelType, int32_t gameServerId, std::optional<Race> value) : Channel(channelType, gameServerId), race(value) {
}

// parameters renamed from the Java names (channelType, gameServerId, race), which would hide members (C4458)
bool RaceChannel::matches(std::optional<ChannelType> type, int32_t gsId, std::optional<Race> requestedRace, std::string_view channelMeta) const {
	return requestedRace == getRace() && type == getChannelType() && gsId == getGameServerId();
}

std::string RaceChannel::name() const {
	return std::string(model::name(getChannelType())) + " (" + raceInitial() + ")";
}

char RaceChannel::raceInitial() const {
	if (!race)
		throw commons::utils::IllegalStateException("Cannot invoke \"com.aionemu.chatserver.model.Race.name()\" because the return value of "
																								"\"com.aionemu.chatserver.model.channel.RaceChannel.getRace()\" is null");
	return model::name(*race)[0];
}

} // namespace aion::chatserver::model::channel
