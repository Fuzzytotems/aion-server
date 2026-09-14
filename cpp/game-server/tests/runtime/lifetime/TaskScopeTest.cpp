// Conformance of TaskScope, lazy publication, retire-list flushing, QuiescentScope/quiescentPoint rules (design §1.2, §2.4, §2.6, C2, C16).

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <set>
#include <thread>

#include "LifetimeTestSupport.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/lifetime/detail/Epoch.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::lifetimetest;

TEST(TaskScopeTest, OutermostScopeGetsUniqueIdAndNestedScopesShareIt) {
	EXPECT_FALSE(TaskScope::active());
	EXPECT_EQ(TaskScope::depth(), 0u);
	EXPECT_EQ(TaskScope::currentScopeId(), 0u);
	std::set<uint64_t> ids;
	for (int i = 0; i < 3; ++i) {
		TaskScope scope(testTask());
		EXPECT_TRUE(TaskScope::active());
		EXPECT_EQ(TaskScope::depth(), 1u);
		uint64_t id = TaskScope::currentScopeId();
		ids.insert(id);
		{
			TaskScope nested(AION_TASK_INFO(TaskKind::CALLBACK_));
			EXPECT_EQ(TaskScope::depth(), 2u);
			EXPECT_EQ(TaskScope::currentScopeId(), id);
			EXPECT_STREQ(TaskScope::currentTaskInfo().kind, TaskKind::TEST) << "nested TaskInfo is ignored";
		}
	}
	EXPECT_EQ(ids.size(), 3u);
	EXPECT_FALSE(TaskScope::active());
}

TEST(TaskScopeTest, TaskIsVisibleToOtherThreads) {
	TaskScope scope(AION_TASK_INFO(TaskKind::SCHEDULED));
	const ThreadContext* self = ThreadContext::currentIfRegistered();
	ThreadContext::TaskSnapshot snapshot;
	std::thread([&] { snapshot = self->task(); }).join();
	EXPECT_TRUE(snapshot.active);
	EXPECT_STREQ(snapshot.info.kind, TaskKind::SCHEDULED);
	EXPECT_EQ(snapshot.scopeId, TaskScope::currentScopeId());
}

TEST(TaskScopeTest, PublicationIsLazyAndEndsWithTheOutermostScope) {
	const ThreadContext& context = ThreadContext::current();
	{
		TaskScope scope(testTask());
		EXPECT_FALSE(TaskScope::isPublished()) << "entering a scope does not publish (RR-4)";
		EXPECT_EQ(context.publishedEpoch.load(), EPOCH_IDLE);
		TaskScope::ensurePublished();
		EXPECT_TRUE(TaskScope::isPublished());
		EXPECT_EQ(context.publishedEpoch.load(), Reclaimer::currentEpoch());
		uint64_t published = context.publishedEpoch.load();
		Reclaimer::getInstance().reclaimNow();
		TaskScope::ensurePublished(); // cheap: already published, keeps the old epoch
		EXPECT_EQ(context.publishedEpoch.load(), published);
		{
			TaskScope nested(testTask());
		}
		EXPECT_TRUE(TaskScope::isPublished()) << "a nested scope exit does not unpublish";
	}
	EXPECT_FALSE(TaskScope::isPublished());
	EXPECT_EQ(context.publishedEpoch.load(), EPOCH_IDLE);
}

TEST(TaskScopeTest, BorrowFreeTasksDoNotPinReclamation) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	std::atomic<bool> release{false};
	std::atomic<bool> inside{false};
	std::thread longTask([&] {
		TaskScope scope(testTask()); // a long task that never loads a pointer (geo build, DAO call)
		inside = true;
		while (!release.load())
			std::this_thread::yield();
	});
	while (!inside.load())
		std::this_thread::yield();
	Ref<Tracked> object = Tracked::create(tracker);
	object.reset();
	drainReclaimer();
	EXPECT_EQ(tracker->destroyedCount(0), 1u);
	release = true;
	longTask.join();
}

TEST(TaskScopeTest, RetireListIsFlushedAtOutermostScopeExit) {
	drainReclaimer();
	{
		TaskScope scope(testTask());
		{
			TaskScope nested(testTask());
			Ref<Tracked> object = Tracked::create(std::make_shared<Tracker>());
		}
		EXPECT_EQ(detail::unflushedRetireCount(), 1u);
		EXPECT_EQ(Reclaimer::getInstance().stats().backlog, 0u) << "not flushed before the outermost scope ends";
	}
	EXPECT_EQ(detail::unflushedRetireCount(), 0u);
	EXPECT_EQ(Reclaimer::getInstance().stats().backlog, 1u);
	drainReclaimer();
}

TEST(TaskScopeTest, RetireOutsideScopesIsVisibleImmediately) {
	drainReclaimer();
	Ref<Tracked> object = Tracked::create(std::make_shared<Tracker>());
	object.reset();
	EXPECT_EQ(detail::unflushedRetireCount(), 0u);
	EXPECT_EQ(Reclaimer::getInstance().stats().backlog, 1u);
	drainReclaimer();
}

