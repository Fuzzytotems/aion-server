// Process memory sampling of the stress harness (kept apart so windows.h stays out of the kernel headers).

#include "StressHarness.h"

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>

#include "aion/commons/utils/WindowsMacroGuard.h"
#elif defined(__linux__)
#include <fstream>
#include <unistd.h>
#endif

namespace aion::gameserver::runtime::stress {

MemorySample sampleProcessMemory() {
	MemorySample sample;
#if defined(_WIN32)
	PROCESS_MEMORY_COUNTERS_EX counters{};
	counters.cb = sizeof(counters);
	if (K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
		sample.privateBytes = counters.PrivateUsage;
		sample.workingSet = counters.WorkingSetSize;
	}
	HEAP_SUMMARY summary{};
	summary.cb = sizeof(summary);
	if (HeapSummary(GetProcessHeap(), 0, &summary))
		sample.heapAllocated = summary.cbAllocated;
#elif defined(__linux__)
	std::ifstream statm("/proc/self/statm");
	uint64_t size = 0;
	uint64_t resident = 0;
	if (statm >> size >> resident) {
		auto page = static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
		sample.privateBytes = size * page;
		sample.workingSet = resident * page;
	}
#endif
	return sample;
}

} // namespace aion::gameserver::runtime::stress
