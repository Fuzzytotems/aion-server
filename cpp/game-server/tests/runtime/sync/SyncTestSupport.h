#pragma once

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>

#include "aion/gameserver/runtime/base/ThreadContext.h"

// Helpers of the sync and fields kernel tests. Concurrency tests must never hang the test run: every test that blocks threads opens a
// HangGuard, which terminates the process with a message if the test does not finish in time.

namespace aion::gameserver::runtime::testsupport {

/** Terminates the process (exit code 3) if not destroyed within `timeout`. */
class HangGuard {
public:
	explicit HangGuard(std::chrono::milliseconds timeout, std::string what = "test")
		: thread([timeout, what = std::move(what)](std::stop_token stop) {
			  std::mutex mutex;
			  std::condition_variable_any condition;
			  std::unique_lock lock(mutex);
			  condition.wait_for(lock, stop, timeout, [] { return false; });
			  if (!stop.stop_requested()) {
				  std::fprintf(stderr, "HangGuard: %s did not finish within %lld ms, terminating\n", what.c_str(),
					  static_cast<long long>(timeout.count()));
				  std::fflush(stderr);
				  std::_Exit(3);
			  }
		  }) {}

	HangGuard(const HangGuard&) = delete;
	HangGuard& operator=(const HangGuard&) = delete;

private:
	std::jthread thread;
};

/** Polls `condition` every millisecond until it is true or `timeout` elapsed. @return the last result */
inline bool waitUntil(const std::function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
	auto deadline = std::chrono::steady_clock::now() + timeout;
	while (!condition()) {
		if (std::chrono::steady_clock::now() >= deadline)
			return condition();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return true;
}

/** Finds the live ThreadContext with `threadId` and calls `visitor` on it. @return true if found */
inline bool withThread(uint64_t threadId, const std::function<void(const ThreadContext&)>& visitor) {
	bool found = false;
	ThreadContext::forEach([&](const ThreadContext& context) {
		if (!found && context.threadId() == threadId) {
			found = true;
			visitor(context);
		}
	});
	return found;
}

/** true if the thread's held-lock records contain `lockId` */
inline bool threadHoldsLock(uint64_t threadId, uintptr_t lockId) {
	bool holds = false;
	withThread(threadId, [&](const ThreadContext& context) {
		uint32_t count = context.heldLockCount.load();
		for (uint32_t i = 0; i < count && i < ThreadContext::MAX_RECORDED_LOCKS; ++i)
			holds = holds || context.heldLocks[i].lockId.load() == lockId;
	});
	return holds;
}

/** true if the thread currently waits for `lockId` */
inline bool threadWaitsFor(uint64_t threadId, uintptr_t lockId) {
	bool waits = false;
	withThread(threadId, [&](const ThreadContext& context) {
		ThreadContext::WaitSnapshot wait = context.wait();
		waits = wait.waiting && wait.lockId == lockId;
	});
	return waits;
}

} // namespace aion::gameserver::runtime::testsupport
