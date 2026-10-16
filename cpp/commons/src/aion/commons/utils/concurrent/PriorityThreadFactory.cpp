#include "aion/commons/utils/concurrent/PriorityThreadFactory.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include "aion/commons/utils/Exception.h"

namespace aion::commons::utils::concurrent {

void setCurrentThreadPriority(int32_t priority) {
	if (priority < MIN_PRIORITY || priority > MAX_PRIORITY)
		throw IllegalArgumentException("Invalid thread priority: " + std::to_string(priority));
#ifdef _WIN32
	// HotSpot's os::java_to_os_priority table for Windows
	static constexpr int PRIORITIES[] = {THREAD_PRIORITY_IDLE,         THREAD_PRIORITY_LOWEST,       THREAD_PRIORITY_LOWEST,
																			 THREAD_PRIORITY_BELOW_NORMAL, THREAD_PRIORITY_BELOW_NORMAL, THREAD_PRIORITY_NORMAL,
																			 THREAD_PRIORITY_NORMAL,       THREAD_PRIORITY_ABOVE_NORMAL, THREAD_PRIORITY_ABOVE_NORMAL,
																			 THREAD_PRIORITY_HIGHEST,      THREAD_PRIORITY_HIGHEST};
	SetThreadPriority(GetCurrentThread(), PRIORITIES[priority]);
#endif
}

void PriorityThreadFactory::checkPriority(int32_t priority) {
	if (priority < MIN_PRIORITY || priority > MAX_PRIORITY)
		throw IllegalArgumentException("Invalid thread priority: " + std::to_string(priority));
}

} // namespace aion::commons::utils::concurrent
