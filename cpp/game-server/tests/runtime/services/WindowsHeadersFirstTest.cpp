// A translation unit that includes <windows.h> before every public runtime kernel header, as a TU does that includes spdlog or Asio first
// (wave1-status.md "Runtime kernel": TaskKind::CALLBACK collided with the CALLBACK macro). It must compile; the test only checks that the
// renamed and macro-shaped names are usable.

#include <gtest/gtest.h>

#include <windows.h>

#include <string_view>

#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/collections/ArrayDeque.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/Collections.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/ConcurrentLinkedQueue.h"
#include "aion/gameserver/runtime/collections/CopyOnWriteArrayList.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/collections/Iterators.h"
#include "aion/gameserver/runtime/collections/JavaEquals.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/fields/Final.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Pct.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/runtime/sched/ExecutorBackend.h"
#include "aion/gameserver/runtime/sched/ForkJoinPool.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/runtime/sched/PoolBackends.h"
#include "aion/gameserver/runtime/sched/SerialExecutor.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"
#include "aion/gameserver/runtime/sched/TimeUnit.h"
#include "aion/gameserver/runtime/services/CleanerQueue.h"
#include "aion/gameserver/runtime/services/Introspection.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/runtime/services/RuntimeLifecycle.h"
#include "aion/gameserver/runtime/sync/BlockingRegion.h"
#include "aion/gameserver/runtime/sync/LockClass.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/runtime/sync/LockRank.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/runtime/sync/RankedMutex.h"
#include "aion/gameserver/runtime/sync/Semaphore.h"
#include "aion/gameserver/runtime/sync/StampedLock.h"
#include "aion/gameserver/runtime/sync/Watchdog.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/cron/ThreadPoolManagerRunnableRunner.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/commons/utils/WindowsMacroGuard.h"

namespace aion::gameserver::runtime {
namespace {

#ifndef CALLBACK
#error "<windows.h> is expected to define CALLBACK; this test would not prove anything"
#endif

TEST(WindowsHeadersFirstTest, KernelNamesSurviveWindowsMacros) {
	EXPECT_EQ(std::string_view(TaskKind::CALLBACK_), "callback");
	PinnedCallback<void()> callback([] {});
	EXPECT_STREQ(callback.getTaskInfo().kind, TaskKind::CALLBACK_);
	EXPECT_THROW(throw ArithmeticException("/ by zero"), commons::utils::Exception);
}

} // namespace
} // namespace aion::gameserver::runtime
