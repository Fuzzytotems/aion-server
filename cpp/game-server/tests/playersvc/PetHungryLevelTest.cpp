// M5j stage 3 CP1 (m5j-plan.md §18.4, P5-08 rest): the PetHungryLevel companion - Java PetHungryLevel.getValue, getNextValue and fromId.

#include <gtest/gtest.h>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/toypet/PetHungryLevelInfo.h"

namespace aion::gameserver::services::toypet {
namespace {

/** PetHungryLevel.java:8-11: HUNGRY(0), CONTENT(1), SEMIFULL(2), FULL(3) */
TEST(PetHungryLevelTest, TheValues) {
	EXPECT_EQ(getValue(PetHungryLevel::HUNGRY), 0);
	EXPECT_EQ(getValue(PetHungryLevel::CONTENT), 1);
	EXPECT_EQ(getValue(PetHungryLevel::SEMIFULL), 2);
	EXPECT_EQ(getValue(PetHungryLevel::FULL), 3);
}

/** PetHungryLevel.java:24-38: the cycle back to HUNGRY after FULL */
TEST(PetHungryLevelTest, TheNextValue) {
	EXPECT_EQ(getNextValue(PetHungryLevel::HUNGRY), PetHungryLevel::CONTENT);
	EXPECT_EQ(getNextValue(PetHungryLevel::CONTENT), PetHungryLevel::SEMIFULL);
	EXPECT_EQ(getNextValue(PetHungryLevel::SEMIFULL), PetHungryLevel::FULL);
	EXPECT_EQ(getNextValue(PetHungryLevel::FULL), PetHungryLevel::HUNGRY);
}

/** PetHungryLevel.java:40-42: values()[value], ArrayIndexOutOfBoundsException outside 0..3 */
TEST(PetHungryLevelTest, FromId) {
	EXPECT_EQ(petHungryLevelFromId(0), PetHungryLevel::HUNGRY);
	EXPECT_EQ(petHungryLevelFromId(3), PetHungryLevel::FULL);
	EXPECT_THROW(petHungryLevelFromId(4), runtime::ArrayIndexOutOfBoundsException);
	EXPECT_THROW(petHungryLevelFromId(-1), runtime::ArrayIndexOutOfBoundsException);
}

} // namespace
} // namespace aion::gameserver::services::toypet
