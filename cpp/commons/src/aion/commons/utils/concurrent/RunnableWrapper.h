#pragma once

#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

#include "aion/commons/utils/concurrent/ExecuteWrapper.h"

namespace aion::commons::utils::concurrent {

/**
 * Java: com.aionemu.commons.utils.concurrent.RunnableWrapper - wraps a task so that running it goes through ExecuteWrapper::execute (execution
 * time measurement, slow execution warnings and exception logging). The wrapper is itself a callable without arguments with a run() method,
 * so it can be stored in a std::function or passed to a thread pool:
 * <pre>
 * pool.execute(RunnableWrapper([this] { update(); }, ThreadConfig::MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING, true));
 * </pre>
 *
 * @author -Nemesiss-
 */
template <detail::Runnable T>
class RunnableWrapper {
public:
	/** Never warns about the execution time, logs exceptions. */
	explicit RunnableWrapper(T runnable) : RunnableWrapper(std::move(runnable), std::numeric_limits<int64_t>::max(), true) {}

	RunnableWrapper(T runnable, int64_t maxRuntimeMsWithoutWarning, bool catchAndLogThrowables)
		: runnable(std::move(runnable)), maxRuntimeMsWithoutWarning(maxRuntimeMsWithoutWarning), catchAndLogThrowables(catchAndLogThrowables) {}

	/** Java: run() */
	void run() { ExecuteWrapper::execute(runnable, maxRuntimeMsWithoutWarning, catchAndLogThrowables); }

	void operator()() { run(); }

private:
	T runnable;
	int64_t maxRuntimeMsWithoutWarning;
	bool catchAndLogThrowables;
};

template <typename T>
RunnableWrapper(T) -> RunnableWrapper<std::decay_t<T>>;

template <typename T>
RunnableWrapper(T, int64_t, bool) -> RunnableWrapper<std::decay_t<T>>;

} // namespace aion::commons::utils::concurrent
