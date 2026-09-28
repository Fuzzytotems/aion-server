#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::controllers::attack {

/**
 * Companion of the generated enum AttackStatus (docs/design/hub-headers.md §13, pattern NpcRatingInfo.h): Java's constructor data and static
 * methods as free functions found by ADL (`isCritical(status)` for Java `status.isCritical()`).
 * <p>
 * Java AttackStatus.java: the ids are the wire values SM_ATTACK and SM_ATTACK_STATUS write, so they are signed and not the ordinal.
 *
 * @author ATracer
 */

namespace detail {
/** Java constructor argument `id` in ordinal order (AttackStatus.java:6-27) */
inline constexpr std::array<int32_t, 22> ATTACK_STATUS_IDS{{
	0,   // DODGE
	1,   // OFFHAND_DODGE
	2,   // PARRY
	3,   // OFFHAND_PARRY
	4,   // BLOCK
	5,   // OFFHAND_BLOCK
	6,   // RESIST
	7,   // OFFHAND_RESIST
	8,   // BUF
	9,   // OFFHAND_BUF
	10,  // NORMALHIT
	11,  // OFFHAND_NORMALHIT
	-64, // CRITICAL_DODGE
	-62, // CRITICAL_PARRY
	-60, // CRITICAL_BLOCK
	-58, // CRITICAL_RESIST
	-54, // CRITICAL
	-47, // OFFHAND_CRITICAL_DODGE
	-45, // OFFHAND_CRITICAL_PARRY
	-43, // OFFHAND_CRITICAL_BLOCK
	-41, // OFFHAND_CRITICAL_RESIST
	-37, // OFFHAND_CRITICAL
}};
static_assert(static_cast<size_t>(AttackStatus::OFFHAND_CRITICAL) + 1 == ATTACK_STATUS_IDS.size(), "one entry per AttackStatus constant");

/** Java constructor argument `counterSkill` in ordinal order */
inline constexpr std::array<bool, 22> ATTACK_STATUS_COUNTER_SKILL{{
	true,  // DODGE
	true,  // OFFHAND_DODGE
	true,  // PARRY
	true,  // OFFHAND_PARRY
	true,  // BLOCK
	true,  // OFFHAND_BLOCK
	true,  // RESIST
	true,  // OFFHAND_RESIST
	false, // BUF
	false, // OFFHAND_BUF
	false, // NORMALHIT
	false, // OFFHAND_NORMALHIT
	true,  // CRITICAL_DODGE
	true,  // CRITICAL_PARRY
	true,  // CRITICAL_BLOCK
	true,  // CRITICAL_RESIST
	false, // CRITICAL
	true,  // OFFHAND_CRITICAL_DODGE
	true,  // OFFHAND_CRITICAL_PARRY
	true,  // OFFHAND_CRITICAL_BLOCK
	true,  // OFFHAND_CRITICAL_RESIST
	false, // OFFHAND_CRITICAL
}};
static_assert(
	static_cast<size_t>(AttackStatus::OFFHAND_CRITICAL) + 1 == ATTACK_STATUS_COUNTER_SKILL.size(), "one entry per AttackStatus constant");

/** Java constructor argument `isCritical` in ordinal order */
inline constexpr std::array<bool, 22> ATTACK_STATUS_CRITICAL{{
	false, // DODGE
	false, // OFFHAND_DODGE
	false, // PARRY
	false, // OFFHAND_PARRY
	false, // BLOCK
	false, // OFFHAND_BLOCK
	false, // RESIST
	false, // OFFHAND_RESIST
	false, // BUF
	false, // OFFHAND_BUF
	false, // NORMALHIT
	false, // OFFHAND_NORMALHIT
	true,  // CRITICAL_DODGE
	true,  // CRITICAL_PARRY
	true,  // CRITICAL_BLOCK
	true,  // CRITICAL_RESIST
	true,  // CRITICAL
	true,  // OFFHAND_CRITICAL_DODGE
	true,  // OFFHAND_CRITICAL_PARRY
	true,  // OFFHAND_CRITICAL_BLOCK
	true,  // OFFHAND_CRITICAL_RESIST
	true,  // OFFHAND_CRITICAL
}};
static_assert(static_cast<size_t>(AttackStatus::OFFHAND_CRITICAL) + 1 == ATTACK_STATUS_CRITICAL.size(), "one entry per AttackStatus constant");

/** Java enum name() of an AttackStatus, for the IllegalArgumentException message of getOffHandStats */
inline constexpr std::array<const char*, 22> ATTACK_STATUS_NAMES{{
	"DODGE",
	"OFFHAND_DODGE",
	"PARRY",
	"OFFHAND_PARRY",
	"BLOCK",
	"OFFHAND_BLOCK",
	"RESIST",
	"OFFHAND_RESIST",
	"BUF",
	"OFFHAND_BUF",
	"NORMALHIT",
	"OFFHAND_NORMALHIT",
	"CRITICAL_DODGE",
	"CRITICAL_PARRY",
	"CRITICAL_BLOCK",
	"CRITICAL_RESIST",
	"CRITICAL",
	"OFFHAND_CRITICAL_DODGE",
	"OFFHAND_CRITICAL_PARRY",
	"OFFHAND_CRITICAL_BLOCK",
	"OFFHAND_CRITICAL_RESIST",
	"OFFHAND_CRITICAL",
}};
static_assert(static_cast<size_t>(AttackStatus::OFFHAND_CRITICAL) + 1 == ATTACK_STATUS_NAMES.size(), "one entry per AttackStatus constant");
} // namespace detail

