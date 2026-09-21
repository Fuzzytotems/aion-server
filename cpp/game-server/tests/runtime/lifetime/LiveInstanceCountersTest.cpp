// Debug live-instance counters by dynamic type (LiveInstanceCounters.h, m5a-plan.md I-03/D8): makeRef counts the created dynamic type, the
// Reclaimer counts destructions by the object's dynamic type, a throwing constructor counts nothing, concurrent creation, the snapshot format.

#include <gtest/gtest.h>

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "LifetimeTestSupport.h"
#include "aion/commons/utils/ClassName.h"
#include "aion/gameserver/runtime/lifetime/LiveInstanceCounters.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::lifetimetest;

namespace {

class CountedBase : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<CountedBase> create() { return makeRef<CountedBase>(); }

protected:
	CountedBase() = default;
	~CountedBase() override = default;
};

class CountedDerived final : public CountedBase {
	AION_MAKE_REF_FRIEND

public:
	static Ref<CountedDerived> create() { return makeRef<CountedDerived>(); }

protected:
	CountedDerived() = default;
	~CountedDerived() override = default;
};

class CountedThrowing final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<CountedThrowing> create() { return makeRef<CountedThrowing>(); }

protected:
	CountedThrowing() { throw std::runtime_error("constructor failure"); }
	~CountedThrowing() override = default;
};

class CountedConcurrent final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<CountedConcurrent> create() { return makeRef<CountedConcurrent>(); }

protected:
	CountedConcurrent() = default;
	~CountedConcurrent() override = default;
};

const LiveCount* find(const std::vector<LiveCount>& counts, const std::type_info& type) {
	const std::string name = aion::commons::utils::getClassName(type);
	auto it = std::ranges::find(counts, name, &LiveCount::className);
	return it == counts.end() ? nullptr : &*it;
}

} // namespace

TEST(LiveInstanceCountersTest, CreationAndReclamationByDynamicType) {
	if (!LIVE_COUNTS_ENABLED)
		GTEST_SKIP() << "live-instance counters count in checked builds only";
	drainReclaimer();
	const int64_t baseBefore = liveCountOf(typeid(CountedBase));
	const int64_t derivedBefore = liveCountOf(typeid(CountedDerived));
	{
		Ref<CountedBase> base = CountedBase::create();
		Ref<CountedBase> derived = CountedDerived::create(); // held through the base type: counted and destroyed as CountedDerived
		Ref<CountedDerived> second = CountedDerived::create();
		EXPECT_EQ(liveCountOf(typeid(CountedBase)), baseBefore + 1);
		EXPECT_EQ(liveCountOf(typeid(CountedDerived)), derivedBefore + 2);

		derived.reset();
		EXPECT_EQ(liveCountOf(typeid(CountedDerived)), derivedBefore + 2) << "a released object counts until the Reclaimer destroys it";
		drainReclaimer();
		EXPECT_EQ(liveCountOf(typeid(CountedDerived)), derivedBefore + 1);
		EXPECT_EQ(liveCountOf(typeid(CountedBase)), baseBefore + 1);
	}
	drainReclaimer();
	std::vector<LiveCount> counts = liveCounts();
	const LiveCount* base = find(counts, typeid(CountedBase));
	const LiveCount* derived = find(counts, typeid(CountedDerived));
	ASSERT_NE(base, nullptr);
	ASSERT_NE(derived, nullptr);
	EXPECT_EQ(base->live, baseBefore);
	EXPECT_GE(base->created, 1u);
	EXPECT_EQ(derived->live, derivedBefore);
	EXPECT_GE(derived->created, 2u);
	EXPECT_TRUE(std::ranges::is_sorted(counts, {}, &LiveCount::className));
}

TEST(LiveInstanceCountersTest, AThrowingConstructorCountsNothing) {
	if (!LIVE_COUNTS_ENABLED)
		GTEST_SKIP() << "live-instance counters count in checked builds only";
	EXPECT_THROW(CountedThrowing::create(), std::runtime_error);
	const std::vector<LiveCount> counts = liveCounts();
	EXPECT_EQ(find(counts, typeid(CountedThrowing)), nullptr);
	EXPECT_EQ(liveCountOf(typeid(CountedThrowing)), 0);
}

TEST(LiveInstanceCountersTest, ConcurrentCreationRegistersOnceAndCountsEveryInstance) {
	if (!LIVE_COUNTS_ENABLED)
		GTEST_SKIP() << "live-instance counters count in checked builds only";
	drainReclaimer();
	std::vector<std::thread> threads;
	for (int t = 0; t < 8; t++) {
		threads.emplace_back([] {
			std::vector<Ref<CountedConcurrent>> objects;
			for (int i = 0; i < 500; i++)
				objects.push_back(CountedConcurrent::create());
		});
	}
	for (std::thread& thread : threads)
		thread.join();
	std::vector<LiveCount> counts = liveCounts();
	EXPECT_EQ(std::ranges::count(counts, aion::commons::utils::getClassName(typeid(CountedConcurrent)), &LiveCount::className), 1);
	const LiveCount* concurrent = find(counts, typeid(CountedConcurrent));
	ASSERT_NE(concurrent, nullptr);
	EXPECT_EQ(concurrent->created, 4000u);
	drainReclaimer();
	EXPECT_EQ(liveCountOf(typeid(CountedConcurrent)), 0);
}

