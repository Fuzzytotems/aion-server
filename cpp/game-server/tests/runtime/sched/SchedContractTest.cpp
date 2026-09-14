// Contract test of aion_gs_runtime_sched (headers stage): compile-time capture rules, Pin, PinnedCallback, deferred Future, and the
// ThreadPoolManager task forms of the design's porting samples (runtime-architecture.md §14.2 a, c, f, g).

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/runtime/sched/ForkJoinPool.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/runtime/sched/SerialExecutor.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

using namespace aion::gameserver::runtime;
using aion::gameserver::utils::ThreadPoolManager;

namespace {

class Npc final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Npc> create() { return makeRef<Npc>(); }
	void despawn() { despawned = true; }
	bool despawned = false;

protected:
	Npc() = default;
	~Npc() override = default;
};

struct SkillTemplate : StaticTemplate {
	int32_t id = 1;
};

struct QuestHandler : Immortal {};

/** an AI part (design §14.2a): schedule(this, &X::method, delay) pins the owning Npc */
class YamenessPortalSummonedAI final : public OwnedPart {
public:
	explicit YamenessPortalSummonedAI(const RefCounted& owner) : OwnedPart(owner) {}
	void handleSpawned() { (void)ThreadPoolManager::getInstance().schedule(this, &YamenessPortalSummonedAI::spawnSummons, 12000); }
	void spawnSummons() {
		(void)ThreadPoolManager::getInstance().schedule(this, [this] { spawnSummons(); }, 60000);
	}
};

/** design §14.2f GeneralUpdateTask */
struct GeneralUpdateTask : TaskStruct {
	int32_t playerId;
	void operator()() const {}
};

struct BadTask : TaskStruct {
	Ptr<Npc> npc; // borrowed pointers are not TaskArgs
	void operator()() const {}
};

struct NamedTask : TaskStruct {
	std::string name;
	Ref<Npc> npc;
	const SkillTemplate* skill = nullptr;
	void operator()() const {}
};

// ---- compile-time rules (design §7.3)
static_assert(TaskArg<int32_t>);
static_assert(TaskArg<Ref<Npc>>);
static_assert(TaskArg<FutureRef>);
static_assert(TaskArg<std::string>);
static_assert(TaskArg<const SkillTemplate*>);
static_assert(TaskArg<QuestHandler*>);
static_assert(TaskArg<std::shared_ptr<const std::string>>);
static_assert(!TaskArg<Ptr<Npc>>);
static_assert(!TaskArg<Npc*>);
static_assert(!TaskArg<std::string_view>);
static_assert(AllFieldsAreTaskArgs<GeneralUpdateTask>);
static_assert(AllFieldsAreTaskArgs<NamedTask>);
static_assert(!AllFieldsAreTaskArgs<BadTask>);
static_assert(UnpinnedTask<GeneralUpdateTask>);
static_assert(!UnpinnedTask<BadTask>);
static_assert(UnpinnedTask<decltype([] {})>);
static_assert(!UnpinnedTask<decltype([x = 1] { (void)x; })>);
static_assert(UnpinnedTask<decltype(bindTask([](Npc&) {}, std::declval<Ref<Npc>>()))>);
static_assert(UnpinnedPeriodicTask<decltype([](Future&) {})>);
static_assert(PerElementSafe<Ref<Npc>>);
static_assert(!PerElementSafe<Ptr<Npc>>);

