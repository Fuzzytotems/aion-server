#include "aion/commons/configuration/transformers/CharTransformer.h"

#include <gtest/gtest.h>

#include "aion/commons/configuration/transformers/StringTransformer.h"

using namespace aion::commons;
using namespace aion::commons::configuration::transformers;

namespace {

std::string causeMessage(std::string_view value) {
	try {
		transform<char16_t>(value);
	} catch (const configuration::TransformationException& e) {
		try {
			std::rethrow_exception(e.cause());
		} catch (const std::exception& cause) {
			return cause.what();
		}
	}
	return "<no exception>";
}

} // namespace

TEST(CharTransformerTest, ExactlyOneUtf16CodeUnit) {
	EXPECT_EQ(transform<char16_t>("a"), u'a');
	EXPECT_EQ(transform<char16_t>(" "), u' ');
	EXPECT_EQ(transform<char16_t>("\xC3\xA4"), u'ä');
	EXPECT_EQ(transform<char16_t>("\xE4\xB8\xAD"), u'中');
	EXPECT_EQ(causeMessage(""), "Cannot convert empty string to character.");
	EXPECT_EQ(causeMessage("ab"), "Too many characters in the value.");
	EXPECT_EQ(causeMessage("\xF0\x9F\x98\x80"), "Too many characters in the value."); // surrogate pair
	EXPECT_EQ(typeName<char16_t>(), "char");
}

TEST(StringTransformerTest, ValueIsUsedAsIs) {
	EXPECT_EQ(transform<std::string>(""), "");
	EXPECT_EQ(transform<std::string>("  a, b \\n "), "  a, b \\n ");
	EXPECT_EQ(typeName<std::string>(), "String");
}
