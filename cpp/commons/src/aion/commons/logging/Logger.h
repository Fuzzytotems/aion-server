#pragma once

#include <concepts>
#include <cstddef>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <spdlog/logger.h>

namespace aion::commons::logging {

namespace detail {

template <typename... Args>
struct LastIsException : std::false_type {};

template <typename Last>
struct LastIsException<Last> : std::bool_constant<std::derived_from<std::remove_cvref_t<Last>, std::exception>> {};

template <typename First, typename Second, typename... Rest>
struct LastIsException<First, Second, Rest...> : LastIsException<Second, Rest...> {};

template <typename... Args>
concept FormatArgs = sizeof...(Args) > 0 && !LastIsException<Args...>::value;

/** Format arguments followed by an exception (slf4j: a trailing Throwable is the exception of the event) */
template <typename... Args>
concept FormatArgsAndException = sizeof...(Args) > 1 && LastIsException<Args...>::value;

template <typename Done, typename... Rest>
struct FormatStringWithoutLastImpl;

template <typename... Done, typename Last>
struct FormatStringWithoutLastImpl<std::tuple<Done...>, Last> {
	using type = fmt::format_string<Done...>;
};

template <typename... Done, typename First, typename Second, typename... Rest>
struct FormatStringWithoutLastImpl<std::tuple<Done...>, First, Second, Rest...> : FormatStringWithoutLastImpl<std::tuple<Done..., First>, Second, Rest...> {};

/** fmt::format_string for all arguments except the last one (the exception) */
template <typename... Args>
using FormatStringWithoutLast = typename FormatStringWithoutLastImpl<std::tuple<>, Args...>::type;

/**
 * @return the position in msg.payload at which the exception text starts, if msg is currently being logged on the calling thread by a Logger
 * call with an exception, otherwise std::string_view::npos. The payload is "message\nexception" (only the exception if the message is empty).
 * PatternLayout uses this to print the message and the exception separately, like logback's %msg and %ex.
 */
size_t getThrowableStart(const spdlog::details::log_msg& msg) noexcept;

} // namespace detail

/**
 * Java: org.slf4j.Logger. A cheap, copyable handle to a named logger. Supported call forms per level:
 * <pre>
 * log.info("plain message");
 * log.info("formatted {} message", arg);        // fmt syntax (slf4j uses {} placeholders too)
 * log.error("message", exception);                // appends exception type, message, stack trace and causes
 * log.error("formatted {} message", arg, exception); // slf4j: a trailing exception is logged as the exception, not formatted
 * catch (...) { log.warnCurrentException("message"); } // Java: catch (Throwable t) { log.warn("message", t); }
 * </pre>
 * Obtain instances via LoggerFactory::getLogger.
 */
class Logger {
public:
	explicit Logger(std::shared_ptr<spdlog::logger> impl) : impl(std::move(impl)) {}

	const std::string& getName() const noexcept { return impl->name(); }

	bool isTraceEnabled() const noexcept { return impl->should_log(spdlog::level::trace); }
	bool isDebugEnabled() const noexcept { return impl->should_log(spdlog::level::debug); }
	bool isInfoEnabled() const noexcept { return impl->should_log(spdlog::level::info); }
	bool isWarnEnabled() const noexcept { return impl->should_log(spdlog::level::warn); }
	bool isErrorEnabled() const noexcept { return impl->should_log(spdlog::level::err); }

	void trace(std::string_view message) const { log(spdlog::level::trace, message); }
	void trace(std::string_view message, const std::exception& e) const { log(spdlog::level::trace, message, e); }
	template <typename... Args>
		requires detail::FormatArgs<Args...>
	void trace(fmt::format_string<Args...> format, Args&&... args) const {
		impl->log(spdlog::level::trace, format, std::forward<Args>(args)...);
	}
	template <typename... Args>
		requires detail::FormatArgsAndException<Args...>
	void trace(detail::FormatStringWithoutLast<Args...> format, Args&&... args) const {
		logFormattedWithException(spdlog::level::trace, format.get(), std::forward<Args>(args)...);
	}

	void debug(std::string_view message) const { log(spdlog::level::debug, message); }
	void debug(std::string_view message, const std::exception& e) const { log(spdlog::level::debug, message, e); }
	template <typename... Args>
		requires detail::FormatArgs<Args...>
	void debug(fmt::format_string<Args...> format, Args&&... args) const {
		impl->log(spdlog::level::debug, format, std::forward<Args>(args)...);
	}
	template <typename... Args>
		requires detail::FormatArgsAndException<Args...>
	void debug(detail::FormatStringWithoutLast<Args...> format, Args&&... args) const {
		logFormattedWithException(spdlog::level::debug, format.get(), std::forward<Args>(args)...);
	}

