#include <gtest/gtest.h>

#include <new>
#include <stdexcept>
#include <thread>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/ExitCode.h"
#include "aion/commons/utils/concurrent/ThreadName.h"
#include "aion/commons/utils/concurrent/UncaughtExceptionHandler.h"

using namespace aion::commons::utils;
using namespace aion::commons::utils::concurrent;

namespace {

[[noreturn]] void throwFromDeepInside() {
	throw std::out_of_range("index 7 out of range");
}

} // namespace

TEST(UncaughtExceptionHandlerTest, DescribeException) {
	std::exception_ptr exception;
	try {
		throw IllegalStateException("broken");
	} catch (...) {
		exception = std::current_exception();
	}
	std::string text = UncaughtExceptionHandler::detail::describe(exception, std::stacktrace::current());
	EXPECT_TRUE(text.starts_with("aion::commons::utils::IllegalStateException: broken\n\tat ")) << text;

	try {
		throw std::runtime_error("plain");
	} catch (...) {
		exception = std::current_exception();
	}
	text = UncaughtExceptionHandler::detail::describe(exception, std::stacktrace::current());
	EXPECT_TRUE(text.starts_with("std::runtime_error: plain\n\tat ")) << text; // the fallback trace is appended

	try {
		throw 42;
	} catch (...) {
		exception = std::current_exception();
	}
	EXPECT_EQ(UncaughtExceptionHandler::detail::describe(exception, {}), "<unknown exception type>");
	EXPECT_EQ(UncaughtExceptionHandler::detail::describe(nullptr, {}), "std::terminate called without an active exception");
}

#if GTEST_HAS_DEATH_TEST

TEST(UncaughtExceptionHandlerDeathTest, ExceptionEscapingThreadIsLoggedAndExitsWithError) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	auto crash = [] {
		UncaughtExceptionHandler::install();
		std::thread([] { // the handler is installed on threads started later, too
			setCurrentThreadName("CrashingThread");
			throwFromDeepInside();
		}).join();
	};
	// the process exits with ExitCode::ERROR_ instead of std::abort() (exit code 3 on Windows), which is independent of gtest's setting that
	// suppresses the C runtime's abort dialog
	EXPECT_EXIT(crash(), testing::ExitedWithCode(ExitCode::ERROR_),
		"Critical Error - Thread \\[CrashingThread\\] terminated abnormally:\nstd::out_of_range: index 7 out of range\n\tat ");
#ifdef _WIN32
	// terminate handlers run before the stack is unwound, so the trace shows where the exception was thrown
	EXPECT_EXIT(crash(), testing::ExitedWithCode(ExitCode::ERROR_), "out of range\n\tat .*throwFromDeepInside");
#endif
}

TEST(UncaughtExceptionHandlerDeathTest, OutOfMemoryExitsForRestart) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	auto crash = [] {
		UncaughtExceptionHandler::install();
		setCurrentThreadName("main");
		try {
			throw std::bad_alloc();
		} catch (...) {
			std::terminate(); // like an exception escaping main (death tests catch exceptions escaping the statement)
		}
	};
	// gtest's simple regular expressions (Windows) cannot span lines with arbitrary content, so check both lines separately
	EXPECT_EXIT(crash(), testing::ExitedWithCode(ExitCode::RESTART), "Critical Error - Thread \\[main\\] terminated abnormally:\nstd::bad_alloc: ");
	EXPECT_EXIT(crash(), testing::ExitedWithCode(ExitCode::RESTART), "Trying to exit gracefully with ExitCode.RESTART...");
}

TEST(UncaughtExceptionHandlerDeathTest, CaughtOutOfMemoryExitsForRestart) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	auto crash = [] {
		UncaughtExceptionHandler::uncaughtException("Worker-1", std::make_exception_ptr(std::bad_alloc()));
		std::this_thread::sleep_for(std::chrono::seconds(10)); // the exit happens asynchronously
	};
	EXPECT_EXIT(crash(), testing::ExitedWithCode(ExitCode::RESTART), "Thread \\[Worker-1\\] terminated abnormally:\nstd::bad_alloc");
}

#endif
