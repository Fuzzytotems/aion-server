#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/attack/fwd.h"

namespace aion::gameserver::controllers::attack {

/**
 * Declaration header of the M5b P5-01 port (docs/design/hub-headers.md §3.5); `fwd.h` already declared the class. A static-only class (K5,
 * fieldmap): `PVP_KILL_LISTS` is the static shim the fieldmap prescribes, and `addKillFor` takes the inner map's own Monitor where Java takes
 * `synchronized (killTimesByVictimId)`.
 *
 * @author Neon
 */
class KillCounter {
private:
	static inline runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::RcHashMap<int32_t, runtime::Ref<runtime::RcArrayList<int64_t>>>>>
		PVP_KILL_LISTS{AION_LOCK_CLASS(KillCounter::PVP_KILL_LISTS#stripe)};

public:
	KillCounter() = delete;

	/**
	 * Increments the killers kill counter for the victim by one and returns the updated count. Old kills get removed over time, so the returned
	 * value represents only kills in the past 24 hours (configurable, see {@link CustomConfig#PVP_DAY_DURATION}).
	 *
	 * @return The count how many times the killer killed given victim.
	 */
	static int32_t addKillFor(int32_t killerId, int32_t victimId);
};

} // namespace aion::gameserver::controllers::attack
