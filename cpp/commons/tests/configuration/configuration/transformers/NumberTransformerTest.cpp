#include "aion/commons/configuration/transformers/NumberTransformer.h"

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include "aion/commons/utils/Exception.h"

using namespace aion::commons;
using namespace aion::commons::configuration::transformers;

namespace {

template <typename F>
std::string errorMessage(F&& f) {
	try {
		f();
	} catch (const utils::IllegalArgumentException& e) {
		return e.what();
	}
	return "<no exception>";
}

} // namespace

TEST(NumberTransformerTest, DecodeIntAcceptsJavaSyntax) {
	EXPECT_EQ(NumberParser::decodeInt("0"), 0);
	EXPECT_EQ(NumberParser::decodeInt("-0"), 0);
	EXPECT_EQ(NumberParser::decodeInt("+5"), 5);
	EXPECT_EQ(NumberParser::decodeInt("123"), 123);
	EXPECT_EQ(NumberParser::decodeInt("-123"), -123);
	EXPECT_EQ(NumberParser::decodeInt("010"), 8);
	EXPECT_EQ(NumberParser::decodeInt("00"), 0);
	EXPECT_EQ(NumberParser::decodeInt("0777"), 511);
	EXPECT_EQ(NumberParser::decodeInt("0x1F"), 31);
	EXPECT_EQ(NumberParser::decodeInt("0X1f"), 31);
	EXPECT_EQ(NumberParser::decodeInt("#ff"), 255);
	EXPECT_EQ(NumberParser::decodeInt("-#10"), -16);
	EXPECT_EQ(NumberParser::decodeInt("+0x10"), 16);
	EXPECT_EQ(NumberParser::decodeInt("0x0000000000000001"), 1);
	EXPECT_EQ(NumberParser::decodeInt("2147483647"), std::numeric_limits<int32_t>::max());
	EXPECT_EQ(NumberParser::decodeInt("-2147483648"), std::numeric_limits<int32_t>::min());
	EXPECT_EQ(NumberParser::decodeInt("0x7fffffff"), std::numeric_limits<int32_t>::max());
	EXPECT_EQ(NumberParser::decodeInt("-0x80000000"), std::numeric_limits<int32_t>::min());
	EXPECT_EQ(NumberParser::decodeInt("-020000000000"), std::numeric_limits<int32_t>::min());
}

TEST(NumberTransformerTest, DecodeIntErrorsMatchJava) {
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt(""); }), "Zero length string");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("-"); }), "For input string: \"-\"");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("+"); }), "For input string: \"\"");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("0x"); }), "For input string: \"\" under radix 16");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("-0x"); }), "For input string: \"-\" under radix 16");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("--1"); }), "Sign character in wrong position");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("0x-1"); }), "Sign character in wrong position");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("-#+1"); }), "Sign character in wrong position");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("0-5"); }), "Sign character in wrong position");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("abc"); }), "For input string: \"abc\"");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt(" 1"); }), "For input string: \" 1\"");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("1 "); }), "For input string: \"1 \""); // no trimming, like Java
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("1.0"); }), "For input string: \"1.0\"");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("08"); }), "For input string: \"8\" under radix 8");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("0b101"); }), "For input string: \"b101\" under radix 8");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("0x1G"); }), "For input string: \"1G\" under radix 16");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("2147483648"); }), "For input string: \"2147483648\"");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("-2147483649"); }), "For input string: \"-2147483649\"");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("0x80000000"); }), "For input string: \"80000000\" under radix 16");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeInt("99999999999999999999999"); }), "For input string: \"99999999999999999999999\"");
}

TEST(NumberTransformerTest, DecodeLong) {
	EXPECT_EQ(NumberParser::decodeLong("9223372036854775807"), std::numeric_limits<int64_t>::max());
	EXPECT_EQ(NumberParser::decodeLong("-9223372036854775808"), std::numeric_limits<int64_t>::min());
	EXPECT_EQ(NumberParser::decodeLong("-0x8000000000000000"), std::numeric_limits<int64_t>::min());
	EXPECT_EQ(NumberParser::decodeLong("0x7FFFFFFFFFFFFFFF"), std::numeric_limits<int64_t>::max());
	EXPECT_EQ(NumberParser::decodeLong("999999999"), 999999999);
	EXPECT_EQ(NumberParser::decodeLong("86400000"), 86400000);
	EXPECT_EQ(errorMessage([] { NumberParser::decodeLong("9223372036854775808"); }), "For input string: \"9223372036854775808\"");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeLong("0x8000000000000000"); }), "For input string: \"8000000000000000\" under radix 16");
}

