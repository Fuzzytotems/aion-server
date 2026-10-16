#include "aion/chatserver/model/message/Message.h"

#include <utility>

#include "aion/chatserver/model/ChatClient.h"
#include "aion/chatserver/model/channel/Channel.h"
#include "aion/chatserver/utils/Utf16Le.h"

namespace aion::chatserver::model::message {

Message::Message(std::shared_ptr<channel::Channel> channel, std::vector<uint8_t> text, std::shared_ptr<ChatClient> sender)
	: channel(std::move(channel)), sender(std::move(sender)), text(std::move(text)) {
}

void Message::setText(std::string_view str) {
	text = utils::Utf16Le::getBytes(str);
}

std::string Message::getTextString() const {
	return utils::Utf16Le::newString(text);
}

} // namespace aion::chatserver::model::message
