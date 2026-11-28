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
#include "aion/chatserver/service/GameServerService.h"

namespace aion::chatserver::model::channel::ChatChannels {

using configs::main::LoggingConfig;

namespace {

/** A channel name is a map, class or language name, so 64 characters is generous. */
constexpr size_t MAX_META_LENGTH = 64;
/** Channels without members are removed, and a real server needs a few dozen at once. */
constexpr size_t MAX_CHANNELS = 256;

/** Guards channel creation and removal together with membership, so a channel can't be removed while someone is joining it. */
std::recursive_mutex& membershipLock() {
	static auto* lock = new std::recursive_mutex(); // leaked like the channels
	return *lock;
}

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

/** Java: release - removes the left channels that have no members anymore. membershipLock must be held. */
void release(const std::vector<std::shared_ptr<Channel>>& leftChannels) {
	State& s = state();
	for (const std::shared_ptr<Channel>& channel : leftChannels) {
		if (--channel->memberCount > 0)
			continue;
		bool removed;
		{
			std::lock_guard lock(s.mutex);
			removed = s.channels.erase(channel->getChannelId()) > 0;
		}
		if (removed && LoggingConfig::LOG_CHANNEL_REQUEST)
			log().info("Removed empty channel {} (id: {})", channel->name(), channel->getChannelId());
	}
}

} // namespace

std::shared_ptr<Channel> join(const std::shared_ptr<ChatClient>& client, std::string_view identifier) {
	std::lock_guard lock(membershipLock());
	std::shared_ptr<Channel> channel = getOrCreate(client, identifier);
	if (channel && !client->isInChannel(*channel)) {
		channel->memberCount++;
		release(client->addChannel(channel));
	}
	return channel;
}

bool leave(ChatClient& client, const std::shared_ptr<Channel>& channel) {
	if (!channel)
		return false;
	std::lock_guard lock(membershipLock());
	if (!client.removeChannel(channel))
		return false;
	release({channel});
	return true;
}

void leaveAll(ChatClient& client) {
	std::lock_guard lock(membershipLock());
	release(client.removeAllChannels());
}

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

	const std::string clientName = client ? client->toString() : "null";
	std::vector<std::string> channelType = splitTypeAndMeta(parts[1]); // { trade, Housing_barrack }
	if (channelType.size() != 2) {
		log().warn("{} requested a channel without name: {}", clientName, identifier);
		return nullptr;
	}

	std::optional<ChannelType> ct = getByIdentifier(channelType[0]);
	if (!ct) {
		log().warn("{} requested a channel of an unknown type: {}", clientName, identifier);
		return nullptr;
	}
	const std::string& channelMeta = channelType[1];
	if (commons::utils::StringUtils::toUtf16(channelMeta).size() > MAX_META_LENGTH) { // Java: String.length(), UTF-16 code units
		log().warn("{} requested a channel with a too long name: {}", clientName, identifier);
		return nullptr;
	}

	std::vector<std::string> channelRestrictions = commons::utils::StringUtils::splitJava(parts[2], "."); // { 1, 0, AION, KOR }
	if (channelRestrictions.size() < 2) {
		log().warn("{} requested a channel with malformed restrictions: {}", clientName, parts[2]);
		return nullptr;
	}
	int32_t gameServerId;
	std::optional<Race> race;
	try {
		gameServerId = commons::utils::parseInt(channelRestrictions[0]);
		race = getById(commons::utils::parseInt(channelRestrictions[1]));
	} catch (const commons::utils::NumberFormatException&) {
		log().warn("{} requested a channel with malformed restrictions: {}", clientName, parts[2]);
		return nullptr;
	}
	// retail: 99 is forbidden and it must be this session's server
	if (gameServerId == 99 || gameServerId != service::GameServerService::GAMESERVER_ID.load()) {
		log().warn("{} requested a channel of game server {}: {}", clientName, gameServerId, identifier);
		return nullptr;
	}
	if (!race) {
		log().warn("{} requested a channel of an unknown race: {}", clientName, identifier);
		return nullptr;
	}
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
		if (s.channels.size() >= MAX_CHANNELS) {
			log().warn("{} requested a new channel, but {} already exist: {}", clientName, s.channels.size(), identifier);
			return nullptr;
		}
		channel = addChannel(s, ct, gameServerId, race, channelMeta);
	}
	if (auto* jobChannel = dynamic_cast<JobChannel*>(channel.get()); jobChannel && !jobChannel->hasAliases())
		log().warn("{} requested channel for unknown class: {}", client->toString(), channelMeta);
	return channel;
}

} // namespace aion::chatserver::model::channel::ChatChannels
