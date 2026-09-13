#include "aion/commons/logging/DiscordChannelAppender.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/logging/PatternLayout.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/commons/utils/concurrent/ThreadName.h"

namespace aion::commons::logging {

namespace StringUtils = utils::StringUtils;

namespace {

constexpr std::u16string_view CODE_BLOCK_END = u"```";
constexpr std::string_view ERROR_HEADER = "Error sending Discord message: ";

std::u16string replaceAll(std::u16string text, std::u16string_view target, std::u16string_view replacement) {
	for (size_t i = text.find(target); i != std::u16string::npos; i = text.find(target, i + replacement.size()))
		text.replace(i, target.size(), replacement);
	return text;
}

std::u16string_view trim(std::u16string_view s) noexcept {
	size_t start = 0;
	size_t end = s.size();
	while (start < end && s[start] <= u' ')
		start++;
	while (end > start && s[end - 1] <= u' ')
		end--;
	return s.substr(start, end - start);
}

/** Java: String.split(regex) for a literal delimiter - trailing empty strings are removed */
std::vector<std::u16string> splitLines(const std::u16string& text) {
	std::vector<std::u16string> lines;
	size_t start = 0;
	while (true) {
		size_t end = text.find(u'\n', start);
		if (end == std::u16string::npos) {
			lines.push_back(text.substr(start));
			break;
		}
		lines.push_back(text.substr(start, end - start));
		start = end + 1;
	}
	if (text.empty())
		return lines;
	while (!lines.empty() && lines.back().empty())
		lines.pop_back();
	return lines;
}

/** Java: String.split(literal, limit) with a positive limit - at most limit parts, trailing empty strings are kept */
std::vector<std::string_view> split(std::string_view text, std::string_view separator, size_t limit) {
	std::vector<std::string_view> parts;
	size_t start = 0;
	while (parts.size() + 1 < limit) {
		size_t end = text.find(separator, start);
		if (end == std::string_view::npos)
			break;
		parts.push_back(text.substr(start, end - start));
		start = end + separator.size();
	}
	parts.push_back(text.substr(start));
	return parts;
}

int64_t calcTotalLength(const std::u16string& line, const std::u16string& sb, bool isInsideCodeBlock) noexcept {
	int64_t length = static_cast<int64_t>(line.size() + sb.size());
	if (isInsideCodeBlock)
		length += static_cast<int64_t>(CODE_BLOCK_END.size());
	return length;
}

/** Upper limit of rate limit durations and reset times taken from response headers (Discord uses seconds to minutes) */
constexpr double MAX_RATE_LIMIT_SECONDS = 365.0 * 24 * 60 * 60;

/**
 * @return the header value as a number of seconds in the range [0, maxSeconds], 0 if absent, invalid or not finite (Java:
 * HttpHeaders.firstValueAsLong(...).orElse(0))
 */
double headerAsSeconds(const std::optional<std::string>& value, double maxSeconds) noexcept {
	if (!value)
		return 0;
	try {
		double seconds = std::stod(*value);
		if (!std::isfinite(seconds) || seconds < 0)
			return 0;
		return std::min(seconds, maxSeconds);
	} catch (const std::exception&) {
		return 0;
	}
}

} // namespace

struct DiscordChannelAppender::Shared {
	Shared(std::string name, std::string webhookUrl, std::string separator, std::chrono::milliseconds maxFlushTime, HttpClient httpClient,
		std::unique_ptr<spdlog::formatter> encoder)
		: name(std::move(name)), webhookUrl(std::move(webhookUrl)), separator(std::move(separator)), maxFlushTime(maxFlushTime),
			httpClient(std::move(httpClient)), log(LoggerFactory::getLogger("com.aionemu.commons.logging.DiscordChannelAppender")),
			encoder(std::move(encoder)) {}

	void run() noexcept;
	void append(const std::string& rawMessage);
	void sendMessage(const Message& message);
	void handleResponse(const HttpResponse& response);
	bool isRateLimited() const noexcept { return floodResetTimeMillis > utils::currentTimeMillis(); }
	bool drained() const noexcept { return queue.empty() && !busy; }

