#include <gtest/gtest.h>

#include <limits>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"

using namespace aion::commons::utils;

namespace {

std::string parseIntError(std::string_view s, int32_t radix = 10) {
	try {
		parseInt(s, radix);
	} catch (const NumberFormatException& e) {
		return e.what();
	}
	return "no exception";
}

} // namespace

TEST(NumbersTest, ParseIntLikeJava) {
	EXPECT_EQ(parseInt("010"), 10); // decimal, unlike Integer.decode
	EXPECT_EQ(parseInt("+42"), 42);
	EXPECT_EQ(parseInt("-0"), 0);
	EXPECT_EQ(parseInt("2147483647"), std::numeric_limits<int32_t>::max());
	EXPECT_EQ(parseInt("-2147483648"), std::numeric_limits<int32_t>::min());
	EXPECT_EQ(parseInt("ff", 16), 255);
	EXPECT_EQ(parseInt("-Zz", 36), -1295);
	EXPECT_EQ(parseIntError("2147483648"), "For input string: \"2147483648\"");
	EXPECT_EQ(parseIntError("0x10"), "For input string: \"0x10\"");
	EXPECT_EQ(parseIntError(" 1"), "For input string: \" 1\"");
	EXPECT_EQ(parseIntError(""), "For input string: \"\"");
	EXPECT_EQ(parseIntError("-"), "For input string: \"-\"");
	EXPECT_EQ(parseIntError("+-1"), "For input string: \"+-1\"");
	EXPECT_EQ(parseIntError("g", 16), "For input string: \"g\" under radix 16");
	EXPECT_EQ(parseIntError("1", 1), "radix 1 less than Character.MIN_RADIX");
	EXPECT_EQ(parseIntError("1", 37), "radix 37 greater than Character.MAX_RADIX");
	EXPECT_THROW(parseInt("x"), IllegalArgumentException); // NumberFormatException extends IllegalArgumentException
}

TEST(NumbersTest, ParseLongLikeJava) {
	EXPECT_EQ(parseLong("9223372036854775807"), std::numeric_limits<int64_t>::max());
	EXPECT_EQ(parseLong("-9223372036854775808"), std::numeric_limits<int64_t>::min());
	EXPECT_EQ(parseLong("0010"), 10);
	EXPECT_THROW(parseLong("9223372036854775808"), NumberFormatException);
	EXPECT_THROW(parseLong("-9223372036854775809"), NumberFormatException);
	EXPECT_THROW(parseLong("1L"), NumberFormatException);
}
