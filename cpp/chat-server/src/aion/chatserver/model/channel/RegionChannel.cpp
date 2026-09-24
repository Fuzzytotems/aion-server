#include "aion/chatserver/model/channel/RegionChannel.h"

namespace aion::chatserver::model::channel {

RegionChannel::RegionChannel(int32_t gameServerId, std::optional<Race> race, std::string_view value)
	: RaceChannel(ChannelType::REGION, gameServerId, race), mapIdentifier(value) {
}

// parameters renamed from the Java names, which would hide members (see RaceChannel::matches)
bool RegionChannel::matches(std::optional<ChannelType> type, int32_t gsId, std::optional<Race> requestedRace, std::string_view requestedMap) const {
	return RaceChannel::matches(type, gsId, requestedRace, requestedMap) && getMapIdentifier() == requestedMap;
}

} // namespace aion::chatserver::model::channel