	void info(std::string_view message) const { log(spdlog::level::info, message); }
	void info(std::string_view message, const std::exception& e) const { log(spdlog::level::info, message, e); }
	template <typename... Args>
		requires detail::FormatArgs<Args...>
	void info(fmt::format_string<Args...> format, Args&&... args) const {
		impl->log(spdlog::level::info, format, std::forward<Args>(args)...);
	}
	template <typename... Args>
		requires detail::FormatArgsAndException<Args...>
	void info(detail::FormatStringWithoutLast<Args...> format, Args&&... args) const {
		logFormattedWithException(spdlog::level::info, format.get(), std::forward<Args>(args)...);
	}

	void warn(std::string_view message) const { log(spdlog::level::warn, message); }
	void warn(std::string_view message, const std::exception& e) const { log(spdlog::level::warn, message, e); }
	template <typename... Args>
		requires detail::FormatArgs<Args...>
	void warn(fmt::format_string<Args...> format, Args&&... args) const {
		impl->log(spdlog::level::warn, format, std::forward<Args>(args)...);
	}
	template <typename... Args>
		requires detail::FormatArgsAndException<Args...>
	void warn(detail::FormatStringWithoutLast<Args...> format, Args&&... args) const {
		logFormattedWithException(spdlog::level::warn, format.get(), std::forward<Args>(args)...);
	}

	void error(std::string_view message) const { log(spdlog::level::err, message); }
	void error(std::string_view message, const std::exception& e) const { log(spdlog::level::err, message, e); }
	template <typename... Args>
		requires detail::FormatArgs<Args...>
	void error(fmt::format_string<Args...> format, Args&&... args) const {
		impl->log(spdlog::level::err, format, std::forward<Args>(args)...);
	}
	template <typename... Args>
		requires detail::FormatArgsAndException<Args...>
	void error(detail::FormatStringWithoutLast<Args...> format, Args&&... args) const {
		logFormattedWithException(spdlog::level::err, format.get(), std::forward<Args>(args)...);
	}

	/** Log the exception currently being handled (inside a catch block), including non-std exceptions. Java: catch (Throwable t) */
	void traceCurrentException(std::string_view message) const { logCurrentException(spdlog::level::trace, message); }
	void debugCurrentException(std::string_view message) const { logCurrentException(spdlog::level::debug, message); }
	void infoCurrentException(std::string_view message) const { logCurrentException(spdlog::level::info, message); }
	void warnCurrentException(std::string_view message) const { logCurrentException(spdlog::level::warn, message); }
	void errorCurrentException(std::string_view message) const { logCurrentException(spdlog::level::err, message); }

	/**
	 * Logs a message with an exception that is already formatted like utils::toStackTraceString (for code that describes exceptions itself, e.g.
	 * with an additional stack trace). The text is handled like the exception of the other overloads.
	 */
	void logWithThrowableText(spdlog::level::level_enum level, std::string_view message, std::string_view throwableText) const;

	/** Access to the underlying spdlog logger, for the logging configuration only. */
	spdlog::logger& spdlogLogger() const noexcept { return *impl; }

private:
	void log(spdlog::level::level_enum level, std::string_view message) const;
	void log(spdlog::level::level_enum level, std::string_view message, const std::exception& e) const;
	void logCurrentException(spdlog::level::level_enum level, std::string_view message) const;

	template <typename... Args>
	void logFormattedWithException(spdlog::level::level_enum level, fmt::string_view format, Args&&... args) const {
		if (!impl->should_log(level))
			return;
		auto arguments = std::forward_as_tuple(args...);
		const size_t exceptionIndex = sizeof...(Args) - 1;
		log(level, formatArguments(format, arguments, std::make_index_sequence<exceptionIndex>()), std::get<exceptionIndex>(arguments));
	}

	template <typename Tuple, size_t... I>
	static std::string formatArguments(fmt::string_view format, Tuple& arguments, std::index_sequence<I...>) {
		return fmt::vformat(format, fmt::make_format_args(std::get<I>(arguments)...));
	}

	std::shared_ptr<spdlog::logger> impl;
};

} // namespace aion::commons::logging
