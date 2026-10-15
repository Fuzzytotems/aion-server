#include "aion/gameserver/runtime/sync/Watchdog.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "aion/commons/utils/WindowsMacroGuard.h"
#endif

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/ExitCode.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/commons/utils/concurrent/ThreadName.h"
#include "aion/gameserver/runtime/sync/MinidumpWriter.h"
#include "aion/gameserver/runtime/sync/RankedMutex.h"

// Implementation notes
// - Two internal locks: a leaf mutex (LockRank::STATS) guards config, probe and listener lists and is never held while user code runs; a plain
//   std::mutex serializes whole checks (detection memory, probe invocation). The latter is not a leaf mutex because probes and listeners (user
//   code, possibly taking leaf mutexes of lower ranks) run while it is held.
// - Deadlock confirmation: a cycle key is the sorted list of (thread id, waited lock id) of its members; it is reported when the key was also
//   present in the previous check, once until it disappears.
// - Task keys: (thread id, start nanos) identify one task run; they are forgotten when the task is no longer observed. quiescentPoint() gives
//   the running task a new scope id but keeps its start time, so a changed scope id of the same key is observed progress: it restarts the
//   stall clock (and re-arms the STALL report) but not the slow-task clock, which measures the whole run like Java's post-hoc warning.
// - Dump rate limit: every check has a generation number, recorded thread-locally on the checking thread while the check runs. dump() writes a
//   minidump only if no minidump was attempted in the same check, the previous attempt is older than minidumpMinInterval and maxMinidumps is
//   not reached; the minidump state has its own std::mutex (never held while other runtime locks are taken), which also serializes the
//   minidump writes of concurrent manual dumps. Stalled tasks of one check are reported in one STALL dump.
//   DEADLOCK is exempt from minidumpMinInterval and from the STALL budget: each cycle is dumped only once and may be followed by quick_exit, so
//   its minidump is the only record of the other threads' stacks. It still obeys one per check and has its own budget of maxMinidumps.
// - Dump order (DEADLOCK/STALL with minidumps): the text is logged before the minidump is written (a helper may take minidumpTimeout + 5 s, and
//   an operator may kill a hung server meanwhile), then one line with the minidump result; listeners see the report with both afterwards.
//   Paths in messages are UTF-8 (path::string() throws for characters outside the ANSI code page), the configured directory is read as UTF-8,
//   and a throwing minidump write becomes the minidump line.

namespace aion::gameserver::runtime {

namespace {

using CycleKey = std::vector<std::pair<uint64_t, uintptr_t>>;
using TaskKey = std::pair<uint64_t, int64_t>;

/** Watchdog-side record of one observed task run. */
struct TaskProgress {
	uint64_t scopeId = 0;
	/** start of the run, or the check that first observed its latest scope id (quiescentPoint) */
	int64_t progressNanos = 0;
	bool stallReported = false;
	bool slowReported = false;
};

bool kindIn(const char* kind, const std::vector<std::string>& kinds) noexcept {
	if (kind == nullptr)
		return false;
	return std::any_of(kinds.begin(), kinds.end(), [kind](const std::string& exempt) { return exempt == kind; });
}

struct WatchdogState {
	RankedMutex<LockRank::STATS> listMutex;
	Watchdog::Config config;
	uint64_t nextId = 1;
	std::vector<std::pair<uint64_t, std::pair<std::string, Watchdog::Probe>>> probes;
	std::vector<std::pair<uint64_t, Watchdog::DumpListener>> listeners;

	std::mutex checkMutex;
	std::set<CycleKey> previousCycles;
	std::set<CycleKey> reportedCycles;
	std::map<TaskKey, TaskProgress> tasks;

	std::mutex threadMutex;
	std::condition_variable wake;
	bool stopRequested = false;
	std::thread thread;
	std::atomic<bool> running{false};
	std::atomic<uint64_t> minidumpCounter{0};

