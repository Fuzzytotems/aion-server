#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/ActionState.h"

namespace aion::gameserver::model {

/**
 * Companion of the generated enum ActionState (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and
 * methods as constexpr free functions found by ADL (`getL10nId(state)` for Java `state.getL10nId()`). Java's ActionState implements L10n; the
 * default method L10n.getL10n() is `ChatUtil::l10n(getL10nId(state))` at call sites (an enum cannot derive the C++ L10n class).
 */

namespace detail {
/** Java constructor argument l10nId in ordinal order */
inline constexpr std::array<int32_t, 15> ACTION_STATE_L10N_IDS{{
	1400053, // STANDING: standing
	1400054, // PATH_FLYING: flying
	1400055, // FREE_FLYING: flying
	1400056, // RIDING: riding
	1400057, // RESTING: resting
	1400058, // SITTING: sitting
	1400059, // DEAD: dead
	1400060, // FLY_DEAD: dead
	1400061, // PERSONAL_SHOP: running a Private Store
	1400062, // LOOTING: looting
	1400063, // FLY_LOOTING: looting
	1400064, // CURRENT_STATUS: in your current status
	1400079, // COMBAT: in combat
	1400082, // GLIDING: gliding
	1401212, // POLYMORPH: Transformation Mode
}};
static_assert(static_cast<size_t>(ActionState::POLYMORPH) + 1 == ACTION_STATE_L10N_IDS.size(), "one entry per ActionState constant");
} // namespace detail

/** Java: ActionState.getL10nId() */
constexpr int32_t getL10nId(ActionState state) noexcept {
	return detail::ACTION_STATE_L10N_IDS[static_cast<size_t>(state)];
}

} // namespace aion::gameserver::model