TEST(NumberTransformerTest, DecodeByteAndShortRangeChecks) {
	EXPECT_EQ(NumberParser::decodeByte("127"), 127);
	EXPECT_EQ(NumberParser::decodeByte("-128"), -128);
	EXPECT_EQ(NumberParser::decodeByte("0x7f"), 127);
	EXPECT_EQ(errorMessage([] { NumberParser::decodeByte("128"); }), "Value 128 out of range from input 128");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeByte("0xFF"); }), "Value 255 out of range from input 0xFF");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeByte("x"); }), "For input string: \"x\"");
	EXPECT_EQ(NumberParser::decodeShort("-32768"), -32768);
	EXPECT_EQ(NumberParser::decodeShort("32767"), 32767);
	EXPECT_EQ(errorMessage([] { NumberParser::decodeShort("-32769"); }), "Value -32769 out of range from input -32769");
}

TEST(NumberTransformerTest, DecodeUnsigned) {
	EXPECT_EQ(NumberParser::decodeUnsigned("255", 255), 255u);
	EXPECT_EQ(NumberParser::decodeUnsigned("-0", 255), 0u);
	EXPECT_EQ(NumberParser::decodeUnsigned("0xFFFFFFFFFFFFFFFF", UINT64_MAX), UINT64_MAX);
	EXPECT_EQ(NumberParser::decodeUnsigned("18446744073709551615", UINT64_MAX), UINT64_MAX);
	EXPECT_EQ(errorMessage([] { NumberParser::decodeUnsigned("256", 255); }), "Value 256 out of range from input 256");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeUnsigned("-1", 255); }), "Value -1 out of range from input -1");
	EXPECT_EQ(errorMessage([] { NumberParser::decodeUnsigned("18446744073709551616", UINT64_MAX); }), "For input string: \"18446744073709551616\"");
}

TEST(NumberTransformerTest, ParseDoubleAcceptsJavaSyntax) {
	EXPECT_EQ(NumberParser::parseDouble("1.5"), 1.5);
	EXPECT_EQ(NumberParser::parseDouble("  1.5 \t"), 1.5);
	EXPECT_EQ(NumberParser::parseDouble("\x01"
	                                    "2\x1F"),
	          2.0); // trim removes all chars <= ' '
	EXPECT_EQ(NumberParser::parseDouble("1e3"), 1000.0);
	EXPECT_EQ(NumberParser::parseDouble("1E+3"), 1000.0);
	EXPECT_EQ(NumberParser::parseDouble("25e-2"), 0.25);
	EXPECT_EQ(NumberParser::parseDouble(".5"), 0.5);
	EXPECT_EQ(NumberParser::parseDouble("5."), 5.0);
	EXPECT_EQ(NumberParser::parseDouble("+5"), 5.0);
	EXPECT_EQ(NumberParser::parseDouble("007.50"), 7.5);
	EXPECT_EQ(NumberParser::parseDouble("1f"), 1.0);
	EXPECT_EQ(NumberParser::parseDouble("1.5D"), 1.5);
	EXPECT_EQ(NumberParser::parseDouble("2e1d"), 20.0);
	EXPECT_EQ(NumberParser::parseDouble("0.01"), 0.01);
	EXPECT_EQ(NumberParser::parseDouble("0.1"), 0.1);
	double negativeZero = NumberParser::parseDouble("-0.0");
	EXPECT_EQ(negativeZero, 0.0);
	EXPECT_TRUE(std::signbit(negativeZero));
	EXPECT_TRUE(std::isnan(NumberParser::parseDouble("NaN")));
	EXPECT_TRUE(std::isnan(NumberParser::parseDouble("-NaN")));
	EXPECT_EQ(NumberParser::parseDouble("Infinity"), std::numeric_limits<double>::infinity());
	EXPECT_EQ(NumberParser::parseDouble("+Infinity"), std::numeric_limits<double>::infinity());
	EXPECT_EQ(NumberParser::parseDouble("-Infinity"), -std::numeric_limits<double>::infinity());
	// hexadecimal floating point
	EXPECT_EQ(NumberParser::parseDouble("0x1.8p1"), 3.0);
	EXPECT_EQ(NumberParser::parseDouble("0X1P-2"), 0.25);
	EXPECT_EQ(NumberParser::parseDouble("0x.8p1"), 1.0);
	EXPECT_EQ(NumberParser::parseDouble("-0x10p+0f"), -16.0);
	EXPECT_EQ(NumberParser::parseDouble("0x1.fffffffffffffp1023"), std::numeric_limits<double>::max());
	// overflow and underflow
	EXPECT_EQ(NumberParser::parseDouble("1e400"), std::numeric_limits<double>::infinity());
	EXPECT_EQ(NumberParser::parseDouble("-1e400"), -std::numeric_limits<double>::infinity());
	EXPECT_EQ(NumberParser::parseDouble("1e99999999999999999999"), std::numeric_limits<double>::infinity());
	EXPECT_EQ(NumberParser::parseDouble("0.000001e-99999999999999999999"), 0.0);
	EXPECT_EQ(NumberParser::parseDouble("1e-400"), 0.0);
	double tiny = NumberParser::parseDouble("-1e-400");
	EXPECT_EQ(tiny, 0.0);
	EXPECT_TRUE(std::signbit(tiny));
	EXPECT_EQ(NumberParser::parseDouble("4.9e-324"), std::numeric_limits<double>::denorm_min());
	EXPECT_EQ(NumberParser::parseDouble("0e99999999999"), 0.0);
	EXPECT_EQ(NumberParser::parseDouble("0x1p99999"), std::numeric_limits<double>::infinity());
	EXPECT_EQ(NumberParser::parseDouble("0x1p-99999"), 0.0);
}

