#pragma once

#include <concepts>
#include <cstddef>
#include <memory>
#include <source_location>
#include <string>
#include <utility>

#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"

namespace aion::gameserver::runtime {

/**
 * Runs tasks one at a time, in submission order, on a ThreadPoolManager pool (design §1.4 "LS/CS link packets run in order per link", §11).
 *
 * - execute() enqueues the task. If no task of this executor is queued in or running on the pool, the task is dispatched to the pool at once;
 *   otherwise it is dispatched by its predecessor right after the predecessor's body returned (or threw). At most one task of an executor is on
 *   the pool at any time, so bodies never overlap and run in submission order, while every task yields the pool thread (a busy link cannot
 *   monopolize a pool thread, and each task is visible to the stats and the watchdog under its own call site).
 * - Each task runs in its own outermost TaskScope (kind SERIAL, its call site) through Future::runFromExecutor: RunnableWrapper semantics, an
 *   exception is logged and the next task runs.
 * - The queue is guarded by a SCHEDULER leaf mutex that is never held while a task runs or while a task is handed to the pool.
 * - When the instant queue is full, the pool's rejection policy may run a dispatched task inline on the dispatching thread. Hand-overs are
 *   iterative (a per-thread trampoline in the outermost finishing task), so a long queue then runs task after task at a bounded stack depth
 *   instead of nesting one level per queued task.
 * - Lifetime: the queue state is shared with the dispatched tasks, so destroying the executor is memory-safe at any time: queued tasks are
 *   cancelled (captures released) and a running task finishes without dispatching further tasks. Pins keep the task's owners alive as usual.
 * - After ThreadPoolManager::shutdown() dispatched tasks are cancelled by the pool; the executor stays blocked (nothing runs after shutdown).
 * - `maxTasksPerTurn` is accepted for source compatibility and ignored: every turn runs exactly one task (see above).
 * Thread-safety: all members are thread-safe.
 */
class SerialExecutor {
public:
	explicit SerialExecutor(std::string name, PoolKind pool = PoolKind::INSTANT, size_t maxTasksPerTurn = 64);
	~SerialExecutor();
	SerialExecutor(const SerialExecutor&) = delete;
	SerialExecutor& operator=(const SerialExecutor&) = delete;

	template <std::invocable F>
	void execute(Pin pin, F&& task, std::source_location where = std::source_location::current()) {
		enqueue(std::move(pin), Future::Body([task = std::forward<F>(task)](Future&) mutable { task(); }), TaskInfo{where, TaskKind::SERIAL});
	}

	template <UnpinnedTask F>
	void execute(F&& task, std::source_location where = std::source_location::current()) {
		enqueue(Pin(), Future::Body([task = std::forward<F>(task)](Future&) mutable { task(); }), TaskInfo{where, TaskKind::SERIAL});
	}

	/** tasks queued and not yet dispatched to the pool (the dispatched or running task is not counted) */
	size_t queuedTasks() const;
	/** true while a task of this executor is dispatched to or running on the pool */
	bool isActive() const;
	const std::string& getName() const noexcept { return name_; }

	/** queue state shared with dispatched tasks (implementation detail) */
	struct State;

private:
	void enqueue(Pin pin, Future::Body body, const TaskInfo& info);

	const std::string name_;
	const PoolKind pool_;
	const size_t maxTasksPerTurn_;
	std::shared_ptr<State> state_;
};

} // namespace aion::gameserver::runtime
