#include "aion/commons/configuration/transformers/EnumTransformer.h"

#include <gtest/gtest.h>

#include "aion/commons/configuration/transformers/OptionalTransformer.h"

using namespace aion::commons;
using namespace aion::commons::configuration::transformers;

namespace {

enum class ItemQuality { JUNK, COMMON, RARE, LEGEND, UNIQUE, EPIC, MYTHIC };

std::string causeMessage(auto&& f) {
	try {
		f();
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

TEST(EnumTransformerTest, NamesAreCaseSensitive) {
	EXPECT_EQ(transform<ItemQuality>("MYTHIC"), ItemQuality::MYTHIC);
	EXPECT_EQ(transform<ItemQuality>("JUNK"), ItemQuality::JUNK);
	EXPECT_EQ(typeName<ItemQuality>(), "ItemQuality");
	EXPECT_EQ(causeMessage([] { transform<ItemQuality>("mythic"); }), "No enum constant ItemQuality.mythic");
	EXPECT_EQ(causeMessage([] { transform<ItemQuality>(" MYTHIC"); }), "No enum constant ItemQuality. MYTHIC");
}

TEST(EnumTransformerTest, EmptyValueIsNulloptOrError) {
	EXPECT_EQ(transform<std::optional<ItemQuality>>(""), std::nullopt);
	EXPECT_EQ(transform<std::optional<ItemQuality>>("RARE"), ItemQuality::RARE);
	EXPECT_EQ(typeName<std::optional<ItemQuality>>(), "ItemQuality");
	EXPECT_EQ(causeMessage([] { transform<ItemQuality>(""); }),
	          "Cannot convert empty string to enum ItemQuality (bind a std::optional to allow empty values)");
	EXPECT_THROW(transform<std::optional<ItemQuality>>("rare"), configuration::TransformationException);
}
