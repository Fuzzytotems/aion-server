#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "aion/gameserver/model/Race.h"

namespace aion::gameserver::model {

/**
 * Companion of the generated enum Race (docs/design/static-data.md §2.5): Java's constructor data and methods as constexpr free functions found by
 * ADL (`getRaceId(race)` for Java `race.getRaceId()`). Java's Race implements L10n (`getL10nId(race)`).
 */

namespace detail {
/** Java constructor arguments (raceId, l10nId) in ordinal order; omitted l10nIds are 0 */
struct RaceData {
	int32_t raceId;
	int32_t l10nId;
};

inline constexpr std::array<RaceData, 48> RACE_DATA{{
	{0, 900240}, // ELYOS (playable races)
	{1, 900241}, // ASMODIANS
	{2, 0},      // LYCAN (npc races)
	{3, 0},      // CONSTRUCT
	{4, 0},      // CARRIER
	{5, 0},      // DRAKAN
	{6, 0},      // LIZARDMAN
	{7, 0},      // TELEPORTER
	{8, 0},      // NAGA
	{9, 0},      // BROWNIE
	{10, 0},     // KRALL
	{11, 0},     // SHULACK
	{12, 0},     // BARRIER
	{13, 0},     // PC_LIGHT_CASTLE_DOOR
	{14, 0},     // PC_DARK_CASTLE_DOOR
	{15, 0},     // DRAGON_CASTLE_DOOR
	{16, 0},     // GCHIEF_LIGHT
	{17, 0},     // GCHIEF_DARK
	{18, 0},     // DRAGON
	{19, 0},     // OUTSIDER
	{20, 0},     // RATMAN
	{21, 0},     // DEMIHUMANOID
	{22, 0},     // UNDEAD
	{23, 0},     // BEAST
	{24, 0},     // MAGICALMONSTER
	{25, 0},     // ELEMENTAL
	{28, 0},     // LIVINGWATER
	{26, 0},     // NONE (special races)
	{27, 0},     // PC_ALL
	{28, 0},     // DEFORM
	{29, 0},     // NEUT (2.6)
	{30, 0},     // GHENCHMAN_LIGHT (2.7)
	{31, 0},     // GHENCHMAN_DARK
	{32, 0},     // EVENT_TOWER_DARK (3.0)
	{33, 0},     // EVENT_TOWER_LIGHT
	{34, 0},     // GOBLIN
	{35, 0},     // TRICODARK
	{36, 0},     // NPC
	{37, 0},     // LIGHT (3.5)
	{38, 0},     // DARK
	{39, 0},     // WORLD_EVENT_DEFTOWER
	{40, 0},     // ORC (4.3)
	{41, 0},     // DRAGONET
	{42, 0},     // SIEGEDRAKAN
	{43, 0},     // GCHIEF_DRAGON
	{44, 0},     // WORLD_EVENT_BONFIRE
	{45, 0},     // DOOR_KILLER (4.7.5)
	{46, 0},     // LF5_Q_ITEM (4.8.0)
}};
static_assert(static_cast<size_t>(Race::LF5_Q_ITEM) + 1 == RACE_DATA.size(), "one entry per Race constant");
} // namespace detail

/** Java: Race.getRaceId() */
constexpr int32_t getRaceId(Race race) noexcept {
	return detail::RACE_DATA[static_cast<size_t>(race)].raceId;
}

/** Java: Race.isAsmoOrEly() */
constexpr bool isAsmoOrEly(Race race) noexcept {
	return race == Race::ELYOS || race == Race::ASMODIANS;
}

/** Java: Race.isPlayerRace() */
constexpr bool isPlayerRace(Race race) noexcept {
	return isAsmoOrEly(race) || race == Race::PC_ALL;
}

/** Java: Race.getL10nId() (L10n) */
constexpr int32_t getL10nId(Race race) noexcept {
	return detail::RACE_DATA[static_cast<size_t>(race)].l10nId;
}

/** Java: Race.getRaceByString(String) - the constant whose name equals fieldName (case-sensitive), null (std::nullopt) if there is none */
constexpr std::optional<Race> getRaceByString(std::string_view fieldName) noexcept {
	const auto& names = xml::EnumTraits<Race>::names;
	for (size_t ordinal = 0; ordinal < names.size(); ++ordinal) {
		if (names[ordinal] == fieldName)
			return static_cast<Race>(ordinal);
	}
	return std::nullopt;
}

} // namespace aion::gameserver::model
