// LeakCensus, zombie breaker and stale-pin warnings (design §5.3, §5.4, D7). See LeakCensus.h.

#include "aion/gameserver/runtime/services/LeakCensus.h"

#include <algorithm>
#include <atomic>
#include <format>
#include <mutex>
#include <new>
#include <unordered_map>
#include <unordered_set>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/sync/RankedMutex.h"
#include "aion/gameserver/runtime/services/detail/ServicesSupport.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::runtime {

namespace {

using TimePoint = std::chrono::steady_clock::time_point;

const commons::logging::Logger& log() {
	static const auto* instance = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.runtime.LeakCensus"));
	return *instance;
}

struct Event {
	Event* next;
	const RefCounted* object;
	const char* className;
	int32_t objectId;
	TimePoint time;
	bool added;
};

struct Entry {
	const char* className;
	int32_t objectId;
	TimePoint removedAt;
	uint32_t refCount = 0;
	bool reported = false;
	bool breakPosted = false;
	std::vector<TaskInfo> pinningTasks;
	std::vector<const char*> cutEdges;
};

struct CensusState {
	std::atomic<Event*> incoming{nullptr};
	std::atomic<bool> installed{false};
	std::atomic<size_t> tracked{0};
	std::atomic<uint64_t> zombieCuts{0};
	/** guards `table` for readers and every write to it, and `config`. Only the scanning thread inserts or erases entries. */
	mutable RankedMutex<LockRank::STATS> tableMutex;
	std::unordered_map<const RefCounted*, Entry> table;
	LeakCensus::Config config;
	// scanning thread only (serialized by the Reclaimer's scan mutex)
	TimePoint nextCheck{};
	TimePoint nextStalePinCheck{};
	std::unordered_set<const Future*> warnedStalePins;
	// install/uninstall
	std::mutex installMutex;
	uint64_t hookId = 0;
};

CensusState& state() {
	static auto* instance = new CensusState();
	return *instance;
}

int64_t minutesBetween(TimePoint from, TimePoint to) noexcept {
	return std::chrono::duration_cast<std::chrono::minutes>(to - from).count();
}

void deleteEvents(Event* list) noexcept {
	while (list != nullptr) {
		Event* next = list->next;
		delete list;
		list = next;
	}
}

/** scanning thread: moves queued events into the table */
void moveIncoming(CensusState& s) noexcept {
	AION_YIELD_POINT("LeakCensus::moveIncoming:take");
	Event* newestFirst = s.incoming.exchange(nullptr, std::memory_order_acq_rel);
	if (newestFirst == nullptr)
		return;
	Event* list = nullptr;
	while (newestFirst != nullptr) {
		Event* next = newestFirst->next;
		newestFirst->next = list;
		list = newestFirst;
		newestFirst = next;
	}
	try {
		std::scoped_lock lock(s.tableMutex);
		for (Event* event = list; event != nullptr; event = event->next) {
			if (event->added)
				s.table.erase(event->object);
			else
				s.table.try_emplace(event->object, Entry{event->className, event->objectId, event->time});
		}
		s.tracked.store(s.table.size(), std::memory_order_release);
	} catch (...) {
		// bad_alloc: the remaining events are dropped (an untracked object is only a missed report)
	}
	deleteEvents(list);
}

void push(CensusState& s, Event* event) noexcept {
	event->next = s.incoming.load(std::memory_order_acquire);
	AION_YIELD_POINT("LeakCensus::push:cas");
	while (!s.incoming.compare_exchange_weak(event->next, event, std::memory_order_acq_rel, std::memory_order_acquire))
		AION_YIELD_POINT("LeakCensus::push:retry");
}

/** Reclaimer destroy observer: runs before `object` is freed, inside the destructor context */
void onDestroy(const RefCounted& object) noexcept {
	CensusState& s = state();
	if (s.incoming.load(std::memory_order_acquire) == nullptr && s.tracked.load(std::memory_order_acquire) == 0)
		return;
	moveIncoming(s);
	// only the scanning thread (this one) inserts or erases, so the lookup needs no lock
	if (s.table.find(&object) == s.table.end())
		return;
	std::scoped_lock lock(s.tableMutex);
	s.table.erase(&object);
	s.tracked.store(s.table.size(), std::memory_order_release);
}

/** ThreadPoolManager::tasksPinning without creating the default pools */
std::vector<TaskInfo> tasksPinning(const RefCounted& owner) {
	std::vector<TaskInfo> infos;
	if (runtime::ExecutorBackend* backend = utils::ThreadPoolManager::installedBackend()) {
		for (const FutureRef& task : backend->pendingTasks())
			if (task->pins(owner))
				infos.push_back(task->getTaskInfo());
	}
	return infos;
}

struct Candidate {
	const RefCounted* object;
	const char* className;
	int32_t objectId;
	TimePoint removedAt;
	uint32_t refCount;
};

void runZombieBreaker(const RefCounted* object, ZombieBreakable* breakable, const char* className, int32_t objectId, TimePoint removedAt) {
	CensusState& s = state();
	std::vector<const char*> edges = breakable->breakKnownEdges();
	int64_t minutes = minutesBetween(removedAt, services_detail::clockNow());
	if (edges.empty())
		log().warn("Zombie breaker: {} (object id {}) is still alive {} minutes after its removal from the world and no known edge was cut", className,
			objectId, minutes);
	for (const char* edge : edges)
		log().warn("Zombie breaker: cut {} of {} (object id {}), removed from the world {} minutes ago: a cycle breaker for this edge is missing", edge,
			className, objectId, minutes);
	s.zombieCuts.fetch_add(edges.size(), std::memory_order_relaxed);
	std::scoped_lock lock(s.tableMutex);
	if (auto it = s.table.find(object); it != s.table.end())
		it->second.cutEdges.insert(it->second.cutEdges.end(), edges.begin(), edges.end());
}

void postZombieBreaker(const Candidate& candidate) {
	auto* breakable = dynamic_cast<ZombieBreakable*>(const_cast<RefCounted*>(candidate.object));
	int64_t minutes = minutesBetween(candidate.removedAt, services_detail::clockNow());
	if (breakable == nullptr) {
		log().warn("Zombie breaker: {} (object id {}) is still alive {} minutes after its removal from the world (refcount {}) and has no zombie breaker",
			candidate.className, candidate.objectId, minutes, candidate.refCount);
		return;
	}
	// runtime code: the pin retains the object while the task is pending; the raw pointers are only used by the pinned body. Never creates the
	// default pools (no backend installed: the breaker is not posted and is retried by nobody; the census entry stays reported)
	(void)utils::ThreadPoolManager::executeIfInstalled(Pin(candidate.object),
		[object = candidate.object, breakable, className = candidate.className, objectId = candidate.objectId, removedAt = candidate.removedAt] {
			runZombieBreaker(object, breakable, className, objectId, removedAt);
		});
}

void checkStalePins(CensusState& s, const std::vector<Candidate>& old, TimePoint now) {
	runtime::ExecutorBackend* backend = utils::ThreadPoolManager::installedBackend(); // never creates the default pools
	if (backend == nullptr)
		return;
	std::vector<FutureRef> pending = backend->pendingTasks();
	std::unordered_map<const RefCounted*, const Candidate*> oldByObject;
	for (const Candidate& candidate : old)
		oldByObject.emplace(candidate.object, &candidate);
	std::unordered_set<const Future*> periodic;
	for (const FutureRef& task : pending) {
		if (!task->isPeriodic())
			continue;
		periodic.insert(task.get());
		if (s.warnedStalePins.contains(task.get()))
			continue;
		// design §5.4: logged when ALL pinned owners have been removed from the world for stalePinAfter (a task pinning only immortals, or also
		// an object still in the world, is not stale)
		std::string pinned;
		bool allOld = false;
		for (const RefCounted* owner : task->pinnedOwners()) {
			if (owner == nullptr)
				continue;
			auto it = oldByObject.find(owner);
			if (it == oldByObject.end()) {
				allOld = false;
				break;
			}
			const Candidate& candidate = *it->second;
			pinned += std::format("{}{} (object id {}), removed from the world {} minutes ago", pinned.empty() ? "" : "; ", candidate.className,
				candidate.objectId, minutesBetween(candidate.removedAt, now));
			allOld = true;
		}
		if (!allOld)
			continue;
		log().warn("Periodic task {} still pins {} (the task is not cancelled)", services_detail::describeTask(task->getTaskInfo()), pinned);
		s.warnedStalePins.insert(task.get());
	}
	std::erase_if(s.warnedStalePins, [&](const Future* task) { return !periodic.contains(task); });
}

/** post-scan hook (scanning thread, TaskScope of kind RECLAIMER, no object is destroyed while it runs) */
void censusHook() {
	CensusState& s = state();
	if (!s.installed.load(std::memory_order_acquire))
		return; // an invocation still in flight after uninstall() returned
	moveIncoming(s);
	if (s.tracked.load(std::memory_order_acquire) == 0) {
		s.warnedStalePins.clear();
		return;
	}
	TimePoint now = services_detail::clockNow();
	LeakCensus::Config config;
	std::vector<Candidate> newLeaks;
	std::vector<Candidate> zombies;
	std::vector<Candidate> old;
	{
		std::scoped_lock lock(s.tableMutex);
		config = s.config;
		if (now < s.nextCheck)
			return;
		s.nextCheck = now + config.checkInterval;
		for (auto& [object, entry] : s.table) {
			uint32_t count = object->refCount();
			entry.refCount = count;
			if (count == 0)
				continue; // waiting for reclamation
			Candidate candidate{object, entry.className, entry.objectId, entry.removedAt, count};
			auto age = now - entry.removedAt;
			if (age >= config.censusAfter && !entry.reported) {
				entry.reported = true;
				newLeaks.push_back(candidate);
			}
			if (config.zombieBreakerEnabled && age >= config.zombieBreakAfter && !entry.breakPosted) {
				entry.breakPosted = true;
				zombies.push_back(candidate);
			}
			if (age >= config.stalePinAfter)
				old.push_back(candidate);
		}
	}
	// ThreadPoolManager takes its SCHEDULER leaf mutex, which ranks below STATS: never under tableMutex
	for (const Candidate& leak : newLeaks) {
		std::vector<TaskInfo> pinning = tasksPinning(*leak.object);
		std::string sites;
		for (const TaskInfo& info : pinning)
			sites += (sites.empty() ? "" : ", ") + services_detail::describeTask(info);
		log().warn("Leak census: {} (object id {}) is still alive {} minutes after its removal from the world, refcount {}, pinned by {} pending task(s){}{}",
			leak.className, leak.objectId, minutesBetween(leak.removedAt, now), leak.refCount, pinning.size(), pinning.empty() ? "" : ": ", sites);
		std::scoped_lock lock(s.tableMutex);
		if (auto it = s.table.find(leak.object); it != s.table.end())
			it->second.pinningTasks = std::move(pinning);
	}
	for (const Candidate& zombie : zombies) {
		try {
			postZombieBreaker(zombie);
		} catch (...) {
			log().errorCurrentException("Zombie breaker: could not post the breaker");
		}
	}
	if (!old.empty() && now >= s.nextStalePinCheck) {
		s.nextStalePinCheck = now + config.stalePinCheckInterval;
		checkStalePins(s, old, now);
	}
}

} // namespace

