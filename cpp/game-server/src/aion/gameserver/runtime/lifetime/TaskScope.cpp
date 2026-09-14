// TaskScope (scope ids, depth, lazy epoch publication), QuiescentScope and quiescentPoint (design §1.2, §2.4, §2.6).

#include "aion/gameserver/runtime/lifetime/TaskScope.h"

#include <mutex>
#include <set>
#include <string>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/detail/Epoch.h"
#include "aion/gameserver/runtime/lifetime/detail/Mutations.h"

namespace aion::gameserver::runtime {

namespace detail {
std::atomic<uint64_t> globalEpoch{1};
} // namespace detail

namespace {

using detail::isMutated;
using detail::Mutation;

std::atomic<uint64_t> nextScopeId{1};
std::atomic<uint64_t> quiescentWarnings{0};

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.runtime.TaskScope"));
	return *logger;
}

void unpublish(ThreadContext& context) noexcept {
	AION_YIELD_POINT("TaskScope::unpublish");
	context.publishedEpoch.store(EPOCH_IDLE, std::memory_order_release);
}

/** C16: logs once per call site (checked builds). */
void warnQuiescentNoOp(const std::source_location& where, const char* reason) noexcept {
	if (!CHECKED)
		return;
	try {
		static auto* mutex = new std::mutex();
		static auto* sites = new std::set<std::pair<std::string, uint32_t>>();
		{
			std::scoped_lock lock(*mutex);
			if (!sites->emplace(where.file_name(), where.line()).second)
				return;
		}
		quiescentWarnings.fetch_add(1, std::memory_order_relaxed);
		log().warn("quiescentPoint() at {}:{} ({}) is a no-op: {} (C16, design §2.6)", where.file_name(), where.line(), where.function_name(), reason);
	} catch (...) {
	}
}

} // namespace

TaskScope::TaskScope(const TaskInfo& info) noexcept {
	ThreadContext& context = ThreadContext::current();
	if (context.scopeDepth++ == 0) {
		uint64_t id = nextScopeId.fetch_add(1, std::memory_order_relaxed);
		context.scopeId.store(id, std::memory_order_release);
		context.setTask(info, id, commons::utils::nanoTime());
	}
}

TaskScope::TaskScope(const TaskInfo& info, uint64_t submitterScopeId) noexcept {
	ThreadContext& context = ThreadContext::current();
	AION_CHECK("C2", context.scopeDepth == 0, "a JOIN helper TaskScope must be the outermost scope of its thread");
	context.scopeDepth = 1;
	context.joinedHelper = true;
	context.scopeId.store(submitterScopeId, std::memory_order_release);
	context.setTask(info, submitterScopeId, commons::utils::nanoTime());
}

TaskScope::~TaskScope() {
	ThreadContext& context = ThreadContext::current();
	AION_CHECK("C2", context.scopeDepth > 0, "TaskScope destroyed on a thread without an open scope (scopes must be destroyed on their thread)");
	if (--context.scopeDepth == 0) {
		if (!isMutated(Mutation::SCOPE_EXIT_NO_UNPUBLISH))
			unpublish(context);
		detail::flushThreadRetireList();
		context.scopeId.store(0, std::memory_order_release);
		context.joinedHelper = false;
		context.clearTask();
	}
}

bool TaskScope::active() noexcept {
	ThreadContext* context = ThreadContext::currentIfRegistered();
	return context != nullptr && context->scopeDepth > 0;
}

uint32_t TaskScope::depth() noexcept {
	ThreadContext* context = ThreadContext::currentIfRegistered();
	return context != nullptr ? context->scopeDepth : 0;
}

uint64_t TaskScope::currentScopeId() noexcept {
	ThreadContext* context = ThreadContext::currentIfRegistered();
	return context != nullptr ? context->scopeId.load(std::memory_order_relaxed) : 0;
}

TaskInfo TaskScope::currentTaskInfo() noexcept {
	ThreadContext* context = ThreadContext::currentIfRegistered();
	return context != nullptr && context->scopeDepth > 0 ? context->task().info : TaskInfo{};
}

bool TaskScope::isJoinedHelper() noexcept {
	ThreadContext* context = ThreadContext::currentIfRegistered();
	return context != nullptr && context->joinedHelper;
}

