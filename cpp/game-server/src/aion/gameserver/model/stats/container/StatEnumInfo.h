#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/stats/container/StatEnum.h"

namespace aion::gameserver::model::stats::container {

/**
 * Companion of the generated enum StatEnum (docs/design/static-data.md §2.5): Java's methods as free functions. Only the static getModifier is
 * here (m5c-plan.md D8, C-03); the constructor data behind getSign() and getItemStoneMask() is not ported yet.
 */

/**
 * Java: StatEnum.getModifier(int skillId) - the stat that boosts the xp of a gathering or crafting skill; nullopt is Java's null (any other
 * skill id)
 */
constexpr std::optional<StatEnum> getModifier(int32_t skillId) noexcept {
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

} // namespace aion::gameserver::model::stats::container
