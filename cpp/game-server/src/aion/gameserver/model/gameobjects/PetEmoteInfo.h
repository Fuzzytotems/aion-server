#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/gameobjects/PetEmote.h"

namespace aion::gameserver::model::gameobjects {

/**
 * Companion of the generated enum PetEmote (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods as
 * free functions found by ADL (`getEmoteId(value)` for Java `value.getEmoteId()`).
 *
 * @author ATracer
 */

namespace detail {
/** Java constructor argument `id` in ordinal order */
inline constexpr std::array<int32_t, 23> PETEMOTE_IDS{
	0, // MOVE_STOP
	8, // MOVE_POSITION_UPDATE
	12, // MOVETO
	128, // NO_INTERACTION
	129, // FLY_START
	130, // FLY_STOP
	131, // FLY
	133, // EMOTION
	134, // EAT_START
	135, // EAT_STOP
	136, // EAT_STOP_HEART
	137, // NOT_HUNGRY
	140, // ATTACK_MODE_FEARLESS
	141, // ATTACK_MODE_FEARFUL
	142, // ALARM
	144, // ALARM_STOP_SHOUTING
	145, // INIT_INTERACTION
	146, // PERFORM_INTERACTION
	147, // GET_MOOD_GIFT
	148, // BUFF
	149, // LOOT_START
	150, // LOOT_STOP
	INT32_MAX, // UNKNOWN
};
static_assert(static_cast<size_t>(PetEmote::UNKNOWN) + 1 == PETEMOTE_IDS.size(), "one entry per PetEmote constant");
} // namespace detail

constexpr int32_t getEmoteId(PetEmote value) noexcept {
	return detail::PETEMOTE_IDS[static_cast<size_t>(value)];
}

/** @return the emote with the id, UNKNOWN if there is none (Java: lookup in the static petEmotes map) */
constexpr PetEmote getEmoteById(int32_t emoteId) noexcept {
	for (size_t i = 0; i < detail::PETEMOTE_IDS.size(); ++i) {
		if (detail::PETEMOTE_IDS[i] == emoteId)
			return static_cast<PetEmote>(i);
	}
	return PetEmote::UNKNOWN;
}

} // namespace aion::gameserver::model::gameobjects
