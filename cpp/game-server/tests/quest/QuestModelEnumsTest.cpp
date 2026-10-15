// P5-06a, M5d (leftovers sweep 2026-10-14): the companions of the enums ConditionOperation and ConditionUnionType (ConditionOperation.java:20-26,
// ConditionUnionType.java:14-20): value() is name(), the static fromValue(v) is valueOf(v), which throws for a name no constant has.

#include <gtest/gtest.h>

#include <string_view>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/questEngine/model/ConditionOperationInfo.h"
#include "aion/gameserver/questEngine/model/ConditionUnionTypeInfo.h"

namespace aion::gameserver::questEngine::model {
namespace {

TEST(QuestModelEnumsTest, ConditionOperationValueIsTheName) {
	EXPECT_EQ(value(ConditionOperation::EQUAL), std::string_view("EQUAL"));
	EXPECT_EQ(value(ConditionOperation::GREATER_EQUAL), std::string_view("GREATER_EQUAL"));
	EXPECT_EQ(value(ConditionOperation::NOT_IN), std::string_view("NOT_IN"));
}

TEST(QuestModelEnumsTest, ConditionOperationFromValueIsValueOf) {
	EXPECT_EQ(fromValueConditionOperation("LESSER"), ConditionOperation::LESSER);
	EXPECT_EQ(fromValueConditionOperation("NOT_EQUAL"), ConditionOperation::NOT_EQUAL);
	EXPECT_EQ(fromValueConditionOperation("IN"), ConditionOperation::IN);
	EXPECT_THROW(fromValueConditionOperation("equal"), commons::utils::IllegalArgumentException); // valueOf is case-sensitive
	EXPECT_THROW(fromValueConditionOperation(""), commons::utils::IllegalArgumentException);
}

TEST(QuestModelEnumsTest, ConditionUnionTypeValueAndFromValue) {
	EXPECT_EQ(value(ConditionUnionType::AND), std::string_view("AND"));
	EXPECT_EQ(value(ConditionUnionType::OR), std::string_view("OR"));
	EXPECT_EQ(fromValueConditionUnionType("AND"), ConditionUnionType::AND);
	EXPECT_EQ(fromValueConditionUnionType("OR"), ConditionUnionType::OR);
	EXPECT_THROW(fromValueConditionUnionType("XOR"), commons::utils::IllegalArgumentException);
}

} // namespace
} // namespace aion::gameserver::questEngine::model
