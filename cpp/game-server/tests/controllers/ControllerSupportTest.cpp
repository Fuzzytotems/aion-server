// P4-11b hand-copied Java constant tables and Java semantics helpers of controllers/ControllerSupport.h: AbnormalState.getId (single bits and
// compound masks), AttackStatus.getBaseStatus, ShieldType.getId, SiegeRace.getByRace, StatEnum.getModifier, the saturating casts and wrapping
// int arithmetic, and the null/unboxing/list helpers that stand for Java's implicit NullPointerException, NoSuchElementException and
// IndexOutOfBoundsException. Expectations are literals derived by hand (and, for the compound AbnormalState masks, by evaluating the
// constructor expressions of AbnormalState.java with a script outside the port), never computed through the tables under test.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::controllers::detail {
namespace {

using attack::AttackStatus;
using model::Race;
using model::siege::SiegeRace;
using model::stats::container::StatEnum;
using skillengine::effect::AbnormalState;
using skillengine::model::ShieldType;

TEST(ControllerSupportTest, AbnormalStateIdsOfSingleStates) {
	// AbnormalState.java: NONE(0), POISON(1 << 0) ... SANCTUARY(1 << 31)
	EXPECT_EQ(getAbnormalStateId(AbnormalState::NONE), 0);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::POISON), 0x1);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::BLEED), 0x2);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::PARALYZE), 0x4);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::SLEEP), 0x8);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::ROOT), 0x10);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::BLIND), 0x20);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::CHARM), 0x40);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::DISEASE), 0x80);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::SILENCE), 0x100);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::FEAR), 0x200);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::CURSE), 0x400);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::CONFUSE), 0x800);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::STUN), 0x1000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::PETRIFICATION), 0x2000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::STUMBLE), 0x4000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::STAGGER), 0x8000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::OPENAERIAL), 0x10000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::SNARE), 0x20000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::SLOW), 0x40000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::SPIN), 0x80000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::BIND), 0x100000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::DEFORM), 0x200000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::PULLED), 0x400000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::NOFLY), 0x800000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::SIMPLE_MOVE_BACK), 0x1000000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::STUNLIKE), 0x2000000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::CANT_MOVE_OR_ATTACK), 0x4000000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::UNK), 0x8000000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::UNK_2), 0x10000000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::HIDE), 0x20000000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::INVULNERABLE_WING), 0x40000000);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::SANCTUARY), static_cast<int32_t>(0x80000000u)) << "1 << 31 is Integer.MIN_VALUE";
}

TEST(ControllerSupportTest, AbnormalStateIdsOfCompoundStates) {
	EXPECT_EQ(getAbnormalStateId(AbnormalState::CANT_ATTACK_STATE), static_cast<int32_t>(0x8049DA0Cu));
	EXPECT_EQ(getAbnormalStateId(AbnormalState::STANCE_OFF), static_cast<int32_t>(0x8049DA04u));
	EXPECT_EQ(getAbnormalStateId(AbnormalState::CANT_MOVE_STATE), static_cast<int32_t>(0x8049D01Cu));
	EXPECT_EQ(getAbnormalStateId(AbnormalState::DISMOUNT_RIDE), 0x006BDA1C);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::AUTOMATICALLY_STANDUP), 0x00699A0C);
	EXPECT_EQ(getAbnormalStateId(AbnormalState::ANY_STUN), 0x0008D000) << "SPIN | STUN | STUMBLE | STAGGER";
}