LeakCensus& LeakCensus::getInstance() {
	static auto* instance = new LeakCensus();
	return *instance;
}

void LeakCensus::configure(const Config& config) {
	CensusState& s = state();
	std::scoped_lock lock(s.tableMutex);
	s.config = config;
	s.nextCheck = {};
	s.nextStalePinCheck = {};
}

LeakCensus::Config LeakCensus::getConfig() const {
	CensusState& s = state();
	std::scoped_lock lock(s.tableMutex);
	return s.config;
}

void LeakCensus::install() {
	CensusState& s = state();
	std::scoped_lock installLock(s.installMutex);
	if (s.installed.load(std::memory_order_acquire))
		return;
	deleteEvents(s.incoming.exchange(nullptr, std::memory_order_acq_rel));
	{
		std::scoped_lock lock(s.tableMutex);
		s.table.clear();
		s.tracked.store(0, std::memory_order_release);
		s.nextCheck = {};
		s.nextStalePinCheck = {};
	}
	s.warnedStalePins.clear();
	Reclaimer::getInstance().setDestroyObserver(&onDestroy);
	s.hookId = Reclaimer::getInstance().addPostScanHook("LeakCensus", [] { censusHook(); });
	s.installed.store(true, std::memory_order_release);
}

void LeakCensus::uninstall() {
	CensusState& s = state();
	std::scoped_lock installLock(s.installMutex);
	if (!s.installed.load(std::memory_order_acquire))
		return;
	s.installed.store(false, std::memory_order_release);
	Reclaimer::getInstance().removePostScanHook(std::exchange(s.hookId, 0));
	Reclaimer::getInstance().setDestroyObserver(nullptr);
	deleteEvents(s.incoming.exchange(nullptr, std::memory_order_acq_rel));
	std::scoped_lock lock(s.tableMutex);
	s.table.clear();
	s.tracked.store(0, std::memory_order_release);
}

