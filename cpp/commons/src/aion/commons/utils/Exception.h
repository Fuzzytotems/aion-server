#pragma once

#include <exception>
#include <stacktrace>
#include <stdexcept>
#include <string>

namespace aion::commons::utils {

/**
 * Base class for all exceptions thrown by project code. It plays the role of Java's RuntimeException: it captures the stack trace at
 * construction and can carry a cause, so that logged errors look like Java stack traces ("Caused by: ...").
 */
class Exception : public std::runtime_error {
public:
	explicit Exception(const std::string& message, std::stacktrace trace = std::stacktrace::current());

	/**
	 * Wraps the exception currently being handled (or the given one) as the cause. Typical use inside a catch block:
	 * <pre>catch (...) { throw Exception("Could not load x", std::current_exception()); }</pre>
	 */
	Exception(const std::string& message, std::exception_ptr cause, std::stacktrace trace = std::stacktrace::current());

	const std::stacktrace& stacktrace() const noexcept { return trace; }
	const std::exception_ptr& cause() const noexcept { return causePtr; }

private:
	std::stacktrace trace;
	std::exception_ptr causePtr;
};

/**
 * Formats an exception like Java's Throwable.printStackTrace(): type and message, the stack trace (if captured) and the cause chain.
 */
std::string toStackTraceString(const std::exception& e);

/**
 * @return the demangled-ish type name of the exception's dynamic type, without MSVC's "class " / "struct " prefixes.
 */
std::string exceptionTypeName(const std::exception& e);

/** Java: IllegalArgumentException */
class IllegalArgumentException : public Exception {
public:
	using Exception::Exception;
};

/** Java: NumberFormatException, thrown by parseInt and parseLong (aion/commons/utils/Numbers.h) */
class NumberFormatException : public IllegalArgumentException {
public:
	using IllegalArgumentException::IllegalArgumentException;
};

/** Java: IllegalStateException */
class IllegalStateException : public Exception {
public:
	using Exception::Exception;
};

/** Java: UnsupportedOperationException */
class UnsupportedOperationException : public Exception {
public:
	using Exception::Exception;
};

/** Java: IndexOutOfBoundsException */
class IndexOutOfBoundsException : public Exception {
public:
	using Exception::Exception;
};

/** Java: IOException */
class IOException : public Exception {
public:
	using Exception::Exception;
};

} // namespace aion::commons::utils