namespace {

/** The publication loop of design §2.4 for a thread whose published epoch is IDLE. */
void publish(ThreadContext& context) noexcept {
	uint64_t epoch;
	do {
		AION_YIELD_POINT("TaskScope::ensurePublished:load");
		epoch = detail::globalEpoch.load(std::memory_order_acquire);
		AION_YIELD_POINT("TaskScope::ensurePublished:store");
		context.publishedEpoch.store(epoch, std::memory_order_release);
		if (isMutated(Mutation::PUBLISH_NO_RECHECK))
			break;
		AION_YIELD_POINT("TaskScope::ensurePublished:recheck");
	} while (detail::globalEpoch.load(std::memory_order_acquire) != epoch);
}

} // namespace

void TaskScope::ensurePublished() noexcept {
	ThreadContext& context = ThreadContext::current();
	AION_CHECK("C2", context.scopeDepth > 0, "pointer load outside a TaskScope (every thread that loads shared pointers must run inside a TaskScope)");
	AION_CHECK("C8", context.destructorContextDepth == 0, "pointer load inside a destructor run by the Reclaimer (destructors must be release-only)");
	if (context.publishedEpoch.load(std::memory_order_relaxed) != EPOCH_IDLE) [[likely]]
		return;
	if (isMutated(Mutation::PUBLISH_NOTHING))
		return;
	publish(context);
}

uint64_t detail::borrowStamp() noexcept {
	ThreadContext* context = ThreadContext::currentIfRegistered();
	if (context == nullptr)
		return 0;
	if (context->scopeDepth > 0 && context->destructorContextDepth == 0 && context->publishedEpoch.load(std::memory_order_relaxed) == EPOCH_IDLE &&
		!isMutated(Mutation::BORROW_NO_PUBLISH) && !isMutated(Mutation::PUBLISH_NOTHING)) [[unlikely]]
		publish(*context);
	return CHECKED ? context->scopeId.load(std::memory_order_relaxed) : 0;
}

bool TaskScope::isPublished() noexcept {
	ThreadContext* context = ThreadContext::currentIfRegistered();
	return context != nullptr && context->publishedEpoch.load(std::memory_order_relaxed) != EPOCH_IDLE;
}

QuiescentScope::QuiescentScope(std::source_location) noexcept {
	++ThreadContext::current().quiescentScopeDepth;
}

QuiescentScope::~QuiescentScope() {
	ThreadContext& context = ThreadContext::current();
	AION_CHECK("C2", context.quiescentScopeDepth > 0, "QuiescentScope destroyed on a thread without an open QuiescentScope");
	--context.quiescentScopeDepth;
}

void quiescentPoint(std::source_location where) noexcept {
	ThreadContext& context = ThreadContext::current();
	if (!isMutated(Mutation::QUIESCENT_IGNORE_RULES)) {
		if (context.quiescentScopeDepth == 0) {
			warnQuiescentNoOp(where, "no QuiescentScope is open");
			return;
		}
		if (context.scopeDepth != 1) {
			warnQuiescentNoOp(where, context.scopeDepth == 0 ? "no TaskScope is open" : "TaskScope::depth() != 1 (enclosing frames may hold borrows)");
			return;
		}
		if (context.joinedHelper) {
			warnQuiescentNoOp(where, "the thread runs a JOIN helper scope (the submitter's borrows are shared)");
			return;
		}
	}
	unpublish(context);
	detail::flushThreadRetireList();
	if (context.scopeDepth > 0 && !context.joinedHelper) {
		uint64_t id = nextScopeId.fetch_add(1, std::memory_order_relaxed);
		context.scopeId.store(id, std::memory_order_release);
		ThreadContext::TaskSnapshot task = context.task();
		context.setTask(task.info, id, task.startNanos);
	}
}

namespace detail::testing {

void advanceEpoch() noexcept {
	globalEpoch.fetch_add(1, std::memory_order_acq_rel);
}

uint64_t quiescentNoOpWarnings() noexcept {
	return quiescentWarnings.load(std::memory_order_relaxed);
}

} // namespace detail::testing

} // namespace aion::gameserver::runtime
