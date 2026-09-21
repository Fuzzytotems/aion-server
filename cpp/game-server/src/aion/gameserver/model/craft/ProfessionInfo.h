#pragma once

#include <array>
#include <cstdint>

#include "aion/gameserver/model/craft/Profession.h"

namespace aion::gameserver::model::craft {

/** Companion of the generated enum Profession (docs/design/static-data.md §2.5): Java's constructor data and methods as free functions (ADL). */

/** Java: Profession.values() in ordinal order */
inline constexpr std::array<Profession, 9> PROFESSION_VALUES{Profession::ESSENCETAPPING, Profession::AETHERTAPPING, Profession::COOKING,
	Profession::WEAPONSMITHING, Profession::ARMORSMITHING, Profession::TAILORING, Profession::ALCHEMY, Profession::HANDICRAFTING,
	Profession::CONSTRUCTION};

/** Java: Profession.getSkillId() */
constexpr int32_t getSkillId(Profession profession) noexcept {
	static constexpr std::array<int32_t, 9> SKILL_IDS{30002, 30003, 40001, 40002, 40003, 40004, 40007, 40008, 40010};
	return SKILL_IDS[static_cast<size_t>(profession)];
}

/** Java: Profession.isCrafting() */
constexpr bool isCrafting(Profession profession) noexcept {
	return getSkillId(profession) >= 40001 && getSkillId(profession) <= 40010;
}

} // namespace aion::gameserver::model::craft
