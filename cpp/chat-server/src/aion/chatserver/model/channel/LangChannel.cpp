#include "aion/chatserver/model/channel/LangChannel.h"

namespace aion::chatserver::model::channel {

LangChannel::LangChannel(int32_t gameServerId, std::optional<Race> race, std::string_view value)
	: RaceChannel(ChannelType::LANG, gameServerId, race), language(value) {
}

// parameters renamed from the Java names, which would hide members (see RaceChannel::matches)
bool LangChannel::matches(std::optional<ChannelType> type, int32_t gsId, std::optional<Race> requestedRace, std::string_view requestedLanguage) const {
	return RaceChannel::matches(type, gsId, requestedRace, requestedLanguage) && getLanguage() == requestedLanguage;
}

std::string LangChannel::name() const {
	return std::string(model::name(getChannelType())) + ": " + language + " (" + raceInitial() + ")";
}

} // namespace aion::chatserver::model::channel
