#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <spdlog/sinks/sink.h>

#include "aion/commons/logging/LevelFilter.h"

namespace aion::commons::logging {

/**
 * Java: com.aionemu.commons.logging.DiscordChannelAppender wrapped in a logback AsyncAppender with neverBlock=true (as configured in the
 * servers' logback.xml) - sends messages via a Discord webhook (see <a href="https://discord.com/developers/docs/resources/webhook#execute-webhook">API
 * docs</a>).
 * <p>
 * Messages are formatted by the encoder on the logging thread (so %thread is correct) and queued; a worker thread
 * ("AsyncAppender-Worker-&lt;name&gt;") sends them. Logging never blocks: when the queue is full, messages are dropped, and when it is 80% full,
 * messages below WARN are dropped (logback's discardingThreshold).
 * <p>
 * If the message is longer than MAX_MESSAGE_LENGTH characters (Discord limit) it will be sent in parts, while keeping code blocks intact. If a
 * separator is configured, the formatted message is split into user name, avatar URL and message ("name|avatar|message"). Rate limit responses
 * (HTTP 429) suspend sending, all messages are dropped until the limit is reset.
 *
 * @author Neon
 */
class DiscordChannelAppender final : public spdlog::sinks::sink {
public:
	/** Discord limit */
	static constexpr int32_t MAX_USERNAME_LENGTH = 32;
	/** Discord limit */
	static constexpr int32_t MAX_MESSAGE_LENGTH = 2000;

	/** The response to a webhook request. */
	struct HttpResponse {
		/** HTTP status code, 0 if no response was received */
		long statusCode = 0;
		std::string body;
		/** value of the Retry-After header, if present */
		std::optional<std::string> retryAfter;
		/** value of the X-RateLimit-Reset header, if present */
		std::optional<std::string> rateLimitReset;
		/** description of a transport error (no response received), empty otherwise */
		std::string error;
	};

	/** Posts the JSON body to the webhook URL and returns the response. Called on the worker thread only. */
	using HttpClient = std::function<HttpResponse(const std::string& url, const std::string& json)>;

	/** One webhook message (Java: the parameters of sendMessage). */
	struct Message {
		std::string content;
		std::optional<std::string> username;
		std::optional<std::string> avatarUrl;

		bool operator==(const Message&) const = default;
	};

	struct Config {
		/** logback appender name, used for the worker thread name */
		std::string name = "discord";
		/** required (Java: webhookUrl); if empty, the appender is not used (messages are ignored) */
		std::string webhookUrl;
		/** required (Java: encoder), usually a PatternLayout */
		std::unique_ptr<spdlog::formatter> encoder;
		/** if not empty, extracts user name and avatar to use by splitting the formatted message with the separator (Java: a regex, here a literal) */
		std::string userNameAvatarUrlMessageSeparator;
		/** filter applied before queueing (logback: the AsyncAppender's filter) */
		std::optional<LevelFilter> filter;
		/** logback AsyncAppender.queueSize */
		int32_t queueSize = 256;
		/** logback AsyncAppender.discardingThreshold, -1 for queueSize / 5 */
		int32_t discardingThreshold = -1;
		/** logback AsyncAppender.maxFlushTime: how long stop() waits for queued messages to be sent, 0 to wait indefinitely */
		std::chrono::milliseconds maxFlushTime{1000};
	};

	/**
	 * @param httpClient sends the requests; by default an HTTP client (cpr) with a timeout of 30 seconds
	 * @throws IllegalArgumentException if the encoder is missing
	 */
	explicit DiscordChannelAppender(Config config, HttpClient httpClient = {});
	~DiscordChannelAppender() override;

	DiscordChannelAppender(const DiscordChannelAppender&) = delete;
	DiscordChannelAppender& operator=(const DiscordChannelAppender&) = delete;

	void log(const spdlog::details::log_msg& msg) override;
	/** Does nothing: messages are sent asynchronously. Use stop() to send the queued messages. */
	void flush() override {}
	/** Replaces the encoder with a PatternLayout of the given logback pattern. */
	void set_pattern(const std::string& pattern) override;
	void set_formatter(std::unique_ptr<spdlog::formatter> formatter) override;

	/** @return false if the webhook URL is empty (Java: the appender is not started) */
	bool isStarted() const noexcept { return started; }

	/**
	 * Java: AsyncAppender.stop() - stops accepting messages, waits up to maxFlushTime for the queued ones to be sent and stops the worker thread.
	 * Calling it again does nothing.
	 * <p>
	 * The worker thread logs its errors through the logger "com.aionemu.commons.logging.DiscordChannelAppender", so do not call stop() (or
	 * destroy the appender) while holding a lock of a sink or logger configuration that this logger writes to: the worker could wait for it while
	 * stop() waits for the worker. LoggerFactory and Logging destroy removed sinks after releasing their locks. If stop() runs on the worker
	 * thread itself (e.g. the last reference was released by the worker's own logging), it does not wait: the worker finishes the queued messages
	 * in the background.
	 */
	void stop();

	/** Waits until all queued messages were processed. @return false on timeout */
	bool awaitIdle(std::chrono::milliseconds timeout);

	/**
	 * Java: append(E) without sending - splits a formatted message into user name, avatar URL and message (if the separator is not empty),
	 * adjusts the user name to Discord's rules and splits the message into parts of at most MAX_MESSAGE_LENGTH characters.
	 */
	static std::vector<Message> createMessages(std::string_view rawMessage, std::string_view separator);

	/**
	 * Java: createMessageParts - splits the message into parts of at most MAX_MESSAGE_LENGTH characters (UTF-16 code units, like Java), at line
	 * breaks where possible, closing and reopening a code block that ends the message in each part.
	 */
	static std::vector<std::string> createMessageParts(std::string_view msg);

	/** Java: replaceForbiddenWord - replaces all occurrences of the word, ignoring case */
	static std::string replaceForbiddenWord(std::string_view username, std::string_view forbiddenWord, std::string_view replacement);

	/** @return the JSON body of the webhook request (fields without value are omitted) */
	static std::string toJson(const Message& message);

private:
	/**
	 * The state shared with the worker thread. The worker owns a reference, so it stays valid if the appender is destroyed while the worker cannot
	 * be joined (see stop()).
	 */
	struct Shared;

	const std::optional<LevelFilter> filter;
	const size_t queueSize;
	const size_t discardingThreshold;
	/** set in the constructor only */
	bool started = false;
	std::shared_ptr<Shared> shared;
	std::thread worker;
};

} // namespace aion::commons::logging
