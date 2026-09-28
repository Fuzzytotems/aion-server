// Array<T> (Java arrays of shared classes, design §3.2/§3.3 J2) and the java.util.concurrent.atomic shims (§3.2, RR-9).

#include <gtest/gtest.h>

#include <atomic>
#include <climits>
#include <string>
#include <thread>
#include <vector>

#include "sync/SyncTestSupport.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::testsupport;
using namespace std::chrono_literals;

namespace {

class Future final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Future> create(int32_t id) { return makeRef<Future>(id); }
	int32_t id;

protected:
	explicit Future(int32_t id) : id(id) {}
	~Future() override = default;
};

class MapRegion final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<MapRegion> create() { return makeRef<MapRegion>(); }

protected:
	MapRegion() = default;
	~MapRegion() override = default;
};

enum class Stage : uint8_t { START, BOSS, END };

} // namespace

// ------------------------------------------------------------------------------------------------------------------------------ Array

TEST(ArrayTest, LengthDefaultsAndBounds) {
	Ref<Array<int32_t>> empty = Array<int32_t>::make(0);
	EXPECT_EQ(empty->length(), 0);
	EXPECT_THROW((void)(*empty)[0], ArrayIndexOutOfBoundsException);
	EXPECT_THROW((void)Array<int32_t>::make(-1), IllegalArgumentException);

	Ref<Array<int64_t>> values = Array<int64_t>::make(3);
	for (int32_t i = 0; i < values->length(); ++i)
		EXPECT_EQ(values->get(i), 0) << "elements are value-initialized like Java";
	(*values)[1] = 7;
	(*values)[2] += 5;
	EXPECT_EQ(values->get(1), 7);
	EXPECT_EQ(values->get(2), 5);
	try {
		(void)(*values)[3];
		ADD_FAILURE() << "expected ArrayIndexOutOfBoundsException";
	} catch (const ArrayIndexOutOfBoundsException& e) {
		EXPECT_STREQ(e.what(), "Index 3 out of bounds for length 3"); // Java message
	}
	EXPECT_THROW((void)values->get(-1), ArrayIndexOutOfBoundsException);

	Ref<Array<Stage>> stages = Array<Stage>::of({Stage::START, Stage::END});
	EXPECT_EQ(stages->get(1), Stage::END);
	std::vector<Stage> iterated;
	for (Stage stage : *stages)
		iterated.push_back(stage);
	EXPECT_EQ(iterated, (std::vector<Stage>{Stage::START, Stage::END}));
	EXPECT_EQ(stages->snapshot(), iterated);
}

TEST(ArrayTest, ReferenceElementsUseTheReadBarrierAndAreReleasedWithTheArray) {
	Ref<Future> first = Future::create(1);
	Ref<Future> second = Future::create(2);
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		Ref<Array<Ref<Future>>> tasks = Array<Ref<Future>>::make(4);
		EXPECT_FALSE((*tasks)[0]) << "null elements";
		EXPECT_TRUE(TaskScope::isPublished()) << "reading a reference slot is a pointer load";
		(*tasks)[0] = first;
		(*tasks)[3] = second;
		EXPECT_EQ(first->refCount(), 2u);
		std::vector<int32_t> ids;
		for (Ptr<Future> task : *tasks)
			ids.push_back(task ? task->id : -1);
		EXPECT_EQ(ids, (std::vector<int32_t>{1, -1, -1, 2}));
		EXPECT_EQ(tasks->get(3), second);
		Ref<Future> previous = (*tasks)[3].exchange(nullptr);
		EXPECT_EQ(previous, second);
		EXPECT_THROW((void)(*tasks)[1]->id, NullPointerException);

		Ref<Array<Ref<Future>>> literal = Array<Ref<Future>>::of({first, second});
		EXPECT_EQ(first->refCount(), 3u);
		EXPECT_EQ(literal->snapshot().size(), 2u);
	}
	EXPECT_TRUE(Reclaimer::getInstance().drain());
	EXPECT_EQ(first->refCount(), 1u) << "destroying the arrays released their elements";
	EXPECT_EQ(second->refCount(), 1u);

	// sibling pointers and strings
	Ref<MapRegion> region = MapRegion::create();
	Ref<Array<MapRegion*>> neighbours = Array<MapRegion*>::make(2);
	(*neighbours)[0] = region.get();
	EXPECT_EQ(neighbours->get(0), region.get());
	EXPECT_EQ(region->refCount(), 1u) << "sibling pointers never retain";
	TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
	Ref<Array<std::string>> names = Array<std::string>::of({"a", "bc"});
	EXPECT_EQ(names->get(1), "bc");
	(*names)[0] = "changed";
	EXPECT_EQ(names->snapshot(), (std::vector<std::string>{"changed", "bc"}));
}

// ------------------------------------------------------------------------------------------------------------------------------ Atomics

