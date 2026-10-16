#include <gtest/gtest.h>

#include "aion/commons/logging/LevelFilter.h"

using namespace aion::commons::logging;
using namespace spdlog::level;

TEST(LevelFilterTest, ExactMatch) {
	constexpr LevelFilter warnings = LevelFilter::exactly(warn);
	EXPECT_TRUE(warnings.accepts(warn));
	EXPECT_FALSE(warnings.accepts(err));
	EXPECT_FALSE(warnings.accepts(info));

	constexpr LevelFilter errors = LevelFilter::exactly(err);
	EXPECT_TRUE(errors.accepts(err));
	EXPECT_TRUE(errors.accepts(critical)); // spdlog's critical is logged as ERROR
	EXPECT_FALSE(errors.accepts(warn));
	EXPECT_EQ(LevelFilter::exactly(critical), errors);
}

TEST(LevelFilterTest, Threshold) {
	constexpr LevelFilter filter = LevelFilter::threshold(warn);
	EXPECT_FALSE(filter.accepts(trace));
	EXPECT_FALSE(filter.accepts(info));
	EXPECT_TRUE(filter.accepts(warn));
	EXPECT_TRUE(filter.accepts(err));
	EXPECT_TRUE(filter.accepts(critical));
	EXPECT_TRUE(LevelFilter{}.accepts(trace));
}
