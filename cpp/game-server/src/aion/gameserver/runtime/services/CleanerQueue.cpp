// CleanerQueue and CleanerDrain (design §6, RR-2). See CleanerQueue.h.

#include "aion/gameserver/runtime/services/CleanerQueue.h"

#include <atomic>
#include <mutex>
#include <new>
#include <string>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::runtime {

namespace {

const commons::logging::Logger& log() {
	static const auto* instance = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.gameobjects.AionObject"));
	return *instance;
}

struct Node {
	Node* next;
	int32_t id;
	const char* className;
};

struct CleanerState {
	/** Treiber stack, newest first; only drains take nodes (all at once), so there is no ABA */
	std::atomic<Node*> head{nullptr};
	/** incremented before a node is published, so a concurrent drain never takes it below zero */
	std::atomic<int64_t> size{0};
	std::atomic<CleanerQueue::CleanerAction> action{nullptr};
	std::atomic<uint64_t> dropped{0};
	std::atomic<uint64_t> postedDrains{0};
	std::atomic<bool> installed{false};
	/** guards hookId and pendingDrain; never held while ThreadPoolManager or the action runs */
	std::mutex hookMutex;
	uint64_t hookId = 0;
	/** the last posted drain (at most one pending or running) */
	FutureRef pendingDrain;
};

CleanerState& cleaner() {
	static auto* instance = new CleanerState();
	return *instance;
}

void defaultAction(int32_t objectId, const char* className) {
	utils::idfactory::IDFactory::getInstance().releaseId(objectId, className);
}

/**
 * Posts a CleanerDrain if ids are queued and no drain is pending; runs on the scanning thread (post-scan hook). Re-checks the installed flag (a
 * hook invocation may still be in flight after uninstall() returned) and never creates the default pools: without an installed backend
 * (startup before the pools, test teardown) nothing is posted and the ids wait for the next scan or the final drainNow().
 */
void postDrainIfNeeded() {
	CleanerState& s = cleaner();
	if (!s.installed.load(std::memory_order_acquire) || s.head.load(std::memory_order_acquire) == nullptr)
		return;
	{
		std::scoped_lock lock(s.hookMutex);
		if (s.pendingDrain && !s.pendingDrain->isDone())
			return;
	}
	runtime::ExecutorBackend* backend = utils::ThreadPoolManager::installedBackend();
	if (backend == nullptr || backend->isShutdown())
		return; // the final drain after shutdown is drainNow()
	FutureRef drain = utils::ThreadPoolManager::submitIfInstalled([] { (void)CleanerQueue::drainNow(); });
	if (!drain)
		return;
	s.postedDrains.fetch_add(1, std::memory_order_relaxed);
	FutureRef previous;
	{
		std::scoped_lock lock(s.hookMutex);
		previous = std::exchange(s.pendingDrain, std::move(drain));
	}
}

} // namespace

void CleanerQueue::push(int32_t objectId, const char* className) noexcept {
	CleanerState& s = cleaner();
	Node* node = new (std::nothrow) Node{nullptr, objectId, className};
	if (node == nullptr) {
		s.dropped.fetch_add(1, std::memory_order_relaxed);
		return;
	}
	s.size.fetch_add(1, std::memory_order_relaxed);
	node->next = s.head.load(std::memory_order_acquire);
	AION_YIELD_POINT("CleanerQueue::push:cas");
	while (!s.head.compare_exchange_weak(node->next, node, std::memory_order_acq_rel, std::memory_order_acquire))
		AION_YIELD_POINT("CleanerQueue::push:retry");
}

void CleanerQueue::install() {
	CleanerState& s = cleaner();
	std::scoped_lock lock(s.hookMutex);
	if (s.installed.load(std::memory_order_acquire))
		return;
	s.hookId = Reclaimer::getInstance().addPostScanHook("CleanerDrain", [] { postDrainIfNeeded(); });
	s.installed.store(true, std::memory_order_release);
}

void CleanerQueue::uninstall() {
	CleanerState& s = cleaner();
	FutureRef previous;
	uint64_t hookId = 0;
	{
		std::scoped_lock lock(s.hookMutex);
		if (!s.installed.load(std::memory_order_acquire))
			return;
		s.installed.store(false, std::memory_order_release);
		hookId = std::exchange(s.hookId, 0);
		previous = std::move(s.pendingDrain);
	}
	Reclaimer::getInstance().removePostScanHook(hookId);
}

bool CleanerQueue::isInstalled() noexcept {
	return cleaner().installed.load(std::memory_order_acquire);
}

void CleanerQueue::setCleanerAction(CleanerAction action) noexcept {
	cleaner().action.store(action, std::memory_order_release);
}

size_t CleanerQueue::drainNow() {
	CleanerState& s = cleaner();
	TaskScope scope(AION_TASK_INFO(TaskKind::CLEANER));
	AION_YIELD_POINT("CleanerQueue::drain:take");
	Node* newestFirst = s.head.exchange(nullptr, std::memory_order_acq_rel);
	Node* list = nullptr;
	while (newestFirst != nullptr) { // reverse into push order
		Node* next = newestFirst->next;
		newestFirst->next = list;
		list = newestFirst;
		newestFirst = next;
	}
	CleanerAction action = s.action.load(std::memory_order_acquire);
	if (action == nullptr)
		action = &defaultAction;
	size_t processed = 0;
	while (list != nullptr) {
		Node* node = list;
		list = node->next;
		s.size.fetch_sub(1, std::memory_order_relaxed);
		try {
			action(node->id, node->className);
		} catch (...) {
			log().errorCurrentException("Cleaner action failed for object ID " + std::to_string(node->id));
		}
		delete node;
		++processed;
	}
	return processed;
}

size_t CleanerQueue::size() noexcept {
	int64_t size = cleaner().size.load(std::memory_order_acquire);
	return size > 0 ? static_cast<size_t>(size) : 0;
}

uint64_t CleanerQueue::getDroppedCount() noexcept {
	return cleaner().dropped.load(std::memory_order_acquire);
}

uint64_t CleanerQueue::getPostedDrainCount() noexcept {
	return cleaner().postedDrains.load(std::memory_order_acquire);
}

} // namespace aion::gameserver::runtime