/** Instantiates the ThreadPoolManager task forms (compile-time contract; the behaviour is tested in the other sched tests). */
[[maybe_unused]] void instantiateThreadPoolManagerForms(Npc& npc, YamenessPortalSummonedAI& ai, const SkillTemplate* skill) {
	ThreadPoolManager& pools = ThreadPoolManager::getInstance();
	FutureRef a = pools.schedule(&npc, [&npc] { npc.despawn(); }, 1000);
	FutureRef b = pools.schedule({&npc, &ai, skill}, [&npc] { npc.despawn(); }, 5, TimeUnit::SECONDS);
	FutureRef c = pools.schedule(GeneralUpdateTask{{}, 5}, 1000);
	FutureRef d = pools.schedule(bindTask([](Npc& claw) { claw.despawn(); }, Ref<Npc>(npc)), 60000 * 5);
	FutureRef e = pools.schedule(&npc, &Npc::despawn, 100);
	FutureRef f = pools.scheduleAtFixedRate({&npc, skill}, [&npc] { npc.despawn(); }, 300, 1000);
	FutureRef g = pools.scheduleAtFixedRate(&npc, [](Future& self) { self.cancel(false); }, 0, 1000);
	FutureRef h = pools.scheduleAtFixedRate([](Future& self) { self.cancel(); }, 0, 1000);
	pools.execute(&npc, [&npc] { npc.despawn(); });
	pools.execute([] {});
	FutureRef i = pools.submit(&npc, [&npc] { npc.despawn(); });
	pools.executeLongRunning(GeneralUpdateTask{{}, 1});
	FutureRef j = pools.submitLongRunning([] {});
	(void)pools.tasksPinning(npc);
	ai.handleSpawned();

	std::vector<Ptr<Npc>> creatures;
	ForkJoinPool::commonPool().parallelForEach(creatures, [](Ptr<Npc> creature) { creature->despawn(); });
	std::vector<Ref<Npc>> spawns;
	ForkJoinPool::commonPool().parallelForEach(spawns, [](const Ref<Npc>& spawn) { spawn->despawn(); }, Isolation::PER_ELEMENT);

	SerialExecutor link("LoginServer");
	link.execute(&npc, [&npc] { npc.despawn(); });
	link.execute([] {});
}

} // namespace

TEST(SchedContractTest, PinRetainsOwnersAndPartsPinTheirOwner) {
	Ref<Npc> npc = Npc::create();
	YamenessPortalSummonedAI ai(*npc);
	SkillTemplate skill;
	{
		Pin pin{npc.get(), &ai, &skill};
		EXPECT_EQ(pin.size(), 1u); // npc and the AI's owner are the same object
		EXPECT_TRUE(pin.pins(*npc));
		EXPECT_EQ(npc->refCount(), 2u);
		Pin copy = pin;
		EXPECT_EQ(npc->refCount(), 3u);
	}
	EXPECT_EQ(npc->refCount(), 1u);
}

TEST(SchedContractTest, PinnedCallbackInvocationAndIdentity) {
	Ref<Npc> npc = Npc::create();
	Npc* raw = npc.get();
	PinnedCallback<void(int32_t)> callback(Pin(raw), [raw](int32_t) { raw->despawn(); });
	PinnedCallback<void(int32_t)> copy = callback;
	EXPECT_TRUE(copy == callback);
	EXPECT_TRUE(callback.pins(*npc));
	copy(1);
	EXPECT_TRUE(npc->despawned);
	PinnedCallback<void()> unpinned([] {});
	unpinned();
	PinnedCallback<void()> empty;
	EXPECT_THROW(empty(), NullPointerException);
}

TEST(SchedContractTest, DeferredFutureRunGetAndCancel) {
	TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
	Ref<Npc> npc = Npc::create();
	Npc* raw = npc.get();
	FutureRef teleport = Future::deferred(Pin(raw), [raw] { raw->despawn(); });
	EXPECT_FALSE(teleport->isDone());
	teleport->run(); // CM_TELEPORT_ANIMATION_DONE: task->run(); task->get();
	teleport->get();
	EXPECT_TRUE(npc->despawned);
	EXPECT_TRUE(teleport->isDone());
	EXPECT_FALSE(teleport->cancel(false));

	FutureRef failing = Future::deferred(Pin(), [] { throw IllegalStateException("boom"); });
	failing->run();
	EXPECT_THROW(failing->get(), ExecutionException);

	FutureRef cancelled = Future::deferred(Pin(raw), [raw] { raw->despawn(); });
	EXPECT_TRUE(cancelled->cancel(false));
	EXPECT_TRUE(cancelled->isCancelled());
	EXPECT_THROW(cancelled->get(), CancellationException);
}

TEST(SchedContractTest, ThreadPoolManagerOnDeterministicExecutor) {
	ManualClock clock;
	auto executor = std::make_unique<DeterministicExecutor>(clock, 42);
	DeterministicExecutor* deterministic = executor.get();
	ThreadPoolManager::installBackend(std::move(executor));
	Ref<Npc> npc = Npc::create();
	Npc* raw = npc.get();
	FutureRef task = ThreadPoolManager::getInstance().schedule(raw, [raw] { raw->despawn(); }, 1000);
	deterministic->advance(std::chrono::milliseconds(999));
	EXPECT_FALSE(npc->despawned);
	deterministic->advance(std::chrono::milliseconds(1));
	EXPECT_TRUE(npc->despawned);
	EXPECT_TRUE(task->isDone());
	ThreadPoolManager::installBackend(nullptr);
}
