#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/ShieldType.h"

namespace aion::gameserver::controllers::detail {

/**
 * C++ only helpers of the P4-11b bodies (included only by .cpp files of the chunk): Java numeric semantics the bodies rely on (CONVENTIONS
 * "Semantics that differ between Java and C++": saturating float/double to int casts, wrapping int arithmetic) and stand-ins for static
 * methods of enums whose companions belong to chunks that are not ported yet (each names the Java method it stands for).
 */

/**
 * Java's implicit null check of a dereference (`template.isX()` on a null lookup result): returns the pointer and throws NullPointerException
 * for null, where the plain C++ dereference would be undefined behaviour. `what` names the dereferenced expression for the message.
 */
template <class T>
T* nonNull(T* value, const char* what) {
	if (value == nullptr)
		throw runtime::NullPointerException(std::string(what) + " is null");
	return value;
}

/** Java unboxing of a nullable Integer/Float/enum (`int i = boxed`, `switch (boxed)`): the value, NullPointerException when absent */
template <class T>
T unbox(const std::optional<T>& value, const char* what) {
	if (!value)
		throw runtime::NullPointerException(std::string(what) + " is null");
	return *value;
}

/** Java List.getFirst(): NoSuchElementException on an empty list */
template <class T>
const T& listGetFirst(const std::vector<T>& list) {
	if (list.empty())
		throw runtime::NoSuchElementException("getFirst on an empty list");
	return list.front();
}

/** Java List.get(index): IndexOutOfBoundsException (JDK wording) outside [0, size) */
template <class T>
const T& listGet(const std::vector<T>& list, int32_t index) {
	if (index < 0 || static_cast<size_t>(index) >= list.size())
		throw runtime::IndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " + std::to_string(list.size()));
	return list[static_cast<size_t>(index)];
}

/** Java (int) floatValue: NaN is 0, out-of-range values saturate */
inline int32_t toInt(float value) noexcept {
	if (std::isnan(value))
		return 0;
	if (value >= 2147483648.0f)
		return std::numeric_limits<int32_t>::max();
	if (value <= -2147483648.0f)
		return std::numeric_limits<int32_t>::min();
	return static_cast<int32_t>(value);
}

/** Java (int) doubleValue: NaN is 0, out-of-range values saturate */
inline int32_t toInt(double value) noexcept {
	if (std::isnan(value))
		return 0;
	if (value >= 2147483647.0)
		return std::numeric_limits<int32_t>::max();
	if (value <= -2147483648.0)
		return std::numeric_limits<int32_t>::min();
	return static_cast<int32_t>(value);
}

/** Java (long) floatValue: NaN is 0, out-of-range values saturate */
inline int64_t toLong(float value) noexcept {
	if (std::isnan(value))
		return 0;
	if (value >= 9223372036854775808.0f)
		return std::numeric_limits<int64_t>::max();
	if (value <= -9223372036854775808.0f)
		return std::numeric_limits<int64_t>::min();
	return static_cast<int64_t>(value);
}

/** Java int a * b (wraps on overflow) */
constexpr int32_t mul(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

/** Java int a + b (wraps on overflow) */
constexpr int32_t add(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

/** Java int a - b (wraps on overflow) */
constexpr int32_t sub(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

/** Java int a / b (Integer.MIN_VALUE / -1 wraps; b == 0 must be excluded by the caller like Java's ArithmeticException) */
constexpr int32_t div(int32_t a, int32_t b) noexcept {
	return (a == std::numeric_limits<int32_t>::min() && b == -1) ? a : a / b;
}

/** Stand-in for Java AttackStatus.getBaseStatus(status) (companion of P5-01) */
constexpr attack::AttackStatus getBaseStatus(attack::AttackStatus status) noexcept {
	using attack::AttackStatus;
	switch (status) {
		case AttackStatus::DODGE:
		case AttackStatus::CRITICAL_DODGE:
		case AttackStatus::OFFHAND_DODGE:
		case AttackStatus::OFFHAND_CRITICAL_DODGE:
			return AttackStatus::DODGE;
		case AttackStatus::RESIST:
		case AttackStatus::CRITICAL_RESIST:
		case AttackStatus::OFFHAND_RESIST:
		case AttackStatus::OFFHAND_CRITICAL_RESIST:
			return AttackStatus::RESIST;
		case AttackStatus::PARRY:
		case AttackStatus::CRITICAL_PARRY:
		case AttackStatus::OFFHAND_PARRY:
		case AttackStatus::OFFHAND_CRITICAL_PARRY:
			return AttackStatus::PARRY;
		case AttackStatus::BLOCK:
		case AttackStatus::CRITICAL_BLOCK:
		case AttackStatus::OFFHAND_BLOCK:
		case AttackStatus::OFFHAND_CRITICAL_BLOCK:
			return AttackStatus::BLOCK;
		default:
			return status;
	}
}

/** Stand-in for Java AbnormalState.getId() (companion of P5-03): the bit of a single state, the union of a compound state */
constexpr int32_t getAbnormalStateId(skillengine::effect::AbnormalState state) noexcept {
	using skillengine::effect::AbnormalState;
	auto bit = [](int32_t shift) { return static_cast<int32_t>(uint32_t{1} << shift); };
	switch (state) {
		case AbnormalState::NONE:
			return 0;
		case AbnormalState::POISON:
			return bit(0);
		case AbnormalState::BLEED:
			return bit(1);
		case AbnormalState::PARALYZE:
			return bit(2);
		case AbnormalState::SLEEP:
			return bit(3);
		case AbnormalState::ROOT:
			return bit(4);
		case AbnormalState::BLIND:
			return bit(5);
		case AbnormalState::CHARM:
			return bit(6);
		case AbnormalState::DISEASE:
			return bit(7);
		case AbnormalState::SILENCE:
			return bit(8);
		case AbnormalState::FEAR:
			return bit(9);
		case AbnormalState::CURSE:
			return bit(10);
		case AbnormalState::CONFUSE:
			return bit(11);
		case AbnormalState::STUN:
			return bit(12);
		case AbnormalState::PETRIFICATION:
			return bit(13);
		case AbnormalState::STUMBLE:
			return bit(14);
		case AbnormalState::STAGGER:
			return bit(15);
		case AbnormalState::OPENAERIAL:
			return bit(16);
		case AbnormalState::SNARE:
			return bit(17);
		case AbnormalState::SLOW:
			return bit(18);
		case AbnormalState::SPIN:
			return bit(19);
		case AbnormalState::BIND:
			return bit(20);
		case AbnormalState::DEFORM:
			return bit(21);
		case AbnormalState::PULLED:
			return bit(22);
		case AbnormalState::NOFLY:
			return bit(23);
		case AbnormalState::SIMPLE_MOVE_BACK:
			return bit(24);
		case AbnormalState::STUNLIKE:
			return bit(25);
		case AbnormalState::CANT_MOVE_OR_ATTACK:
			return bit(26);
		case AbnormalState::UNK:
			return bit(27);
		case AbnormalState::UNK_2:
			return bit(28);
		case AbnormalState::HIDE:
			return bit(29);
		case AbnormalState::INVULNERABLE_WING:
			return bit(30);
		case AbnormalState::SANCTUARY:
			return bit(31);
		case AbnormalState::CANT_ATTACK_STATE:
			return getAbnormalStateId(AbnormalState::SPIN) | getAbnormalStateId(AbnormalState::STUN) | getAbnormalStateId(AbnormalState::SLEEP) | getAbnormalStateId(AbnormalState::STUMBLE) | getAbnormalStateId(AbnormalState::STAGGER) | getAbnormalStateId(AbnormalState::OPENAERIAL) | getAbnormalStateId(AbnormalState::PARALYZE) | getAbnormalStateId(AbnormalState::FEAR) | getAbnormalStateId(AbnormalState::PULLED) | getAbnormalStateId(AbnormalState::SANCTUARY) | getAbnormalStateId(AbnormalState::CONFUSE);
		case AbnormalState::STANCE_OFF:
			return getAbnormalStateId(AbnormalState::SPIN) | getAbnormalStateId(AbnormalState::STUN) | getAbnormalStateId(AbnormalState::STUMBLE) | getAbnormalStateId(AbnormalState::STAGGER) | getAbnormalStateId(AbnormalState::OPENAERIAL) | getAbnormalStateId(AbnormalState::PARALYZE) | getAbnormalStateId(AbnormalState::FEAR) | getAbnormalStateId(AbnormalState::PULLED) | getAbnormalStateId(AbnormalState::SANCTUARY) | getAbnormalStateId(AbnormalState::CONFUSE);
		case AbnormalState::CANT_MOVE_STATE:
			return getAbnormalStateId(AbnormalState::SPIN) | getAbnormalStateId(AbnormalState::ROOT) | getAbnormalStateId(AbnormalState::SLEEP) | getAbnormalStateId(AbnormalState::STUMBLE) | getAbnormalStateId(AbnormalState::STUN) | getAbnormalStateId(AbnormalState::STAGGER) | getAbnormalStateId(AbnormalState::OPENAERIAL) | getAbnormalStateId(AbnormalState::PARALYZE) | getAbnormalStateId(AbnormalState::PULLED) | getAbnormalStateId(AbnormalState::SANCTUARY);
		case AbnormalState::DISMOUNT_RIDE:
			return getAbnormalStateId(AbnormalState::SPIN) | getAbnormalStateId(AbnormalState::ROOT) | getAbnormalStateId(AbnormalState::SLEEP) | getAbnormalStateId(AbnormalState::STUMBLE) | getAbnormalStateId(AbnormalState::STUN) | getAbnormalStateId(AbnormalState::STAGGER) | getAbnormalStateId(AbnormalState::OPENAERIAL) | getAbnormalStateId(AbnormalState::PARALYZE) | getAbnormalStateId(AbnormalState::PULLED) | getAbnormalStateId(AbnormalState::FEAR) | getAbnormalStateId(AbnormalState::SNARE) | getAbnormalStateId(AbnormalState::DEFORM) | getAbnormalStateId(AbnormalState::CONFUSE);
		case AbnormalState::AUTOMATICALLY_STANDUP:
			return getAbnormalStateId(AbnormalState::PARALYZE) | getAbnormalStateId(AbnormalState::SLEEP) | getAbnormalStateId(AbnormalState::FEAR) | getAbnormalStateId(AbnormalState::STUN) | getAbnormalStateId(AbnormalState::STAGGER) | getAbnormalStateId(AbnormalState::OPENAERIAL) | getAbnormalStateId(AbnormalState::SPIN) | getAbnormalStateId(AbnormalState::DEFORM) | getAbnormalStateId(AbnormalState::PULLED) | getAbnormalStateId(AbnormalState::CONFUSE);
		case AbnormalState::ANY_STUN:
			return getAbnormalStateId(AbnormalState::SPIN) | getAbnormalStateId(AbnormalState::STUN) | getAbnormalStateId(AbnormalState::STUMBLE) | getAbnormalStateId(AbnormalState::STAGGER);
	}
	return 0;
}

/** Stand-in for Java ShieldType.getId() (constructor data of ShieldType.java; the enum companion of P5-02 does not exist yet) */
constexpr int32_t shieldTypeId(skillengine::model::ShieldType shieldType) noexcept {
	using skillengine::model::ShieldType;
	switch (shieldType) {
		case ShieldType::CONVERT:
			return 0;
		case ShieldType::REFLECTOR:
			return 1 << 0;
		case ShieldType::NORMAL:
			return 1 << 1;
		case ShieldType::UNK:
			return 1 << 2;
		case ShieldType::PROTECT:
			return 1 << 3;
		case ShieldType::MPSHIELD:
			return 1 << 4;
		case ShieldType::SKILL_REFLECTOR:
			return 1 << 5;
	}
	return 0;
}

/** Stand-in for Java SiegeRace.getByRace(race) (the SiegeRace companion of P5-12a does not declare it yet) */
constexpr model::siege::SiegeRace siegeRaceByRace(model::Race race) noexcept {
	switch (race) {
		case model::Race::ASMODIANS:
			return model::siege::SiegeRace::ASMODIANS;
		case model::Race::ELYOS:
			return model::siege::SiegeRace::ELYOS;
		default:
			return model::siege::SiegeRace::BALAUR;
	}
}

/** Stand-in for Java StatEnum.getModifier(skillId) (the StatEnum companion of P5-01 does not exist yet): nullopt is Java's null */
constexpr std::optional<model::stats::container::StatEnum> statEnumGetModifier(int32_t skillId) noexcept {
	using model::stats::container::StatEnum;
	switch (skillId) {
		case 30001:
		case 30002:
			return StatEnum::BOOST_ESSENCETAPPING_XP_RATE;
		case 30003:
			return StatEnum::BOOST_AETHERTAPPING_XP_RATE;
		case 40001:
			return StatEnum::BOOST_COOKING_XP_RATE;
		case 40002:
			return StatEnum::BOOST_WEAPONSMITHING_XP_RATE;
		case 40003:
			return StatEnum::BOOST_ARMORSMITHING_XP_RATE;
		case 40004:
			return StatEnum::BOOST_TAILORING_XP_RATE;
		case 40007:
			return StatEnum::BOOST_ALCHEMY_XP_RATE;
		case 40008:
			return StatEnum::BOOST_HANDICRAFTING_XP_RATE;
		case 40010:
			return StatEnum::BOOST_MENUISIER_XP_RATE;
		default:
			return std::nullopt;
	}
}

} // namespace aion::gameserver::controllers::detail
