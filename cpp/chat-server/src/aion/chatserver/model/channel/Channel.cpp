#include "aion/chatserver/model/channel/Channel.h"

#include <typeinfo>

#include <fmt/format.h>

#include "aion/chatserver/utils/IdFactory.h"
#include "aion/commons/utils/ClassName.h"

namespace aion::chatserver::model::channel {

Channel::Channel(ChannelType channelType, int32_t gameServerId)
	: channelType(channelType), gameServerId(gameServerId), channelId(utils::IdFactory::getInstance().nextId()) {
}

std::string Channel::toString() const {
	return fmt::format("com.aionemu.chatserver.model.channel.{}@{:x}", commons::utils::getSimpleClassName(typeid(*this)),
		static_cast<uint32_t>(channelId));
}

} // namespace aion::chatserver::model::channel
