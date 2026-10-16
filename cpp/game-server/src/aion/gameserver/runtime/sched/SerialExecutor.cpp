#include "aion/gameserver/runtime/sched/SerialExecutor.h"

#include <deque>
#include <mutex>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/detail/SchedRuntime.h"
#include "aion/gameserver/runtime/sync/RankedMutex.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::runtime {

namespace {

using SchedulerLock = std::unique_lock<RankedMutex<LockRank::SCHEDULER>>;

const commons::logging::Logger& log() {
	static const auto* instance =
		new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.runtime.SerialExecutor"));
	return *instance;
}

/**
 * Hand-over trampoline of one thread (review finding: unbounded recursion). A successor is dispatched from the finishing task's continuation;
 * when the instant queue is full the pool's rejection policy runs the successor INLINE on the calling thread, and its own continuation would
 * dispatch the next one inline again, one stack level per queued task. The outermost continuation on a thread therefore owns a trampoline:
 * nested continuations (successors running inline inside it) only append their state here, and the outermost one dispatches them in a loop.
 * The list is intrusive (State::trampolineNext), so appending never allocates: a state has at most one task on the pool, hence at most one
 * pending hand-over.
 */
struct Trampoline {
	std::shared_ptr<SerialExecutor::State> head;
	SerialExecutor::State* tail = nullptr;
};

thread_local Trampoline* currentTrampoline = nullptr;

} // namespace

struct SerialExecutor::State {
	explicit State(PoolKind pool) : pool(pool) {}

	const PoolKind pool;
	mutable RankedMutex<LockRank::SCHEDULER> mutex;
	std::deque<FutureRef> queue;
	/** a task is dispatched to or running on the pool */
	bool active = false;
	bool closed = false;
	/** next pending hand-over of the trampoline this state is queued in (owned by that trampoline's thread) */
	std::shared_ptr<State> trampolineNext;

	/** hands a task to the pool (no lock held) */
	static void dispatch(const std::shared_ptr<State>& state, FutureRef task) noexcept {
		try {
			utils::ThreadPoolManager::getInstance().backend().execute(state->pool, std::move(task));
		} catch (...) {
			try {
				log().errorCurrentException("Could not dispatch a serial task to the pool");
			} catch (...) {
			}
			SchedulerLock lock(state->mutex);
			state->active = false;
		}
	}

	/**
	 * Called by a dispatched task after its body (its continuation): hands over to the next queued task. Iterative: see Trampoline. The
	 * successor's dispatch happens after the finishing body returned (still inside its pool thread's run), in submission order.
	 */
	static void taskFinished(std::shared_ptr<State> state) noexcept {
		if (Trampoline* trampoline = currentTrampoline) {
			// nested inside an outer hand-over on this thread (the successor ran inline): let the outer frame dispatch
			SerialExecutor::State* raw = state.get();
			if (trampoline->tail != nullptr)
				trampoline->tail->trampolineNext = std::move(state);
			else
				trampoline->head = std::move(state);
			trampoline->tail = raw;
			return;
		}
		Trampoline trampoline;
		currentTrampoline = &trampoline;
		onTaskFinished(state);
		while (trampoline.head) {
			std::shared_ptr<State> next = std::move(trampoline.head);
			trampoline.head = std::move(next->trampolineNext);
			if (!trampoline.head)
				trampoline.tail = nullptr;
			onTaskFinished(next);
		}
		currentTrampoline = nullptr;
	}

	/** dispatches the next queued task or ends the turn */
	static void onTaskFinished(const std::shared_ptr<State>& state) noexcept {
		FutureRef next;
		{
			SchedulerLock lock(state->mutex);
			if (state->closed || state->queue.empty()) {
				state->active = false;
				return;
			}
			next = std::move(state->queue.front());
			state->queue.pop_front();
		}
		dispatch(state, std::move(next));
	}
};

SerialExecutor::SerialExecutor(std::string name, PoolKind pool, size_t maxTasksPerTurn)
	: name_(std::move(name)), pool_(pool == PoolKind::LONG_RUNNING ? PoolKind::LONG_RUNNING : PoolKind::INSTANT), maxTasksPerTurn_(maxTasksPerTurn),
		state_(std::make_shared<State>(pool_)) {
}

SerialExecutor::~SerialExecutor() {
	std::deque<FutureRef> queued;
	{
		SchedulerLock lock(state_->mutex);
		state_->closed = true;
		queued.swap(state_->queue);
	}
	for (FutureRef& task : queued)
		(void)task->cancel(false);
}

void SerialExecutor::enqueue(Pin pin, Future::Body body, const TaskInfo& info) {
	// the continuation runs after the body, also when it throws (the exception is then logged by runFromExecutor)
	Future::Body serialBody([state = std::weak_ptr<State>(state_), body = std::move(body)](Future& future) mutable {
		struct Continuation {
			std::weak_ptr<State>& state;
			~Continuation() {
				if (std::shared_ptr<State> locked = state.lock())
					State::taskFinished(std::move(locked));
			}
		} continuation{state};
		body(future);
	});
	Future::Schedule schedule;
	schedule.pool = PoolKind::NONE;
	FutureRef task = Future::create(std::move(pin), std::move(serialBody), info, schedule);
	{
		SchedulerLock lock(state_->mutex);
		if (state_->closed)
			throw IllegalStateException("SerialExecutor " + name_ + " is closed");
		if (state_->active) {
			state_->queue.push_back(std::move(task));
			return;
		}
		state_->active = true;
	}
	State::dispatch(state_, std::move(task));
}

size_t SerialExecutor::queuedTasks() const {
	SchedulerLock lock(state_->mutex);
	return state_->queue.size();
}

bool SerialExecutor::isActive() const {
	SchedulerLock lock(state_->mutex);
	return state_->active;
}

} // namespace aion::gameserver::runtime
