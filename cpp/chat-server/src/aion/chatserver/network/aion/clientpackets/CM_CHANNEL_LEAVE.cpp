#include "aion/chatserver/network/aion/clientpackets/CM_CHANNEL_LEAVE.h"

#include "aion/chatserver/model/ChatClient.h"
#include "aion/chatserver/model/channel/Channel.h"
#include "aion/chatserver/model/channel/ChatChannels.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::chatserver::network::aion::clientpackets {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.network.aion.clientpackets.CM_CHANNEL_LEAVE"));
	return *logger;
}

} // namespace

void CM_CHANNEL_LEAVE::readImpl() {
	readC(); // 0
	readH(); // 0
	readB(16); // 0
	channelId = readD();
}

void CM_CHANNEL_LEAVE::runImpl() {
	std::shared_ptr<model::channel::Channel> channel = model::channel::ChatChannels::getChannelById(channelId);
	if (!getChatClient()->removeChannel(channel))
		log().warn("{}, couldn't leave channel: {} (id: {})", getChatClient()->toString(), channel ? channel->toString() : "null", channelId);
}

} // namespace aion::chatserver::network::aion::clientpackets
