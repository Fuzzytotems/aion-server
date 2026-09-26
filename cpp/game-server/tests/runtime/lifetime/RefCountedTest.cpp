// Conformance of RefCounted (design §2.2, §2.4, C4, C5, C15), makeRef, Immortal and the release protocol's observable effects.

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "LifetimeTestSupport.h"
#include "aion/gameserver/runtime/lifetime/detail/Epoch.h"
#include "aion/gameserver/runtime/lifetime/detail/RefCountedAccess.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::lifetimetest;
using detail::RefCountedAccess;

namespace {

class Plain final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Plain> create() { return makeRef<Plain>(); }

protected:
	Plain() = default;
	~Plain() override = default;
};

/** Constructor that throws after (optionally) creating a nested managed object. */
class Throwing final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static inline std::atomic<int> destroyedBases{0};

protected:
	explicit Throwing(bool nested) {
		if (nested)
			inner = Plain::create();
		throw std::runtime_error("constructor failed");
	}
	~Throwing() override = default;

private:
	Ref<Plain> inner;
};

/** A RefCounted with a RefCounted data member (the member must stay unmanaged). */
class Member : public RefCounted {
public:
	Member() = default;
};

class Container final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Container> create() { return makeRef<Container>(); }
	Member member;
	Ref<Plain> nested = Plain::create(); // makeRef inside a constructor

protected:
	Container() = default;
	~Container() override = default;
};

class TestImmortal final : public Immortal {
public:
	TestImmortal() = default;
	~TestImmortal() = default;
};

} // namespace

TEST(RefCountedTest, MakeRefCreatesManagedObjectWithCountOne) {
	Ref<Plain> object = Plain::create();
	EXPECT_EQ(object->refCount(), 1u);
	EXPECT_TRUE(object->isManaged());
	EXPECT_FALSE(RefCountedAccess::queued(*object).load());
	EXPECT_EQ(RefCountedAccess::retireEpoch(*object).load(), 0u);
}

TEST(RefCountedTest, NonLastReleasesDoNotQueue) {
	drainReclaimer();
	Ref<Plain> object = Plain::create();
	{
		Ref<Plain> copy = object;
		EXPECT_EQ(object->refCount(), 2u);
	}
	EXPECT_EQ(object->refCount(), 1u);
	EXPECT_FALSE(RefCountedAccess::queued(*object).load());
	EXPECT_EQ(Reclaimer::getInstance().stats().backlog, 0u);
}

TEST(RefCountedTest, EveryReleaseStampsTheCurrentEpochMonotonically) {
	Ref<Plain> object = Plain::create();
	Ref<Plain> copy = object;
	uint64_t epoch = Reclaimer::currentEpoch();
	copy.reset(); // 2 -> 1 also stamps (see RefCounted.h: the design's stampless fast path is unsafe under count ABA)
	EXPECT_EQ(RefCountedAccess::retireEpoch(*object).load(), epoch);
	// a lower epoch never lowers the stamp
	RefCountedAccess::retireEpoch(*object).store(epoch + 5);
	copy = object;
	copy.reset();
	EXPECT_EQ(RefCountedAccess::retireEpoch(*object).load(), epoch + 5);
}

TEST(RefCountedTest, LastReleaseQueuesExactlyOnceAndResurrectionIsLegal) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	SharedLocation<Tracked> location;
	location.store(Tracked::create(tracker));
	{
		TaskScope scope(testTask());
		Ptr<Tracked> borrowed = location.load();
		location.store(nullptr); // last release: queued, in this thread's retire list
		EXPECT_EQ(borrowed->refCount(), 0u);
		EXPECT_TRUE(RefCountedAccess::queued(*borrowed).load());
		EXPECT_EQ(detail::unflushedRetireCount(), 1u);

		Ref<Tracked> resurrected(borrowed); // 0 -> 1
		EXPECT_EQ(borrowed->refCount(), 1u);
		resurrected.reset(); // 1 -> 0 again: still queued, not pushed twice
		EXPECT_EQ(detail::unflushedRetireCount(), 1u);
		EXPECT_EQ(tracker->destroyedCount(0), 0u);
	}
	drainReclaimer();
	EXPECT_EQ(tracker->destroyedCount(0), 1u);
}

