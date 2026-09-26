#include "aion/chatserver/model/channel/ChatChannels.h"

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "aion/chatserver/configs/main/LoggingConfig.h"
#include "aion/chatserver/model/ChannelType.h"
#include "aion/chatserver/model/ChatClient.h"
#include "aion/chatserver/model/Race.h"
#include "aion/chatserver/model/channel/JobChannel.h"
#include "aion/chatserver/model/channel/LangChannel.h"
#include "aion/chatserver/model/channel/LfgChannel.h"
#include "aion/chatserver/model/channel/RegionChannel.h"
#include "aion/chatserver/model/channel/TradeChannel.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::chatserver::model::channel::ChatChannels {

using configs::main::LoggingConfig;

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.model.channel.ChatChannels"));
	return *logger;
}

struct State {
	std::mutex mutex;
	/** Java: ConcurrentHashMap&lt;Integer, Channel&gt;; iterated in id order */
	std::map<int32_t, std::shared_ptr<Channel>> channels;
};

State& state() {
	static auto* s = new State(); // leaked: clients and messages keep channels, and may do so while static objects are destroyed
	return *s;
}

/** Java: array[index] */
const std::string& at(const std::vector<std::string>& array, size_t index) {
	if (index >= array.size())
		throw commons::utils::IndexOutOfBoundsException(fmt::format("Index {} out of bounds for length {}", index, array.size()));
	return array[index];
}

/** Java: s.split("_", 2) - at most two parts, the second one keeps further separators, empty parts are kept */
std::vector<std::string> splitTypeAndMeta(std::string_view s) {
	size_t separator = s.find('_');
	if (separator == std::string_view::npos)
		return {std::string(s)};
	return {std::string(s.substr(0, separator)), std::string(s.substr(separator + 1))};
}

void requireClient(const std::shared_ptr<ChatClient>& client) {
	if (!client)
		throw commons::utils::IllegalStateException("Cannot invoke \"com.aionemu.chatserver.model.ChatClient.getRace()\" because \"client\" is null");
}

/** Java: addChannel. The caller holds the mutex. */
std::shared_ptr<Channel> addChannel(State& s, std::optional<ChannelType> ct, int32_t gameServerId, std::optional<Race> race, std::string_view channelMeta) {
	if (!ct) // Java: the switch on a null enum throws a NullPointerException
		throw commons::utils::IllegalStateException("Cannot invoke \"com.aionemu.chatserver.model.ChannelType.ordinal()\" because \"ct\" is null");
	std::shared_ptr<Channel> channel;
	switch (*ct) {
		case ChannelType::REGION:
			channel = std::make_shared<RegionChannel>(gameServerId, race, channelMeta);
			break;
		case ChannelType::TRADE:
			channel = std::make_shared<TradeChannel>(gameServerId, race, channelMeta);
			break;
		case ChannelType::LFG:
			channel = std::make_shared<LfgChannel>(gameServerId, race);
			break;
		case ChannelType::JOB:
			channel = std::make_shared<JobChannel>(gameServerId, race, channelMeta);
			break;
		case ChannelType::LANG:
			channel = std::make_shared<LangChannel>(gameServerId, race, channelMeta);
			break;
	}
	s.channels.insert_or_assign(channel->getChannelId(), channel);
	return channel;
}

} // namespace

std::shared_ptr<Channel> getChannelById(int32_t channelId) {
	State& s = state();
	std::shared_ptr<Channel> channel;
	{
		std::lock_guard lock(s.mutex);
		auto found = s.channels.find(channelId);
		if (found != s.channels.end())
			channel = found->second;
	}
	if (!channel && LoggingConfig::LOG_CHANNEL_INVALID)
		log().warn("No registered channel with id {}", channelId);
	return channel;
}

std::shared_ptr<Channel> getOrCreate(const std::shared_ptr<ChatClient>& client, std::string_view identifier) {
	if (LoggingConfig::LOG_CHANNEL_REQUEST)
		log().info("{} requested channel: {}", client ? client->toString() : "null", identifier);

	std::vector<std::string> parts = commons::utils::StringUtils::splitJava(identifier, "\x01"); // { @, trade_Housing_barrack, 1.0.AION.KOR }
	if (parts.size() != 3)
		return nullptr;

	std::vector<std::string> channelType = splitTypeAndMeta(parts[1]); // { trade, Housing_barrack }
	std::vector<std::string> channelRestrictions = commons::utils::StringUtils::splitJava(parts[2], "."); // { 1, 0, AION, KOR }

	std::optional<ChannelType> ct = getByIdentifier(at(channelType, 0));
	const std::string& channelMeta = at(channelType, 1);
	int32_t gameServerId = commons::utils::parseInt(at(channelRestrictions, 0));
	std::optional<Race> race = getById(commons::utils::parseInt(at(channelRestrictions, 1)));
	requireClient(client);
	if (client->getRace() != race && client->getAccessLevel() == 0) {
		log().warn("{} requested channel of race: {}", client->toString(), toString(race));
		return nullptr;
	}

	std::shared_ptr<Channel> channel;
	{
		State& s = state();
		std::lock_guard lock(s.mutex);
		for (const auto& [id, existing] : s.channels) {
			if (existing->matches(ct, gameServerId, race, channelMeta))
				return existing;
		}
		channel = addChannel(s, ct, gameServerId, race, channelMeta);
	}
	if (auto* jobChannel = dynamic_cast<JobChannel*>(channel.get()); jobChannel && !jobChannel->hasAliases())
		log().warn("{} requested channel for unknown class: {}", client->toString(), channelMeta);
	return channel;
}

} // namespace aion::chatserver::model::channel::ChatChannels
