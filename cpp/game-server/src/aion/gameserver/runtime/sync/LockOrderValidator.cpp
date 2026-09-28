#include "aion/gameserver/runtime/sync/LockOrderValidator.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <format>
#include <mutex>
#include <stacktrace>
#include <unordered_map>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/sync/RankedMutex.h"
#include "aion/gameserver/runtime/sync/detail/HeldLocks.h"

// Implementation notes
// - The per-thread stack of held game-level locks used for edges lives in a trivially destructible thread_local (checked builds only), not in
//   ThreadContext: it needs the LockClass (ThreadContext stores only the class name for the watchdog) and is never read by other threads.
// - Edge keys are (from class id << 32 | to class id); class ids start at 1, so 0 marks an empty cache slot. Markers for same-class nesting
//   and self-deadlock reports use the top bits.
// - Symbolizing a std::stacktrace (DbgHelp) is slow and happens only for new reports, outside the graph mutex.

namespace aion::gameserver::runtime {

namespace {

constexpr uint32_t MAX_TRACKED = 64;
constexpr size_t EDGE_CACHE_SIZE = 256;
constexpr uint64_t SAME_CLASS_MARKER = 1ull << 63;
constexpr uint64_t SELF_LOCK_MARKER = 1ull << 62;

std::atomic<bool> enabledFlag{CHECKED};
std::atomic<bool> failOnReportFlag{false};

struct TrackedLock {
	const LockClass* lockClass;
	uintptr_t lockId;
	bool shared;
};

struct ThreadState {
	std::array<TrackedLock, MAX_TRACKED> held{};
	uint32_t count = 0;
	std::array<uint64_t, EDGE_CACHE_SIZE> edgeCache{};
	uint64_t cacheGeneration = 0;
};

thread_local ThreadState threadState;

struct EdgeInfo {
	std::stacktrace stack;
	const char* threadName;
};

struct Graph {
	RankedMutex<LockRank::STATS> mutex;
	std::unordered_map<uint64_t, EdgeInfo> edges;
	std::unordered_map<uint32_t, std::vector<const LockClass*>> successors;
	std::unordered_map<uint32_t, const LockClass*> classes;
	std::vector<LockOrderValidator::Report> reports;
	std::unordered_map<std::string, size_t> reportIndex;
	std::atomic<uint64_t> generation{1};
	std::atomic<uint64_t> failures{0};
};

Graph& graph() {
	static auto* instance = new Graph(); // leaked: locks may be used during static destruction
	return *instance;
}

const commons::logging::Logger& logger() {
	static const commons::logging::Logger log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.runtime.LockOrderValidator");
	return log;
}

uint64_t edgeKey(const LockClass& from, const LockClass& to) noexcept {
	return (static_cast<uint64_t>(from.id()) << 32) | to.id();
}

size_t cacheSlot(uint64_t key) noexcept {
	return static_cast<size_t>((key * 0x9E3779B97F4A7C15ull) >> 56) % EDGE_CACHE_SIZE;
}

bool cacheContains(const ThreadState& state, uint64_t key) noexcept {
	return state.edgeCache[cacheSlot(key)] == key;
}

void cacheInsert(ThreadState& state, uint64_t key) noexcept {
	state.edgeCache[cacheSlot(key)] = key;
}

void syncCacheGeneration(ThreadState& state) noexcept {
	uint64_t generation = graph().generation.load(std::memory_order_acquire);
	if (state.cacheGeneration != generation) {
		state.edgeCache.fill(0);
		state.cacheGeneration = generation;
	}
}

bool holdsLeafMutex(const ThreadContext& context) noexcept {
	return context.heldLeafCount != 0;
}

std::string heldClassList(const ThreadState& state) {
	std::string text = "[";
	uint32_t recorded = std::min(state.count, MAX_TRACKED);
	for (uint32_t i = 0; i < recorded; ++i) {
		if (i != 0)
			text += ", ";
		text += state.held[i].lockClass->name();
	}
	if (state.count > MAX_TRACKED)
		text += std::format(", ... {} more", state.count - MAX_TRACKED);
	return text + "]";
}

/** Breadth-first search for a path from -> ... -> to over recorded edges. Returns the classes on the path (from first, to last) or empty. */
std::vector<const LockClass*> findPath(const Graph& g, const LockClass& from, const LockClass& to) {
	std::unordered_map<uint32_t, const LockClass*> parent;
	std::vector<const LockClass*> queue{&from};
	parent.emplace(from.id(), nullptr);
	for (size_t head = 0; head < queue.size(); ++head) {
		const LockClass* current = queue[head];
		if (current == &to) {
			std::vector<const LockClass*> path;
			for (const LockClass* node = current; node != nullptr; node = parent[node->id()])
				path.push_back(node);
			std::reverse(path.begin(), path.end());
			return path;
		}
		auto successors = g.successors.find(current->id());
		if (successors == g.successors.end())
			continue;
		for (const LockClass* next : successors->second)
			if (parent.emplace(next->id(), current).second)
				queue.push_back(next);
	}
	return {};
}

/** A report found under the graph mutex, formatted and published after it is released. */
struct PendingReport {
	LockOrderValidator::ReportKind kind;
	std::string key;
	const LockClass* held;
	const LockClass* acquired;
	/** reverse path edges (CYCLE): from, to, first-occurrence info */
	std::vector<std::tuple<const LockClass*, const LockClass*, EdgeInfo>> reversePath;
};

std::string reportKey(LockOrderValidator::ReportKind kind, const LockClass& held, const LockClass& acquired) {
	return std::format("{}:{}:{}", static_cast<int>(kind), held.id(), acquired.id());
}

} // namespace

LockOrderValidator& LockOrderValidator::getInstance() {
	static auto* instance = new LockOrderValidator();
	return *instance;
}

bool LockOrderValidator::isEnabled() const noexcept {
	return CHECKED && enabledFlag.load(std::memory_order_relaxed);
}

void LockOrderValidator::setEnabled(bool value) noexcept {
	enabledFlag.store(value, std::memory_order_relaxed);
}

void LockOrderValidator::beforeAcquire(const LockClass& lockClass, uintptr_t lockId, bool shared) noexcept {
	if (!CHECKED || !enabledFlag.load(std::memory_order_relaxed))
		return;
	ThreadState& state = threadState;
	if (state.count == 0)
		return;
	ThreadContext& context = ThreadContext::current();
	if (context.lockdepSuppressionDepth != 0 || holdsLeafMutex(context))
		return;
	syncCacheGeneration(state);

	std::array<const LockClass*, MAX_TRACKED> newEdges{};
	uint32_t newEdgeCount = 0;
	bool sameClass = false;
	bool selfLock = false;
	uint32_t recorded = std::min(state.count, MAX_TRACKED);
	for (uint32_t i = 0; i < recorded; ++i) {
		const TrackedLock& held = state.held[i];
		if (held.lockId == lockId) {
			if (!(shared && held.shared))
				selfLock = !cacheContains(state, edgeKey(lockClass, lockClass) | SELF_LOCK_MARKER);
			continue;
		}
		if (held.lockClass == &lockClass) {
			sameClass = !cacheContains(state, edgeKey(lockClass, lockClass) | SAME_CLASS_MARKER);
			continue;
		}
		uint64_t key = edgeKey(*held.lockClass, lockClass);
		if (cacheContains(state, key))
			continue;
		if (std::find(newEdges.begin(), newEdges.begin() + newEdgeCount, held.lockClass) == newEdges.begin() + newEdgeCount)
			newEdges[newEdgeCount++] = held.lockClass;
	}
	if (newEdgeCount == 0 && !sameClass && !selfLock)
		return;

	try {
		std::stacktrace stack = std::stacktrace::current(1);
		const char* threadName = context.threadName();
		std::vector<PendingReport> pending;
		Graph& g = graph();
		{
			std::scoped_lock lock(g.mutex);
			g.classes.emplace(lockClass.id(), &lockClass);
			for (uint32_t i = 0; i < newEdgeCount; ++i) {
				const LockClass& from = *newEdges[i];
				g.classes.emplace(from.id(), &from);
				auto [edge, inserted] = g.edges.try_emplace(edgeKey(from, lockClass), EdgeInfo{stack, threadName});
				if (!inserted)
					continue;
				g.successors[from.id()].push_back(&lockClass);
				std::vector<const LockClass*> path = findPath(g, lockClass, from);
				if (path.size() < 2)
					continue;
				PendingReport report{ReportKind::CYCLE, reportKey(ReportKind::CYCLE, from, lockClass), &from, &lockClass, {}};
				for (size_t p = 0; p + 1 < path.size(); ++p)
					report.reversePath.emplace_back(path[p], path[p + 1], g.edges.at(edgeKey(*path[p], *path[p + 1])));
				pending.push_back(std::move(report));
			}
			auto countOrQueue = [&](bool self) {
				std::string key = reportKey(ReportKind::SAME_CLASS_NESTING, lockClass, lockClass) + (self ? ":self" : "");
				if (auto existing = g.reportIndex.find(key); existing != g.reportIndex.end())
					++g.reports[existing->second].occurrences;
				else
					pending.push_back(PendingReport{ReportKind::SAME_CLASS_NESTING, std::move(key), &lockClass, &lockClass, {}});
			};
			if (sameClass)
				countOrQueue(false);
			if (selfLock)
				countOrQueue(true);
		}

		std::vector<Report> formatted;
		if (!pending.empty()) {
			std::string acquisitionStack = std::to_string(stack);
			std::string held = heldClassList(state);
			for (PendingReport& item : pending) {
				Report report;
				report.kind = item.kind;
				report.heldLockClass = item.held->name();
				report.acquiredLockClass = item.acquired->name();
				report.acquisitionStack = acquisitionStack;
				report.occurrences = 1;
				if (item.kind == ReportKind::CYCLE) {
					std::string reverse;
					std::string pathText = "'" + item.acquired->name() + "'";
					for (auto& [from, to, info] : item.reversePath) {
						pathText += " -> '" + to->name() + "'";
						reverse += std::format("  '{}' -> '{}' first acquired on thread '{}' at:\n{}\n", from->name(), to->name(), info.threadName,
							std::to_string(info.stack));
					}
					report.reverseEdgeStack = std::move(reverse);
					report.text = std::format(
						"Lock-order inversion (possible deadlock): thread '{}' acquires lock class '{}' while holding '{}', but the reverse order {} was "
						"recorded before.\nHeld locks: {}\nAcquisition '{}' -> '{}' at:\n{}\nReverse path:\n{}",
						threadName, item.acquired->name(), item.held->name(), pathText, held, item.held->name(), item.acquired->name(), acquisitionStack,
						report.reverseEdgeStack);
				} else if (item.key.ends_with(":self")) {
					report.text = std::format(
						"Thread '{}' acquires lock class '{}' again while already holding the same non-reentrant lock (self-deadlock). Held locks: {}\nat:\n{}",
						threadName, item.acquired->name(), held, acquisitionStack);
				} else {
					report.text = std::format(
						"Thread '{}' nests two different locks of the same class '{}' (order between instances of one class is not validated). Held locks: "
						"{}\nat:\n{}",
						threadName, item.acquired->name(), held, acquisitionStack);
				}
				formatted.push_back(std::move(report));
			}
			std::vector<size_t> published;
			{
				std::scoped_lock lock(g.mutex);
				for (size_t i = 0; i < formatted.size(); ++i) {
					if (auto existing = g.reportIndex.find(pending[i].key); existing != g.reportIndex.end()) {
						++g.reports[existing->second].occurrences;
						continue;
					}
					g.reportIndex.emplace(pending[i].key, g.reports.size());
					g.reports.push_back(formatted[i]);
					published.push_back(i);
				}
			}
			for (size_t i : published) {
				if (formatted[i].kind == ReportKind::CYCLE) {
					if (failOnReportFlag.load(std::memory_order_relaxed))
						g.failures.fetch_add(1, std::memory_order_relaxed);
					logger().error(formatted[i].text);
				} else {
					logger().warn(formatted[i].text);
				}
			}
		}

		for (uint32_t i = 0; i < newEdgeCount; ++i)
			cacheInsert(state, edgeKey(*newEdges[i], lockClass));
		if (sameClass)
			cacheInsert(state, edgeKey(lockClass, lockClass) | SAME_CLASS_MARKER);
		if (selfLock)
			cacheInsert(state, edgeKey(lockClass, lockClass) | SELF_LOCK_MARKER);
	} catch (...) {
		// allocation or logging failure: validation is best effort (D5), the acquisition proceeds
	}
}

void LockOrderValidator::afterAcquire(const LockClass& lockClass, uintptr_t lockId, uint8_t rank, bool shared) noexcept {
	detail::pushHeldLock(ThreadContext::current(), lockClass.name().c_str(), lockId, rank);
	if (CHECKED && rank == 0) {
		ThreadState& state = threadState;
		if (state.count < MAX_TRACKED)
			state.held[state.count] = TrackedLock{&lockClass, lockId, shared};
		++state.count;
	}
}

void LockOrderValidator::afterRelease(uintptr_t lockId) noexcept {
	ThreadContext* context = ThreadContext::currentIfRegistered();
	if (context == nullptr)
		return;
	detail::popHeldLock(*context, lockId);
	if (CHECKED) {
		ThreadState& state = threadState;
		uint32_t recorded = std::min(state.count, MAX_TRACKED);
		for (uint32_t i = recorded; i-- > 0;) {
			if (state.held[i].lockId == lockId) {
				for (uint32_t j = i; j + 1 < recorded; ++j)
					state.held[j] = state.held[j + 1];
				--state.count;
				return;
			}
		}
		if (state.count > MAX_TRACKED)
			--state.count;
	}
}

void LockOrderValidator::onBlocking(const char* what, const std::source_location& where) noexcept {
	if (!CHECKED || !enabledFlag.load(std::memory_order_relaxed))
		return;
	ThreadState& state = threadState;
	if (state.count == 0)
		return;
	ThreadContext& context = ThreadContext::current();
	if (context.lockdepSuppressionDepth != 0 || holdsLeafMutex(context))
		return;
	try {
		const LockClass& innermost = *state.held[std::min(state.count, MAX_TRACKED) - 1].lockClass;
		std::string key = std::format("blocking:{}@{}:{}", what != nullptr ? what : "?", where.file_name(), where.line());
		Graph& g = graph();
		std::string text;
		{
			std::scoped_lock lock(g.mutex);
			if (auto existing = g.reportIndex.find(key); existing != g.reportIndex.end()) {
				++g.reports[existing->second].occurrences;
				return;
			}
		}
		text = std::format("Blocking wait '{}' while holding game-level locks {} on thread '{}' at {}:{} ({})", what != nullptr ? what : "?",
			heldClassList(state), context.threadName(), where.file_name(), where.line(), where.function_name());
		bool published = false;
		{
			std::scoped_lock lock(g.mutex);
			if (auto existing = g.reportIndex.find(key); existing != g.reportIndex.end()) {
				++g.reports[existing->second].occurrences;
			} else {
				Report report;
				report.kind = ReportKind::BLOCKING_UNDER_MONITOR;
				report.heldLockClass = innermost.name();
				report.acquiredLockClass = what != nullptr ? what : "";
				report.text = text;
				report.occurrences = 1;
				g.reportIndex.emplace(std::move(key), g.reports.size());
				g.reports.push_back(std::move(report));
				published = true;
			}
		}
		if (published)
			logger().warn(text);
	} catch (...) {
		// best effort
	}
}

std::vector<LockOrderValidator::Report> LockOrderValidator::getReports() const {
	Graph& g = graph();
	std::scoped_lock lock(g.mutex);
	return g.reports;
}

size_t LockOrderValidator::reportCount(ReportKind kind) const {
	Graph& g = graph();
	std::scoped_lock lock(g.mutex);
	return static_cast<size_t>(std::count_if(g.reports.begin(), g.reports.end(), [kind](const Report& report) { return report.kind == kind; }));
}

uint64_t LockOrderValidator::failureCount() const noexcept {
	return graph().failures.load(std::memory_order_relaxed);
}

void LockOrderValidator::clearReports(bool resetGraph) {
	Graph& g = graph();
	std::scoped_lock lock(g.mutex);
	g.reports.clear();
	g.reportIndex.clear();
	g.failures.store(0, std::memory_order_relaxed);
	if (resetGraph) {
		g.edges.clear();
		g.successors.clear();
		g.classes.clear();
		g.generation.fetch_add(1, std::memory_order_acq_rel);
	}
}

void LockOrderValidator::setFailOnReport(bool value) noexcept {
	failOnReportFlag.store(value, std::memory_order_relaxed);
}

bool LockOrderValidator::isFailOnReport() const noexcept {
	return failOnReportFlag.load(std::memory_order_relaxed);
}

std::vector<std::string> LockOrderValidator::describe() const {
	static constexpr std::array<const char*, 3> kindNames{"CYCLE", "SAME_CLASS_NESTING", "BLOCKING_UNDER_MONITOR"};
	std::vector<std::string> lines;
	Graph& g = graph();
	std::scoped_lock lock(g.mutex);
	lines.push_back(std::format("lock-order validator: {}, {} lock classes, {} edges, {} reports, failOnReport={}", isEnabled() ? "enabled" : "disabled",
		g.classes.size(), g.edges.size(), g.reports.size(), isFailOnReport()));
	for (const Report& report : g.reports) {
		if (report.kind == ReportKind::BLOCKING_UNDER_MONITOR)
			lines.push_back(std::format("{} x{}: {}", kindNames[static_cast<size_t>(report.kind)], report.occurrences, report.text));
		else
			lines.push_back(std::format("{} x{}: '{}' -> '{}'", kindNames[static_cast<size_t>(report.kind)], report.occurrences, report.heldLockClass,
				report.acquiredLockClass));
	}
	return lines;
}

LockdepSuppression::LockdepSuppression(const char*) noexcept {
	++ThreadContext::current().lockdepSuppressionDepth;
}

LockdepSuppression::~LockdepSuppression() {
	--ThreadContext::current().lockdepSuppressionDepth;
}

} // namespace aion::gameserver::runtime