TEST(ControllerSupportTest, StanceOffMaskOfStanceObserverAbnormalsetted) {
	// StanceObserver.abnormalsetted: (state.getId() & AbnormalState.STANCE_OFF.getId()) != 0 stops the stance
	const int32_t stanceOff = getAbnormalStateId(AbnormalState::STANCE_OFF);
	for (AbnormalState stops : {AbnormalState::STUN, AbnormalState::SPIN, AbnormalState::STUMBLE, AbnormalState::STAGGER, AbnormalState::OPENAERIAL,
	         AbnormalState::PARALYZE, AbnormalState::FEAR, AbnormalState::PULLED, AbnormalState::SANCTUARY, AbnormalState::CONFUSE,
	         AbnormalState::ANY_STUN, AbnormalState::CANT_ATTACK_STATE})
		EXPECT_NE(getAbnormalStateId(stops) & stanceOff, 0) << static_cast<int>(stops);
	for (AbnormalState keeps : {AbnormalState::NONE, AbnormalState::POISON, AbnormalState::BLEED, AbnormalState::SLEEP, AbnormalState::ROOT,
	         AbnormalState::SNARE, AbnormalState::SLOW, AbnormalState::SILENCE, AbnormalState::HIDE, AbnormalState::DEFORM})
		EXPECT_EQ(getAbnormalStateId(keeps) & stanceOff, 0) << static_cast<int>(keeps);
}

TEST(ControllerSupportTest, AttackStatusBaseStatus) {
	EXPECT_EQ(getBaseStatus(AttackStatus::DODGE), AttackStatus::DODGE);
	EXPECT_EQ(getBaseStatus(AttackStatus::CRITICAL_DODGE), AttackStatus::DODGE);
	EXPECT_EQ(getBaseStatus(AttackStatus::OFFHAND_DODGE), AttackStatus::DODGE);
	EXPECT_EQ(getBaseStatus(AttackStatus::OFFHAND_CRITICAL_DODGE), AttackStatus::DODGE);
	EXPECT_EQ(getBaseStatus(AttackStatus::RESIST), AttackStatus::RESIST);
	EXPECT_EQ(getBaseStatus(AttackStatus::CRITICAL_RESIST), AttackStatus::RESIST);
	EXPECT_EQ(getBaseStatus(AttackStatus::OFFHAND_RESIST), AttackStatus::RESIST);
	EXPECT_EQ(getBaseStatus(AttackStatus::OFFHAND_CRITICAL_RESIST), AttackStatus::RESIST);
	EXPECT_EQ(getBaseStatus(AttackStatus::PARRY), AttackStatus::PARRY);
	EXPECT_EQ(getBaseStatus(AttackStatus::CRITICAL_PARRY), AttackStatus::PARRY);
	EXPECT_EQ(getBaseStatus(AttackStatus::OFFHAND_PARRY), AttackStatus::PARRY);
	EXPECT_EQ(getBaseStatus(AttackStatus::OFFHAND_CRITICAL_PARRY), AttackStatus::PARRY);
	EXPECT_EQ(getBaseStatus(AttackStatus::BLOCK), AttackStatus::BLOCK);
	EXPECT_EQ(getBaseStatus(AttackStatus::CRITICAL_BLOCK), AttackStatus::BLOCK);
	EXPECT_EQ(getBaseStatus(AttackStatus::OFFHAND_BLOCK), AttackStatus::BLOCK);
	EXPECT_EQ(getBaseStatus(AttackStatus::OFFHAND_CRITICAL_BLOCK), AttackStatus::BLOCK);
	// default: the status itself
	EXPECT_EQ(getBaseStatus(AttackStatus::CRITICAL), AttackStatus::CRITICAL);
	EXPECT_EQ(getBaseStatus(AttackStatus::OFFHAND_CRITICAL), AttackStatus::OFFHAND_CRITICAL);
	EXPECT_EQ(getBaseStatus(AttackStatus::NORMALHIT), AttackStatus::NORMALHIT);
	EXPECT_EQ(getBaseStatus(AttackStatus::OFFHAND_NORMALHIT), AttackStatus::OFFHAND_NORMALHIT);
	EXPECT_EQ(getBaseStatus(AttackStatus::BUF), AttackStatus::BUF);
	EXPECT_EQ(getBaseStatus(AttackStatus::OFFHAND_BUF), AttackStatus::OFFHAND_BUF);
}

