#include "aion/commons/configuration/transformers/CommaSeparatedValueTransformer.h"

#include <gtest/gtest.h>

using namespace aion::commons::configuration::transformers;
using Tokens = std::vector<std::string>;

TEST(CommaSeparatedValueTransformerTest, JavaDocExample) {
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("a,b,\"c,d\", e , \" f \" "), (Tokens{"a", "b", "c,d", "e", " f "}));
}

TEST(CommaSeparatedValueTransformerTest, EmptyTokens) {
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues(""), Tokens{});
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("   "), Tokens{});
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("a,"), (Tokens{"a"}));
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("a, "), (Tokens{"a"}));
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues(",a"), (Tokens{"", "a"}));
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("a,,b"), (Tokens{"a", "", "b"}));
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues(" , "), (Tokens{""}));
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues(",,"), (Tokens{"", ""}));
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("a,,"), (Tokens{"a", ""}));
	// the last token is dropped if it is empty after quote stripping
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("\"\""), Tokens{});
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("a,\"\""), (Tokens{"a"}));
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("\"\",a"), (Tokens{"", "a"}));
}

TEST(CommaSeparatedValueTransformerTest, Quotes) {
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("\"a\"b\""), (Tokens{"a\"b"}));
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("\"a"), (Tokens{"\"a"}));
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("\"a,b"), (Tokens{"\"a,b"})); // unbalanced quote: no more splitting
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("x\"y,z\"w,v"), (Tokens{"x\"y,z\"w", "v"}));
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("\""), (Tokens{"\""}));
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("\"\"\"\""), (Tokens{"\"\""}));
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues(" \" a \" ,b"), (Tokens{" a ", "b"}));
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("\"0 0 0,12,20 ? * *\""), (Tokens{"0 0 0,12,20 ? * *"}));
}

TEST(CommaSeparatedValueTransformerTest, TrimsControlCharactersAndKeepsUtf8) {
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("\t\x01"
	                                                             "a\n,\xC3\xA4 "),
	          (Tokens{"a", "\xC3\xA4"}));
	EXPECT_EQ(CommaSeparatedValueTransformer::splitAndTrimValues("%s, \xC2\xBBJDev\xC2\xAB\xEE\x81\x8A%s"),
	          (Tokens{"%s", "\xC2\xBBJDev\xC2\xAB\xEE\x81\x8A%s"}));
}
