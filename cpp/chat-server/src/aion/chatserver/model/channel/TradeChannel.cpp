#include "aion/chatserver/model/channel/TradeChannel.h"

namespace aion::chatserver::model::channel {

TradeChannel::TradeChannel(int32_t gameServerId, std::optional<Race> race, std::string_view value)
	: RaceChannel(ChannelType::TRADE, gameServerId, race), mapIdentifier(value) {
}

// parameters renamed from the Java names, which would hide members (see RaceChannel::matches)
bool TradeChannel::matches(std::optional<ChannelType> type, int32_t gsId, std::optional<Race> requestedRace, std::string_view requestedMap) const {
	return getMapIdentifier() == requestedMap && RaceChannel::matches(type, gsId, requestedRace, requestedMap);
}

} // namespace aion::chatserver::model::channel
