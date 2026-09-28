#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/gameobjects/state/CreatureState.h"

namespace aion::gameserver::model::gameobjects::state {

/**
 * Companion of the generated enum CreatureState (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and
 * methods as free functions found by ADL (`getId(state)` for Java `state.getId()`, `mustMatchExact(state)`).
 *
 * @author ATracer, Sweetkr
 */

namespace detail {
/** Java constructor arguments (id, mustMatchExact) in ordinal order */
struct CreatureStateData {
	int32_t id;
	bool mustMatchExact;
};

inline constexpr int32_t CREATURE_STATE_ACTIVE = 1;
inline constexpr int32_t CREATURE_STATE_FLYING = 1 << 1;
inline constexpr int32_t CREATURE_STATE_RESTING = 1 << 2;
inline constexpr int32_t CREATURE_STATE_FLOATING_CORPSE = 1 << 3;

inline constexpr std::array<CreatureStateData, 14> CREATURE_STATE_DATA{{
	{CREATURE_STATE_ACTIVE, false},                                                                 // ACTIVE 1
	{CREATURE_STATE_FLYING, false},                                                                 // FLYING 2
	{CREATURE_STATE_RESTING, false},                                                                // RESTING 4
	{CREATURE_STATE_FLOATING_CORPSE, false},                                                        // FLOATING_CORPSE 8
	{1 << 4, false},                                                                                // UNK 16
	{1 << 5, false},                                                                                // WEAPON_EQUIPPED 32
	{1 << 6, false},                                                                                // WALK_MODE 64 (set = walking, unset = running)
	{1 << 7, false},                                                                                // POWERSHARD 128
	{1 << 8, false},                                                                                // TREATMENT 256
	{1 << 9, false},                                                                                // GLIDING 512
	{CREATURE_STATE_FLYING + CREATURE_STATE_RESTING, true},                                         // CHAIR 2 + 4
	{CREATURE_STATE_ACTIVE + CREATURE_STATE_FLYING + CREATURE_STATE_RESTING, false},                // DEAD 1 + 2 + 4
	{CREATURE_STATE_ACTIVE + CREATURE_STATE_FLYING + CREATURE_STATE_FLOATING_CORPSE, true},         // PRIVATE_SHOP 1 + 2 + 8
	{CREATURE_STATE_RESTING + CREATURE_STATE_FLOATING_CORPSE, false},                               // LOOTING 4 + 8
}};
static_assert(static_cast<size_t>(CreatureState::LOOTING) + 1 == CREATURE_STATE_DATA.size(), "one entry per CreatureState constant");
} // namespace detail

constexpr int32_t getId(CreatureState state) noexcept {
	return detail::CREATURE_STATE_DATA[static_cast<size_t>(state)].id;
}

/** Multibit states (CHAIR, PRIVATE_SHOP) that Creature.isInState compares exactly */
constexpr bool mustMatchExact(CreatureState state) noexcept {
	return detail::CREATURE_STATE_DATA[static_cast<size_t>(state)].mustMatchExact;
}

} // namespace aion::gameserver::model::gameobjects::state
