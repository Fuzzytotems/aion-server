#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/chatserver/model/channel/RaceChannel.h"

namespace aion::chatserver::model::channel {

/**
 * The channel of a class (client identifier "job"). Clients of different languages name the class differently, so a channel matches all
 * localized names of its class.
 * <p>
 * Java: com.aionemu.chatserver.model.channel.JobChannel
 *
 * @author ATracer, Neon
 */
class JobChannel : public RaceChannel {
public:
	/**
	 * @param classIdentifier
	 *          the class name sent by the client; a "[f:" suffix is cut off before the aliases are looked up
	 * @throws commons::utils::IndexOutOfBoundsException if nothing is left of the class name (e.g. "[f:"; Java:
	 *           ArrayIndexOutOfBoundsException), after the channel id was taken from IdFactory
	 */
	JobChannel(int32_t gameServerId, std::optional<Race> race, std::string_view classIdentifier);

	/** @return true if the class name is one of the known classes */
	bool hasAliases() const noexcept { return classIdentifiers.size() > 1; }

	/** Note that like in Java the class name of a request is compared as it is, including a "[f:" suffix. */
	bool matches(std::optional<ChannelType> channelType, int32_t gameServerId, std::optional<Race> race, std::string_view classIdentifier) const override;

	/** @return the first alias and the race initial, e.g. "Gladiator (E)" */
	std::string name() const override;

private:
	/** Java: withAliases(classIdentifier) - the alias set containing the name (in list order), or only the name itself */
	static std::vector<std::string> withAliases(std::string_view classIdentifier);

	/** Java: Set&lt;String&gt; (a LinkedHashSet or a singleton), kept in insertion order */
	const std::vector<std::string> classIdentifiers;
};

} // namespace aion::chatserver::model::channel
