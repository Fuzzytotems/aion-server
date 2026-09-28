#include "aion/loginserver/utils/ScheduledExecutor.h"

#include <algorithm>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/concurrent/ThreadName.h"

namespace aion::loginserver::utils {

namespace {

// leaked, so executors owned by static objects can log during static destruction
const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.utils.ScheduledExecutor"));
	return *logger;
}

} // namespace

ScheduledExecutor::ScheduledExecutor(std::string threadName) : threadName(std::move(threadName)) {
	thread = std::thread([this] { run(); });
}

ScheduledExecutor::~ScheduledExecutor() {
	shutdown();
	if (thread.joinable()) {
		if (thread.get_id() == std::this_thread::get_id())
			thread.detach(); // destroyed by its own task: not supported, but must not call std::terminate
		else
			thread.join();
	}
}

std::shared_ptr<ScheduledExecutor::ScheduledFuture> ScheduledExecutor::scheduleAtFixedRate(std::function<void()> task,
	std::chrono::milliseconds initialDelay, std::chrono::milliseconds period) {
	if (!task)
		throw commons::utils::IllegalArgumentException("task must not be empty");
	return scheduleAtFixedRate(std::function<void(ScheduledFuture&)>([task = std::move(task)](ScheduledFuture&) { task(); }), initialDelay, period);
}

std::shared_ptr<ScheduledExecutor::ScheduledFuture> ScheduledExecutor::scheduleAtFixedRate(std::function<void(ScheduledFuture& future)> task,
	std::chrono::milliseconds initialDelay, std::chrono::milliseconds period) {
	if (!task)
		throw commons::utils::IllegalArgumentException("task must not be empty");
	if (period.count() <= 0)
		throw commons::utils::IllegalArgumentException("period must be positive");
	auto future = std::make_shared<ScheduledFuture>();
	{
		std::lock_guard lock(mutex);
		if (shutDown)
			throw commons::utils::IllegalStateException("ScheduledExecutor " + threadName + " was shut down");
		entries.push_back({std::move(task), std::chrono::steady_clock::now() + std::max(initialDelay, std::chrono::milliseconds(0)), period, future});
	}
	changed.notify_all();
	return future;
}

void ScheduledExecutor::shutdown() {
	{
		std::lock_guard lock(mutex);
		if (shutDown)
			return;
		shutDown = true;
		for (Entry& entry : entries)
			entry.future->cancel();
		entries.clear();
	}
	changed.notify_all();
}

bool ScheduledExecutor::isTerminated() const {
	std::lock_guard lock(mutex);
	return terminated;
}

bool ScheduledExecutor::awaitTermination(std::chrono::milliseconds timeout) {
	std::unique_lock lock(mutex);
	return terminatedCondition.wait_for(lock, timeout, [this] { return terminated; });
}

void ScheduledExecutor::run() {
	commons::utils::concurrent::setCurrentThreadName(threadName);
	std::unique_lock lock(mutex);
	while (!shutDown) {
		std::erase_if(entries, [](const Entry& entry) { return entry.future->isCancelled(); });
		if (entries.empty()) {
			changed.wait(lock);
			continue;
		}
		auto next = std::ranges::min_element(entries, {}, &Entry::nextRun);
		if (next->nextRun > std::chrono::steady_clock::now()) {
			changed.wait_until(lock, next->nextRun);
			continue; // entries may have changed
		}
		// take the task out while it runs, so the entry list can change meanwhile
		Entry entry = std::move(*next);
		entries.erase(next);
		lock.unlock();
		bool failed = false;
		if (!entry.future->isCancelled()) {
			try {
				entry.task(*entry.future);
			} catch (...) {
				failed = true;
				try {
					log().errorCurrentException("Scheduled task of " + threadName + " failed and will not run again");
				} catch (...) {
				}
			}
		}
		lock.lock();
		if (failed)
			entry.future->cancel();
		if (!shutDown && !entry.future->isCancelled()) {
			entry.nextRun += entry.period;
			entries.push_back(std::move(entry));
		}
	}
	entries.clear();
	terminated = true;
	terminatedCondition.notify_all();
}

} // namespace aion::loginserver::utils
