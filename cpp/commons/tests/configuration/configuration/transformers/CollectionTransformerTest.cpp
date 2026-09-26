#include <filesystem>

#include <gtest/gtest.h>

#include "aion/commons/configuration/transformers/PropertyTransformers.h"

using namespace aion::commons;
using namespace aion::commons::configuration::transformers;

namespace {

enum class Race { ELYOS, ASMODIANS };

} // namespace

TEST(CollectionTransformerTest, Vectors) {
	EXPECT_EQ(transform<std::vector<int32_t>>("1, 5"), (std::vector<int32_t>{1, 5}));
	EXPECT_EQ(transform<std::vector<int32_t>>(""), std::vector<int32_t>{});
	EXPECT_EQ(transform<std::vector<float>>("1, 1.5,0.25"), (std::vector<float>{1.0f, 1.5f, 0.25f}));
	EXPECT_EQ(transform<std::vector<std::string>>("//invis, //invul, //enemy none, //see"),
	          (std::vector<std::string>{"//invis", "//invul", "//enemy none", "//see"}));
	EXPECT_EQ(transform<std::vector<Race>>("ELYOS,ASMODIANS,ELYOS"), (std::vector<Race>{Race::ELYOS, Race::ASMODIANS, Race::ELYOS}));
	EXPECT_EQ(transform<std::vector<bool>>("true,0"), (std::vector<bool>{true, false}));
	EXPECT_EQ(transform<std::vector<std::filesystem::path>>("./data/handlers/admincommands, ./data/handlers/playercommands"),
	          (std::vector<std::filesystem::path>{"./data/handlers/admincommands", "./data/handlers/playercommands"}));
	EXPECT_EQ(transform<std::vector<std::optional<Race>>>("ELYOS,,ASMODIANS"),
	          (std::vector<std::optional<Race>>{Race::ELYOS, std::nullopt, Race::ASMODIANS}));
	EXPECT_EQ(typeName<std::vector<float>>(), "float[]");
}

TEST(CollectionTransformerTest, Sets) {
	EXPECT_EQ(transform<std::set<int32_t>>("210050000, 400010000,210050000"), (std::set<int32_t>{210050000, 400010000}));
	EXPECT_EQ(transform<std::set<int32_t>>(""), std::set<int32_t>{});
	EXPECT_EQ(transform<std::unordered_set<std::string>>("a, b,a"), (std::unordered_set<std::string>{"a", "b"}));
	EXPECT_EQ(typeName<std::set<int32_t>>(), "Set");
}

TEST(CollectionTransformerTest, InvalidElementIsReportedWithNestedCause) {
	try {
		transform<std::vector<int32_t>>("1,x");
		FAIL();
	} catch (const configuration::TransformationException& e) {
		EXPECT_STREQ(e.what(), "Error parsing \"1,x\" as int[]");
		try {
			std::rethrow_exception(e.cause());
		} catch (const configuration::TransformationException& element) {
			EXPECT_STREQ(element.what(), "Error parsing \"x\" as int");
			try {
				std::rethrow_exception(element.cause());
			} catch (const utils::IllegalArgumentException& cause) {
				EXPECT_STREQ(cause.what(), "For input string: \"x\"");
			}
		}
	}
	// middle empty elements are not dropped
	EXPECT_THROW(transform<std::set<int32_t>>("1,,2"), configuration::TransformationException);
	EXPECT_THROW(transform<std::vector<int32_t>>(" , "), configuration::TransformationException);
}
