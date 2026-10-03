// The last undeclared bodies of P5-02a/P5-02b (2026-10-03, owner's request): the companions of the generated enums TargetAttribute,
// StigmaType and EffectReserved.ResourceType, pinned to the Java enums (TargetAttribute.java, StigmaType.java, EffectReserved.java).

#include <gtest/gtest.h>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/condition/TargetAttributeInfo.h"
#include "aion/gameserver/skillengine/model/EffectReserved_ResourceTypeInfo.h"
#include "aion/gameserver/skillengine/model/StigmaTypeInfo.h"

namespace aion::gameserver::skillengine {
namespace {

TEST(SkillEnumCompanionsTest, TargetAttributeValueIsTheNameAndFromValueIsValueOf) {
	using condition::TargetAttribute;
	EXPECT_EQ(condition::value(TargetAttribute::NPC), "NPC");
	EXPECT_EQ(condition::value(TargetAttribute::SELF), "SELF");
	EXPECT_EQ(condition::fromValue("ALL"), TargetAttribute::ALL);
	EXPECT_EQ(condition::fromValue("NONE"), TargetAttribute::NONE);
	EXPECT_THROW(static_cast<void>(condition::fromValue("npc")), runtime::IllegalArgumentException) << "valueOf is case-sensitive";
}

TEST(SkillEnumCompanionsTest, StigmaTypeIds) {
	EXPECT_EQ(model::getId(model::StigmaType::NONE), 0);
	EXPECT_EQ(model::getId(model::StigmaType::BASIC), 1);
	EXPECT_EQ(model::getId(model::StigmaType::ADVANCED), 2);
}

TEST(SkillEnumCompanionsTest, ResourceTypeValues) {
	using model::EffectReserved_ResourceType;
	EXPECT_EQ(model::getValue(EffectReserved_ResourceType::HP), 0);
	EXPECT_EQ(model::getValue(EffectReserved_ResourceType::MP), 1);
	EXPECT_EQ(model::getValue(EffectReserved_ResourceType::FP), 2);
	EXPECT_EQ(model::getValue(EffectReserved_ResourceType::DP), 3);
}

} // namespace
} // namespace aion::gameserver::skillengine
