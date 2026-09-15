// P4-11b CreatureController task map and delete invariants, and the material skill actor with its K4 MaterialSkillTask, against real Npcs on
// the deterministic executor. Expectations are derived by hand from CreatureController.java (hasTask, hasScheduledTask, getAndRemoveTask,
// cancelTask, cancelTaskIfPresent, addTask, cancelAllTasks, onDelete), VisibleObjectController.java and AbstractMaterialSkillActor.java; the
// delete breakers are the C++-only LogoutBreakers::onDelete steps (runtime-architecture.md §5.3).

#include "ControllersTestSupport.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/observer/AbstractCollisionObserver.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/controllers/observer/ZoneCollisionMaterialActor.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/templates/materials/MaterialSkill.bind.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

namespace aion::gameserver::controllers::testing {
namespace {

using model::TaskId;
using runtime::FutureRef;
using runtime::Ptr;
using runtime::Ref;

std::atomic<int32_t> taskRuns{0};
std::atomic<int32_t> destroyedActors{0};

FutureRef scheduleCountingTask(int64_t delay) {
	return utils::ThreadPoolManager::getInstance().schedule([] { taskRuns.fetch_add(1); }, delay);
}

/** Records onRemoved: the delete breaker must not call it */
class RemovalObserver final : public observer::ActionObserver {
	AION_MAKE_REF_FRIEND
public:
	std::atomic<int32_t> removed{0};
	static Ref<RemovalObserver> create() { return runtime::makeRef<RemovalObserver>(); }
	void onRemoved() override { removed.fetch_add(1); }

protected:
	RemovalObserver() : ActionObserver(observer::ObserverType::ALL) {}
	~RemovalObserver() override = default;
};

/** ZoneCollisionMaterialActor with a destruction counter (the actor is otherwise the real class) */
class CountedMaterialActor final : public observer::ZoneCollisionMaterialActor {
	AION_MAKE_REF_FRIEND
public:
	static Ref<CountedMaterialActor> create(model::gameobjects::Creature& creature, std::vector<const model::templates::materials::MaterialSkill*> skills) {
		return runtime::makeRef<CountedMaterialActor>(creature, std::move(skills));
	}

	bool touched() const { return isTouched.get(); }

protected:
	CountedMaterialActor(model::gameobjects::Creature& creature, std::vector<const model::templates::materials::MaterialSkill*> skills)
		: ZoneCollisionMaterialActor(creature, nullptr, std::move(skills), CheckType::TOUCH) {}
	~CountedMaterialActor() override { destroyedActors.fetch_add(1); }
};

const model::templates::materials::MaterialSkill* materialSkill() {
	static const model::templates::materials::MaterialSkill* skill = [] {
		xml::LoadContext context;
		return xml::bindString<model::templates::materials::MaterialSkill>(context, R"(<skill id="8801" level="1" frequency="2"/>)").release();
	}();
	return skill;
}

class CreatureControllerTest : public ControllersTest {
protected:
	void SetUp() override {
		ControllersTest::SetUp();
		taskRuns = 0;
		destroyedActors = 0;
	}
};

TEST_F(CreatureControllerTest, TaskMapFollowsJava) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ControllersTestNpc> npc = createNpc();
	CreatureController& controller = npc->getController();
	EXPECT_FALSE(controller.hasTask(TaskId::DECAY));
	EXPECT_FALSE(controller.hasScheduledTask(TaskId::DECAY));

	FutureRef first = scheduleCountingTask(1000);
	controller.addTask(TaskId::DECAY, first);
	EXPECT_TRUE(controller.hasTask(TaskId::DECAY));
	EXPECT_TRUE(controller.hasScheduledTask(TaskId::DECAY));

	FutureRef second = scheduleCountingTask(1000);
	controller.addTask(TaskId::DECAY, second);
	EXPECT_TRUE(first->isCancelled()) << "addTask cancels the task it replaces";
	EXPECT_FALSE(second->isCancelled());

	FutureRef stranger = scheduleCountingTask(1000);
	EXPECT_FALSE(controller.cancelTaskIfPresent(TaskId::DECAY, stranger)) << "remove(key, value) compares the future's identity";
	EXPECT_FALSE(stranger->isCancelled());
	EXPECT_TRUE(controller.hasTask(TaskId::DECAY));
	EXPECT_TRUE(controller.cancelTaskIfPresent(TaskId::DECAY, second));
	EXPECT_TRUE(second->isCancelled());
	EXPECT_FALSE(controller.hasTask(TaskId::DECAY));

