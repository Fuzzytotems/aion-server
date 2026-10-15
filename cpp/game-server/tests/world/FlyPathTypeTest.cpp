// P4-10 (the leftovers sweep, 2026-10-14): FlyPathType's constructor data (FlyPathType.java:8-20), sent by CM_LEVEL_READY's
// SM_WINDSTREAM_ANNOUNCE.

#include <gtest/gtest.h>

#include "aion/gameserver/model/flypath/FlyPathTypeInfo.h"

namespace aion::gameserver::model::flypath {
namespace {

TEST(FlyPathTypeTest, Ids) {
	EXPECT_EQ(getId(FlyPathType::GEYSER), 0);
	EXPECT_EQ(getId(FlyPathType::ONE_WAY), 1);
	EXPECT_EQ(getId(FlyPathType::TWO_WAY), 2);
}

} // namespace
} // namespace aion::gameserver::model::flypath
