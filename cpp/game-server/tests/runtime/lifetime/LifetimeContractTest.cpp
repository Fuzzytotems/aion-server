// Contract test of aion_gs_runtime_lifetime (headers stage): the public API shape. Protocol conformance, PCT and mutation tests live in the
// other files of this directory.

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Pct.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "support/ContractSupport.h"

using namespace aion::gameserver::runtime;

namespace {

int destroyedObjects = 0;

class VisibleObject : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<VisibleObject> create(int32_t objectId) { return makeRef<VisibleObject>(objectId); }
	int32_t getObjectId() const noexcept { return objectId; }
	bool equals(const VisibleObject& other) const noexcept { return objectId == other.objectId; }
	int32_t hashCode() const noexcept { return objectId; }

protected:
	explicit VisibleObject(int32_t objectId) : objectId(objectId) {}
	~VisibleObject() override { ++destroyedObjects; }

private:
	const int32_t objectId;
};

class Npc final : public VisibleObject {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Npc> create(int32_t objectId) { return makeRef<Npc>(objectId); }

protected:
	using VisibleObject::VisibleObject;
	~Npc() override = default;
};

/** late-bound controller part (design §3.2.1 pattern 3) */
class NpcController final : public OwnedPart {
public:
	NpcController() = default;
	void setOwner(const RefCounted& owner) { bindOwner(owner); }
};

/** replaceable AI part (PartSlot OWNER) and storage parts (PartSlot RECLAIMER, PartMap) */
class AbstractAI final : public OwnedPart {
public:
	explicit AbstractAI(const RefCounted& owner, int32_t kind) : OwnedPart(owner), kind(kind) {}
	const int32_t kind;
};

class Creature final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Creature> create() { return makeRef<Creature>(); }

	const std::unique_ptr<NpcController> controller = std::make_unique<NpcController>();
	PartSlot<AbstractAI> ai{*this};
	PartSlot<AbstractAI, RetireTo::RECLAIMER> warehouse{*this};
	PartMap<int32_t, AbstractAI> accountData{*this};
	PartList<AbstractAI> templates{*this};
	SelfOrRef<Creature> target{static_cast<const RefCounted&>(*this)};

protected:
	Creature() { controller->setOwner(*this); }
	~Creature() override = default;
};

TaskInfo testTask() {
	return AION_TASK_INFO(TaskKind::TEST);
}

} // namespace

TEST(LifetimeContractTest, MakeRefRefAndPtrBasics) {
	TaskScope scope(testTask());
	Ref<Npc> npc = Npc::create(7);
	EXPECT_EQ(npc->refCount(), 1u);
	EXPECT_TRUE(npc->isManaged());
	Ref<VisibleObject> asVisible = npc; // converting copy retains
	EXPECT_EQ(npc->refCount(), 2u);
	Ptr<VisibleObject> borrowed = asVisible;
	EXPECT_EQ(borrowed->getObjectId(), 7);
	EXPECT_TRUE(borrowed == npc);
	Ref<VisibleObject> fromBorrow = borrowed; // Ref(Ptr) needs no barrier
	EXPECT_EQ(npc->refCount(), 3u);

	Ptr<VisibleObject> none;
	EXPECT_THROW((void)none->getObjectId(), NullPointerException);
	Ref<Npc> nullRef;
	EXPECT_THROW((void)nullRef->getObjectId(), NullPointerException);
}

TEST(LifetimeContractTest, CastAndAs) {
	TaskScope scope(testTask());
	Ref<VisibleObject> plain = VisibleObject::create(1);
	Ref<VisibleObject> npc = Npc::create(2);
	EXPECT_TRUE(static_cast<bool>(cast<Npc>(npc)));
	EXPECT_THROW((void)cast<Npc>(plain), ClassCastException);
	EXPECT_FALSE(static_cast<bool>(as<Npc>(plain)));
	EXPECT_FALSE(static_cast<bool>(cast<Npc>(Ptr<VisibleObject>())));
}

TEST(LifetimeContractTest, LastReleaseDefersDestructionUntilScopeEnds) {
	Reclaimer::getInstance().drain(); // objects of earlier tests
	destroyedObjects = 0;
	{
		TaskScope scope(testTask());
		TaskScope::ensurePublished();
		EXPECT_TRUE(TaskScope::isPublished());
		Ref<Npc> npc = Npc::create(3);
		Ptr<Npc> borrow = npc;
		npc.reset(); // last release: stamped, queued
		Reclaimer::getInstance().reclaimNow();
		EXPECT_EQ(destroyedObjects, 0) << "a published scope that could have borrowed the object is still active";
		EXPECT_EQ(borrow->getObjectId(), 3);
	}
	Reclaimer::getInstance().drain();
	EXPECT_EQ(destroyedObjects, 1);
}

