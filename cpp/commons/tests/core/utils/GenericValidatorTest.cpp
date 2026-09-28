#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "aion/commons/utils/ExitCode.h"
#include "aion/commons/utils/GenericValidator.h"

using namespace aion::commons::utils;

TEST(GenericValidatorTest, StringsCollectionsAndNumbers) {
	EXPECT_TRUE(GenericValidator::isBlankOrNull(""));
	EXPECT_TRUE(GenericValidator::isBlankOrNull(static_cast<const char*>(nullptr)));
	EXPECT_FALSE(GenericValidator::isBlankOrNull(" ")); // like Java: not trimmed
	EXPECT_FALSE(GenericValidator::isBlankOrNull(std::string("x")));

	std::vector<int> items;
	EXPECT_TRUE(GenericValidator::isBlankOrNull(items));
	items.push_back(1);
	EXPECT_FALSE(GenericValidator::isBlankOrNull(items));
	EXPECT_TRUE(GenericValidator::isBlankOrNull(static_cast<const std::vector<int>*>(nullptr)));
	EXPECT_FALSE(GenericValidator::isBlankOrNull(&items));
	EXPECT_TRUE(GenericValidator::isBlankOrNull(std::map<int, int>{}));
	EXPECT_FALSE(GenericValidator::isBlankOrNull(std::unordered_set<int>{3}));

	EXPECT_TRUE(GenericValidator::isBlankOrNull(0));
	EXPECT_TRUE(GenericValidator::isBlankOrNull(0.0f));
	EXPECT_FALSE(GenericValidator::isBlankOrNull(int64_t{-2}));
	EXPECT_TRUE(GenericValidator::isBlankOrNull(std::optional<int32_t>{}));
	EXPECT_TRUE(GenericValidator::isBlankOrNull(std::optional<double>{0.0}));
	EXPECT_FALSE(GenericValidator::isBlankOrNull(std::optional<int32_t>{7}));
}

TEST(GenericValidatorTest, ExitCodesLikeJava) {
	EXPECT_EQ(ExitCode::NORMAL, 0);
	EXPECT_EQ(ExitCode::ERROR_, 1);
	EXPECT_EQ(ExitCode::RESTART, 2);
}