TEST(ControllerSupportTest, ShieldTypeIds) {
	// ShieldType.java: CONVERT(0), REFLECTOR(1 << 0), NORMAL(1 << 1), UNK(1 << 2), PROTECT(1 << 3), MPSHIELD(1 << 4), SKILL_REFLECTOR(1 << 5)
	EXPECT_EQ(shieldTypeId(ShieldType::CONVERT), 0);
	EXPECT_EQ(shieldTypeId(ShieldType::REFLECTOR), 1);
	EXPECT_EQ(shieldTypeId(ShieldType::NORMAL), 2);
	EXPECT_EQ(shieldTypeId(ShieldType::UNK), 4);
	EXPECT_EQ(shieldTypeId(ShieldType::PROTECT), 8);
	EXPECT_EQ(shieldTypeId(ShieldType::MPSHIELD), 16);
	EXPECT_EQ(shieldTypeId(ShieldType::SKILL_REFLECTOR), 32);
}

TEST(ControllerSupportTest, SiegeRaceByRace) {
	// SiegeRace.getByRace: ASMODIANS -> ASMODIANS, ELYOS -> ELYOS, default -> BALAUR
	EXPECT_EQ(siegeRaceByRace(Race::ASMODIANS), SiegeRace::ASMODIANS);
	EXPECT_EQ(siegeRaceByRace(Race::ELYOS), SiegeRace::ELYOS);
	EXPECT_EQ(siegeRaceByRace(Race::NPC), SiegeRace::BALAUR);
	EXPECT_EQ(siegeRaceByRace(Race::PC_ALL), SiegeRace::BALAUR);
	EXPECT_EQ(siegeRaceByRace(Race::DRAKAN), SiegeRace::BALAUR);
}

TEST(ControllerSupportTest, StatEnumModifierOfGatheringAndCraftingSkills) {
	EXPECT_EQ(statEnumGetModifier(30001), StatEnum::BOOST_ESSENCETAPPING_XP_RATE);
	EXPECT_EQ(statEnumGetModifier(30002), StatEnum::BOOST_ESSENCETAPPING_XP_RATE);
	EXPECT_EQ(statEnumGetModifier(30003), StatEnum::BOOST_AETHERTAPPING_XP_RATE);
	EXPECT_EQ(statEnumGetModifier(40001), StatEnum::BOOST_COOKING_XP_RATE);
	EXPECT_EQ(statEnumGetModifier(40002), StatEnum::BOOST_WEAPONSMITHING_XP_RATE);
	EXPECT_EQ(statEnumGetModifier(40003), StatEnum::BOOST_ARMORSMITHING_XP_RATE);
	EXPECT_EQ(statEnumGetModifier(40004), StatEnum::BOOST_TAILORING_XP_RATE);
	EXPECT_EQ(statEnumGetModifier(40007), StatEnum::BOOST_ALCHEMY_XP_RATE);
	EXPECT_EQ(statEnumGetModifier(40008), StatEnum::BOOST_HANDICRAFTING_XP_RATE);
	EXPECT_EQ(statEnumGetModifier(40010), StatEnum::BOOST_MENUISIER_XP_RATE);
	EXPECT_EQ(statEnumGetModifier(40005), std::nullopt) << "not in the switch: Java null";
	EXPECT_EQ(statEnumGetModifier(0), std::nullopt);
}