bool LeakCensus::isInstalled() const noexcept {
	return state().installed.load(std::memory_order_acquire);
}

void LeakCensus::onRemovedFromWorld(const RefCounted& object, const char* className, int32_t objectId) noexcept {
	CensusState& s = state();
	if (!s.installed.load(std::memory_order_acquire))
		return;
	auto* event = new (std::nothrow) Event{nullptr, &object, className, objectId, services_detail::clockNowNoexcept(), false};
	if (event != nullptr)
		push(s, event);
}

void LeakCensus::onAddedToWorld(const RefCounted& object) noexcept {
	CensusState& s = state();
	if (!s.installed.load(std::memory_order_acquire))
		return;
	auto* event = new (std::nothrow) Event{nullptr, &object, nullptr, 0, TimePoint{}, true};
	if (event != nullptr)
		push(s, event);
}

std::vector<LeakCensus::LeakReport> LeakCensus::getLeaks() const {
	CensusState& s = state();
	TimePoint now = services_detail::clockNow();
	std::vector<std::pair<TimePoint, LeakReport>> reports;
	{
		std::scoped_lock lock(s.tableMutex);
		for (const auto& [object, entry] : s.table) {
			if (!entry.reported)
				continue;
			LeakReport report;
			report.className = entry.className != nullptr ? entry.className : "";
			report.objectId = entry.objectId;
			report.refCount = entry.refCount;
			report.removedFor = std::chrono::duration_cast<std::chrono::seconds>(now - entry.removedAt);
			report.pinningTasks = entry.pinningTasks;
			report.cutEdges = entry.cutEdges;
			reports.emplace_back(entry.removedAt, std::move(report));
		}
	}
	std::stable_sort(reports.begin(), reports.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
	std::vector<LeakReport> result;
	result.reserve(reports.size());
	for (auto& [removedAt, report] : reports)
		result.push_back(std::move(report));
	return result;
}

size_t LeakCensus::trackedCount() const {
	return state().tracked.load(std::memory_order_acquire);
}

uint64_t LeakCensus::zombieCutCount() const noexcept {
	return state().zombieCuts.load(std::memory_order_acquire);
}

} // namespace aion::gameserver::runtime
