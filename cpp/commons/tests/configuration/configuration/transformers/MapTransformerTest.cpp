#include <unordered_map>

#include <gtest/gtest.h>

#include "aion/commons/configuration/transformers/PropertyTransformers.h"

using namespace aion::commons;
using namespace aion::commons::configuration::transformers;

namespace {

enum class HouseType { ESTATE, MANSION, HOUSE, STUDIO, PALACE };

} // namespace

TEST(MapTransformerTest, TransformsKeysAndValues) {
	std::map<std::string, std::string> values{{"HOUSE", "20"}, {"PALACE", "1"}};
	auto map = MapTransformer::transform<std::map<HouseType, int32_t>>(values);
	EXPECT_EQ(map, (std::map<HouseType, int32_t>{{HouseType::HOUSE, 20}, {HouseType::PALACE, 1}}));

	std::vector<std::pair<std::string, std::string>> ordered{{"0x10", "5"}, {"16", "6"}, {"0x20", "7"}};
	auto byOpcode = MapTransformer::transform<std::unordered_map<int32_t, int32_t>>(ordered);
	EXPECT_EQ(byOpcode, (std::unordered_map<int32_t, int32_t>{{16, 6}, {32, 7}})); // later entries replace earlier ones

	auto strings =
	  MapTransformer::transform<std::map<std::string, std::optional<HouseType>>>(std::map<std::string, std::string>{{"a", ""}, {"b", "STUDIO"}});
	EXPECT_EQ(strings.at("a"), std::nullopt);
	EXPECT_EQ(strings.at("b"), HouseType::STUDIO);

	auto empty = MapTransformer::transform<std::map<std::string, int8_t>>(std::map<std::string, std::string>{});
	EXPECT_TRUE(empty.empty());
}

TEST(MapTransformerTest, InvalidKey) {
	try {
		MapTransformer::transform<std::map<HouseType, int32_t>>(std::map<std::string, std::string>{{"CASTLE", "1"}});
		FAIL();
	} catch (const configuration::TransformationException& e) {
		EXPECT_STREQ(e.what(), "Error parsing \"CASTLE\" as HouseType");
	}
}

TEST(MapTransformerTest, InvalidValue) {
	try {
		MapTransformer::transform<std::map<std::string, int8_t>>(std::map<std::string, std::string>{{"ban", "1000"}});
		FAIL();
	} catch (const configuration::TransformationException& e) {
		EXPECT_STREQ(e.what(), "Could not transform property: ban");
		try {
			std::rethrow_exception(e.cause());
		} catch (const configuration::TransformationException& cause) {
			EXPECT_STREQ(cause.what(), "Error parsing \"1000\" as byte");
		}
	}
}
