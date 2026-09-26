#include "aion/chatserver/network/aion/clientpackets/CM_CHANNEL_MESSAGE.h"

#include "aion/chatserver/configs/main/LoggingConfig.h"
#include "aion/chatserver/dao/ChatLogDAO.h"
#include "aion/chatserver/model/ChatClient.h"
#include "aion/chatserver/model/channel/Channel.h"
#include "aion/chatserver/model/channel/ChatChannels.h"
#include "aion/chatserver/model/message/Message.h"
#include "aion/chatserver/network/aion/serverpackets/SM_CHANNEL_MESSAGE.h"
#include "aion/chatserver/network/netty/handler/ClientChannelHandler.h"
#include "aion/chatserver/service/BroadcastService.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"

namespace aion::chatserver::network::aion::clientpackets {

using configs::main::LoggingConfig;
using model::message::Message;

void CM_CHANNEL_MESSAGE::readImpl() {
	readH();
	readC();
	readD();
	readD();
	readD();
	readD();
	channelId = readD();
	readC();
	int32_t contentLength = readH() * 2;
	content = readB(contentLength);
}

void CM_CHANNEL_MESSAGE::runImpl() {
	std::shared_ptr<model::channel::Channel> channel = model::channel::ChatChannels::getChannelById(channelId);
	if (!channel)
		return;
	std::shared_ptr<model::ChatClient> client = getChatClient();
	Message message(channel, content.value_or(std::vector<uint8_t>()), client);
	if (client->isGagged()) {
		int64_t gagTimeMin = (client->getGagTime() - commons::utils::currentTimeMillis()) / 1000 / 60;
		message.setText("You have been gagged for " + std::to_string(gagTimeMin) + " minutes.");
		clientChannelHandler->sendPacket(serverpackets::SM_CHANNEL_MESSAGE(message));
		return;
	}
	int32_t floodProtectionTime = client->nextMessageTimeSec(channel->getChannelType());
	if (floodProtectionTime > 0) {
		message.setText("You can chat again in this channel in " + std::to_string(floodProtectionTime) + " second" + (floodProtectionTime == 1 ? "." : "s."));
		clientChannelHandler->sendPacket(serverpackets::SM_CHANNEL_MESSAGE(message));
		return;
	}
	client->updateLastMessageTime(channel->getChannelType());
	service::BroadcastService::getInstance().broadcastMessage(message);

	if (LoggingConfig::LOG_CHAT)
		commons::logging::LoggerFactory::getLogger("CHAT_LOG")
			.info("[{}] {}: {}", message.getChannel()->name(), message.getSender()->getName(), message.getTextString());

	if (LoggingConfig::LOG_CHAT_TO_DB)
		dao::ChatLogDAO::save(message.getSender()->getName(), message.getTextString(), message.getChannel()->name());
}

std::string CM_CHANNEL_MESSAGE::toString() const {
	// Java: Arrays.toString(content) - signed bytes, "null" for a null array
	std::string bytes = "null";
	if (content) {
		bytes = "[";
		for (size_t i = 0; i < content->size(); i++) {
			if (i > 0)
				bytes += ", ";
			bytes += std::to_string(static_cast<int8_t>((*content)[i]));
		}
		bytes += "]";
	}
	return "CM_CHANNEL_MESSAGE [channelId=" + std::to_string(channelId) + ", content=" + bytes + "]";
}

} // namespace aion::chatserver::network::aion::clientpackets