TEST(NumberTransformerTest, ParseDoubleErrorsMatchJava) {
	EXPECT_EQ(errorMessage([] { NumberParser::parseDouble(""); }), "empty String");
	EXPECT_EQ(errorMessage([] { NumberParser::parseDouble("  "); }), "empty String");
	EXPECT_EQ(errorMessage([] { NumberParser::parseDouble("1.2.3"); }), "multiple points");
	for (std::string_view input : {"+",     "-",   ".",   "nan", "NaNd",  "Infinityx", "inf",       "1e",    "1e+",   "1ef", "1,5",
	                               "1.5ff", "1d5", "abc", "0x",  "0x1.8", "0xp1",      "0x1.2.3p1", "0x1pf", "1_000", "--1", "e5"}) {
		EXPECT_EQ(errorMessage([&] { NumberParser::parseDouble(input); }), "For input string: \"" + std::string(input) + "\"") << input;
	}
	// the message contains the trimmed input
	EXPECT_EQ(errorMessage([] { NumberParser::parseDouble(" x "); }), "For input string: \"x\"");
}

TEST(NumberTransformerTest, ParseFloatRoundsDirectlyToFloat) {
	EXPECT_EQ(NumberParser::parseFloat("1.1"), 1.1f);
	EXPECT_EQ(NumberParser::parseFloat("0.1f"), 0.1f);
	EXPECT_EQ(NumberParser::parseFloat("3.4028235e38"), std::numeric_limits<float>::max());
	EXPECT_EQ(NumberParser::parseFloat("3.5e38"), std::numeric_limits<float>::infinity());
	EXPECT_EQ(NumberParser::parseFloat("1e-46"), 0.0f);
	EXPECT_EQ(NumberParser::parseFloat("1.4e-45"), std::numeric_limits<float>::denorm_min());
	EXPECT_TRUE(std::isnan(NumberParser::parseFloat("NaN")));
}

TEST(NumberTransformerTest, TransformerDispatchesByType) {
	EXPECT_EQ(transform<int8_t>("-0x80"), -128);
	EXPECT_EQ(transform<int16_t>("0x7FFF"), 32767);
	EXPECT_EQ(transform<int32_t>("#7FFFFFFF"), 2147483647);
	EXPECT_EQ(transform<int64_t>("-9223372036854775808"), std::numeric_limits<int64_t>::min());
	EXPECT_EQ(transform<uint8_t>("255"), 255);
	EXPECT_EQ(transform<uint16_t>("0xFFFF"), 65535);
	EXPECT_EQ(transform<uint32_t>("4294967295"), 4294967295u);
	EXPECT_EQ(transform<uint64_t>("18446744073709551615"), UINT64_MAX);
	EXPECT_EQ(transform<long>("-5"), -5L);
	EXPECT_EQ(transform<float>("2.5"), 2.5f);
	EXPECT_EQ(transform<double>("2.5"), 2.5);
	EXPECT_EQ(typeName<int8_t>(), "byte");
	EXPECT_EQ(typeName<int16_t>(), "short");
	EXPECT_EQ(typeName<int32_t>(), "int");
	EXPECT_EQ(typeName<int64_t>(), "long");
	EXPECT_EQ(typeName<uint16_t>(), "uint16_t");
	EXPECT_EQ(typeName<float>(), "float");
	EXPECT_EQ(typeName<double>(), "double");
}

TEST(NumberTransformerTest, TransformWrapsErrors) {
	try {
		transform<int32_t>("12x");
		FAIL();
	} catch (const configuration::TransformationException& e) {
		EXPECT_STREQ(e.what(), "Error parsing \"12x\" as int");
		ASSERT_TRUE(e.cause());
		try {
			std::rethrow_exception(e.cause());
		} catch (const utils::IllegalArgumentException& cause) {
			EXPECT_STREQ(cause.what(), "For input string: \"12x\"");
		}
	}
	EXPECT_THROW(transform<int8_t>("128"), configuration::TransformationException);
	EXPECT_THROW(transform<uint32_t>("4294967296"), configuration::TransformationException);
	EXPECT_THROW(transform<float>("1.0.0"), configuration::TransformationException);
}