TEST(LifetimeContractTest, ScopesAndQuiescentPoints) {
	EXPECT_FALSE(TaskScope::active());
	TaskScope outer(testTask());
	uint64_t id = TaskScope::currentScopeId();
	EXPECT_NE(id, 0u);
	{
		TaskScope nested(testTask());
		EXPECT_EQ(TaskScope::depth(), 2u);
		EXPECT_EQ(TaskScope::currentScopeId(), id);
		quiescentPoint(); // no-op: no QuiescentScope, depth 2
		EXPECT_EQ(TaskScope::currentScopeId(), id);
	}
	QuiescentScope quiescent;
	TaskScope::ensurePublished();
	quiescentPoint();
	EXPECT_NE(TaskScope::currentScopeId(), id);
	EXPECT_FALSE(TaskScope::isPublished());
}

TEST(LifetimeContractTest, StaleBorrowThrowsInCheckedBuilds) {
	Ref<Npc> npc = Npc::create(4);
	Ptr<Npc> escaped;
	{
		TaskScope scope(testTask());
		escaped = npc;
	}
	TaskScope later(testTask());
#if AION_CHECKED
	EXPECT_THROW((void)escaped->getObjectId(), IllegalStateException);
#else
	EXPECT_EQ(escaped->getObjectId(), 4);
#endif
}

TEST(LifetimeContractTest, PartsShareTheOwnersLifetime) {
	TaskScope scope(testTask());
	Ref<Creature> creature = Creature::create();
	EXPECT_EQ(&creature->controller->partOwner(), creature.get());

	Ref<NpcController> controllerRef(creature->controller.get()); // a Ref to a part retains the owner
	EXPECT_EQ(creature->refCount(), 2u);
	controllerRef.reset();

	creature->ai.set(std::make_unique<AbstractAI>(*creature, 1));
	creature->ai.set(std::make_unique<AbstractAI>(*creature, 2)); // old AI kept until the owner dies
	EXPECT_EQ(creature->ai->kind, 2);

	creature->warehouse.set(std::make_unique<AbstractAI>(*creature, 3));
	creature->warehouse.set(std::make_unique<AbstractAI>(*creature, 4)); // old part retired to the Reclaimer
	EXPECT_EQ(creature->warehouse->kind, 4);

	creature->accountData.put(1, std::make_unique<AbstractAI>(*creature, 5));
	EXPECT_EQ(creature->accountData.get(1)->kind, 5);
	EXPECT_TRUE(creature->accountData.remove(1));
	EXPECT_FALSE(static_cast<bool>(creature->accountData.get(1)));

	creature->templates.add(std::make_unique<AbstractAI>(*creature, 6));
	EXPECT_EQ(creature->templates.get(0)->kind, 6);

	uint32_t countBefore = creature->refCount(); // includes the owner reference held by the retired warehouse part
	creature->target = Ptr<Creature>(*creature); // self: non-retaining
	EXPECT_EQ(creature->refCount(), countBefore);
	Ref<Creature> other = Creature::create();
	creature->target = Ptr<Creature>(other); // foreign: retaining
	EXPECT_EQ(other->refCount(), 2u);
	creature->target = nullptr;
	EXPECT_EQ(other->refCount(), 1u);
}

TEST(LifetimeContractTest, ReclaimerStatsAndHooks) {
	Reclaimer& reclaimer = Reclaimer::getInstance();
	int hookRuns = 0;
	uint64_t hook = reclaimer.addPostScanHook("contract", [&] { ++hookRuns; });
	reclaimer.reclaimNow();
	reclaimer.removePostScanHook(hook);
	EXPECT_EQ(hookRuns, 1);
	Reclaimer::Stats stats = reclaimer.stats();
	EXPECT_GE(stats.epoch, 1u);
	EXPECT_FALSE(Reclaimer::inDestructorContext());
}

TEST(LifetimeContractTest, PctSchedulerApiShape) {
	pct::ScheduleResult result = pct::explore(
		pct::schedulesFromEnvironment(10), 1,
		[](uint64_t) {
			auto counter = std::make_shared<std::atomic<int>>(0);
			return std::vector<std::function<void()>>{[counter] { counter->fetch_add(1); }, [counter] { counter->fetch_add(1); }};
		});
	EXPECT_TRUE(result.completed);
}

#if AION_CHECKED
TEST(LifetimeContractDeathTest, RetainOfUnmanagedObjectTerminates) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	struct StackObject : RefCounted {};
	// C15: a RefCounted that was not created by makeRef (only possible through a derived class that exposes its constructor)
	EXPECT_DEATH(
		{
			StackObject* object = new StackObject();
			object->retain();
		},
		"C15");
}
#endif
