#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/skillengine/model/DispelSlotType.h"
#include "aion/gameserver/skillengine/model/SkillTargetSlot.h"

namespace aion::gameserver::skillengine::model {

/**
 * Companion of the generated enum SkillTargetSlot (docs/design/static-data.md §2.5): Java's constructor data, constant and methods as free
 * functions found by ADL (`getId(slot)` for Java `slot.getId()`). Pure data.
 *
 * @author ATracer, Cheatkiller, Neon
 */

/** Java: SkillTargetSlot.FULLSLOTS (BUFF | DEBUFF | CHANT | SPEC | SPEC2 | BOOST | NOSHOW) */
inline constexpr int32_t SKILL_TARGET_SLOT_FULLSLOTS = 127;

/** Java: SkillTargetSlot.getId() - BUFF(1), DEBUFF(2), CHANT(4), SPEC(8), SPEC2(16), BOOST(32), NOSHOW(64), NONE(128) */
constexpr int32_t getId(SkillTargetSlot slot) noexcept {
	return static_cast<int32_t>(uint32_t{1} << static_cast<uint32_t>(slot));
}

/** Java: SkillTargetSlot.of(DispelSlotType) - null (std::nullopt) for the other dispel slot types */
constexpr std::optional<SkillTargetSlot> of(DispelSlotType dispelSlotType) noexcept {
	switch (dispelSlotType) {
		case DispelSlotType::BUFF:
			return SkillTargetSlot::BUFF;
		case DispelSlotType::DEBUFF:
			return SkillTargetSlot::DEBUFF;
		case DispelSlotType::SPECIAL2:
			return SkillTargetSlot::SPEC2;
		default:
			return std::nullopt;
	}
}

} // namespace aion::gameserver::skillengine::model