TEST(TaskScopeTest, FullRetireListIsFlushedInsideLongScopes) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	TaskScope scope(testTask());
	std::vector<Ref<Tracked>> objects;
	for (int i = 0; i < 60; ++i)
		objects.push_back(Tracked::create(tracker));
	objects.clear();
	EXPECT_EQ(detail::unflushedRetireCount(), 60u);
	for (int i = 0; i < 200; ++i)
		Reclaimer::retireNode(std::make_unique<TrackedNode>(std::make_shared<Tracker>()));
	EXPECT_LT(detail::unflushedRetireCount(), 256u);
	EXPECT_GE(Reclaimer::getInstance().stats().backlog, 256u);
}

TEST(TaskScopeTest, ThreadExitFlushesTheRetireList) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	std::thread([&] {
		TaskScope scope(testTask());
		Ref<Tracked> object = Tracked::create(tracker);
		object.reset();
		// the scope ends after the Ref, then the thread exits
	}).join();
	std::thread([&] {
		// released by a thread_local destructor at thread exit (outside any scope)
		thread_local Ref<Tracked> threadLocal;
		threadLocal = Tracked::create(tracker);
	}).join();
	drainReclaimer();
	EXPECT_EQ(tracker->destroyedCount(0), 1u);
	EXPECT_EQ(tracker->destroyedCount(1), 1u);
}

TEST(TaskScopeTest, QuiescentPointInValidScopeUnpublishesAndRenewsTheScopeId) {
	TaskScope scope(testTask());
	QuiescentScope quiescent;
	TaskScope::ensurePublished();
	uint64_t before = TaskScope::currentScopeId();
	quiescentPoint();
	EXPECT_FALSE(TaskScope::isPublished());
	EXPECT_NE(TaskScope::currentScopeId(), before);
	EXPECT_EQ(ThreadContext::current().task().scopeId, TaskScope::currentScopeId());
	EXPECT_TRUE(TaskScope::active());
}

TEST(TaskScopeTest, QuiescentPointOutsideValidScopesIsAWarnedNoOp) {
	uint64_t warningsBefore = detail::testing::quiescentNoOpWarnings();
	auto expectNoOp = [](const char* what) {
		bool published = TaskScope::isPublished();
		uint64_t id = TaskScope::currentScopeId();
		quiescentPoint();
		EXPECT_EQ(TaskScope::isPublished(), published) << what;
		EXPECT_EQ(TaskScope::currentScopeId(), id) << what;
	};
	{
		TaskScope scope(testTask());
		TaskScope::ensurePublished();
		expectNoOp("no QuiescentScope");
		QuiescentScope quiescent;
		{
			TaskScope nested(testTask());
			expectNoOp("depth 2");
		}
	}
	std::thread([&] {
		TaskScope helper(testTask(), 12345);
		QuiescentScope quiescent;
		TaskScope::ensurePublished();
		expectNoOp("JOIN helper");
	}).join();
	expectNoOp("outside scopes");
#if AION_CHECKED
	// warnings are once per call site for the whole process: under --gtest_repeat only the first iteration warns
	static bool firstIteration = true;
	const uint64_t expectedPerSite = firstIteration ? 1u : 0u;
	firstIteration = false;
	uint64_t warnings = detail::testing::quiescentNoOpWarnings() - warningsBefore;
	EXPECT_EQ(warnings, expectedPerSite) << "one warning per call site (all calls share the lambda's call site)";
	for (int i = 0; i < 3; ++i)
		quiescentPoint(); // a second site: warned once
	EXPECT_EQ(detail::testing::quiescentNoOpWarnings() - warningsBefore, 2 * expectedPerSite);
#else
	(void)warningsBefore;
#endif
}

TEST(TaskScopeTest, JoinHelperScopeAdoptsSubmitterId) {
	TaskScope scope(testTask());
	uint64_t submitter = TaskScope::currentScopeId();
	std::thread([submitter] {
		{
			TaskScope helper(AION_TASK_INFO(TaskKind::FORK_JOIN), submitter);
			EXPECT_EQ(TaskScope::currentScopeId(), submitter);
			EXPECT_TRUE(TaskScope::isJoinedHelper());
			TaskScope::ensurePublished();
			EXPECT_TRUE(TaskScope::isPublished()) << "helpers publish their own epoch";
		}
		EXPECT_FALSE(TaskScope::isJoinedHelper());
		EXPECT_FALSE(TaskScope::isPublished());
	}).join();
}

#if AION_CHECKED
TEST(TaskScopeDeathTest, PointerLoadOutsideScopeTerminates) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	EXPECT_DEATH(TaskScope::ensurePublished(), "C2");
}

TEST(TaskScopeDeathTest, JoinHelperScopeMustBeOutermost) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	EXPECT_DEATH(
		{
			TaskScope outer(testTask());
			TaskScope helper(testTask(), 1);
		},
		"C2");
}
#endif
