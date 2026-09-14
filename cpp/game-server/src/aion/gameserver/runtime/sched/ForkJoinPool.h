#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <source_location>
#include <type_traits>

#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"

namespace aion::gameserver::runtime {

/** Isolation of parallelForEach helpers (design §2.6, §7.6). Tag objects, so PER_ELEMENT constraints are checked at compile time. */
struct Isolation {
	/** helpers join the caller's scope id: Ptrs of the submitter are valid on helpers; quiescentPoint is a no-op on helpers (default) */
	struct JoinTag {};
	/** each element runs in its own TaskScope; elements must be Refs, immortal/template pointers or values (compile-time) */
	struct PerElementTag {};
	static constexpr JoinTag JOIN{};
	static constexpr PerElementTag PER_ELEMENT{};
};

/** Element types allowed with Isolation::PER_ELEMENT: TaskArgs (Ref<X>, immortal and template pointers, values). */
template <class E>
concept PerElementSafe = TaskArg<std::remove_cvref_t<E>>;

/**
 * Java ForkJoinPool.commonPool() for parallel streams (design §1.1, §7.6): MoveTaskManager ticks and startup `parallel()` sites.
 *
 * parallelForEach splits [0, size) over `getParallelism()` helper threads ("ForkJoin-n", cores - 1) plus the calling thread, which also works
 * and waits (inside a BlockingRegion) until every element has run. The first exception thrown by `fn` is rethrown on the caller after all
 * elements finished (remaining elements still run, like Java's parallel forEach which completes other subtasks).
 * Serial (on the caller, in order) when the pool is serial (`gameserver.debug.single_executor`, `setSerial(true)` for serial_movement), when
 * called from a helper thread of this pool, or for fewer than 2 elements.
 * Thread-safety: parallelForEach may be called concurrently from several threads.
 */
class ForkJoinPool {
public:
	static ForkJoinPool& commonPool();

	/** helper threads (cores - 1, at least 1) */
	int32_t getParallelism() const noexcept;

	template <std::ranges::random_access_range R, class F>
		requires std::invocable<F&, std::ranges::range_reference_t<R>>
	void parallelForEach(R&& items, F&& fn, Isolation::JoinTag = Isolation::JOIN, std::source_location where = std::source_location::current()) {
		auto begin = std::ranges::begin(items);
		struct Context {
			decltype(begin) first;
			F* function;
		} context{begin, &fn};
		runIndexed(static_cast<size_t>(std::ranges::distance(items)),
			[](void* opaque, size_t index) {
				auto* c = static_cast<Context*>(opaque);
				(*c->function)(c->first[static_cast<std::ranges::range_difference_t<R>>(index)]);
			},
			&context, false, TaskInfo{where, TaskKind::FORK_JOIN});
	}

	template <std::ranges::random_access_range R, class F>
		requires std::invocable<F&, std::ranges::range_reference_t<R>> && PerElementSafe<std::ranges::range_value_t<R>>
	void parallelForEach(R&& items, F&& fn, Isolation::PerElementTag, std::source_location where = std::source_location::current()) {
		auto begin = std::ranges::begin(items);
		struct Context {
			decltype(begin) first;
			F* function;
		} context{begin, &fn};
		runIndexed(static_cast<size_t>(std::ranges::distance(items)),
			[](void* opaque, size_t index) {
				auto* c = static_cast<Context*>(opaque);
				(*c->function)(c->first[static_cast<std::ranges::range_difference_t<R>>(index)]);
			},
			&context, true, TaskInfo{where, TaskKind::FORK_JOIN});
	}

	/** serial mode for `gameserver.debug.serial_movement` / single_executor */
	void setSerial(bool serial) noexcept;
	bool isSerial() const noexcept;
	/** stops the helper threads (shutdown); later calls run serially */
	void shutdown();

private:
	ForkJoinPool() = default;

	/**
	 * Runs body(context, i) for every i in [0, count) as described above.
	 * JOIN: the caller publishes its epoch before the job is visible to helpers (TaskScope::ensurePublished; it then stays published until its
	 * outermost scope ends), so objects loaded by any participant stay valid for the caller and every helper even after that participant left
	 * the job: Ptrs may be handed between helpers and the caller (e.g. collected into a caller-owned vector). The caller works on elements
	 * too, inside a nested TaskScope (depth >= 2, so quiescentPoint() in an element is a warned no-op on the caller as well); helpers open
	 * TaskScope(info, callerScopeId) (joined helper scopes). A caller without a scope gets an outermost scope for the duration of the call.
	 * PER_ELEMENT: only helper threads run elements, each element in its own outermost TaskScope(info); the caller just waits in a
	 * BlockingRegion (it cannot open an outermost scope inside its own task). In serial mode the caller runs the elements itself in nested
	 * scopes (no reclamation between elements, acceptable for debug modes).
	 */
	void runIndexed(size_t count, void (*body)(void* context, size_t index), void* context, bool perElement, const TaskInfo& info);
};

} // namespace aion::gameserver::runtime