	controller.addTask(TaskId::PRISON, stranger);
	advance(std::chrono::milliseconds(1000));
	EXPECT_EQ(taskRuns, 1) << "only the uncancelled task ran";
	EXPECT_TRUE(controller.hasTask(TaskId::PRISON)) << "a finished task stays in the map";
	EXPECT_FALSE(controller.hasScheduledTask(TaskId::PRISON)) << "but is no longer scheduled";

	FutureRef removed = controller.getAndRemoveTask(TaskId::PRISON);
	EXPECT_EQ(removed.get(), stranger.get());
	EXPECT_FALSE(controller.hasTask(TaskId::PRISON));
	EXPECT_FALSE(controller.getAndRemoveTask(TaskId::PRISON)) << "null when absent";

	FutureRef pending = scheduleCountingTask(5000);
	controller.addTask(TaskId::SKILL_USE, pending);
	FutureRef cancelled = controller.cancelTask(TaskId::SKILL_USE);
	EXPECT_EQ(cancelled.get(), pending.get());
	EXPECT_TRUE(pending->isCancelled());
	EXPECT_FALSE(controller.cancelTask(TaskId::SKILL_USE));
}

TEST_F(CreatureControllerTest, ConcurrentAddTaskKeepsExactlyOneUncancelledTask) {
	Ref<ControllersTestNpc> npc;
	{
		CONTROLLERS_TEST_SCOPE;
		npc = createNpc();
	}
	constexpr int threads = 8;
	std::vector<std::vector<FutureRef>> added(threads);
	std::atomic<bool> go{false};
	std::vector<std::thread> workers;
	for (int t = 0; t < threads; ++t) {
		workers.emplace_back([&, t] {
			while (!go.load())
				std::this_thread::yield();
			for (int i = 0; i < 100; ++i) {
				CONTROLLERS_TEST_SCOPE;
				FutureRef task = scheduleCountingTask(60000);
				added[static_cast<size_t>(t)].push_back(task);
				npc->getController().addTask(TaskId::DESPAWN, task);
			}
		});
	}
	go = true;
	for (std::thread& worker : workers)
		worker.join();
	int uncancelled = 0;
	for (const std::vector<FutureRef>& list : added)
		for (const FutureRef& task : list)
			uncancelled += task->isCancelled() ? 0 : 1;
	EXPECT_EQ(uncancelled, 1) << "compute cancels every replaced task under the stripe monitor, so exactly the stored one survives";
	CONTROLLERS_TEST_SCOPE;
	FutureRef stored = npc->getController().cancelTask(TaskId::DESPAWN);
	ASSERT_TRUE(stored);
	EXPECT_TRUE(stored->isCancelled());
}

TEST_F(CreatureControllerTest, OnDeleteCancelsTasksAndBreaksObserverAndTargetEdges) {
	int32_t objectId = 0;
	Ref<RemovalObserver> observer;
	std::vector<FutureRef> tasks;
	{
		CONTROLLERS_TEST_SCOPE;
		Ref<ControllersTestNpc> npc = createNpc();
		Ref<ControllersTestNpc> other = createNpc();
		objectId = npc->getObjectId();
		for (TaskId id : {TaskId::DECAY, TaskId::PRISON, TaskId::SKILL_USE}) {
			tasks.push_back(scheduleCountingTask(1000));
			npc->getController().addTask(id, tasks.back());
		}
		observer = RemovalObserver::create();
		npc->getObserveController()->addObserver(*observer);
		npc->breakTarget();
		other->breakTarget();

		npc->getController().onDelete();

		for (const FutureRef& task : tasks)
			EXPECT_TRUE(task->isCancelled());
		EXPECT_FALSE(npc->getController().hasTask(TaskId::DECAY));
		EXPECT_FALSE(npc->getController().hasTask(TaskId::PRISON));
		EXPECT_FALSE(npc->getObserveController()->hasObservers()) << "LogoutBreakers D2";
		EXPECT_EQ(observer->removed, 0) << "the breaker does not notify";
		EXPECT_FALSE(npc->getTarget()) << "LogoutBreakers D1";
		runtime::LeakCensus::getInstance().onRemovedFromWorld(*npc, "Npc", objectId);
		runtime::LeakCensus::getInstance().onRemovedFromWorld(*other, "Npc", other->getObjectId());
	}
	tasks.clear();
	advance(std::chrono::milliseconds(2000));
	EXPECT_EQ(taskRuns, 0);
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(destroyedTestNpcs.load(), 2) << "no task, observer or target keeps a deleted npc alive";
	EXPECT_EQ(runtime::LeakCensus::getInstance().trackedCount(), 0u);
	EXPECT_TRUE(runtime::LeakCensus::getInstance().getLeaks().empty());
}

TEST_F(CreatureControllerTest, OnDespawnCancelsTheDecayTaskAndStopsMovement) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ControllersTestNpc> npc = createNpc();
	// Java: SpawnEngine gives every spawned npc a NpcKnownList; abortMove broadcasts SM_MOVE to the (here empty) sighted players
	npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
	FutureRef decay = scheduleCountingTask(1000);
	FutureRef other = scheduleCountingTask(1000);
	npc->getController().addTask(TaskId::DECAY, decay);
	npc->getController().addTask(TaskId::SHOUT, other);
	ASSERT_TRUE(npc->getMoveController()->moveToPoint(1.0f, 2.0f, 3.0f));

