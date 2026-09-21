#pragma once

#include <atomic>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <typeinfo>
#include <vector>

#include "aion/gameserver/runtime/base/Checked.h"

namespace aion::gameserver::runtime {

class RefCounted;

/**
 * Debug live-instance counters of RefCounted objects by dynamic type (docs/design/m5a-plan.md D8, I-03). No Java counterpart.
 *
 * - makeRef<T> (and with it VisibleObject::create<T> and every X::create) counts a created instance of T after T's constructor returned
 *   (a constructor that throws counts nothing). makeRef constructs exactly T, so T is the dynamic type.
 * - The Reclaimer counts a destroyed instance of the object's dynamic type right before it runs the destructor, next to its destroy observer
 *   (destroyChunk). Only the Reclaimer destroys RefCounted objects, so `live` is created minus destroyed.
 * - liveCounts() is a snapshot of every type created since process start (also those at 0), for the final census of the check-output mode
 *   (F-07 live_counts.txt: Player, Item, PlayerCommonData, Account must be 0 after the shutdown logout, drain and reclaimNow).
 *   Objects waiting in the Reclaimer backlog still count as live: drain first.
 * - Parts (OwnedPart: storages, controllers, lists) are not RefCounted and are not counted. They cannot outlive their owner: a Ref to a part
 *   retains the owner, and a retired part holds a Ref to its owner until it is destroyed. A live PlayerStorage therefore always shows as a live
 *   Player or Account.
 *
 * Checked builds only (AION_CHECKED, like the other debug checks C1-C16): in release builds makeRef and the Reclaimer count nothing and
 * liveCounts() is empty. Cost per creation: one predicted load and two atomic increments; per destruction: one type lookup (a one-entry cache
 * of the scanning thread, else a shared lock and a hash lookup).
 * Thread-safety: counting is lock-free after the first creation of a type (registered once under a mutex); reads are racy snapshots.
 */
struct LiveCount {
	/** commons::utils::getClassName of the type, e.g. "aion::gameserver::model::gameobjects::player::Player" */
	std::string className;
	/** created minus destroyed (a snapshot) */
	int64_t live = 0;
	uint64_t created = 0;
};

/** true if makeRef and the Reclaimer count (checked builds) */
inline constexpr bool LIVE_COUNTS_ENABLED = AION_CHECKED != 0;

/** @return every counted type with its live and created counts, sorted by class name; empty in release builds */
std::vector<LiveCount> liveCounts();

/** @return the live count of one type (0 if never created, or in release builds) */
int64_t liveCountOf(const std::type_info& type);

/**
 * Writes the counts: a header line "# live instance counts v1" followed by one line per type of liveCounts(), tab separated:
 * <tt>live created className</tt>.
 */
void writeLiveCounts(std::ostream& out);

namespace detail {

/** The counter of one type (a constinit function-local static of countLiveInstanceCreated<T>). Internal. */
struct LiveTypeCounter {
	std::atomic<int64_t> live{0};
	std::atomic<uint64_t> created{0};
	std::atomic<bool> registered{false};
	const std::type_info* type = nullptr; // written once under the registry mutex before `registered` is set
	LiveTypeCounter* next = nullptr;      // published list, written before publication
};

/** Registers the counter of `type` (idempotent, thread-safe). @return false if the registry could not allocate (the instance is not counted) */
bool registerLiveType(LiveTypeCounter& counter, const std::type_info& type) noexcept;

/** makeRef<T>: counts a created T (checked builds) */
template <class T>
void countLiveInstanceCreated() noexcept {
	// lint: L8 the per-type counter of the debug live-instance counts (atomic members, registered once)
	static constinit LiveTypeCounter counter;
	if (!counter.registered.load(std::memory_order_acquire)) [[unlikely]] {
		if (!registerLiveType(counter, typeid(T)))
			return;
	}
	counter.live.fetch_add(1, std::memory_order_relaxed);
	counter.created.fetch_add(1, std::memory_order_relaxed);
}

/** Reclaimer: counts the destruction of `object` (its dynamic type), before its destructor runs (checked builds) */
void countLiveInstanceDestroyed(const RefCounted& object) noexcept;

} // namespace detail

} // namespace aion::gameserver::runtime