TEST(RefCountedTest, ScanClearsQueuedOfLiveObjectsAndTheNextLastReleaseRequeues) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	SharedLocation<Tracked> location;
	location.store(Tracked::create(tracker));
	Ref<Tracked> keep;
	{
		TaskScope scope(testTask());
		Ptr<Tracked> borrowed = location.load();
		location.store(nullptr);
		keep = Ref<Tracked>(borrowed);
	}
	Reclaimer::getInstance().reclaimNow(); // count 1: dropped from the queue with queued cleared
	EXPECT_FALSE(RefCountedAccess::queued(*keep).load());
	EXPECT_EQ(Reclaimer::getInstance().stats().backlog, 0u);
	keep.reset(); // pushed again
	EXPECT_EQ(Reclaimer::getInstance().stats().backlog, 1u);
	drainReclaimer();
	EXPECT_EQ(tracker->destroyedCount(0), 1u);
}

TEST(RefCountedTest, ConcurrentRetainReleaseKeepsExactCounts) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	Ref<Tracked> shared = Tracked::create(tracker);
	std::vector<std::thread> threads;
	for (int t = 0; t < 8; ++t) {
		threads.emplace_back([&shared] {
			for (int i = 0; i < 20000; ++i) {
				Ref<Tracked> copy = shared;
				Ref<Tracked> second = copy;
			}
		});
	}
	for (auto& thread : threads)
		thread.join();
	EXPECT_EQ(shared->refCount(), 1u);
	EXPECT_FALSE(RefCountedAccess::queued(*shared).load());
	shared.reset();
	drainReclaimer();
	EXPECT_EQ(tracker->destroyedCount(0), 1u);
}

TEST(RefCountedTest, ThrowingConstructorFreesMemoryWithoutPublishing) {
	drainReclaimer();
	uint64_t destroyedBefore = Reclaimer::getInstance().stats().destroyedTotal;
	EXPECT_THROW((void)makeRef<Throwing>(false), std::runtime_error);
	EXPECT_THROW((void)makeRef<Throwing>(true), std::runtime_error); // the nested object is released by the unwinding member
	EXPECT_EQ(Reclaimer::getInstance().stats().backlog, 1u) << "only the nested Plain is queued";
	drainReclaimer();
	EXPECT_EQ(Reclaimer::getInstance().stats().destroyedTotal, destroyedBefore + 1);
}

TEST(RefCountedTest, NestedMakeRefIsManagedAndDataMembersAreNot) {
	Ref<Container> container = Container::create();
	EXPECT_TRUE(container->isManaged());
	EXPECT_TRUE(container->nested->isManaged());
#if AION_CHECKED
	EXPECT_FALSE(container->member.isManaged());
#endif
}

namespace {

/**
 * Review finding: a constructor that retains and releases `this` (a Pin(this) handed to a task that finishes at once, a PartSlot<RECLAIMER>::set,
 * a temporary Ref) must not be able to take the count to 0 while it runs; a scan during the constructor must not destroy the object.
 */
class SelfReferencing final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<SelfReferencing> create(const std::shared_ptr<Tracker>& tracker) { return makeRef<SelfReferencing>(tracker); }
	uint32_t countInConstructor = 0;
	uint32_t countAfterTemporaryRef = 0;
	bool queuedInConstructor = true;
	bool survivedScans = false;

protected:
	explicit SelfReferencing(const std::shared_ptr<Tracker>& tracker) : tracker_(tracker), id_(tracker->registerObject(this)) {
		countInConstructor = refCount();
		{
			Ref<SelfReferencing> temporary(*this); // released at once, outside any TaskScope: an unsafe 1 -> 0 would push it immediately
		}
		countAfterTemporaryRef = refCount();
		queuedInConstructor = RefCountedAccess::queued(*this).load();
		Reclaimer::getInstance().reclaimNow();
		Reclaimer::getInstance().reclaimNow();
		survivedScans = tracker_->destroyedCount(id_) == 0;
	}
	~SelfReferencing() override { tracker_->destroyed[id_].fetch_add(1); }

private:
	const std::shared_ptr<Tracker> tracker_;
	const uint32_t id_;
};

} // namespace

