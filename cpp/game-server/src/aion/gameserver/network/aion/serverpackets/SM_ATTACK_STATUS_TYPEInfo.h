#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"

namespace aion::gameserver::network::aion::serverpackets {

/** Companion of the generated nested enum SM_ATTACK_STATUS.TYPE (docs/design/static-data.md §2.5): Java's constructor data (ADL). */

namespace detail {
/** SM_ATTACK_STATUS.TYPE values in ordinal order (several constants share a value, e.g. DAMAGE and HP) */
inline constexpr std::array<int32_t, 31> ATTACK_STATUS_TYPE_VALUES{{
	1,  // TYPE1
	2,  // TYPE2
	9,  // TYPE9
	11, // TYPE11
	12, // TYPE12
	14, // TYPE14
	25, // TYPE25
	3,  // NATURAL_HP
	4,  // USED_HP: when skill uses hp as cost parameter
	5,  // REGULAR
	6,  // ABSORBED_HP
	7,  // DAMAGE
	7,  // HP
	8,  // PROTECTDMG
	10, // DELAYDAMAGE
	12, // DROWNING
	13, // HPAFTERRES: when setting hp after resurrection
	15, // MAGICCOUNTERATK
	16, // DISPELBUFFCOUNTERATK
	17, // FALL_DAMAGE
	18, // DOOR_REPAIR
	19, // HEAL_MP
	20, // DAMAGE_MP
	20, // ABSORBED_MP
	21, // MP
	22, // NATURAL_MP
	23, // USED_MP: when skill uses mp as cost parameter
	24, // FP_RINGS
	26, // FP
	26, // FP_DAMAGE
	27, // NATURAL_FP
}};
static_assert(static_cast<size_t>(SM_ATTACK_STATUS_TYPE::NATURAL_FP) + 1 == ATTACK_STATUS_TYPE_VALUES.size(), "one entry per TYPE constant");
} // namespace detail

/** Java: SM_ATTACK_STATUS.TYPE.getValue() */
constexpr int32_t getValue(SM_ATTACK_STATUS_TYPE type) noexcept {
	return detail::ATTACK_STATUS_TYPE_VALUES[static_cast<size_t>(type)];
}

} // namespace aion::gameserver::network::aion::serverpackets