	// CreatureController.onDespawn: actor, cancelTask(DECAY), moveController.abortMove() (resetMove, then SM_MOVE through
	// setAndSendStopMove), aggroList.clear(). The last step is AggroList.clear of P5-01, still unported, so the call ends with its
	// UnportedException after the controller's own steps.
	EXPECT_THROW(npc->recordingController().despawnAsCreature(), runtime::UnportedException);
	EXPECT_TRUE(decay->isCancelled());
	EXPECT_FALSE(npc->getController().hasTask(TaskId::DECAY));
	EXPECT_FALSE(other->isCancelled()) << "only the decay task belongs to the despawn";
	EXPECT_EQ(npc->recordingController().stopMoves, 1) << "abortMove resets a started move (NpcMoveController.resetMove)";
	npc->getController().cancelAllTasks();
	EXPECT_TRUE(other->isCancelled());
}

TEST_F(CreatureControllerTest, MaterialSkillTaskIsRegisteredOnceAndReleasedByAbort) {
	int32_t objectId = 0;
	{
		CONTROLLERS_TEST_SCOPE;
		Ref<ControllersTestNpc> npc = createNpc();
		objectId = npc->getObjectId();
		Ref<CountedMaterialActor> actor = CountedMaterialActor::create(*npc, {materialSkill()});

		actor->act();
		EXPECT_TRUE(npc->getController().hasTask(TaskId::ZONE_MATERIAL_ACTION));
		FutureRef first = npc->getController().getAndRemoveTask(TaskId::ZONE_MATERIAL_ACTION);
		ASSERT_TRUE(first);
		npc->getController().addTask(TaskId::ZONE_MATERIAL_ACTION, first);
		actor->act();
		EXPECT_FALSE(first->isCancelled()) << "act() does nothing while the controller has the task";

		advance(std::chrono::milliseconds(3500)); // the periodic MaterialSkillTask runs: not touched and not spawned, so it applies nothing
		EXPECT_FALSE(first->isCancelled());
		EXPECT_TRUE(first->isPeriodic());

		actor->died(*npc);
		EXPECT_FALSE(actor->touched());
		EXPECT_TRUE(first->isCancelled()) << "abort cancels the fixed-rate task";
		EXPECT_FALSE(npc->getController().hasTask(TaskId::ZONE_MATERIAL_ACTION));
		actor->abort(); // idempotent: the atomic reference is already empty

		actor->act();
		EXPECT_TRUE(npc->getController().hasTask(TaskId::ZONE_MATERIAL_ACTION)) << "after abort the actor can act again";
		actor->abort();
		EXPECT_FALSE(npc->getController().hasTask(TaskId::ZONE_MATERIAL_ACTION));
		EXPECT_EQ(executor->pendingTaskCount(), 0u) << "no periodic MaterialSkillTask is left to keep the actor alive";
		runtime::LeakCensus::getInstance().onRemovedFromWorld(*npc, "Npc", objectId);
	}
	runtime::Reclaimer::getInstance().drain();
	advance(std::chrono::milliseconds(2000));
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(destroyedActors.load(), 1);
	EXPECT_EQ(destroyedTestNpcs.load(), 1);
	EXPECT_TRUE(runtime::LeakCensus::getInstance().getLeaks().empty());
}

} // namespace
} // namespace aion::gameserver::controllers::testing
