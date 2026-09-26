#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/summons/UnsummonType.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorState.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"

namespace aion::gameserver::model::gameobjects::detail {

/**
 * C++ only, private to P4-11a: constructor data and static methods of Java enums and constant classes of other chunks that have no companion
 * header yet, plus the server-day arithmetic of UseableItemObject, as pure functions the tests check against the Java tables. Replace each part
 * by the owner's companion once it exists (ItemMask: P4-13, UnsummonType: P5-08, StaticDoorState: P4-07b, AbyssRankEnum: P5-01, ServerTime
 * with(LocalTime): P4-05).
 */

/** Java: the int constants of model.items.ItemMask that Item reads */
namespace item_mask {
inline constexpr int32_t TRADEABLE = 1 << 1;
inline constexpr int32_t SELLABLE = 1 << 2;
inline constexpr int32_t STORABLE_IN_WH = 1 << 3;
inline constexpr int32_t STORABLE_IN_AWH = 1 << 4;
inline constexpr int32_t STORABLE_IN_LWH = 1 << 5;
inline constexpr int32_t CAN_PROC_ENCHANT = 1 << 10;
inline constexpr int32_t REMODELABLE = 1 << 12;
inline constexpr int32_t CAN_AP_EXTRACT = 1 << 16;
inline constexpr int32_t LEGION_TRADEABLE = 1 << 18;
} // namespace item_mask

/** Java UnsummonType constructor argument delayMillis, in ordinal order */
inline constexpr std::array<int32_t, 8> UNSUMMON_DELAY_MILLIS{
	0,    // LOGOUT
	0,    // DISTANCE
	3000, // COMMAND
	0,    // SUMMON_DEATH
	0,    // MASTER_DEATH
	0,    // UNSPECIFIED
	3000, // SKILL_ORDER
	0,    // PET_ORDER_UNSUMMON_EFFECT
};

/** Java UnsummonType.isInstant(): delayMillis == 0 */
constexpr bool isInstant(summons::UnsummonType type) noexcept {
	return UNSUMMON_DELAY_MILLIS[static_cast<size_t>(type)] == 0;
}

/** Java StaticDoorState constructor argument flag, in ordinal order */
inline constexpr std::array<int32_t, 5> STATIC_DOOR_STATE_FLAGS{0, 1 << 0, 1 << 1, 1 << 2, 1 << 3};

/** Java StaticDoorState.getFlag() */
constexpr int32_t flagOf(templates::staticdoor::StaticDoorState state) noexcept {
	return STATIC_DOOR_STATE_FLAGS[static_cast<size_t>(state)];
}

/** Java StaticDoorState.setStates(int flags, EnumSet<StaticDoorState> state): NONE is skipped; `states` offers add and remove */
template <class StateSet>
void setStates(int32_t flags, StateSet& states) {
	for (size_t ordinal = 0; ordinal < STATIC_DOOR_STATE_FLAGS.size(); ++ordinal) {
		const auto state = static_cast<templates::staticdoor::StaticDoorState>(ordinal);
		if (state == templates::staticdoor::StaticDoorState::NONE)
			continue;
		if ((flags & flagOf(state)) == 0)
			states.remove(state);
		else
			states.add(state);
	}
}

/** Java AbyssRankEnum.getId(): ordinal + 1 (GRADE9_SOLDIER 1 .. SUPREME_COMMANDER 18) */
constexpr int32_t abyssRankId(utils::stats::AbyssRankEnum rank) noexcept {
	return static_cast<int32_t>(rank) + 1;
}

/**
 * Java `ServerTime.now().with(LocalTime.MAX).toEpochSecond() * 1000` at the instant `nowMillis`: the last second of the server day. Like
 * ZonedDateTime.with (resolveLocal), an ambiguous 23:59:59 keeps the offset of `now` if it is one of the two offsets (otherwise the earlier
 * offset), and a 23:59:59 in a gap is moved forward by the gap, which is the instant of the local time at the offset before the gap.
 */
inline int64_t endOfServerDayMillis(int64_t nowMillis, const std::chrono::time_zone* zone) {
	const std::chrono::sys_seconds now = std::chrono::floor<std::chrono::seconds>(std::chrono::sys_time<std::chrono::milliseconds>{
		std::chrono::milliseconds(nowMillis)});
	const std::chrono::local_seconds endOfDay = std::chrono::floor<std::chrono::days>(zone->to_local(now)) + std::chrono::seconds(86399);
	const std::chrono::local_info info = zone->get_info(endOfDay);
	std::chrono::seconds offset = info.first.offset;
	if (info.result == std::chrono::local_info::ambiguous && zone->get_info(now).offset == info.second.offset)
		offset = info.second.offset;
	return (endOfDay.time_since_epoch() - offset).count() * 1000;
}

/** Java `System.currentTimeMillis() + cd * 1000` for a house object cooldown in seconds: the int product wraps before it is widened */
constexpr int64_t cooldownReuseTimeMillis(int64_t nowMillis, int32_t cdSeconds) noexcept {
	return nowMillis + static_cast<int32_t>(static_cast<uint32_t>(cdSeconds) * 1000u);
}

} // namespace aion::gameserver::model::gameobjects::detail
