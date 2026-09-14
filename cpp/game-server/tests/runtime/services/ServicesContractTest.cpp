// Contract test of aion_gs_runtime_services (headers stage): API shapes of CronExpression/CronService, IDFactory, CleanerQueue, LeakCensus and
// the introspection reports. Behaviour is tested in the other files of this directory.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include "aion/commons/configuration/transformers/PropertyTransformer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/services/CleanerQueue.h"
#include "aion/gameserver/runtime/services/Introspection.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/utils/cron/ThreadPoolManagerRunnableRunner.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "sync/LockdepTestSupport.h"

using namespace aion::gameserver;
using runtime::Ref;

// every test of this executable fails on a new lock-order cycle (conventions: "Lock-order validator reports fail tests")
AION_LOCKDEP_FAIL_TESTS_ON_CYCLES();

namespace {

struct Siege final : runtime::RefCounted, runtime::ZombieBreakable {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Siege> create() { return runtime::makeRef<Siege>(); }
	std::vector<const char*> breakKnownEdges() override { return {"target"}; }

protected:
	Siege() = default;
	~Siege() override = default;
};

[[maybe_unused]] void instantiateApi() {
	using namespace services::cron;
	CronService::initSingleton(std::make_unique<utils::cron::ThreadPoolManagerRunnableRunner>(), std::chrono::current_zone());
	Ref<Siege> siege = Siege::create();
	Siege* raw = siege.get();
	Ref<JobDetail> job = CronService::getInstance().schedule(CronJob(runtime::Pin(raw), [raw] { (void)raw; }), "0 0 12 ? * MON-FRI");
	(void)CronService::getInstance().schedule(CronJob([] {}), CronExpressions::getOrCreate("0 0/5 * * * ?"), true);
	(void)CronService::getInstance().cancel(job.get());
	(void)CronService::getInstance().findNextFireTimes(typeid(int));
	const CronExpression* configured = aion::commons::configuration::transformers::PropertyTransformer<const CronExpression*>::parseObject("");
	(void)configured;

	runtime::LeakCensus::getInstance().onRemovedFromWorld(*siege, "Siege", 1);
	(void)runtime::LeakCensus::getInstance().getLeaks();
	(void)runtime::introspection::debugReclaimer();
}

} // namespace

TEST(ServicesContractTest, IDFactoryAllocatesValidIds) {
	auto& factory = utils::idfactory::IDFactory::getInstance();
	factory.resetForTests();
	int32_t id = factory.nextId();
	EXPECT_GT(id, 0);
	EXPECT_FALSE(utils::idfactory::IDFactory::isInvalidId(id));
	EXPECT_TRUE(utils::idfactory::IDFactory::isInvalidId(6484));
	std::vector<int32_t> used{100, 101};
	factory.lockIds(used);
	EXPECT_THROW(factory.lockIds(used), utils::idfactory::IDFactoryError);
	factory.releaseId(id, "Npc");
	factory.resetForTests();
}

TEST(ServicesContractTest, CleanerQueueDrainsIds) {
	runtime::CleanerQueue::setCleanerAction([](int32_t, const char*) {});
	runtime::CleanerQueue::push(7);
	EXPECT_EQ(runtime::CleanerQueue::drainNow(), 1u);
	runtime::CleanerQueue::setCleanerAction(nullptr);
}