TEST(AtomicTest, NumbersFollowJavaIncludingOverflow) {
	AtomicInteger integer{INT32_MAX};
	EXPECT_EQ(integer.incrementAndGet(), INT32_MIN) << "Java int overflow wraps";
	EXPECT_EQ(integer.getAndDecrement(), INT32_MIN);
	EXPECT_EQ(integer.get(), INT32_MAX);
	integer.set(10);
	EXPECT_EQ(integer.getAndAdd(5), 10);
	EXPECT_EQ(integer.addAndGet(-20), -5);
	EXPECT_EQ(integer.getAndSet(3), -5);
	EXPECT_FALSE(integer.compareAndSet(4, 5));
	EXPECT_TRUE(integer.compareAndSet(3, 4));
	EXPECT_EQ(integer.getAndUpdate([](int32_t v) { return v * 2; }), 4);
	EXPECT_EQ(integer.updateAndGet([](int32_t v) { return v + 1; }), 9);
	EXPECT_EQ(integer.getAndAccumulate(3, [](int32_t a, int32_t b) { return a * b; }), 9);
	EXPECT_EQ(integer.accumulateAndGet(7, [](int32_t a, int32_t b) { return a - b; }), 20);
	EXPECT_EQ(integer.toString(), "20");
	EXPECT_EQ(integer.longValue(), 20);

	AtomicLong along{INT64_MIN};
	EXPECT_EQ(along.decrementAndGet(), INT64_MAX);
	EXPECT_EQ(along.getAndIncrement(), INT64_MAX);
	EXPECT_EQ(along.get(), INT64_MIN);

	AtomicBoolean flag;
	EXPECT_FALSE(flag.getAndSet(true));
	EXPECT_EQ(flag.toString(), "true");
	flag.lazySet(false);
	EXPECT_FALSE(flag.get());
}

TEST(AtomicTest, ConcurrentUpdatesAreAtomic) {
	HangGuard guard(60s);
	AtomicLong counter;
	AtomicInteger updated;
	AtomicReference<int32_t> reference{0};
	AtomicBoolean once;
	std::atomic<int> winners{0};
	AtomicLongArray stages{4};
	std::vector<std::thread> threads;
	for (int t = 0; t < 8; ++t)
		threads.emplace_back([&] {
			if (once.compareAndSet(false, true))
				winners.fetch_add(1);
			for (int i = 0; i < 10000; ++i) {
				counter.incrementAndGet();
				updated.updateAndGet([](int32_t v) { return v + 2; });
				reference.updateAndGet([](int32_t v) { return v + 1; });
				stages.getAndIncrement(i % 4);
			}
		});
	for (std::thread& thread : threads)
		thread.join();
	EXPECT_EQ(winners.load(), 1);
	EXPECT_EQ(counter.get(), 80000);
	EXPECT_EQ(updated.get(), 160000);
	EXPECT_EQ(reference.get(), 80000);
	for (int32_t i = 0; i < 4; ++i)
		EXPECT_EQ(stages.get(i), 20000);
}

TEST(AtomicTest, AtomicReferenceOfValuesAndRefs) {
	AtomicReference<Stage> stage{Stage::START};
	EXPECT_EQ(stage.getAndSet(Stage::BOSS), Stage::START);
	EXPECT_FALSE(stage.compareAndSet(Stage::START, Stage::END));
	EXPECT_TRUE(stage.compareAndSet(Stage::BOSS, Stage::END));
	EXPECT_EQ(stage.get(), Stage::END);

	TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
	Ref<Future> first = Future::create(1);
	AtomicReference<Ref<Future>> task{AION_LOCK_CLASS(Effect::task)};
	EXPECT_FALSE(task.get());
	EXPECT_TRUE(task.compareAndSet(nullptr, first));
	EXPECT_EQ(task.get(), first);
	Ptr<Future> previous = task.getAndUpdate([](Ptr<Future> old) { return Future::create(old->id + 1); });
	EXPECT_EQ(previous, first);
	EXPECT_EQ(task.get()->id, 2);
	Ptr<Future> next = task.updateAndGet([](Ptr<Future> old) { return Future::create(old->id * 10); });
	EXPECT_EQ(next->id, 20);
	Ref<Future> removed = task.getAndSet(nullptr);
	EXPECT_EQ(removed->id, 20);
	EXPECT_EQ(&monitorOf(task).lockClass, &LockClass::named("Effect::task"));
}

TEST(AtomicTest, LongArrayBoundsAndMonitors) {
	AtomicLongArray array{AION_LOCK_CLASS(TheHexwayInstance::stageStartMillis), 6};
	EXPECT_EQ(array.length(), 6);
	array.set(5, 100);
	EXPECT_EQ(array.getAndSet(5, 7), 100);
	EXPECT_TRUE(array.compareAndSet(5, 7, 8));
	EXPECT_EQ(array.addAndGet(5, 2), 10);
	EXPECT_EQ(array.decrementAndGet(5), 9);
	EXPECT_THROW((void)array.get(6), ArrayIndexOutOfBoundsException);
	EXPECT_THROW(array.set(-1, 0), ArrayIndexOutOfBoundsException);
	EXPECT_THROW(AtomicLongArray(-1), IllegalArgumentException);
	SYNCHRONIZED(array) {
		EXPECT_TRUE(array.monitor().isHeldByCurrentThread());
	}
	EXPECT_EQ(&monitorOf(array).lockClass, &LockClass::named("TheHexwayInstance::stageStartMillis"));

	AtomicInteger anonymous;
	EXPECT_EQ(&monitorOf(anonymous).lockClass, &LockClass::ofType(typeid(AtomicInteger))) << "fallback: the shim type";
}
