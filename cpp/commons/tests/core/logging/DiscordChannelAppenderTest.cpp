#include <gtest/gtest.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <thread>

#include <nlohmann/json.hpp>
#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/DiscordChannelAppender.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/logging/PatternLayout.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/TimeUtils.h"

using namespace aion::commons;
using namespace aion::commons::logging;
using Message = DiscordChannelAppender::Message;
using HttpResponse = DiscordChannelAppender::HttpResponse;

namespace {

std::string repeat(char c, size_t count) {
	return std::string(count, c);
}

std::string joinLines(const std::vector<std::string>& lines) {
	return utils::StringUtils::join(lines, "\n");
}

/** A webhook that records requests and answers with a configurable response, optionally blocking until released. */
class FakeWebhook {
public:
	DiscordChannelAppender::HttpClient client() {
		return [this](const std::string& url, const std::string& json) {
			std::unique_lock lock(mutex);
			requests.push_back(nlohmann::json::parse(json));
			urls.push_back(url);
			entered = true;
			condition.notify_all();
			if (blocking)
				condition.wait_for(lock, std::chrono::milliseconds(500), [this] { return !blocking; });
			return response;
		};
	}

	void waitUntilEntered() {
		std::unique_lock lock(mutex);
		condition.wait_for(lock, std::chrono::seconds(5), [this] { return entered; });
	}

	void release() {
		std::lock_guard lock(mutex);
		blocking = false;
		condition.notify_all();
	}

	std::vector<std::string> contents() {
		std::lock_guard lock(mutex);
		std::vector<std::string> result;
		for (const auto& request : requests)
			result.push_back(request["content"]);
		return result;
	}

	std::mutex mutex;
	std::condition_variable condition;
	std::vector<nlohmann::json> requests;
	std::vector<std::string> urls;
	HttpResponse response{.statusCode = 204};
	bool blocking = false;
	bool entered = false;
};

class CapturedAppenderLog {
public:
	CapturedAppenderLog() {
		sink->set_formatter(std::make_unique<PatternLayout>("%level|%thread|%msg\\n", nullptr, false));
		LoggerFactory::configure("com.aionemu.commons.logging.DiscordChannelAppender", {.sinks = {sink}, .additive = false});
	}
	~CapturedAppenderLog() { LoggerFactory::removeConfig("com.aionemu.commons.logging.DiscordChannelAppender"); }
	std::string str() const {
		sink->flush();
		return stream.str();
	}

private:
	std::ostringstream stream;
	std::shared_ptr<spdlog::sinks::ostream_sink_mt> sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
};

DiscordChannelAppender::Config config(std::string pattern = "%logger{0}|avatar|%msg", std::optional<LevelFilter> filter = std::nullopt) {
	return {
		.name = "test",
		.webhookUrl = "https://discord.test/api/webhooks/1/token",
		.encoder = std::make_unique<PatternLayout>(pattern, nullptr, false),
		.userNameAvatarUrlMessageSeparator = "|",
		.filter = filter,
	};
}

void log(DiscordChannelAppender& appender, std::string_view text, spdlog::level::level_enum level = spdlog::level::warn) {
	spdlog::details::log_msg msg(spdlog::source_loc{}, "com.aionemu.gameserver.GameServer", level, spdlog::string_view_t(text.data(), text.size()));
	appender.log(msg);
}

} // namespace

TEST(DiscordChannelAppenderTest, SplitsUserNameAvatarAndMessage) {
	EXPECT_EQ(DiscordChannelAppender::createMessages("MailService [main]|https://x/a.png|Hello", "|"),
		(std::vector<Message>{{"Hello", "MailService [main]", "https://x/a.png"}}));
	EXPECT_EQ(DiscordChannelAppender::createMessages(" name |  avatar | msg ", "|"), (std::vector<Message>{{" msg ", "name", "avatar"}}));
	EXPECT_EQ(DiscordChannelAppender::createMessages("user|avatar|msg|with|pipes", "|"), (std::vector<Message>{{"msg|with|pipes", "user", "avatar"}}));
	EXPECT_EQ(DiscordChannelAppender::createMessages("avatar|msg", "|"), (std::vector<Message>{{"msg", std::nullopt, "avatar"}}));
	EXPECT_EQ(DiscordChannelAppender::createMessages("just a message", "|"), (std::vector<Message>{{"just a message", std::nullopt, std::nullopt}}));
	EXPECT_EQ(DiscordChannelAppender::createMessages("a|b|c", ""), (std::vector<Message>{{"a|b|c", std::nullopt, std::nullopt}}));
	EXPECT_EQ(DiscordChannelAppender::createMessages("u||", "|"), (std::vector<Message>{{"", "u", ""}}));
}

