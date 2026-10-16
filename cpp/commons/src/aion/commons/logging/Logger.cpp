#include "aion/commons/logging/Logger.h"

#include "aion/commons/utils/Exception.h"

namespace aion::commons::logging {

namespace {

/** The message with an exception that the calling thread is currently logging (see detail::getThrowableStart) */
struct ThrowableEvent {
	const char* payload = nullptr;
	size_t payloadSize = 0;
	size_t throwableStart = std::string_view::npos;
};

thread_local ThrowableEvent currentThrowableEvent;

/** Publishes the event for the duration of the logging call and restores the previous one afterwards (in case a sink logs itself) */
class ThrowableEventScope {
public:
	ThrowableEventScope(std::string_view payload, size_t throwableStart) noexcept : previous(currentThrowableEvent) {
		currentThrowableEvent = {payload.data(), payload.size(), throwableStart};
	}
	~ThrowableEventScope() { currentThrowableEvent = previous; }

	ThrowableEventScope(const ThrowableEventScope&) = delete;
	ThrowableEventScope& operator=(const ThrowableEventScope&) = delete;

private:
	ThrowableEvent previous;
};

} // namespace

namespace detail {

size_t getThrowableStart(const spdlog::details::log_msg& msg) noexcept {
	const ThrowableEvent& event = currentThrowableEvent;
	if (event.payload && msg.payload.data() == event.payload && msg.payload.size() == event.payloadSize)
		return event.throwableStart;
	return std::string_view::npos;
}

} // namespace detail

void Logger::log(spdlog::level::level_enum level, std::string_view message) const {
	impl->log(level, message);
}

void Logger::log(spdlog::level::level_enum level, std::string_view message, const std::exception& e) const {
	if (!impl->should_log(level))
		return;
	logWithThrowableText(level, message, utils::toStackTraceString(e));
}

void Logger::logWithThrowableText(spdlog::level::level_enum level, std::string_view message, std::string_view throwableText) const {
	if (!impl->should_log(level))
		return;
	std::string text(message);
	if (!text.empty())
		text += '\n';
	size_t throwableStart = text.size();
	text += throwableText;
	ThrowableEventScope scope(text, throwableStart);
	impl->log(level, std::string_view(text));
}

void Logger::logCurrentException(spdlog::level::level_enum level, std::string_view message) const {
	if (!impl->should_log(level))
		return;
	try {
		throw;
	} catch (const std::exception& e) {
		log(level, message, e);
	} catch (...) {
		logWithThrowableText(level, message, "<unknown exception type>");
	}
}

} // namespace aion::commons::logging