// m5a-plan.md D8: the check-output leak check (CheckOutput::checkLiveCounts) names the classes that must be gone once the runtime shut down.
// The matcher behind it: a name matches a whole class name or a trailing part of one at a "::" boundary, a class at 0 is never a leak, and a
// name nothing was created for contributes nothing (so a guard for a class the run never reaches passes instead of failing).
TEST(LiveInstanceCountersTest, LiveInstancesOfMatchesQualifiedNamesAndTheirTrailingParts) {
	const std::vector<LiveCount> counts = {
		{.className = "aion::gameserver::model::gameobjects::Item", .live = 0, .created = 78},
		{.className = "aion::gameserver::model::gameobjects::Npc", .live = 82127, .created = 82133},
		{.className = "aion::gameserver::model::gameobjects::player::Player", .live = 2, .created = 6},
		{.className = "aion::gameserver::skillengine::task::`anonymous namespace'::GatheringTask_ActionObserver", .live = 1, .created = 1},
		{.className = "aion::gameserver::world::knownlist::KnownObject", .live = 3, .created = 330},
	};
	const std::vector<std::string> classes = {"model::gameobjects::player::Player", "model::gameobjects::Item", "world::knownlist::KnownObject",
		"GatheringTask_ActionObserver", "model::gameobjects::Summon"};

	const std::vector<LiveCount> leaks = liveInstancesOf(counts, classes);
	ASSERT_EQ(leaks.size(), 3u);
	EXPECT_EQ(leaks[0].className, "aion::gameserver::model::gameobjects::player::Player") << "sorted by class name";
	EXPECT_EQ(leaks[0].live, 2);
	EXPECT_EQ(leaks[1].className, "aion::gameserver::skillengine::task::`anonymous namespace'::GatheringTask_ActionObserver")
	  << "a bare class name matches past MSVC's anonymous namespace component";
	EXPECT_EQ(leaks[2].className, "aion::gameserver::world::knownlist::KnownObject");
	for (const LiveCount& leak : leaks)
		EXPECT_NE(leak.className, "aion::gameserver::model::gameobjects::Npc") << "the world keeps its npcs at shutdown: Npc is not in the list";
}

TEST(LiveInstanceCountersTest, LiveInstancesOfDoesNotMatchAcrossANameBoundary) {
	const std::vector<LiveCount> counts = {
		{.className = "aion::gameserver::model::gameobjects::player::NotAPlayer", .live = 1, .created = 1},
		{.className = "aion::gameserver::model::gameobjects::PlayerLike::Player", .live = 1, .created = 1},
		{.className = "Player", .live = 1, .created = 1},
	};
	const std::vector<LiveCount> leaks = liveInstancesOf(counts, {"gameobjects::player::Player", "Player"});
	ASSERT_EQ(leaks.size(), 2u);
	EXPECT_EQ(leaks[0].className, "Player") << "an entry equal to the whole class name matches";
	EXPECT_EQ(leaks[1].className, "aion::gameserver::model::gameobjects::PlayerLike::Player");
	for (const LiveCount& leak : leaks)
		EXPECT_NE(leak.className, "aion::gameserver::model::gameobjects::player::NotAPlayer") << "\"Player\" must not match \"NotAPlayer\"";
}

TEST(LiveInstanceCountersTest, LiveInstancesOfReadsTheRealCounters) {
	if (!LIVE_COUNTS_ENABLED)
		GTEST_SKIP() << "live-instance counters count in checked builds only";
	drainReclaimer();
	const std::string name = aion::commons::utils::getClassName(typeid(CountedDerived));
	{
		Ref<CountedDerived> object = CountedDerived::create();
		const std::vector<LiveCount> leaks = liveInstancesOf({name});
		ASSERT_FALSE(leaks.empty());
		EXPECT_EQ(leaks[0].className, name);
		EXPECT_GE(leaks[0].live, 1);
	}
	drainReclaimer();
	EXPECT_TRUE(liveInstancesOf({name}).empty()) << "a class at 0 is not a leak";
	EXPECT_TRUE(liveInstancesOf({"a::class::NobodyEverCreated"}).empty());
}

TEST(LiveInstanceCountersTest, WriteFormat) {
	Ref<CountedDerived> object = CountedDerived::create();
	std::ostringstream out;
	writeLiveCounts(out);
	std::string text = out.str();
	EXPECT_TRUE(text.starts_with("# live instance counts v1\n")) << text;
	if (LIVE_COUNTS_ENABLED) {
		const std::vector<LiveCount> counts = liveCounts();
		const LiveCount* derived = find(counts, typeid(CountedDerived));
		ASSERT_NE(derived, nullptr);
		const std::string line = std::to_string(derived->live) + "\t" + std::to_string(derived->created) + "\t" + derived->className + "\n";
		EXPECT_NE(text.find(line), std::string::npos) << text;
	} else {
		EXPECT_EQ(text, "# live instance counts v1\n");
	}
}
