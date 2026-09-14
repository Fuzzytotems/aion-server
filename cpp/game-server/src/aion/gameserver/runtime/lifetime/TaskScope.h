#pragma once

#include <cstdint>
#include <source_location>

#include "aion/gameserver/runtime/base/TaskInfo.h"

namespace aion::gameserver::runtime {

/**
 * RAII task scope, opened by every entry point (design §1.2): pool workers, the PacketProcessor decorator, AionConnection strand callbacks,
 * cron runners, ForkJoin helpers, startup and shutdown phases, the CleanerDrain, tests.
 *
 * - Entering the outermost scope assigns a new process-unique scope id (Ptr stamps, C1) and records the TaskInfo and start time in the
 *   thread's ThreadContext (watchdog, slow-task warnings, Reclaimer stats). Nested scopes only increment the depth counter; their TaskInfo is
 *   ignored.
 * - The epoch is NOT published on entry; the first pointer-loading operation calls ensurePublished() (lazy publication, design §2.4 RR-4), so a
 *   task that borrows nothing never holds up reclamation.
 * - Leaving the outermost scope stores IDLE into the published epoch, flushes the thread's retire list to the Reclaimer and clears the task
 *   record. Every Ptr created inside the scope is invalid afterwards.
 * - The rule "anything reached during a task stays readable until the task ends" holds on every registered thread, IO threads included.
 *
 * Thread-safety: a scope belongs to the thread that created it and must be destroyed on that thread, in LIFO order. Constructors are noexcept.
 */
class TaskScope {
public:
	/** Opens (or nests into) the calling thread's task scope. */
	explicit TaskScope(const TaskInfo& info) noexcept;

	/**
	 * JOIN helper scope (design §2.6, §7.6 Isolation::JOIN): the calling ForkJoin helper thread adopts `submitterScopeId`, so Ptrs created by
	 * the blocked submitter stay valid on the helper, and quiescentPoint() is a no-op until this scope ends. Must be an outermost scope on the
	 * helper thread. The helper publishes its own epoch lazily as usual.
	 */
	TaskScope(const TaskInfo& info, uint64_t submitterScopeId) noexcept;

	~TaskScope();
	TaskScope(const TaskScope&) = delete;
	TaskScope& operator=(const TaskScope&) = delete;

	/** true if the calling thread is inside a TaskScope */
	static bool active() noexcept;
	/** nesting depth on the calling thread (0 outside scopes) */
	static uint32_t depth() noexcept;
	/** id of the calling thread's outermost scope (or the adopted id of a JOIN helper), 0 outside scopes; changes on quiescentPoint() */
	static uint64_t currentScopeId() noexcept;
	/** the TaskInfo of the calling thread's outermost scope (default TaskInfo outside scopes) */
	static TaskInfo currentTaskInfo() noexcept;
	/** true if the calling thread runs a JOIN helper scope */
	static bool isJoinedHelper() noexcept;

	/**
	 * Read barrier (design §2.4): publishes the global epoch for the calling thread if it is not published yet:
	 * `if (active == IDLE) do { e = E.load(); active.store(e); } while (E.load() != e);`
	 * Cheap when already published (one TLS load and a predicted branch). Called by every pointer-loading operation before it loads.
	 * Checked builds: terminates with a stack trace if called outside a TaskScope (C2) or inside the Reclaimer's destructor context (C8).
	 * Yield points: "TaskScope::ensurePublished:load", ":store", ":recheck".
	 */
	static void ensurePublished() noexcept;

	/** true if the calling thread has published an epoch in its current scope (tests, assertions) */
	static bool isPublished() noexcept;
};

/**
 * Marks a block, opened directly in a task body (TaskScope::depth() == 1, not a JOIN helper), whose enclosing frames hold only Refs, pinned
 * captures and immortals (design §1.2, §2.6). Required for quiescentPoint(). Nestable (inner scopes are no-ops); must be destroyed on the
 * creating thread. Opening it at depth != 1 or on a JOIN helper is legal but makes quiescentPoint() a warned no-op (C16).
 */
class QuiescentScope {
public:
	explicit QuiescentScope(std::source_location where = std::source_location::current()) noexcept;
	~QuiescentScope();
	QuiescentScope(const QuiescentScope&) = delete;
	QuiescentScope& operator=(const QuiescentScope&) = delete;
};

/**
 * Inside a valid QuiescentScope (open, TaskScope::depth() == 1, not a JOIN helper): unpublishes the epoch, flushes the thread's retire list
 * and starts a new scope id, so every older Ptr throws in checked builds (C1) and objects borrowed so far may be reclaimed.
 * Anywhere else: no-op, plus a once-per-call-site warning in checked builds (C16, design §2.6 RR-6).
 * Typical use: one call at the top of each iteration of a long loop over a std::vector<Ref<T>> snapshot (PeriodicSaveService, FixPath).
 */
void quiescentPoint(std::source_location where = std::source_location::current()) noexcept;

} // namespace aion::gameserver::runtime
