#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <exception>
#include <functional>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#include "aion/commons/utils/concurrent/ThreadName.h"
#include "aion/commons/utils/concurrent/UncaughtExceptionHandler.h"

namespace aion::commons::utils::concurrent {

/** Java: Thread.MIN_PRIORITY */
inline constexpr int32_t MIN_PRIORITY = 1;
/** Java: Thread.NORM_PRIORITY */
inline constexpr int32_t NORM_PRIORITY = 5;
/** Java: Thread.MAX_PRIORITY */
inline constexpr int32_t MAX_PRIORITY = 10;

/**
 * Java: Thread.currentThread().setPriority(priority). Maps Java priorities (MIN_PRIORITY to MAX_PRIORITY) to the operating system like the
 * HotSpot JVM: on Windows 1-2 lowest, 3-4 below normal, 5-6 normal, 7-8 above normal, 9-10 highest. Other systems ignore priorities (like
 * HotSpot's default policy on Linux).
 *
 * @throws IllegalArgumentException if the priority is not in the range MIN_PRIORITY to MAX_PRIORITY
 */
void setCurrentThreadPriority(int32_t priority);

/**
 * Java: com.aionemu.commons.utils.concurrent.PriorityThreadFactory - starts threads named "&lt;name&gt;-&lt;number&gt;" (numbered from 1)
 * with the given priority.
 * <p>
 * An exception escaping the task is passed to UncaughtExceptionHandler::uncaughtException and ends only that thread, like in Java.
 *
 * @author -Nemesiss-
 */
class PriorityThreadFactory {
public:
	/**
	 * @param name prefix of the thread names (Java: also the thread group name)
	 * @param prio priority of new threads, see setCurrentThreadPriority
	 */
	PriorityThreadFactory(std::string name, int32_t prio) : prio(prio), name(std::move(name)) {}

	/**
	 * Java: newThread(Runnable). Unlike Java, the thread is started immediately. The returned std::jthread joins on destruction, so keep it (or
	 * call detach() on it) - discarding the result would block the caller until the task has finished.
	 *
	 * @throws IllegalArgumentException if the priority of this factory is invalid (the thread is not started then)
	 */
	template <typename F>
		requires std::invocable<std::decay_t<F>&>
	[[nodiscard("the thread joins when the returned std::jthread is destroyed")]] std::jthread newThread(F&& runnable) {
		checkPriority(prio);
		std::string threadName = name + "-" + std::to_string(threadNumber.fetch_add(1));
		return std::jthread([runnable = std::forward<F>(runnable), threadName = std::move(threadName), prio = prio]() mutable noexcept {
			try {
				setCurrentThreadName(threadName);
				setCurrentThreadPriority(prio);
				std::invoke(runnable);
			} catch (...) {
				UncaughtExceptionHandler::uncaughtException(threadName, std::current_exception());
			}
		});
	}

	const std::string& getName() const noexcept { return name; }
	int32_t getPriority() const noexcept { return prio; }

private:
	static void checkPriority(int32_t priority);

	/** Priority of new threads */
	const int32_t prio;
	/** Thread name prefix */
	const std::string name;
	/** Number of created threads */
	std::atomic<int32_t> threadNumber{1};
};

} // namespace aion::commons::utils::concurrent
