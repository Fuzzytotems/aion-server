#pragma once

#include <cstddef>
#include <cstdint>

namespace aion::gameserver::runtime {

/**
 * ID release for auto-release objects (design §6 group (a), RR-2/RR-11), replacing Java's Cleaner registered in AionObject.java:31-34.
 *
 * - `~AionObject` (and other release-only destructors) call CleanerQueue::push(objectId, className): lock-free MPSC (a Treiber stack drained
 *   as a whole), noexcept, legal inside the Reclaimer's destructor context (it touches no game object and takes no lock). If the node cannot
 *   be allocated the id is dropped and counted (getDroppedCount), never thrown.
 * - install() registers a Reclaimer post-scan hook: after each scan with a non-empty queue it posts one CleanerDrain task to the instant pool
 *   (ThreadPoolManager::submit; at most one posted drain is pending or running at a time; nothing is posted after ThreadPoolManager::shutdown).
 * - The CleanerDrain runs as an ordinary instant-pool task (TaskScope with the drain's call site; a nested CLEANER scope) and calls the cleaner
 *   action for each id taken from the queue, in push order per pushing thread (FIFO for a single thread; ids pushed by different threads
 *   concurrently have no defined relative order, as with Java's Cleaner). The action is the Java Cleaner body,
 *   `if (!RespawnService::setAutoReleaseId(id)) IDFactory::releaseId(id)` (RespawnService.java:95-100,201-226), which may take Monitors
 *   (SYNCHRONIZED handoff with RespawnService.unregister) because it is ordinary task code. An exception thrown by the action for one id is
 *   logged and the drain continues with the next id.
 * - A drain processes the ids queued when it starts; ids pushed while it runs (e.g. by the action itself) are left for the next drain.
 * - drainNow() runs a drain on the calling thread (ThreadPoolManager::shutdown's final drain, design §11 step 6; tests).
 * Thread-safety: all members are thread-safe. install/uninstall must not race with each other.
 */
class CleanerQueue {
public:
	/**
	 * The Java Cleaner body for one id. `className` is the class name given to push (static storage, may be nullptr).
	 * Default (until the game model installs its own): IDFactory::getInstance().releaseId(objectId, className).
	 */
	using CleanerAction = void (*)(int32_t objectId, const char* className);

	/** Destructors: queue an id for release. Lock-free, noexcept. `className` must have static storage duration (or be nullptr). */
	static void push(int32_t objectId, const char* className = nullptr) noexcept;

	/** Installs the Reclaimer post-scan hook that posts CleanerDrain tasks (idempotent). */
	static void install();
	/** Removes the hook (tests). A drain already posted still runs. */
	static void uninstall();
	static bool isInstalled() noexcept;

	/** nullptr restores the default action. */
	static void setCleanerAction(CleanerAction action) noexcept;

	/** Runs the cleaner action for every queued id on the calling thread (inside a TaskScope, nested if one is active). @return ids processed */
	static size_t drainNow();

	/** ids queued and not yet taken by a drain (racy snapshot) */
	static size_t size() noexcept;
	static bool isEmpty() noexcept { return size() == 0; }

	/** ids lost because a queue node could not be allocated */
	static uint64_t getDroppedCount() noexcept;
	/** CleanerDrain tasks posted by the hook so far (tests, //debug) */
	static uint64_t getPostedDrainCount() noexcept;
};

} // namespace aion::gameserver::runtime
