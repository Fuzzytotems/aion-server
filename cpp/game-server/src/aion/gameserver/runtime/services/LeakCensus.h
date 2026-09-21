#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::runtime {

/**
 * Interface of objects the zombie breaker may cut (design §5.3 "Zombie breaker", D7). VisibleObject implements it later: breakKnownEdges()
 * clears only the edges listed `zombie-safe` in cycles.toml (target, kisk, storage actors, observers, KnownList entries, summon links) and
 * returns the names of the edges it actually cut (static strings, e.g. "target"), for the warning.
 * Runs on the instant pool in a normal TaskScope while the object is pinned, so it may take Monitors and call services. Must be idempotent.
 * Exceptions are logged.
 */
class ZombieBreakable {
public:
	virtual std::vector<const char*> breakKnownEdges() = 0;

protected:
	~ZombieBreakable() = default;
};

/**
 * Leak census and zombie breaker (design §5.4, §5.3, RR-15/RR-18, D7).
 *
 * Hooks for the model code
 * - `World.removeObject` calls onRemovedFromWorld(object, className, objectId); `World.storeObject` of an object removed before calls
 *   onAddedToWorld(object). Both push an event {pointer, class name, id, time} to a lock-free MPSC queue: noexcept, no retain, no lock. The
 *   caller must hold a reference or borrow of the object during the call (it always does). Without install() both are no-ops.
 * - Objects implementing ZombieBreakable (RefCounted + ZombieBreakable, found by dynamic_cast) are eligible for the zombie breaker.
 *
 * Census (on the scanning thread only, i.e. the Reclaimer thread or a reclaimNow() caller; install() registers a post-scan hook and the
 * Reclaimer destroy observer)
 * - Events move from the queue into a private table (keyed by address, first removal time kept). The destroy observer moves pending events
 *   and erases the object's entry before the Reclaimer frees it, so the table's raw pointers never dangle; the census only reads an entry's
 *   reference count, and dereferences an entry only inside a post-scan hook, where no object can be destroyed.
 * - Every `checkInterval` (1 s): objects with a reference count > 0 removed longer than `censusAfter` (gameserver.debug.leak_census_minutes,
 *   10 min) are reported once as leaks (warning with class, id, refcount and the call sites of pending tasks pinning them, from
 *   ThreadPoolManager::tasksPinning; strings are static). Objects at count 0 are waiting for reclamation, not leaking.
 * - Zombie breaker (D7): objects with count > 0 removed longer than `zombieBreakAfter` (gameserver.runtime.zombie_break_minutes, 30 min) get
 *   breakKnownEdges() posted once to the instant pool, pinned by the task; each cut edge is logged as a warning naming class, id and edge (a
 *   missing cycle breaker to fix) and counted in zombieCutCount(); an object that is not ZombieBreakable, or whose breaker cut nothing, gets a
 *   warning saying so.
 * - Stale pins (design §5.4 last bullet, C13): every `stalePinCheckInterval` (1 min), a pending periodic task whose pinned owners
 *   (Future::pinnedOwners; immortals and templates are not owners) have ALL been removed from the world longer than `stalePinAfter` (10 min)
 *   is logged once (not cancelled). The warning starts with "Leak census: stale pin:" - the M5a scenario gate greps the log for "stale pin"
 *   (m5a-plan.md §5.7 Q8), so the literal is part of the contract. A stale pin is always a subset of the census leaks: only an object that was
 *   removed from the world and is still referenced can be in the table, and the pinning task is one of the references. It names the task that
 *   holds a leak, which the leak report cannot do for a task whose site is unknown.
 * - An object re-added to the world (onAddedToWorld) is removed from the census.
 * Times use the steady clock of ThreadPoolManager's backend (ManualClock in tests). The hooks never create the default pools
 * (ThreadPoolManager::clock/installedBackend) and do nothing once uninstall() has cleared the installed flag.
 *
 * Locks: a STATS leaf mutex guards the table for readers (getLeaks, trackedCount); ThreadPoolManager is only called without it.
 * Thread-safety: all members are thread-safe; reports are snapshots. install/uninstall/configure must not race with each other, and
 * uninstall() must not race with a running scan (tests call it between scans).
 */
class LeakCensus {
public:
	struct Config {
		/** gameserver.debug.leak_census_minutes */
		std::chrono::milliseconds censusAfter{std::chrono::minutes(10)};
		/** gameserver.runtime.zombie_break_minutes */
		std::chrono::milliseconds zombieBreakAfter{std::chrono::minutes(30)};
		bool zombieBreakerEnabled = true;
		/** how often the post-scan hook examines the table */
		std::chrono::milliseconds checkInterval{std::chrono::seconds(1)};
		/** periodic tasks pinning objects removed this long are logged (C13) */
		std::chrono::milliseconds stalePinAfter{std::chrono::minutes(10)};
		std::chrono::milliseconds stalePinCheckInterval{std::chrono::minutes(1)};
	};

	struct LeakReport {
		std::string className;
		int32_t objectId = 0;
		/** at the last census check */
		uint32_t refCount = 0;
		std::chrono::seconds removedFor{0};
		/** pending tasks pinning the object when it was reported */
		std::vector<TaskInfo> pinningTasks;
		/** edges cut by the zombie breaker so far (static strings) */
		std::vector<const char*> cutEdges;
	};

	static LeakCensus& getInstance();

	void configure(const Config& config);
	Config getConfig() const;
	/** Registers the Reclaimer post-scan hook and destroy observer (idempotent). Starts with an empty table. */
	void install();
	/** Removes the hook and the destroy observer and forgets every entry and queued event. */
	void uninstall();
	bool isInstalled() const noexcept;

	/** World.removeObject. `className` must have static storage duration. */
	void onRemovedFromWorld(const RefCounted& object, const char* className, int32_t objectId) noexcept;
	/** World.storeObject of an object that was removed before */
	void onAddedToWorld(const RefCounted& object) noexcept;

	/** `//debug leaks`: objects reported as leaks that are still alive, oldest removal first, including zombie-breaker cuts */
	std::vector<LeakReport> getLeaks() const;
	/** number of census entries (removed objects not yet destroyed; events not yet moved by a scan are not counted) */
	size_t trackedCount() const;
	/** total zombie-breaker cuts since start (stress harness pass criterion: 0) */
	uint64_t zombieCutCount() const noexcept;

private:
	LeakCensus() = default;
};

} // namespace aion::gameserver::runtime
