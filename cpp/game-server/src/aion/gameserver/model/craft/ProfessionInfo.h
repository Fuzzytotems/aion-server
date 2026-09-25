#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "aion/gameserver/model/craft/Profession.h"

namespace aion::gameserver::model::craft {

/**
 * Companion of the generated enum Profession (docs/design/static-data.md §2.5): Java's constructor data and methods as free functions (ADL).
 * <p>
 * The six functions after isCrafting (header request m5c-h04, m5c-plan.md I-02) are defined in ProfessionInfo.cpp, where their bodies are ported
 * (C-01); the two constexpr ones above were ported with the companion (m5a-plan.md E2-03).
 */

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

/** Java: Profession.getUpgradeCost(skillLevel) - the cost at the skill levels the switch names, null (std::nullopt) at any other level */
std::optional<int32_t> getUpgradeCost(Profession profession, int32_t skillLevel);

/** Java: Profession.getMaxUpgradableLevel() */
int32_t getMaxUpgradableLevel(Profession profession);

/** Java: Profession.getClientName() - the l10n name of the profession's skill */
std::string getClientName(Profession profession);

/** Java: Profession.getClientName(skillLevel) - the grade name, a space and getClientName() */
std::string getClientName(Profession profession, int32_t skillLevel);

/** Java: Profession.getSkillGrade(skillLevel) (private in Java, called by getClientName(skillLevel)) */
std::string getSkillGrade(Profession profession, int32_t skillLevel);

/** Java: Profession.getBySkillId(skillId) - the first constant with the skill id, null (std::nullopt) if there is none */
std::optional<Profession> getBySkillId(int32_t skillId);

} // namespace aion::gameserver::model::craft