TEST(DiscordChannelAppenderTest, UserNameRules) {
	EXPECT_EQ(DiscordChannelAppender::replaceForbiddenWord("DiscordChannelAppender", "discord", "Dscrd"), "DscrdChannelAppender");
	EXPECT_EQ(DiscordChannelAppender::replaceForbiddenWord("discord DISCORD x", "Discord", "Dscrd"), "Dscrd Dscrd x");
	EXPECT_EQ(DiscordChannelAppender::replaceForbiddenWord("nothing", "discord", "Dscrd"), "nothing");

	auto messages = DiscordChannelAppender::createMessages("DiscordChannelAppender [AsyncAppender-Worker-app_status_discord_async]|a|m", "|");
	ASSERT_EQ(messages.size(), 1u);
	EXPECT_EQ(messages[0].username, "DscrdChannelAppender [AsyncAppe…"); // 31 characters and an ellipsis
	auto exact = DiscordChannelAppender::createMessages(repeat('n', 32) + "|a|m", "|");
	EXPECT_EQ(exact[0].username, repeat('n', 32));
}

TEST(DiscordChannelAppenderTest, ShortMessagesAreNotSplit) {
	EXPECT_EQ(DiscordChannelAppender::createMessageParts(""), (std::vector<std::string>{""}));
	EXPECT_EQ(DiscordChannelAppender::createMessageParts("line1\r\nline2"), (std::vector<std::string>{"line1\nline2"}));
	std::string maximum = repeat('m', 1999) + "ä"; // 2000 UTF-16 code units, 2001 bytes
	EXPECT_EQ(DiscordChannelAppender::createMessageParts(maximum), (std::vector<std::string>{maximum}));
}

TEST(DiscordChannelAppenderTest, LongMessagesAreSplitAtLines) {
	std::string line1 = repeat('a', 1500);
	std::string line2 = repeat('b', 1000);
	std::string line3 = repeat('c', 10);
	// like Java, a part that is ended because the next line does not fit keeps its line break
	EXPECT_EQ(DiscordChannelAppender::createMessageParts(line1 + "\n" + line2 + "\r\n" + line3),
		(std::vector<std::string>{line1 + "\n", line2 + "\n" + line3}));
}

TEST(DiscordChannelAppenderTest, OverlongLinesAreTruncated) {
	EXPECT_EQ(DiscordChannelAppender::createMessageParts(repeat('x', 4500)), (std::vector<std::string>{repeat('x', 1999) + "…"}));
}

TEST(DiscordChannelAppenderTest, CodeBlocksAreContinuedInEachPart) {
	std::vector<std::string> trace;
	for (int i = 1; i <= 30; i++)
		trace.push_back(repeat(static_cast<char>('A' + i % 26), 97) + (i < 10 ? "0" : "") + std::to_string(i));
	std::string message = "Error\n```qml\n" + joinLines(trace) + "\n```";
	auto parts = DiscordChannelAppender::createMessageParts(message);
	ASSERT_EQ(parts.size(), 2u);
	std::vector<std::string> first(trace.begin(), trace.begin() + 19);
	std::vector<std::string> second(trace.begin() + 19, trace.end());
	EXPECT_EQ(parts[0], "Error\n```qml\n" + joinLines(first) + "\n```");
	EXPECT_EQ(parts[1], "```qml\n" + joinLines(second) + "\n```");
}

