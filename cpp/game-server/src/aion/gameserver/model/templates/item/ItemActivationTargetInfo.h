#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/templates/item/ItemActivationTarget.h"

namespace aion::gameserver::model::templates::item {

/**
 * Companion of the generated enum ItemActivationTarget (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and
 * methods as free functions found by ADL (`getRace(target)` for Java `target.getRace()`).
 *
 * @author Rolandas
 */

namespace detail {
/** Java constructor argument `race` in ordinal order (null for the constants without a race) */
inline constexpr std::array<std::optional<Race>, 16> ITEM_ACTIVATION_TARGET_RACES{
	std::nullopt,               // STANDALONE
	std::nullopt,               // TARGET
	std::nullopt,               // MYMENTO
	std::nullopt,               // WORLD_EVENT_CAKE_D
	std::nullopt,               // WORLD_EVENT_CAKE_L
	Race::BROWNIE,              // BROWNIE
	Race::GCHIEF_LIGHT,         // GCHIEF_LIGHT
	Race::GHENCHMAN_LIGHT,      // GHENCHMAN_LIGHT
	Race::GHENCHMAN_DARK,       // GHENCHMAN_DARK
	Race::KRALL,                // KRALL
	Race::LF5_Q_ITEM,           // LF5_Q_ITEM
	Race::LIVINGWATER,          // LIVINGWATER
	Race::LYCAN,                // LYCAN
	Race::EVENT_TOWER_LIGHT,    // EVENT_TOWER_LIGHT
	Race::EVENT_TOWER_DARK,     // EVENT_TOWER_DARK
	Race::WORLD_EVENT_DEFTOWER, // WORLD_EVENT_DEFTOWER
};
static_assert(static_cast<size_t>(ItemActivationTarget::WORLD_EVENT_DEFTOWER) + 1 == ITEM_ACTIVATION_TARGET_RACES.size(),
	"one entry per ItemActivationTarget constant");
} // namespace detail

/** @return the race, std::nullopt for Java null */
constexpr std::optional<Race> getRace(ItemActivationTarget target) noexcept {
	return detail::ITEM_ACTIVATION_TARGET_RACES[static_cast<size_t>(target)];
}

} // namespace aion::gameserver::model::templates::item
