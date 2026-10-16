#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/controllers/observer/ObserverType.h"

namespace aion::gameserver::controllers::observer {

/**
 * Companion of the generated enum ObserverType (docs/design/static-data.md §2.5): Java's constructor data (the observer mask) and
 * matchesObserver as free functions.
 *
 * @author ATracer
 */

namespace detail {
inline constexpr int32_t OBSERVER_MOVE = 1;
inline constexpr int32_t OBSERVER_ATTACK = 1 << 1;
inline constexpr int32_t OBSERVER_ATTACKED = 1 << 2;
inline constexpr int32_t OBSERVER_EQUIP = 1 << 3;
inline constexpr int32_t OBSERVER_UNEQUIP = 1 << 4;
inline constexpr int32_t OBSERVER_STARTSKILLCAST = 1 << 5;
inline constexpr int32_t OBSERVER_DEATH = 1 << 6;
inline constexpr int32_t OBSERVER_DOT_ATTACKED = 1 << 7;
inline constexpr int32_t OBSERVER_ITEMUSE = 1 << 8;
inline constexpr int32_t OBSERVER_ABNORMALSETTED = 1 << 9;
inline constexpr int32_t OBSERVER_SUMMONRELEASE = 1 << 10;
inline constexpr int32_t OBSERVER_SIT = 1 << 11;
inline constexpr int32_t OBSERVER_HP_CHANGED = 1 << 12;
inline constexpr int32_t OBSERVER_ENDSKILLCAST = 1 << 13;
inline constexpr int32_t OBSERVER_BOOSTSKILLCOST = 1 << 14;

inline constexpr std::array<int32_t, 20> OBSERVER_MASKS{{
	OBSERVER_MOVE,
	OBSERVER_ATTACK,
	OBSERVER_ATTACKED,
	OBSERVER_EQUIP,
	OBSERVER_UNEQUIP,
	OBSERVER_STARTSKILLCAST,
	OBSERVER_DEATH,
	OBSERVER_DOT_ATTACKED,
	OBSERVER_ITEMUSE,
	OBSERVER_ABNORMALSETTED,
	OBSERVER_SUMMONRELEASE,
	OBSERVER_SIT,
	OBSERVER_HP_CHANGED,
	OBSERVER_ENDSKILLCAST,
	OBSERVER_BOOSTSKILLCOST,
	OBSERVER_EQUIP | OBSERVER_UNEQUIP,                          // EQUIP_UNEQUIP
	OBSERVER_ATTACK | OBSERVER_ATTACKED,                        // ATTACK_DEFEND
	OBSERVER_DOT_ATTACKED | OBSERVER_ATTACK | OBSERVER_ATTACKED, // DOT_ATTACK_DEFEND
	OBSERVER_MOVE | OBSERVER_DEATH,                             // MOVE_OR_DIE
	OBSERVER_MOVE | OBSERVER_ATTACK | OBSERVER_ATTACKED | OBSERVER_EQUIP | OBSERVER_UNEQUIP | OBSERVER_STARTSKILLCAST | OBSERVER_DEATH |
		OBSERVER_DOT_ATTACKED | OBSERVER_ITEMUSE | OBSERVER_ABNORMALSETTED | OBSERVER_SUMMONRELEASE | OBSERVER_SIT | OBSERVER_HP_CHANGED |
		OBSERVER_ENDSKILLCAST | OBSERVER_BOOSTSKILLCOST, // ALL
}};
static_assert(static_cast<size_t>(ObserverType::ALL) + 1 == OBSERVER_MASKS.size(), "one entry per ObserverType constant");
} // namespace detail

/** Java: ObserverType.observerMask */
constexpr int32_t getObserverMask(ObserverType type) noexcept {
	return detail::OBSERVER_MASKS[static_cast<size_t>(type)];
}

/** Java: observer.matchesObserver(observerType): every bit of observerType's mask is set in observer's mask. */
constexpr bool matchesObserver(ObserverType observer, ObserverType observerType) noexcept {
	return (getObserverMask(observerType) & getObserverMask(observer)) == getObserverMask(observerType);
}

} // namespace aion::gameserver::controllers::observer