TEST(ControllerSupportTest, JavaNarrowingCastsSaturate) {
	EXPECT_EQ(toInt(std::numeric_limits<float>::quiet_NaN()), 0);
	EXPECT_EQ(toInt(3.99f), 3);
	EXPECT_EQ(toInt(-3.99f), -3) << "toward zero";
	EXPECT_EQ(toInt(3.0e9f), std::numeric_limits<int32_t>::max());
	EXPECT_EQ(toInt(-3.0e9f), std::numeric_limits<int32_t>::min());
	EXPECT_EQ(toInt(std::numeric_limits<double>::infinity()), std::numeric_limits<int32_t>::max());
	EXPECT_EQ(toInt(2147483647.5), std::numeric_limits<int32_t>::max());
	EXPECT_EQ(toInt(-2147483648.9), std::numeric_limits<int32_t>::min());
	EXPECT_EQ(toInt(std::numeric_limits<double>::quiet_NaN()), 0);
	EXPECT_EQ(toLong(1.0e19f), std::numeric_limits<int64_t>::max());
	EXPECT_EQ(toLong(-1.0e19f), std::numeric_limits<int64_t>::min());
	EXPECT_EQ(toLong(-2.5f), -2);
	EXPECT_EQ(toLong(std::numeric_limits<float>::quiet_NaN()), 0);
	// GatherableController.rewardPlayer: (int) (0.0031 * (skillLvl + 5.3) * (skillLvl + 1592.8) + 60), 0.0031 * 5.3 * 1592.8 + 60 = 86.17...
	EXPECT_EQ(toInt(0.0031 * (0 + 5.3) * (0 + 1592.8) + 60), 86);
	// skill level 400: 0.0031 * 405.3 * 1992.8 + 60 = 2563.8...
	EXPECT_EQ(toInt(0.0031 * (400 + 5.3) * (400 + 1592.8) + 60), 2563);
}

TEST(ControllerSupportTest, JavaIntArithmeticWraps) {
	EXPECT_EQ(add(std::numeric_limits<int32_t>::max(), 1), std::numeric_limits<int32_t>::min());
	EXPECT_EQ(sub(std::numeric_limits<int32_t>::min(), 1), std::numeric_limits<int32_t>::max());
	EXPECT_EQ(mul(65536, 65536), 0) << "2^32 wraps to 0";
	EXPECT_EQ(mul(46341, 46341), -2147479015) << "46341^2 = 2147488281 = 2^31 + 4633";
	EXPECT_EQ(div(std::numeric_limits<int32_t>::min(), -1), std::numeric_limits<int32_t>::min());
	EXPECT_EQ(div(-7, 2), -3);
}

TEST(ControllerSupportTest, NullAndUnboxingHelpersThrowNullPointerException) {
	int32_t value = 5;
	EXPECT_EQ(nonNull(&value, "value"), &value);
	EXPECT_THROW(static_cast<void>(nonNull(static_cast<const int32_t*>(nullptr), "QUEST_DATA.getQuestById(questId)")), runtime::NullPointerException);
	try {
		static_cast<void>(nonNull(static_cast<int32_t*>(nullptr), "template"));
		FAIL() << "no exception";
	} catch (const runtime::NullPointerException& e) {
		EXPECT_NE(std::string(e.what()).find("template is null"), std::string::npos) << e.what();
	}
	EXPECT_EQ(unbox(std::optional<int32_t>(7), "maxLevel"), 7);
	EXPECT_THROW(static_cast<void>(unbox(std::optional<float>(), "exitX")), runtime::NullPointerException);
}

TEST(ControllerSupportTest, ListHelpersThrowTheJavaExceptions) {
	std::vector<int32_t> list{10, 20};
	EXPECT_EQ(listGetFirst(list), 10);
	EXPECT_EQ(listGet(list, 1), 20);
	EXPECT_THROW(static_cast<void>(listGet(list, 2)), runtime::IndexOutOfBoundsException);
	EXPECT_THROW(static_cast<void>(listGet(list, -1)), runtime::IndexOutOfBoundsException);
	try {
		static_cast<void>(listGet(list, 2));
	} catch (const runtime::IndexOutOfBoundsException& e) {
		EXPECT_NE(std::string(e.what()).find("Index 2 out of bounds for length 2"), std::string::npos) << e.what();
	}
	std::vector<int32_t> empty;
	EXPECT_THROW(static_cast<void>(listGetFirst(empty)), runtime::NoSuchElementException);
}

} // namespace
} // namespace aion::gameserver::controllers::detail
