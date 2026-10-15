#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <typeinfo>
#include <utility>

#include "aion/commons/utils/TimeUtils.h"

namespace aion::commons::utils::concurrent {

namespace detail {

/** An object with a run() method, like a Java Runnable or a network packet */
template <typename T>
concept HasRunMethod = requires(T& t) { t.run(); };

/** A pointer-like to an object with a run() method (raw pointer, std::unique_ptr, std::shared_ptr) */
template <typename T>
concept PointsToRunnable = !HasRunMethod<T> && requires(T& t) {
	(*t).run();
	typeid(*t);
};

/**
 * What ExecuteWrapper can execute (Java: Runnable): an object with a run() method (called in preference), a pointer-like to one, or any
 * callable without arguments (a lambda, std::function, a function pointer, ...).
 */
template <typename T>
concept Runnable = HasRunMethod<T> || PointsToRunnable<T> || std::invocable<T&>;

template <Runnable T>
void run(T& runnable) {
	if constexpr (HasRunMethod<T>)
		runnable.run();
	else if constexpr (PointsToRunnable<T>)
		(*runnable).run();
	else
		std::invoke(runnable);
}

/** Java: runnable.getClass() - the dynamic type for polymorphic objects, the stored target for std::function */
template <typename R, typename... Args>
const std::type_info& runnableType(const std::function<R(Args...)>& runnable) noexcept {
	return runnable.target_type();
}

template <typename T>
const std::type_info& runnableType(const T& runnable) {
	if constexpr (PointsToRunnable<T>)
		return typeid(*runnable);
	else
		return typeid(runnable);
}

/** Records the stats (if enabled) and logs a warning if the duration exceeded the expected time */
void afterExecution(const std::type_info& type, int64_t durationNanos, int64_t expectedMaxExecutionTimeMillis);

/** Logs the exception currently being handled as "Exception in a Runnable execution:" */
void logExecutionException() noexcept;

} // namespace detail

/**
 * Java: com.aionemu.commons.utils.concurrent.ExecuteWrapper - an Executor that runs tasks in the calling thread, measures their execution time
 * (recorded by RunnableStatsManager if CommonsConfig::RUNNABLESTATS_ENABLE) and logs a warning if it exceeds the expected maximum. Exceptions
 * are logged or rethrown.
 * <pre>
 * ExecuteWrapper wrapper(ThreadConfig::MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING);
 * wrapper.execute(packet);                                        // calls packet.run(), stats and warnings use the packet's dynamic class
 * ExecuteWrapper::execute([&] { doWork(); }, 5000, false);       // exceptions propagate to the caller
 * </pre>
 *
 * @author NB4L1, Neon
 */
class ExecuteWrapper {
public:
	explicit ExecuteWrapper(int64_t expectedMaxExecutionTimeMillis) noexcept : expectedMaxExecutionTimeMillis(expectedMaxExecutionTimeMillis) {}

	/** Java: Executor.execute(Runnable) - runs the task, logging (not propagating) its exceptions */
	template <detail::Runnable T>
	void execute(T&& runnable) const {
		execute(std::forward<T>(runnable), expectedMaxExecutionTimeMillis, true);
	}

	/** Allows using the wrapper where a callable taking the task is expected (e.g. PacketProcessor's Executor). */
	template <detail::Runnable T>
	void operator()(T&& runnable) const {
		execute(std::forward<T>(runnable));
	}

	/**
	 * Runs the task in the calling thread. The execution time is only measured if the task completes normally.
	 * <p>
	 * The runnable is not accessed after its run() returns (its type is determined before), so a task may destroy or replace the object that holds
	 * it (e.g. a std::function slot or a self-deleting task). A pointed-to task must stay alive while its run() executes.
	 *
	 * @param catchAndLogThrowables if true, exceptions of the task are logged, otherwise they propagate to the caller
	 */
	template <detail::Runnable T>
	static void execute(T&& runnable, int64_t expectedMaxExecutionTimeMillis, bool catchAndLogThrowables) {
		try {
			const std::type_info& type = detail::runnableType(runnable); // Java: getClass() - read before the task can release itself
			int64_t begin = nanoTime();
			detail::run(runnable);
			int64_t durationNanos = nanoTime() - begin;
			detail::afterExecution(type, durationNanos, expectedMaxExecutionTimeMillis);
		} catch (...) {
			if (!catchAndLogThrowables)
				throw;
			detail::logExecutionException();
		}
	}

	int64_t getExpectedMaxExecutionTimeMillis() const noexcept { return expectedMaxExecutionTimeMillis; }

private:
	int64_t expectedMaxExecutionTimeMillis;
};

} // namespace aion::commons::utils::concurrent
