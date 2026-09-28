#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace aion::chatserver::model {
class ChatClient;
}

namespace aion::chatserver::model::channel {
class Channel;
}

namespace aion::chatserver::model::message {

/**
 * A chat message: the channel, the sender and the text as UTF-16LE bytes (as the client sent it).
 * <p>
 * Java: com.aionemu.chatserver.model.message.Message
 *
 * @author ATracer
 */
class Message {
public:
	Message(std::shared_ptr<channel::Channel> channel, std::vector<uint8_t> text, std::shared_ptr<ChatClient> sender);

	const std::shared_ptr<channel::Channel>& getChannel() const noexcept { return channel; }

	const std::vector<uint8_t>& getText() const noexcept { return text; }

	/** Replaces the text (Java: str.getBytes(StandardCharsets.UTF_16LE)). */
	void setText(std::string_view str);

	/** @return the size of the text in bytes */
	int32_t size() const noexcept { return static_cast<int32_t>(text.size()); }

	const std::shared_ptr<ChatClient>& getSender() const noexcept { return sender; }

	/** @return the text as a string (Java: new String(text, StandardCharsets.UTF_16LE), see utils::Utf16Le::decode) */
	std::string getTextString() const;

private:
	std::shared_ptr<channel::Channel> channel;
	std::shared_ptr<ChatClient> sender;
	std::vector<uint8_t> text;
};

} // namespace aion::chatserver::model::message
