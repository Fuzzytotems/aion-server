#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "aion/chatserver/model/channel/RaceChannel.h"

namespace aion::chatserver::model::channel {

/**
 * A language channel (client identifier "User", which private user channels use as well).
 * <p>
 * Java: com.aionemu.chatserver.model.channel.LangChannel
 *
 * @author Unknown, Neon
 */
class LangChannel : public RaceChannel {
public:
	LangChannel(int32_t gameServerId, std::optional<Race> race, std::string_view language);

	const std::string& getLanguage() const noexcept { return language; }

	bool matches(std::optional<ChannelType> channelType, int32_t gameServerId, std::optional<Race> race, std::string_view language) const override;

	/** @return "LANG: &lt;language&gt; (&lt;race initial&gt;)" */
	std::string name() const override;

private:
	const std::string language;
};

} // namespace aion::chatserver::model::channel
