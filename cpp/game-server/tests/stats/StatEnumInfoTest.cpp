// StatEnum.getModifier (P5-01; m5c-plan.md C-03, D8): the StatEnum companion model/stats/container/StatEnumInfo.h. Expectations come from the
// Java switch (StatEnum.java:250-262); the skill ids are the profession skills of skill_templates.xml:204322-204385 (30001 Collection, 30002
// Essencetapping, 30003 Aethertapping, 40001-40010 the crafts, 40009 Morph Substances).

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/stats/container/StatEnumInfo.h"

namespace aion::gameserver::model::stats::container {
namespace {

TEST(StatEnumInfoTest, GetModifierGivesEachBoostedProfessionItsXpRateStat) {
	// StatEnum.java:252-260: the two collection skills share one stat
	EXPECT_EQ(getModifier(30001), StatEnum::BOOST_ESSENCETAPPING_XP_RATE);
	EXPECT_EQ(getModifier(30002), StatEnum::BOOST_ESSENCETAPPING_XP_RATE);
	EXPECT_EQ(getModifier(30003), StatEnum::BOOST_AETHERTAPPING_XP_RATE);
	EXPECT_EQ(getModifier(40001), StatEnum::BOOST_COOKING_XP_RATE);
	EXPECT_EQ(getModifier(40002), StatEnum::BOOST_WEAPONSMITHING_XP_RATE);
	EXPECT_EQ(getModifier(40003), StatEnum::BOOST_ARMORSMITHING_XP_RATE);
	EXPECT_EQ(getModifier(40004), StatEnum::BOOST_TAILORING_XP_RATE);
	EXPECT_EQ(getModifier(40007), StatEnum::BOOST_ALCHEMY_XP_RATE);
	EXPECT_EQ(getModifier(40008), StatEnum::BOOST_HANDICRAFTING_XP_RATE);
	EXPECT_EQ(getModifier(40010), StatEnum::BOOST_MENUISIER_XP_RATE);
}

TEST(StatEnumInfoTest, GetModifierIsNullForEveryOtherSkill) {
	// StatEnum.java:261 (default -> null): 40005, 40006 and 40009 are profession skills of skill_templates.xml without a boost stat; 9889 is a
	// potion's "Healing" (skill_templates.xml:91905); the other ids are no skill at all
	for (int32_t skillId : {0, -1, 9889, 29999, 30000, 30004, 40000, 40005, 40006, 40009, 40011})
		EXPECT_EQ(getModifier(skillId), std::nullopt) << "skill " << skillId;
}

} // namespace
} // namespace aion::gameserver::model::stats::container