/** Java: AttackStatus.getId() */
constexpr int32_t getId(AttackStatus status) noexcept {
	return detail::ATTACK_STATUS_IDS[static_cast<size_t>(status)];
}

/** Java: AttackStatus.isCounterSkill() */
constexpr bool isCounterSkill(AttackStatus status) noexcept {
	return detail::ATTACK_STATUS_COUNTER_SKILL[static_cast<size_t>(status)];
}

/** Java: AttackStatus.isCritical() */
constexpr bool isCritical(AttackStatus status) noexcept {
	return detail::ATTACK_STATUS_CRITICAL[static_cast<size_t>(status)];
}

/** Java: AttackStatus.name() */
constexpr const char* getName(AttackStatus status) noexcept {
	return detail::ATTACK_STATUS_NAMES[static_cast<size_t>(status)];
}

/**
 * Java: AttackStatus.getOffHandStats(mainHandStatus) - the switch has no default, so every status it does not name (the eight OFFHAND_*
 * constants) falls through to the throw.
 *
 * @throws IllegalArgumentException ("Invalid mainHandStatus " + mainHandStatus)
 */
inline AttackStatus getOffHandStats(AttackStatus mainHandStatus) {
	switch (mainHandStatus) {
		case AttackStatus::DODGE:
			return AttackStatus::OFFHAND_DODGE;
		case AttackStatus::PARRY:
			return AttackStatus::OFFHAND_PARRY;
		case AttackStatus::BLOCK:
			return AttackStatus::OFFHAND_BLOCK;
		case AttackStatus::RESIST:
			return AttackStatus::OFFHAND_RESIST;
		case AttackStatus::BUF:
			return AttackStatus::OFFHAND_BUF;
		case AttackStatus::NORMALHIT:
			return AttackStatus::OFFHAND_NORMALHIT;
		case AttackStatus::CRITICAL:
			return AttackStatus::OFFHAND_CRITICAL;
		case AttackStatus::CRITICAL_DODGE:
			return AttackStatus::OFFHAND_CRITICAL_DODGE;
		case AttackStatus::CRITICAL_PARRY:
			return AttackStatus::OFFHAND_CRITICAL_PARRY;
		case AttackStatus::CRITICAL_BLOCK:
			return AttackStatus::OFFHAND_CRITICAL_BLOCK;
		case AttackStatus::CRITICAL_RESIST:
			return AttackStatus::OFFHAND_CRITICAL_RESIST;
		default:
			throw runtime::IllegalArgumentException(std::string("Invalid mainHandStatus ") + getName(mainHandStatus));
	}
}

/** Java: AttackStatus.getBaseStatus(status) */
constexpr AttackStatus getBaseStatus(AttackStatus status) noexcept {
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

/** Java: AttackStatus.getCriticalStatusFor(status) */
constexpr AttackStatus getCriticalStatusFor(AttackStatus status) noexcept {
	switch (status) {
		case AttackStatus::DODGE:
			return AttackStatus::CRITICAL_DODGE;
		case AttackStatus::OFFHAND_DODGE:
			return AttackStatus::OFFHAND_CRITICAL_DODGE;
		case AttackStatus::PARRY:
			return AttackStatus::CRITICAL_PARRY;
		case AttackStatus::OFFHAND_PARRY:
			return AttackStatus::OFFHAND_CRITICAL_PARRY;
		case AttackStatus::BLOCK:
			return AttackStatus::CRITICAL_BLOCK;
		case AttackStatus::OFFHAND_BLOCK:
			return AttackStatus::OFFHAND_CRITICAL_BLOCK;
		case AttackStatus::NORMALHIT:
			return AttackStatus::CRITICAL;
		case AttackStatus::OFFHAND_NORMALHIT:
			return AttackStatus::OFFHAND_CRITICAL;
		default:
			return status;
	}
}

} // namespace aion::gameserver::controllers::attack
