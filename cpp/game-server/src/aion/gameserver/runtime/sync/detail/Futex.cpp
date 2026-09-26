#include "aion/gameserver/runtime/sync/detail/Futex.h"

#include <chrono>
#include <thread>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#pragma comment(lib, "Synchronization.lib")
#elif defined(__linux__)
#include <cerrno>
#include <climits>
#include <ctime>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace aion::gameserver::runtime::detail {

// the address of the atomic is the address of its 4-byte value on every supported standard library
static_assert(sizeof(std::atomic<uint32_t>) == sizeof(uint32_t));
static_assert(std::is_standard_layout_v<std::atomic<uint32_t>>);

#if defined(_WIN32)

bool futexWait(std::atomic<uint32_t>& word, uint32_t expected, int64_t timeoutNanos) noexcept {
	DWORD millis = INFINITE;
	if (timeoutNanos >= 0) {
		// round up, so a positive timeout never becomes a zero-length poll; cap below INFINITE
		int64_t ms = (timeoutNanos + 999'999) / 1'000'000;
		millis = ms >= static_cast<int64_t>(INFINITE) ? INFINITE - 1 : static_cast<DWORD>(ms);
	}
	if (WaitOnAddress(&word, &expected, sizeof(uint32_t), millis))
		return true;
	return GetLastError() != ERROR_TIMEOUT;
}

void futexWakeOne(std::atomic<uint32_t>& word) noexcept {
	WakeByAddressSingle(&word);
}

void futexWakeAll(std::atomic<uint32_t>& word) noexcept {
	WakeByAddressAll(&word);
}

#elif defined(__linux__)

bool futexWait(std::atomic<uint32_t>& word, uint32_t expected, int64_t timeoutNanos) noexcept {
	timespec timeout{};
	timespec* timeoutPointer = nullptr;
	if (timeoutNanos >= 0) {
		timeout.tv_sec = static_cast<time_t>(timeoutNanos / 1'000'000'000);
		timeout.tv_nsec = static_cast<long>(timeoutNanos % 1'000'000'000);
		timeoutPointer = &timeout;
	}
	long result = syscall(SYS_futex, static_cast<void*>(&word), FUTEX_WAIT_PRIVATE, expected, timeoutPointer, nullptr, 0);
	return !(result == -1 && errno == ETIMEDOUT);
}

void futexWakeOne(std::atomic<uint32_t>& word) noexcept {
	syscall(SYS_futex, static_cast<void*>(&word), FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0);
}

void futexWakeAll(std::atomic<uint32_t>& word) noexcept {
	syscall(SYS_futex, static_cast<void*>(&word), FUTEX_WAKE_PRIVATE, INT_MAX, nullptr, nullptr, 0);
}

#else

bool futexWait(std::atomic<uint32_t>& word, uint32_t expected, int64_t timeoutNanos) noexcept {
	if (timeoutNanos < 0) {
		word.wait(expected, std::memory_order_acquire);
		return true;
	}
	if (word.load(std::memory_order_acquire) != expected)
		return true;
	std::this_thread::sleep_for(std::chrono::nanoseconds(timeoutNanos < 1'000'000 ? timeoutNanos : 1'000'000));
	return true; // callers re-check their deadline
}

void futexWakeOne(std::atomic<uint32_t>& word) noexcept {
	word.notify_one();
}

void futexWakeAll(std::atomic<uint32_t>& word) noexcept {
	word.notify_all();
}

#endif

} // namespace aion::gameserver::runtime::detail
