#include "aion/commons/configuration/transformers/PatternTransformer.h"

#include <gtest/gtest.h>

#include "aion/commons/configuration/transformers/OptionalTransformer.h"

using namespace aion::commons;
using namespace aion::commons::configuration::transformers;

TEST(PatternTransformerTest, CompilesEcmaScript) {
	std::regex pattern = transform<std::regex>("[a-zA-Z]{2,16}");
	EXPECT_TRUE(std::regex_match("Bob", pattern));
	EXPECT_FALSE(std::regex_match("B", pattern));
	EXPECT_FALSE(std::regex_match("Bob1", pattern));
	EXPECT_EQ(typeName<std::regex>(), "Pattern");
}

TEST(PatternTransformerTest, EmptyValueIsNulloptOrError) {
	EXPECT_FALSE(transform<std::optional<std::regex>>("").has_value());
	ASSERT_TRUE(transform<std::optional<std::regex>>(".{1,32}").has_value());
	EXPECT_THROW(transform<std::regex>(""), configuration::TransformationException);
	EXPECT_FALSE(transform<std::optional<std::wregex>>("").has_value());
}

TEST(PatternTransformerTest, SyntaxErrorsAreWrapped) {
	try {
		transform<std::optional<std::regex>>("[a-z");
		FAIL();
	} catch (const configuration::TransformationException& e) {
		EXPECT_STREQ(e.what(), "Error parsing \"[a-z\" as Pattern");
		EXPECT_THROW(std::rethrow_exception(e.cause()), std::regex_error);
	}
}

TEST(PatternTransformerTest, WideRegexCountsUtf16CodeUnitsLikeJava) {
	std::string umlauts = "\xC3\xA4\xC3\xB6\xC3\xBC"; // 3 characters, 6 UTF-8 bytes
	EXPECT_FALSE(std::regex_match(umlauts, transform<std::regex>("^.{1,3}$")));
	std::wregex wide = transform<std::wregex>("^.{1,3}$");
	EXPECT_TRUE(std::regex_match(PatternTransformer::toWide(umlauts), wide));
	EXPECT_TRUE(std::regex_match(L"ä", transform<std::wregex>("\xC3\xA4"))); // non-ASCII pattern characters
}
