#include <gtest/gtest.h>

#include <stacktrace>
#include <stdexcept>
#include <type_traits>

#include "aion/commons/utils/Exception.h"

using namespace aion::commons::utils;

#ifdef _MSC_VER
#define TEST_NOINLINE __declspec(noinline)
#else
#define TEST_NOINLINE [[gnu::noinline]]
#endif

namespace {

TEST_NOINLINE void throwIllegalStateFromHere() {
	throw IllegalStateException("thrown here");
}

TEST_NOINLINE void throwExceptionWithCauseFromHere() {
	try {
		throw std::runtime_error("cause");
	} catch (...) {
		throw Exception("wrapped here", std::current_exception());
	}
}

std::string firstFrame(const Exception& e) {
	return e.stacktrace().empty() ? std::string() : std::to_string(e.stacktrace()[0]);
}

} // namespace

TEST(ExceptionTest, StackTraceStartsInTheThrowingFunction) {
	// Java: the first element of the stack trace is the method that created the throwable
	try {
		throwIllegalStateFromHere();
		FAIL() << "no exception";
	} catch (const IllegalStateException& e) {
		EXPECT_NE(firstFrame(e).find("throwIllegalStateFromHere"), std::string::npos) << toStackTraceString(e);
	}
	try {
		throwExceptionWithCauseFromHere();
		FAIL() << "no exception";
	} catch (const Exception& e) {
		EXPECT_NE(firstFrame(e).find("throwExceptionWithCauseFromHere"), std::string::npos) << toStackTraceString(e);
	}
}

TEST(ExceptionTest, StackTraceStringLikeJava) {
	try {
		throwExceptionWithCauseFromHere();
	} catch (const Exception& e) {
		std::string text = toStackTraceString(e);
		EXPECT_TRUE(text.starts_with("aion::commons::utils::Exception: wrapped here\n\tat ")) << text;
		EXPECT_NE(text.find("\nCaused by: std::runtime_error: cause"), std::string::npos) << text;
	}
	EXPECT_EQ(toStackTraceString(std::out_of_range("x")), "std::out_of_range: x");
}

TEST(ExceptionTest, JavaTypeHierarchy) {
	// Java: ArithmeticException extends RuntimeException, so catch (RuntimeException e) catches it
	try {
		throw ArithmeticException("/ by zero");
	} catch (const Exception& e) {
		EXPECT_STREQ(e.what(), "/ by zero");
		EXPECT_EQ(exceptionTypeName(e), "aion::commons::utils::ArithmeticException");
	}
	EXPECT_FALSE((std::is_base_of_v<IllegalArgumentException, ArithmeticException>));
	EXPECT_TRUE((std::is_base_of_v<IllegalArgumentException, NumberFormatException>));
	EXPECT_TRUE((std::is_base_of_v<std::runtime_error, ArithmeticException>));
}