	std::atomic<uint64_t> checkGeneration{0};
	std::mutex minidumpMutex;
	uint64_t lastMinidumpCheck = 0;
	int64_t lastMinidumpNanos = 0;
	bool minidumpAttempted = false;
	int32_t minidumpCount = 0;
	/** DEADLOCK minidumps attempted (their own budget of maxMinidumps) */
	int32_t deadlockMinidumpCount = 0;
	std::string lastMinidumpPath;
};

/** the generation of the check running on this thread (0 outside checkNow) */
thread_local uint64_t currentCheckGeneration = 0;

WatchdogState& state() {
	static auto* instance = new WatchdogState(); // leaked: usable during static destruction
	return *instance;
}

const commons::logging::Logger& logger() {
	static const commons::logging::Logger log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.runtime.Watchdog");
	return log;
}

bool debuggerPresent() noexcept {
#if defined(_WIN32)
	return IsDebuggerPresent() != 0;
#else
	return false;
#endif
}

int64_t millisSince(int64_t startNanos, int64_t nowNanos) noexcept {
	return (nowNanos - startNanos) / 1'000'000;
}

std::string describeThread(const Watchdog::ThreadSnapshot& thread, int64_t now, bool involved) {
	std::string line = std::format("  {}thread '{}' (id {})", involved ? "* " : "", thread.threadName != nullptr ? thread.threadName : "?", thread.threadId);
	if (thread.task.active) {
		const std::source_location& where = thread.task.info.where;
		line += std::format(": task '{}' from {}:{} ({}), scope {}, running {} ms", thread.task.info.kind != nullptr ? thread.task.info.kind : "?",
			where.file_name(), where.line(), where.function_name(), thread.task.scopeId, millisSince(thread.task.startNanos, now));
	} else {
		line += ": no task";
	}
	if (thread.blocking.active)
		line += std::format("; blocking in '{}' at {}:{} for {} ms", thread.blocking.what != nullptr ? thread.blocking.what : "?",
			thread.blocking.where.file_name(), thread.blocking.where.line(), millisSince(thread.blocking.sinceNanos, now));
	if (thread.wait.waiting)
		line += std::format("; waiting for lock '{}' ({:#x}), owner at wait start: thread {}, for {} ms",
			thread.wait.lockClassName != nullptr ? thread.wait.lockClassName : "?", thread.wait.lockId, thread.wait.ownerThreadId,
			millisSince(thread.wait.waitStartNanos, now));
	if (thread.heldLockCount != 0) {
		line += "; holds [";
		for (size_t i = 0; i < thread.heldLockClasses.size(); ++i) {
			if (i != 0)
				line += ", ";
			line += thread.heldLockClasses[i] != nullptr ? thread.heldLockClasses[i] : "?";
		}
		if (thread.heldLockCount > thread.heldLockClasses.size())
			line += std::format(", ... {} more", thread.heldLockCount - thread.heldLockClasses.size());
		line += "]";
	}
	if (thread.publishedEpoch != EPOCH_IDLE)
		line += std::format("; published epoch {}", thread.publishedEpoch);
	return line;
}

#if defined(_WIN32)
/** a configured path (UTF-8, like every configuration string) */
std::filesystem::path utf8Path(std::string_view text) {
	return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}

/** a path as UTF-8 text (path::string() throws for characters outside the ANSI code page) */
std::string utf8Text(const std::filesystem::path& path) {
	std::u8string text = path.u8string();
	return std::string(text.begin(), text.end());
}

/** Writes a minidump subject to the rate limit (implementation notes); returns the path written (empty if none) and the line for the text. */
std::pair<std::string, std::string> writeRateLimitedMinidump(const Watchdog::Config& config, Watchdog::Reason reason) {
	WatchdogState& s = state();
	std::scoped_lock lock(s.minidumpMutex);
	int64_t now = commons::utils::nanoTime();
	bool deadlock = reason == Watchdog::Reason::DEADLOCK;
	if (currentCheckGeneration != 0 && s.lastMinidumpCheck == currentCheckGeneration)
		return {{}, "Minidump: one per watchdog check, see " + (s.lastMinidumpPath.empty() ? std::string("the first dump of this check") : s.lastMinidumpPath)};
	if (!deadlock && s.minidumpAttempted && config.minidumpMinInterval.count() > 0 &&
		millisSince(s.lastMinidumpNanos, now) < config.minidumpMinInterval.count())
		return {{}, std::format("Minidump: skipped (rate limit: one per {} ms; the previous one {} ms ago: {})", config.minidumpMinInterval.count(),
						millisSince(s.lastMinidumpNanos, now), s.lastMinidumpPath.empty() ? std::string("not written") : s.lastMinidumpPath)};
	int32_t& count = deadlock ? s.deadlockMinidumpCount : s.minidumpCount;
	if (config.maxMinidumps > 0 && count >= config.maxMinidumps)
		return {{}, deadlock ? std::format("Minidump: skipped (limit of {} DEADLOCK minidumps per run reached)", config.maxMinidumps)
							 : std::format("Minidump: skipped (limit of {} minidumps per run reached)", config.maxMinidumps)};

	// counted before writing: a failing or hanging writer is not retried on every check
	s.lastMinidumpCheck = currentCheckGeneration;
	s.lastMinidumpNanos = now;
	s.minidumpAttempted = true;
	count++;
	s.lastMinidumpPath.clear();

	auto wallClock = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
	std::filesystem::path path = utf8Path(config.minidumpDirectory) /
		std::format("watchdog-{}-{:%Y%m%d-%H%M%S}-{}-{}.dmp", Watchdog::reasonName(reason), wallClock, GetCurrentProcessId(),
			s.minidumpCounter.fetch_add(1, std::memory_order_relaxed));
	std::filesystem::path helper = utf8Path(config.minidumpHelperExecutable);
	if (helper.empty() && MinidumpWriter::isSelfHelperRegistered())
		helper = MinidumpWriter::currentExecutable();
	MinidumpWriter::Result result = helper.empty() ? MinidumpWriter::writeInProcess(path, config.minidumpTimeout)
												   : MinidumpWriter::writeWithHelper(helper, GetCurrentProcessId(), path, config.minidumpTimeout);
	if (!result.written())
		return {{}, "Minidump: could not be written (" + result.message + ")"};
	std::string written = utf8Text(path);
	s.lastMinidumpPath = written;
	return {written, "Minidump (all thread stacks, " + result.message + "): " + written};
}

/** the message of the exception being handled */
std::string currentExceptionMessage() {
	try {
		throw;
	} catch (const std::exception& e) {
		return e.what();
	} catch (...) {
		return "unknown exception";
	}
}
#endif

/** Tarjan's strongly connected components; returns components that contain a cycle (size > 1 or a self-loop). */
std::vector<std::vector<size_t>> findCycles(const std::vector<std::vector<size_t>>& adjacency) {
	size_t n = adjacency.size();
	std::vector<int64_t> index(n, -1);
	std::vector<int64_t> lowLink(n, 0);
	std::vector<bool> onStack(n, false);
	std::vector<size_t> stack;
	std::vector<std::vector<size_t>> cycles;
	int64_t nextIndex = 0;

	struct Frame {
		size_t node;
		size_t edge;
	};
	for (size_t root = 0; root < n; ++root) {
		if (index[root] >= 0)
			continue;
		std::vector<Frame> frames{{root, 0}};
		index[root] = lowLink[root] = nextIndex++;
		stack.push_back(root);
		onStack[root] = true;
		while (!frames.empty()) {
			Frame& frame = frames.back();
			size_t node = frame.node;
			if (frame.edge < adjacency[node].size()) {
				size_t next = adjacency[node][frame.edge++];
				if (index[next] < 0) {
					index[next] = lowLink[next] = nextIndex++;
					stack.push_back(next);
					onStack[next] = true;
					frames.push_back({next, 0});
				} else if (onStack[next]) {
					lowLink[node] = std::min(lowLink[node], index[next]);
				}
				continue;
			}
			if (lowLink[node] == index[node]) {
				std::vector<size_t> component;
				size_t member;
				do {
					member = stack.back();
					stack.pop_back();
					onStack[member] = false;
					component.push_back(member);
				} while (member != node);
				bool selfLoop = std::find(adjacency[node].begin(), adjacency[node].end(), node) != adjacency[node].end();
				if (component.size() > 1 || selfLoop)
					cycles.push_back(std::move(component));
			}
			frames.pop_back();
			if (!frames.empty())
				lowLink[frames.back().node] = std::min(lowLink[frames.back().node], lowLink[node]);
		}
	}
	return cycles;
}

std::vector<Watchdog::DumpListener> copyListeners() {
	WatchdogState& s = state();
	std::vector<Watchdog::DumpListener> listeners;
	std::scoped_lock lock(s.listMutex);
	for (auto& [id, listener] : s.listeners)
		listeners.push_back(listener);
	return listeners;
}

void logText(Watchdog::Reason reason, const std::string& text) noexcept {
	try {
		if (reason == Watchdog::Reason::DEADLOCK || reason == Watchdog::Reason::STALL)
			logger().error(text);
		else
			logger().warn(text);
	} catch (...) {
		// logging is best effort
	}
}

void notifyListeners(const Watchdog::DumpReport& report) {
	for (Watchdog::DumpListener& listener : copyListeners()) {
		try {
			listener(report);
		} catch (...) {
			logger().warnCurrentException("Watchdog dump listener threw");
		}
	}
}

void publish(const Watchdog::DumpReport& report) {
	logText(report.reason, report.text);
	notifyListeners(report);
}

} // namespace

const char* Watchdog::reasonName(Reason reason) noexcept {
	switch (reason) {
		case Reason::DEADLOCK:
			return "DEADLOCK";
		case Reason::STALL:
			return "STALL";
		case Reason::SLOW_TASK:
			return "SLOW_TASK";
		case Reason::RECLAIM_LAG:
			return "RECLAIM_LAG";
		case Reason::BACKLOG:
			return "BACKLOG";
		case Reason::MANUAL:
			return "MANUAL";
	}
	return "?";
}

Watchdog& Watchdog::getInstance() {
	static auto* instance = new Watchdog();
	return *instance;
}

void Watchdog::configure(const Config& config) {
	WatchdogState& s = state();
	std::scoped_lock lock(s.listMutex);
	s.config = config;
}

void Watchdog::start(const Config& config) {
	configure(config);
	WatchdogState& s = state();
	std::scoped_lock lock(s.threadMutex);
	if (s.running.load(std::memory_order_acquire))
		return;
	s.stopRequested = false;
	s.running.store(true, std::memory_order_release);
	s.thread = std::thread([this] {
		commons::utils::concurrent::setCurrentThreadName("Watchdog");
		ThreadContext::current().refreshName();
		WatchdogState& ws = state();
		std::unique_lock threadLock(ws.threadMutex);
		while (!ws.stopRequested) {
			std::chrono::milliseconds period = getConfig().period;
			ws.wake.wait_for(threadLock, period, [&ws] { return ws.stopRequested; });
			if (ws.stopRequested)
				break;
			threadLock.unlock();
			Config current = getConfig();
			if (current.evenUnderDebugger || !debuggerPresent()) {
				try {
					checkNow();
				} catch (...) {
					logger().errorCurrentException("Watchdog check failed");
				}
			}
			threadLock.lock();
		}
	});
}

void Watchdog::stop() {
	WatchdogState& s = state();
	std::thread thread;
	{
		std::scoped_lock lock(s.threadMutex);
		if (!s.running.load(std::memory_order_acquire))
			return;
		s.stopRequested = true;
		thread = std::move(s.thread);
	}
	s.wake.notify_all();
	if (thread.joinable())
		thread.join();
	s.running.store(false, std::memory_order_release);
}

bool Watchdog::isRunning() const noexcept {
	return state().running.load(std::memory_order_acquire);
}

Watchdog::Config Watchdog::getConfig() const {
	WatchdogState& s = state();
	std::scoped_lock lock(s.listMutex);
	return s.config;
}

void Watchdog::checkNow() {
	WatchdogState& s = state();
	std::scoped_lock checkLock(s.checkMutex);
	struct CheckGeneration {
		explicit CheckGeneration(uint64_t generation) noexcept { currentCheckGeneration = generation; }
		~CheckGeneration() { currentCheckGeneration = 0; }
		CheckGeneration(const CheckGeneration&) = delete;
		CheckGeneration& operator=(const CheckGeneration&) = delete;
	} generation(s.checkGeneration.fetch_add(1, std::memory_order_relaxed) + 1);
	Config config = getConfig();
	std::vector<ThreadSnapshot> threads = snapshotThreads();
	int64_t now = commons::utils::nanoTime();

	// ------------------------------------------------------------------------------------------------------------------- wait-graph cycles
	std::unordered_map<uint64_t, size_t> byThreadId;
	std::unordered_map<uintptr_t, std::vector<size_t>> holders;
	for (size_t i = 0; i < threads.size(); ++i) {
		byThreadId.emplace(threads[i].threadId, i);
		for (uintptr_t lockId : threads[i].heldLockIds)
			holders[lockId].push_back(i);
	}
	std::vector<std::vector<size_t>> adjacency(threads.size());
	for (size_t i = 0; i < threads.size(); ++i) {
		const ThreadContext::WaitSnapshot& wait = threads[i].wait;
		if (!wait.waiting)
			continue;
		if (auto found = holders.find(wait.lockId); found != holders.end()) {
			for (size_t holder : found->second)
				if (std::find(adjacency[i].begin(), adjacency[i].end(), holder) == adjacency[i].end())
					adjacency[i].push_back(holder);
		} else if (wait.ownerThreadId != 0) {
			if (auto owner = byThreadId.find(wait.ownerThreadId);
				owner != byThreadId.end() && threads[owner->second].heldLockCount > threads[owner->second].heldLockIds.size())
				adjacency[i].push_back(owner->second);
		}
	}
	std::set<CycleKey> currentCycles;
	std::vector<std::pair<CycleKey, std::vector<size_t>>> confirmed;
	for (std::vector<size_t>& component : findCycles(adjacency)) {
		CycleKey key;
		for (size_t member : component)
			key.emplace_back(threads[member].threadId, threads[member].wait.lockId);
		std::sort(key.begin(), key.end());
		if (s.previousCycles.contains(key) && !s.reportedCycles.contains(key))
			confirmed.emplace_back(key, component);
		currentCycles.insert(std::move(key));
	}
	s.previousCycles = currentCycles;
	std::erase_if(s.reportedCycles, [&](const CycleKey& key) { return !currentCycles.contains(key); });
	for (auto& [key, component] : confirmed) {
		s.reportedCycles.insert(key);
		std::sort(component.begin(), component.end());
		std::string summary = "deadlock between " + std::to_string(component.size()) + " thread(s):";
		std::vector<uint64_t> ids;
		for (size_t member : component) {
			const ThreadSnapshot& thread = threads[member];
			ids.push_back(thread.threadId);
			std::string holderNames;
			for (size_t holder : adjacency[member]) {
				if (std::find(component.begin(), component.end(), holder) == component.end())
					continue;
				if (!holderNames.empty())
					holderNames += ", ";
				holderNames += std::format("'{}' (id {})", threads[holder].threadName, threads[holder].threadId);
			}
			summary += std::format(" '{}' (id {}) waits for '{}' held by {};", thread.threadName, thread.threadId,
				thread.wait.lockClassName != nullptr ? thread.wait.lockClassName : "?", holderNames);
		}
		dump(Reason::DEADLOCK, summary, ids);
		if (config.restartOnDeadlock && !debuggerPresent()) {
			logger().error("Watchdog: restart_on_deadlock is set, exiting with ExitCode.RESTART");
			std::quick_exit(commons::utils::ExitCode::RESTART);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------- stalls, slow tasks
	std::set<TaskKey> activeTasks;
	std::vector<uint64_t> stalledIds;
	std::vector<std::string> stalledDescriptions;
	for (const ThreadSnapshot& thread : threads) {
		if (!thread.task.active)
			continue;
		TaskKey key{thread.threadId, thread.task.startNanos};
		activeTasks.insert(key);
		auto [entry, inserted] = s.tasks.try_emplace(key, TaskProgress{thread.task.scopeId, thread.task.startNanos, false, false});
		TaskProgress& progress = entry->second;
		if (!inserted && progress.scopeId != thread.task.scopeId) {
			progress.scopeId = thread.task.scopeId; // quiescentPoint: the task made progress
			progress.progressNanos = now;
			progress.stallReported = false;
		}
		const char* kind = thread.task.info.kind;
		int64_t runningMillis = millisSince(thread.task.startNanos, now);
		int64_t sinceProgressMillis = millisSince(progress.progressNanos, now);
		const std::source_location& where = thread.task.info.where;
		auto describe = [&] {
			std::string text = std::format("task '{}' from {}:{} ({}) on thread '{}' (id {}) running for {} ms", kind != nullptr ? kind : "?",
				where.file_name(), where.line(), where.function_name(), thread.threadName, thread.threadId, runningMillis);
			if (progress.progressNanos != thread.task.startNanos)
				text += std::format(", {} ms since its last quiescent point", sinceProgressMillis);
			return text;
		};
		if (sinceProgressMillis >= config.stall.count() && !progress.stallReported && !kindIn(kind, config.stallExemptKinds)) {
			progress.stallReported = true;
			progress.slowReported = true; // a stall dump supersedes the slow-task warning
			stalledIds.push_back(thread.threadId);
			stalledDescriptions.push_back(describe());
		} else if (runningMillis >= config.slowTaskWarning.count() && !progress.slowReported && !kindIn(kind, config.slowTaskExemptKinds)) {
			progress.slowReported = true;
			DumpReport report;
			report.reason = Reason::SLOW_TASK;
			report.summary = "slow " + describe();
			report.threadIds = {thread.threadId};
			report.threads = {thread};
			report.text = "Watchdog SLOW_TASK: " + report.summary + "\n" + describeThread(thread, now, true);
			publish(report);
		}
	}
	std::erase_if(s.tasks, [&](const auto& entry) { return !activeTasks.contains(entry.first); });
	if (stalledIds.size() == 1) {
		dump(Reason::STALL, "stalled " + stalledDescriptions[0], stalledIds);
	} else if (!stalledIds.empty()) {
		// one dump for all stalls of this check (a burst of parallel stalls used to write one dump and minidump per thread)
		std::string summary = std::to_string(stalledIds.size()) + " stalled tasks:";
		for (const std::string& description : stalledDescriptions)
			summary += "\n  stalled " + description;
		dump(Reason::STALL, summary, stalledIds);
	}

	// ------------------------------------------------------------------------------------------------------------------- probes
	std::vector<std::pair<std::string, Probe>> probes;
	{
		std::scoped_lock lock(s.listMutex);
		for (auto& [id, probe] : s.probes)
			probes.push_back(probe);
	}
	for (auto& [name, probe] : probes) {
		try {
			probe(*this, threads);
		} catch (...) {
			logger().warnCurrentException("Watchdog probe '" + name + "' threw");
		}
	}
}

uint64_t Watchdog::addProbe(std::string name, Probe probe) {
	WatchdogState& s = state();
	std::scoped_lock lock(s.listMutex);
	uint64_t id = s.nextId++;
	s.probes.emplace_back(id, std::make_pair(std::move(name), std::move(probe)));
	return id;
}

void Watchdog::removeProbe(uint64_t probeId) {
	WatchdogState& s = state();
	std::scoped_lock lock(s.listMutex);
	std::erase_if(s.probes, [probeId](const auto& entry) { return entry.first == probeId; });
}

uint64_t Watchdog::addDumpListener(DumpListener listener) {
	WatchdogState& s = state();
	std::scoped_lock lock(s.listMutex);
	uint64_t id = s.nextId++;
	s.listeners.emplace_back(id, std::move(listener));
	return id;
}

void Watchdog::removeDumpListener(uint64_t listenerId) {
	WatchdogState& s = state();
	std::scoped_lock lock(s.listMutex);
	std::erase_if(s.listeners, [listenerId](const auto& entry) { return entry.first == listenerId; });
}

Watchdog::DumpReport Watchdog::dump(Reason reason, std::string_view summary, std::vector<uint64_t> involvedThreadIds) {
	DumpReport report;
	report.reason = reason;
	report.summary = std::string(summary);
	report.threadIds = std::move(involvedThreadIds);
	report.threads = snapshotThreads();
	int64_t now = commons::utils::nanoTime();
	report.text = std::format("Watchdog {}: {}\nThreads ({}):", reasonName(reason), report.summary, report.threads.size());
	for (const ThreadSnapshot& thread : report.threads) {
		bool involved = std::find(report.threadIds.begin(), report.threadIds.end(), thread.threadId) != report.threadIds.end();
		report.text += "\n" + describeThread(thread, now, involved);
	}
#if defined(_WIN32)
	Config config = getConfig();
	if ((reason == Reason::DEADLOCK || reason == Reason::STALL) && config.writeMinidump) {
		// the text first (implementation notes): it survives a writer that takes long or a process killed meanwhile
		logText(reason, report.text);
		std::string line;
		try {
			auto [path, written] = writeRateLimitedMinidump(config, reason);
			report.minidumpPath = std::move(path);
			line = std::move(written);
		} catch (...) {
			line = "Minidump: could not be written (" + currentExceptionMessage() + ")";
		}
		logText(reason, std::format("Watchdog {}: {}", reasonName(reason), line));
		report.text += "\n" + line;
		notifyListeners(report);
		return report;
	}
#else
	report.text += "\nThread stacks: not available on this platform (TODO)";
#endif
	publish(report);
	return report;
}

std::vector<Watchdog::ThreadSnapshot> Watchdog::snapshotThreads() const {
	std::vector<ThreadSnapshot> threads;
	ThreadContext::forEach([&](const ThreadContext& context) {
		ThreadSnapshot snapshot;
		snapshot.threadId = context.threadId();
		snapshot.threadName = context.threadName();
		snapshot.task = context.task();
		snapshot.blocking = context.blocking();
		snapshot.wait = context.wait();
		uint32_t held = context.heldLockCount.load(std::memory_order_acquire);
		snapshot.heldLockCount = held;
		for (uint32_t i = 0; i < held && i < ThreadContext::MAX_RECORDED_LOCKS; ++i) {
			snapshot.heldLockClasses.push_back(context.heldLocks[i].lockClassName.load(std::memory_order_acquire));
			snapshot.heldLockIds.push_back(context.heldLocks[i].lockId.load(std::memory_order_acquire));
		}
		snapshot.publishedEpoch = context.publishedEpoch.load(std::memory_order_acquire);
		threads.push_back(std::move(snapshot));
	});
	return threads;
}

} // namespace aion::gameserver::runtime