	const std::string name;
	const std::string webhookUrl;
	const std::string separator;
	const std::chrono::milliseconds maxFlushTime;
	HttpClient httpClient; // used by the worker thread only
	/** obtained on construction, so the worker never needs the LoggerFactory lock (which the thread stopping the appender may hold) */
	const Logger log;

	std::mutex mutex;
	std::condition_variable queueCondition;
	std::condition_variable idleCondition;
	std::unique_ptr<spdlog::formatter> encoder; // guarded by mutex
	std::deque<std::string> queue;              // guarded by mutex
	bool busy = false;                          // guarded by mutex
	bool stopping = false;                      // guarded by mutex
	std::atomic<bool> aborting{false};
	std::atomic<int64_t> floodResetTimeMillis{0};
	std::unique_ptr<cpr::Session> session; // used by the worker thread only
};

DiscordChannelAppender::DiscordChannelAppender(Config config, HttpClient httpClient)
	: filter(config.filter), queueSize(static_cast<size_t>(std::max(1, config.queueSize))),
		discardingThreshold(config.discardingThreshold < 0 ? queueSize / 5 : static_cast<size_t>(config.discardingThreshold)) {
	if (!config.encoder)
		throw utils::IllegalArgumentException("<encoder> is missing");
	shared = std::make_shared<Shared>(std::move(config.name), std::move(config.webhookUrl), std::move(config.userNameAvatarUrlMessageSeparator),
		config.maxFlushTime, std::move(httpClient), std::move(config.encoder));
	if (shared->webhookUrl.empty())
		return; // <webhookUrl> is empty, appender will not be used
	if (!shared->httpClient) {
		// created here rather than on the worker thread, so cpr's and libcurl's global state is initialized before (and destroyed after) the
		// functions registered with std::atexit afterwards, e.g. by Logging::init
		shared->session = std::make_unique<cpr::Session>();
		shared->httpClient = [state = shared.get()](const std::string& url, const std::string& json) { // the client is owned by the state
			cpr::Session& s = *state->session;
			s.SetUrl(cpr::Url{url});
			s.SetHeader(cpr::Header{{"User-Agent", "DiscordChannelAppender/1.0"}, {"Content-Type", "application/json"}});
			s.SetBody(cpr::Body{json});
			// Deviation: Java's HttpClient waits indefinitely, a timeout keeps a hanging connection from blocking all further messages
			s.SetTimeout(cpr::Timeout{std::chrono::seconds(30)});
			s.SetProgressCallback(cpr::ProgressCallback([state](cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, intptr_t) {
				return !state->aborting.load(); // cancels the request when stop() gives up waiting
			}));
			cpr::Response response = s.Post();
			HttpResponse result;
			if (response.error.code != cpr::ErrorCode::OK) {
				result.error = response.error.message.empty() ? "request failed" : response.error.message;
				return result;
			}
			result.statusCode = response.status_code;
			result.body = std::move(response.text);
			if (auto it = response.header.find("Retry-After"); it != response.header.end())
				result.retryAfter = it->second;
			if (auto it = response.header.find("X-RateLimit-Reset"); it != response.header.end())
				result.rateLimitReset = it->second;
			return result;
		};
	}
	worker = std::thread([state = shared] { state->run(); });
	started = true;
}

DiscordChannelAppender::~DiscordChannelAppender() {
	stop();
}

void DiscordChannelAppender::log(const spdlog::details::log_msg& msg) {
	if (!isStarted() || (filter && !filter->accepts(msg.level)))
		return;
	Shared& s = *shared;
	{
		std::lock_guard lock(s.mutex);
		if (s.stopping)
			return;
		size_t remainingCapacity = queueSize - s.queue.size();
		// logback AsyncAppender: events up to INFO are discarded when the queue is almost full, all events when it is full (neverBlock)
		if (remainingCapacity < discardingThreshold && msg.level <= spdlog::level::info)
			return;
		if (remainingCapacity == 0)
			return;
		spdlog::memory_buf_t formatted;
		s.encoder->format(msg, formatted);
		s.queue.emplace_back(formatted.data(), formatted.size());
	}
	s.queueCondition.notify_one();
}

void DiscordChannelAppender::set_pattern(const std::string& pattern) {
	set_formatter(std::make_unique<PatternLayout>(pattern));
}

void DiscordChannelAppender::set_formatter(std::unique_ptr<spdlog::formatter> formatter) {
	if (!formatter)
		throw utils::IllegalArgumentException("<encoder> is missing");
	std::lock_guard lock(shared->mutex);
	shared->encoder = std::move(formatter);
}

void DiscordChannelAppender::stop() {
	Shared& s = *shared;
	std::unique_lock lock(s.mutex);
	if (s.stopping)
		return;
	s.stopping = true;
	s.queueCondition.notify_all();
	if (!started)
		return;
	if (worker.get_id() == std::this_thread::get_id()) {
		// the worker cannot wait for itself: it sends the remaining messages and exits, keeping the shared state alive until then
		lock.unlock();
		worker.detach();
		return;
	}
	bool flushed = true;
	if (s.maxFlushTime.count() > 0)
		flushed = s.idleCondition.wait_for(lock, s.maxFlushTime, [&s] { return s.drained(); });
	else
		s.idleCondition.wait(lock, [&s] { return s.drained(); });
	if (!flushed) {
		std::fprintf(stderr, "WARN in DiscordChannelAppender[%s] - Max queue flush timeout (%lld ms) exceeded. Approximately %zu queued events were possibly discarded.\n",
			s.name.c_str(), static_cast<long long>(s.maxFlushTime.count()), s.queue.size());
		s.aborting = true;
	}
	lock.unlock();
	s.queueCondition.notify_all();
	worker.join();
}

bool DiscordChannelAppender::awaitIdle(std::chrono::milliseconds timeout) {
	Shared& s = *shared;
	std::unique_lock lock(s.mutex);
	return s.idleCondition.wait_for(lock, timeout, [&s] { return s.drained(); });
}

void DiscordChannelAppender::Shared::run() noexcept {
	try {
		utils::concurrent::setCurrentThreadName("AsyncAppender-Worker-" + name);
	} catch (...) {
	}
	std::unique_lock lock(mutex);
	while (true) {
		queueCondition.wait(lock, [this] { return stopping || !queue.empty(); });
		if (queue.empty() || aborting)
			break;
		std::string rawMessage = std::move(queue.front());
		queue.pop_front();
		busy = true;
		lock.unlock();
		try {
			append(rawMessage);
		} catch (...) {
			// logback's AppenderBase catches appender exceptions (and reports them as status messages)
		}
		lock.lock();
		busy = false;
		idleCondition.notify_all();
	}
	queue.clear();
	busy = false;
	idleCondition.notify_all();
}

void DiscordChannelAppender::Shared::append(const std::string& rawMessage) {
	for (const Message& message : createMessages(rawMessage, separator))
		sendMessage(message);
}

std::vector<DiscordChannelAppender::Message> DiscordChannelAppender::createMessages(std::string_view rawMessage, std::string_view separator) {
	std::optional<std::string> username;
	std::optional<std::string> avatarUrl;
	std::string_view msg = rawMessage;
	if (!separator.empty()) {
		std::vector<std::string_view> parts = split(rawMessage, separator, 3);
		for (size_t partCount = 0; partCount < parts.size(); partCount++) {
			std::string_view part = parts[parts.size() - 1 - partCount];
			if (partCount == 0)
				msg = part;
			else if (partCount == 1)
				avatarUrl = std::string(StringUtils::trim(part));
			else if (partCount == 2)
				username = std::string(StringUtils::trim(part));
		}
	}
	if (username) {
		username = replaceForbiddenWord(*username, "discord", "Dscrd"); // Discord API rejects usernames containing "discord"
		std::u16string utf16 = StringUtils::toUtf16(*username);
		if (utf16.size() > MAX_USERNAME_LENGTH)
			username = StringUtils::toUtf8(std::u16string_view(utf16).substr(0, MAX_USERNAME_LENGTH - 1)) + "…";
	}
	std::vector<Message> messages;
	for (std::string& part : createMessageParts(msg))
		messages.push_back(Message{std::move(part), username, avatarUrl});
	return messages;
}

std::string DiscordChannelAppender::replaceForbiddenWord(std::string_view username, std::string_view forbiddenWord, std::string_view replacement) {
	std::string result(username);
	std::string word = StringUtils::toLowerCase(forbiddenWord);
	if (word.empty())
		return result;
	for (size_t i = StringUtils::toLowerCase(result).rfind(word); i != std::string::npos;
		i = i == 0 ? std::string::npos : StringUtils::toLowerCase(result).rfind(word, i - 1)) {
		result = result.substr(0, i) + std::string(replacement) + result.substr(i + word.size());
	}
	return result;
}

std::vector<std::string> DiscordChannelAppender::createMessageParts(std::string_view message) {
	// try to slightly shrink message due to the low message length limit
	std::u16string msg = replaceAll(StringUtils::toUtf16(message), u"\r\n", u"\n");
	if (msg.size() <= MAX_MESSAGE_LENGTH)
		return {StringUtils::toUtf8(msg)};

	std::vector<std::string> messageParts;
	std::u16string codeBlockStart;
	int64_t codeStartIndex = INT32_MAX;
	int64_t codeEndIndex = -1;
	if (trim(msg).ends_with(CODE_BLOCK_END)) {
		// Java: CODE_BLOCK_TYPE_PATTERN.matcher(msg).find() with the pattern (```(?:[a-z]+\r?\n)?)
		size_t matchStart = msg.find(CODE_BLOCK_END);
		size_t matchEnd = matchStart + CODE_BLOCK_END.size();
		size_t letters = matchEnd;
		while (letters < msg.size() && msg[letters] >= u'a' && msg[letters] <= u'z')
			letters++;
		if (letters > matchEnd) {
			if (letters + 1 < msg.size() && msg[letters] == u'\r' && msg[letters + 1] == u'\n')
				matchEnd = letters + 2;
			else if (letters < msg.size() && msg[letters] == u'\n')
				matchEnd = letters + 1;
		}
		codeBlockStart = replaceAll(msg.substr(matchStart, matchEnd - matchStart), u"\n", u"");
		codeStartIndex = static_cast<int64_t>(matchEnd) + 1;
		codeEndIndex = static_cast<int64_t>(msg.rfind(CODE_BLOCK_END)) - 1;
	}
	int64_t msgPosition = -1;
	std::vector<std::u16string> lines = splitLines(msg);
	std::u16string sb;
	sb.reserve(MAX_MESSAGE_LENGTH);
	for (int64_t i = 0; i < static_cast<int64_t>(lines.size()); i++) {
		const std::u16string& line = lines[static_cast<size_t>(i)];
		int64_t lineLength = static_cast<int64_t>(line.size());
		msgPosition += lineLength + (i == 0 ? 0 : 1);
		bool isNewMessagePart = sb.empty();
		bool isInsideCodeBlock = msgPosition >= codeStartIndex && msgPosition <= codeEndIndex;
		if (isNewMessagePart && isInsideCodeBlock && line.find(codeBlockStart) == std::u16string::npos)
			(sb += codeBlockStart) += u'\n';
		else if (!isNewMessagePart)
			sb += u'\n';
		int64_t overflowingChars = calcTotalLength(line, sb, isInsideCodeBlock) - MAX_MESSAGE_LENGTH;
		if (overflowingChars <= 0) { // fits into current messagePart
			sb += line;
			if (i < static_cast<int64_t>(lines.size()) - 1)
				continue;
		} else if (isNewMessagePart) { // must be truncated
			bool wasInsideCodeBlock = isInsideCodeBlock;
			isInsideCodeBlock = msgPosition - overflowingChars >= codeStartIndex && msgPosition - overflowingChars <= codeEndIndex;
			if (wasInsideCodeBlock != isInsideCodeBlock)
				overflowingChars = calcTotalLength(line, sb, isInsideCodeBlock) - MAX_MESSAGE_LENGTH;
			// Deviation: Java throws StringIndexOutOfBoundsException for a negative length, which cannot happen with Discord's limits
			int64_t keep = std::max<int64_t>(0, lineLength - overflowingChars - 1);
			sb.append(line, 0, static_cast<size_t>(keep));
			sb += u'…';
		}
		if (isInsideCodeBlock) {
			std::u16string codeBlockStartPlusNewLine = codeBlockStart + u'\n';
			// Deviation: Java compares sb.lastIndexOf(...) with the expected end position, which also matches (and then throws) if sb is one character
			// shorter than the searched string and does not contain it
			if (sb.ends_with(codeBlockStartPlusNewLine)) {
				// don't generate an empty code block at the end of the string
				sb.resize(sb.size() - codeBlockStartPlusNewLine.size());
				if (!sb.empty() && sb.back() == u'\n') // remove empty line
					sb.pop_back();
			} else {
				sb += CODE_BLOCK_END;
			}
		}
		if (!sb.empty() || line.empty()) // don't add an empty messagePart if it's because of a removed empty code block (see above)
			messageParts.push_back(StringUtils::toUtf8(sb));
		sb.clear();
		if (!isNewMessagePart && overflowingChars > 0) { // this line will be the start of a new messagePart
			msgPosition -= lineLength + (i == 0 ? 0 : 1); // avoid duplicate count
			i--;
		}
	}
	return messageParts;
}

std::string DiscordChannelAppender::toJson(const Message& message) {
	// Deviation: Java puts the values in Map.of, which throws a NullPointerException (so nothing is sent) if user name or avatar are null
	nlohmann::json json = {{"content", message.content}};
	if (message.username)
		json["username"] = *message.username;
	if (message.avatarUrl)
		json["avatar_url"] = *message.avatarUrl;
	return json.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

void DiscordChannelAppender::Shared::sendMessage(const Message& message) {
	if (isRateLimited())
		return;
	try {
		HttpResponse response = httpClient(webhookUrl, toJson(message));
		if (!response.error.empty())
			throw utils::IOException(response.error);
		handleResponse(response);
	} catch (const std::exception& e) {
		if (aborting) // Java: InterruptedException is ignored
			return;
		if (message.content.find(ERROR_HEADER) == std::string::npos) // avoid potential recursive message sending (if appender sends warnings)
			log.warn(std::string(ERROR_HEADER) + message.content + "\nCaused by: " + e.what());
	}
}

void DiscordChannelAppender::Shared::handleResponse(const HttpResponse& response) {
	if (response.statusCode == 429) {
		int64_t now = utils::currentTimeMillis();
		// Deviation: Java treats Retry-After as milliseconds and X-RateLimit-Reset as an integer, but Discord sends seconds (X-RateLimit-Reset with
		// fractions, which Java fails to parse). Values that are not finite or out of range are ignored or limited.
		double retryAfterSeconds = headerAsSeconds(response.retryAfter, MAX_RATE_LIMIT_SECONDS);
		int64_t resetTime;
		if (retryAfterSeconds > 0)
			resetTime = now + static_cast<int64_t>(retryAfterSeconds * 1000);
		else
			resetTime = static_cast<int64_t>(headerAsSeconds(response.rateLimitReset, static_cast<double>(now) / 1000 + MAX_RATE_LIMIT_SECONDS) * 1000);
		floodResetTimeMillis = resetTime > now ? resetTime : now + 3000;
		throw utils::IOException("Flood control for channel triggered, reset in " + std::to_string((floodResetTimeMillis - now) / 1000) +
			"s. Meanwhile, all messages will be dropped.");
	} else if (response.statusCode != 204) {
		throw utils::IOException("Server returned status code " + std::to_string(response.statusCode) + (response.body.empty() ? "" : ": " + response.body));
	}
}

} // namespace aion::commons::logging
