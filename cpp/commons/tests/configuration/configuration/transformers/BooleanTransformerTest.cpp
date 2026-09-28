#include "aion/commons/configuration/transformers/BooleanTransformer.h"

#include <gtest/gtest.h>

using namespace aion::commons;
using namespace aion::commons::configuration::transformers;

TEST(BooleanTransformerTest, AcceptsTrueFalseOneZero) {
	for (std::string_view value : {"true", "TRUE", "True", "tRuE", "1"})
		EXPECT_TRUE(transform<bool>(value)) << value;
	for (std::string_view value : {"false", "FALSE", "False", "0"})
		EXPECT_FALSE(transform<bool>(value)) << value;
	EXPECT_EQ(typeName<bool>(), "boolean");
}

TEST(BooleanTransformerTest, RejectsEverythingElse) {
	for (std::string_view value : {"", "yes", "no", "on", " true", "true ", "01", "2", "-1", "t"}) {
		try {
			transform<bool>(value);
			FAIL() << value;
		} catch (const configuration::TransformationException& e) {
			EXPECT_EQ(std::string(e.what()), "Error parsing \"" + std::string(value) + "\" as boolean");
			try {
				std::rethrow_exception(e.cause());
			} catch (const utils::IllegalArgumentException& cause) {
				EXPECT_STREQ(cause.what(), "Only true, false, 1 and 0 are allowed.");
			}
		}
	}
}