TEST(DiscordChannelAppenderTest, SplitPartsRespectTheLimit) {
	std::vector<std::string> lines;
	for (int i = 0; i < 400; i++)
		lines.push_back("\tat frame " + std::to_string(i) + " " + repeat('z', static_cast<size_t>(i % 50) * 7));
	std::string message = "Something failed\n```qml\naion::Exception: x\n" + joinLines(lines) + "\n```";
	auto parts = DiscordChannelAppender::createMessageParts(message);
	ASSERT_GT(parts.size(), 5u);
	for (const auto& part : parts) {
		EXPECT_LE(utils::StringUtils::utf16Length(part), DiscordChannelAppender::MAX_MESSAGE_LENGTH);
		size_t markers = 0;
		for (size_t i = part.find("```"); i != std::string::npos; i = part.find("```", i + 3))
			markers++;
		EXPECT_EQ(markers % 2, 0u) << part;
	}
}

TEST(DiscordChannelAppenderTest, Json) {
	auto json = nlohmann::json::parse(DiscordChannelAppender::toJson({"c\"ontent\n", "user", "https://a"}));
	EXPECT_EQ(json, (nlohmann::json{{"content", "c\"ontent\n"}, {"username", "user"}, {"avatar_url", "https://a"}}));
	EXPECT_EQ(nlohmann::json::parse(DiscordChannelAppender::toJson({"only content", std::nullopt, std::nullopt})), (nlohmann::json{{"content", "only content"}}));
	EXPECT_NO_THROW(DiscordChannelAppender::toJson({"invalid \xFF utf-8", std::nullopt, std::nullopt}));
}

TEST(DiscordChannelAppenderTest, SendsFilteredMessagesInOrder) {
	FakeWebhook webhook;
	DiscordChannelAppender appender(config("%logger{0}|https://x/%level.png|%msg", LevelFilter::threshold(spdlog::level::warn)), webhook.client());
	ASSERT_TRUE(appender.isStarted());
	log(appender, "first");
	log(appender, "ignored info", spdlog::level::info);
	log(appender, "second", spdlog::level::err);
	log(appender, repeat('x', 2500));
	ASSERT_TRUE(appender.awaitIdle(std::chrono::seconds(5)));

	EXPECT_EQ(webhook.contents(), (std::vector<std::string>{"first", "second", repeat('x', 1999) + "…"}));
	std::lock_guard lock(webhook.mutex);
	EXPECT_EQ(webhook.urls[0], "https://discord.test/api/webhooks/1/token");
	EXPECT_EQ(webhook.requests[0]["username"], "GameServer");
	EXPECT_EQ(webhook.requests[0]["avatar_url"], "https://x/WARN.png");
	EXPECT_EQ(webhook.requests[1]["avatar_url"], "https://x/ERROR.png");
}

TEST(DiscordChannelAppenderTest, RateLimitDropsMessages) {
	CapturedAppenderLog captured;
	FakeWebhook webhook;
	webhook.response = {.statusCode = 429, .body = R"({"retry_after": 60})", .retryAfter = "60"};
	DiscordChannelAppender appender(config(), webhook.client());
	log(appender, "limited");
	ASSERT_TRUE(appender.awaitIdle(std::chrono::seconds(5)));
	log(appender, "dropped");
	ASSERT_TRUE(appender.awaitIdle(std::chrono::seconds(5)));

	EXPECT_EQ(webhook.contents(), (std::vector<std::string>{"limited"}));
	EXPECT_EQ(captured.str(), "WARN|AsyncAppender-Worker-test|Error sending Discord message: limited\nCaused by: Flood control for channel triggered, "
														"reset in 60s. Meanwhile, all messages will be dropped.\n");
}