TEST(RefCountedTest, ConstructorRunsWithCountOneSoTemporaryReferencesToThisAreSafe) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	Ref<SelfReferencing> object = SelfReferencing::create(tracker);
	EXPECT_EQ(object->countInConstructor, 1u) << "makeRef's reference exists while the constructor runs";
	EXPECT_EQ(object->countAfterTemporaryRef, 1u);
	EXPECT_FALSE(object->queuedInConstructor) << "a temporary reference to this must not queue the object";
	EXPECT_TRUE(object->survivedScans) << "the object was destroyed while its constructor ran";
	EXPECT_EQ(object->refCount(), 1u) << "the returned Ref adopts the constructor's count";
	object.reset();
	drainReclaimer();
	EXPECT_EQ(tracker->destroyedCount(0), 1u);
}

TEST(RefCountedTest, ThrowingConstructorMayRetainAndReleaseThis) {
	struct Temporary final : RefCounted {
		Temporary() {
			Ref<Temporary> self(*this);
			self.reset();
			throw std::runtime_error("threw after a temporary reference");
		}
	};
	EXPECT_THROW((void)makeRef<Temporary>(), std::runtime_error);
	drainReclaimer();
}

TEST(RefCountedTest, ImmortalRegistration) {
	auto immortal = std::make_unique<TestImmortal>();
	EXPECT_TRUE(Immortal::isRegistered(immortal.get()));
	const Immortal* address = immortal.get();
	immortal.reset();
#if AION_CHECKED
	EXPECT_FALSE(Immortal::isRegistered(address));
#else
	(void)address;
#endif
}

TEST(RefCountedTest, MonitorIsPerObject) {
	Ref<Plain> first = Plain::create();
	Ref<Plain> second = Plain::create();
	EXPECT_NE(&first->monitor(), &second->monitor());
	SYNCHRONIZED(*first) {
		EXPECT_TRUE(first->monitor().isHeldByCurrentThread());
		EXPECT_FALSE(second->monitor().isHeldByCurrentThread());
	}
}

#if AION_CHECKED
TEST(RefCountedDeathTest, RetainOfStackObjectTerminatesWithClassName) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	EXPECT_DEATH(
		{
			Member onStack;
			onStack.retain();
		},
		"C15.*Member");
}

TEST(RefCountedDeathTest, ReleaseUnderflowTerminates) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	EXPECT_DEATH(
		{
			Ref<Plain> object = Plain::create();
			Plain* raw = object.leak();
			raw->release(); // 1 -> 0, queued
			raw->release(); // underflow
		},
		"C4");
}

TEST(RefCountedDeathTest, RetainAfterDestructionTerminates) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	// checked builds keep destroyed objects poisoned with a DEAD cookie in the delayed-free FIFO (C3); ASan builds report the use after free
	EXPECT_DEATH(
		{
			Reclaimer::getInstance().drain(128);
			Ref<Plain> object = Plain::create();
			Plain* raw = object.get();
			object.reset();
			Reclaimer::getInstance().drain(128);
			raw->retain();
		},
		"");
}

TEST(RefCountedDeathTest, DestroyingAReferencedObjectOutsideTheReclaimerTerminates) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	struct Publishing : RefCounted {
		Publishing() {
			publish(*this);
			throw std::runtime_error("published this, then threw");
		}
		static void publish(const RefCounted& self) {
			self.retain(); // a reference that outlives the throwing constructor
		}
	};
	struct Factory {
		static void make() { (void)makeRef<Publishing>(); }
	};
	EXPECT_DEATH(Factory::make(), "C5");
}

#endif