TEST(DiscordChannelAppenderTest, RateLimitResetHeader) {
	CapturedAppenderLog captured;
	FakeWebhook webhook;
	auto reset = static_cast<double>(utils::currentTimeMillis() + 120'500) / 1000.0;
	webhook.response = {.statusCode = 429, .rateLimitReset = std::to_string(reset)};
	DiscordChannelAppender appender(config(), webhook.client());
	log(appender, "limited");
	ASSERT_TRUE(appender.awaitIdle(std::chrono::seconds(5)));
	EXPECT_NE(captured.str().find("reset in 120s"), std::string::npos) << captured.str();

	FakeWebhook expired;
	expired.response = {.statusCode = 429};
	DiscordChannelAppender second(config(), expired.client());
	log(second, "limited");
	ASSERT_TRUE(second.awaitIdle(std::chrono::seconds(5)));
	EXPECT_NE(captured.str().find("reset in 3s"), std::string::npos) << captured.str(); // no usable header: 3 seconds
}

TEST(DiscordChannelAppenderTest, InvalidRateLimitHeaders) {
	// std::stod accepts "inf", "nan" and huge values, whose conversion to int64_t was undefined behaviour
	auto resetIn = [](HttpResponse response) {
		CapturedAppenderLog captured;
		FakeWebhook webhook;
		webhook.response = std::move(response);
		DiscordChannelAppender appender(config(), webhook.client());
		log(appender, "limited");
		EXPECT_TRUE(appender.awaitIdle(std::chrono::seconds(5)));
		std::string text = captured.str();
		size_t start = text.find("reset in ");
		return start == std::string::npos ? std::string() : text.substr(start + 9, text.find('s', start + 9) - start - 9);
	};
	const std::string oneYear = std::to_string(365 * 24 * 60 * 60);
	const std::string oneYearMinusRounding = std::to_string(365 * 24 * 60 * 60 - 1);
	EXPECT_EQ(resetIn({.statusCode = 429, .retryAfter = "inf"}), "3");
	EXPECT_EQ(resetIn({.statusCode = 429, .retryAfter = "nan"}), "3");
	EXPECT_EQ(resetIn({.statusCode = 429, .retryAfter = "-5"}), "3");
	EXPECT_EQ(resetIn({.statusCode = 429, .retryAfter = "1e300"}), oneYear);
	std::string absolute = resetIn({.statusCode = 429, .rateLimitReset = "1e30"});
	EXPECT_TRUE(absolute == oneYear || absolute == oneYearMinusRounding) << absolute; // an absolute time, subject to rounding
	EXPECT_EQ(resetIn({.statusCode = 429, .rateLimitReset = "-inf"}), "3");
}

TEST(DiscordChannelAppenderTest, DestroyedByItsOwnWorkerThread) {
	// the last reference is released on the worker thread: stop() cannot join the worker, which must still finish safely
	FakeWebhook webhook;
	auto cfg = config();
	cfg.maxFlushTime = std::chrono::seconds(3);
	std::shared_ptr<DiscordChannelAppender> appender;
	std::atomic<int> calls{0};
	appender = std::make_shared<DiscordChannelAppender>(std::move(cfg), [&](const std::string& url, const std::string& json) {
		HttpResponse response = webhook.client()(url, json);
		if (calls++ == 0) {
			auto begin = std::chrono::steady_clock::now();
			appender.reset(); // destroys the appender on its worker thread
			EXPECT_LT(std::chrono::steady_clock::now() - begin, std::chrono::milliseconds(1000)); // does not wait for itself
		}
		return response;
	});
	log(*appender, "first");
	log(*appender, "second");
	for (int i = 0; i < 500 && webhook.contents().size() < 2; i++)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	EXPECT_EQ(webhook.contents(), (std::vector<std::string>{"first", "second"})); // queued messages are still sent
	std::this_thread::sleep_for(std::chrono::milliseconds(50)); // let the worker exit
}

TEST(DiscordChannelAppenderTest, RemovedFromConfigWhileWorkerLogs) {
	// the configuring thread destroys the appender (and joins its worker) while the worker logs an error through the logging system
	CapturedAppenderLog captured;
	FakeWebhook webhook;
	webhook.blocking = true;
	webhook.response = {.statusCode = 400};
	auto cfg = config();
	cfg.maxFlushTime = std::chrono::seconds(5);
	LoggerFactory::configure("test.discord.CHAT_LOG", {.sinks = {std::make_shared<DiscordChannelAppender>(std::move(cfg), webhook.client())}, .additive = false});
	LoggerFactory::getLogger("test.discord.CHAT_LOG").warn("chat");
	webhook.waitUntilEntered();
	LoggerFactory::removeConfig("test.discord.CHAT_LOG"); // stops the appender, waiting for the worker which logs "Error sending Discord message"
	EXPECT_NE(captured.str().find("Error sending Discord message: chat"), std::string::npos) << captured.str();
}

TEST(DiscordChannelAppenderTest, ErrorsAreLoggedWithoutRecursion) {
	CapturedAppenderLog captured;
	FakeWebhook webhook;
	webhook.response = {.statusCode = 400, .body = R"({"message": "Invalid Form Body"})"};
	DiscordChannelAppender appender(config(), webhook.client());
	log(appender, "bad");
	log(appender, "Error sending Discord message: bad\nCaused by: x");
	ASSERT_TRUE(appender.awaitIdle(std::chrono::seconds(5)));
	EXPECT_EQ(captured.str(),
		"WARN|AsyncAppender-Worker-test|Error sending Discord message: bad\nCaused by: Server returned status code 400: {\"message\": \"Invalid Form Body\"}\n");

	FakeWebhook unreachable;
	unreachable.response = {.error = "Could not resolve host: discord.test"};
	DiscordChannelAppender second(config(), unreachable.client());
	log(second, "offline");
	ASSERT_TRUE(second.awaitIdle(std::chrono::seconds(5)));
	EXPECT_NE(captured.str().find("Error sending Discord message: offline\nCaused by: Could not resolve host: discord.test"), std::string::npos);
}

TEST(DiscordChannelAppenderTest, NeverBlocksAndDiscardsWhenQueueIsFull) {
	FakeWebhook webhook;
	webhook.blocking = true;
	auto cfg = config();
	cfg.queueSize = 4;
	cfg.discardingThreshold = 2;
	DiscordChannelAppender appender(std::move(cfg), webhook.client());
	log(appender, "1");
	webhook.waitUntilEntered(); // the worker is busy with message 1, the queue is empty
	log(appender, "2");
	log(appender, "3", spdlog::level::info);
	log(appender, "4", spdlog::level::info);
	log(appender, "5 discarded info", spdlog::level::info); // remaining capacity 1 is below the discarding threshold
	log(appender, "6");
	log(appender, "7 dropped"); // queue full
	webhook.release();
	ASSERT_TRUE(appender.awaitIdle(std::chrono::seconds(5)));
	EXPECT_EQ(webhook.contents(), (std::vector<std::string>{"1", "2", "3", "4", "6"}));
}

TEST(DiscordChannelAppenderTest, StopSendsQueuedMessages) {
	FakeWebhook webhook;
	DiscordChannelAppender appender(config(), webhook.client());
	for (int i = 0; i < 10; i++)
		log(appender, std::to_string(i));
	appender.stop();
	EXPECT_EQ(webhook.contents().size(), 10u);
	log(appender, "after stop");
	appender.stop();
	EXPECT_EQ(webhook.contents().size(), 10u);
}

TEST(DiscordChannelAppenderTest, StopGivesUpAfterMaxFlushTime) {
	FakeWebhook webhook;
	webhook.blocking = true; // each request takes 500 ms
	auto cfg = config();
	cfg.maxFlushTime = std::chrono::milliseconds(50);
	DiscordChannelAppender appender(std::move(cfg), webhook.client());
	log(appender, "slow");
	webhook.waitUntilEntered();
	log(appender, "never sent");
	auto begin = std::chrono::steady_clock::now();
	appender.stop();
	EXPECT_LT(std::chrono::steady_clock::now() - begin, std::chrono::seconds(2));
	EXPECT_EQ(webhook.contents(), (std::vector<std::string>{"slow"}));
}

TEST(DiscordChannelAppenderTest, NotStartedWithoutWebhookUrl) {
	FakeWebhook webhook;
	auto cfg = config();
	cfg.webhookUrl.clear();
	DiscordChannelAppender appender(std::move(cfg), webhook.client());
	EXPECT_FALSE(appender.isStarted());
	log(appender, "ignored");
	EXPECT_TRUE(appender.awaitIdle(std::chrono::milliseconds(10)));
	EXPECT_TRUE(webhook.contents().empty());

	auto missingEncoder = config();
	missingEncoder.encoder.reset();
	EXPECT_THROW(DiscordChannelAppender(std::move(missingEncoder)), utils::IllegalArgumentException);
}
