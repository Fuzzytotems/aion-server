// ConcurrentHashMap, ConcurrentKeySet, CopyOnWriteArrayList/Set, ConcurrentLinkedQueue/Deque shims (design §3.3, §4.1, RR-1, RR-10, RR-17).

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "CollectionsTestSupport.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/runtime/collections/Collections.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::testcollections;

namespace {

class ConcurrentCollectionsTest : public CollectionsTest {};

/** Runs a scenario for the lock-free and the locked-reads (AION_CHM_LOCKED_READS) variant. */
template <template <bool> class Scenario>
void forBothReadModes() {
	Scenario<false>::run();
	Scenario<true>::run();
}

template <bool LockedReads>
struct BasicsScenario {
	static void run() {
		SCOPED_TRACE(LockedReads ? "locked reads" : "lock-free reads");
		ConcurrentHashMap<int32_t, Ref<Npc>, LockedReads> map{AION_LOCK_CLASS(World::allObjects#stripe)};
		EXPECT_TRUE(map.isEmpty());
		EXPECT_FALSE(map.get(1));
		Ref<Npc> a = Npc::create(1);
		Ref<Npc> b = Npc::create(2);
		EXPECT_FALSE(map.put(1, a));
		EXPECT_EQ(map.put(1, b), a);
		EXPECT_EQ(map.get(1), b);
		EXPECT_EQ(map.putIfAbsent(1, a), b);
		EXPECT_FALSE(map.putIfAbsent(2, a));
		EXPECT_TRUE(map.containsKey(2));
		EXPECT_TRUE(map.containsValue(a));
		EXPECT_EQ(map.getOrDefault(3, b), b);
		EXPECT_EQ(map.size(), 2);
		EXPECT_EQ(map.replace(2, b), a);
		EXPECT_FALSE(map.replace(3, b));
		EXPECT_FALSE(map.replace(2, a, a));
		EXPECT_TRUE(map.replace(2, b, a));
		EXPECT_FALSE(map.remove(2, b));
		EXPECT_TRUE(map.remove(2, a));
		EXPECT_EQ(map.remove(1), b);
		EXPECT_TRUE(map.isEmpty());
		EXPECT_THROW(map.put(4, Ref<Npc>()), NullPointerException);

		ConcurrentHashMap<std::string, int32_t, LockedReads> counters;
		for (int i = 0; i < 5; ++i)
			counters.merge("kills", 1, [](int32_t x, int32_t y) { return std::optional<int32_t>(x + y); });
		EXPECT_EQ(counters.get("kills"), 5);
		EXPECT_EQ(counters.computeIfPresent("kills", [](const std::string&, int32_t value) { return value + 1; }), 6);
		EXPECT_FALSE(counters.computeIfPresent("deaths", [](int32_t value) { return value; }).has_value());
		EXPECT_EQ(counters.computeIfAbsent("deaths", [] { return 1; }), 1);
		EXPECT_FALSE(counters.compute("deaths", [](std::optional<int32_t>) { return std::optional<int32_t>(); }).has_value());
		EXPECT_FALSE(counters.containsKey("deaths"));
		counters.clear();
		EXPECT_TRUE(counters.isEmpty());
		EXPECT_EQ(counters.mappingCount(), 0);
	}
};

template <bool LockedReads>
struct ResizeScenario {
	static void run() {
		SCOPED_TRACE(LockedReads ? "locked reads" : "lock-free reads");
		ConcurrentHashMap<int32_t, Ref<Npc>, LockedReads> map;
		constexpr int32_t COUNT = 5000;
		for (int32_t i = 0; i < COUNT; ++i)
			ASSERT_FALSE(map.put(i, Npc::create(i)));
		EXPECT_EQ(map.size(), COUNT);
		for (int32_t i = 0; i < COUNT; ++i) {
			Ptr<Npc> npc = map.get(i);
			ASSERT_TRUE(npc);
			ASSERT_EQ(npc->getObjectId(), i);
		}
		for (int32_t i = 0; i < COUNT; i += 2)
			ASSERT_TRUE(map.remove(i));
		std::set<int32_t> seen;
		for (auto [key, value] : map.entrySet()) {
			EXPECT_EQ(key, value->getObjectId());
			EXPECT_TRUE(seen.insert(key).second) << "duplicate key " << key;
		}
		EXPECT_EQ(static_cast<int32_t>(seen.size()), COUNT / 2);
		int32_t values = 0;
		for (Ptr<Npc> value : map.values())
			values += value ? 1 : 0;
		EXPECT_EQ(values, COUNT / 2);
		EXPECT_EQ(map.keySet().toVector().size(), static_cast<size_t>(COUNT / 2));
		EXPECT_EQ(map.snapshot().size(), static_cast<size_t>(COUNT / 2));
	}
};

template <bool LockedReads>
struct RecursionScenario {
	static void run() {
		SCOPED_TRACE(LockedReads ? "locked reads" : "lock-free reads");
		ConcurrentHashMap<int32_t, int32_t, LockedReads> map{AION_LOCK_CLASS(ConcurrentCollectionsTest::map#stripe)};
		map.put(1, 10);
		EXPECT_THROW(map.compute(1, [&map](std::optional<int32_t> old) {
			map.put(1, 11);
			return old;
		}),
			IllegalStateException);
		EXPECT_THROW(map.computeIfAbsent(2, [&map] { return *map.computeIfAbsent(2, [] { return 1; }); }), IllegalStateException);
		EXPECT_THROW(map.merge(1, 1, [&map](int32_t x, int32_t) {
			(void)map.remove(1);
			return x;
		}),
			IllegalStateException);
		EXPECT_EQ(map.get(1), 10);
		EXPECT_FALSE(map.containsKey(2));
		Monitor& stripe = map.stripeMonitor(1);
		EXPECT_FALSE(stripe.isHeldByCurrentThread());

		// keys of the same stripe: reentrancy, not recursion
		int32_t sameStripe = 2;
		while (&map.stripeMonitor(sameStripe) != &stripe)
			++sameStripe;
		int32_t otherStripe = 2;
		while (&map.stripeMonitor(otherStripe) == &stripe)
			++otherStripe;
		EXPECT_EQ(map.compute(1, [&](const int32_t&, std::optional<int32_t> old) {
			EXPECT_TRUE(stripe.isHeldByCurrentThread());
			EXPECT_EQ(map.get(1), 10); // reads of the computed key see the old mapping
			map.put(sameStripe, 1);
			map.compute(otherStripe, [&](std::optional<int32_t>) { return std::optional<int32_t>(map.get(sameStripe).value_or(0) + 1); });
			for (int32_t i = 1000; i < 1200; ++i)
				map.put(i, i); // resizes tables, including the stripe being computed
			return std::optional<int32_t>(*old + 1);
		}),
			11);
		EXPECT_EQ(map.get(1), 11);
		EXPECT_EQ(map.get(sameStripe), 1);
		EXPECT_EQ(map.get(otherStripe), 2);
		EXPECT_EQ(map.get(1100), 1100);
	}
};

template <bool LockedReads>
struct ViewsScenario {
	static void run() {
		SCOPED_TRACE(LockedReads ? "locked reads" : "lock-free reads");
		ConcurrentHashMap<int32_t, Ref<Player>, LockedReads> map;
		for (int32_t i = 0; i < 20; ++i)
			map.put(i, Player::create(i));
		EXPECT_TRUE(map.keySet().contains(3));
		EXPECT_TRUE(map.values().contains(Player::create(4))); // Java equals on values
		EXPECT_TRUE(map.values().removeIf([](const Ptr<Player>& player) { return player->getObjectId() < 5; }));
		EXPECT_EQ(map.size(), 15);
		auto it = map.keySet().iterator();
		while (it.hasNext())
			if (it.next() % 2 == 0)
				it.remove();
		EXPECT_EQ(map.size(), 8);
		auto entries = map.entrySet().iterator();
		while (entries.hasNext())
			if (entries.next().key == 5)
				entries.remove();
		EXPECT_FALSE(map.containsKey(5));
		EXPECT_TRUE(map.keySet().remove(7));
		EXPECT_TRUE(map.values().remove(Player::create(9)));
		EXPECT_TRUE(map.entrySet().remove(MapEntry<int32_t, Ref<Player>>{11, map.get(11)}));
		EXPECT_TRUE(map.removeIf([](const int32_t& key, const Ptr<Player>&) { return key == 13; }));
		int32_t sum = 0;
		map.forEach([&sum](const int32_t& key, const Ptr<Player>& player) {
			EXPECT_EQ(key, player->getObjectId());
			sum += key;
		});
		EXPECT_EQ(sum, 15 + 17 + 19);
		std::vector<Ptr<Player>> players = map.values();
		EXPECT_EQ(players.size(), 3u);
		map.clear();
		EXPECT_TRUE(map.isEmpty());
		EXPECT_TRUE(map.keySet().begin() == std::default_sentinel);
	}
};

} // namespace

TEST_F(ConcurrentCollectionsTest, ConcurrentHashMapBasics) {
	forBothReadModes<BasicsScenario>();
}

TEST_F(ConcurrentCollectionsTest, ConcurrentHashMapResizeKeepsEveryMapping) {
	int64_t before = liveObjects.load();
	forBothReadModes<ResizeScenario>();
	reclaimAll();
	EXPECT_EQ(liveObjects.load(), before); // tables, retired nodes and their Refs are all released
}

TEST_F(ConcurrentCollectionsTest, ConcurrentHashMapRecursiveUpdateThrowsReentrancyWorks) {
	forBothReadModes<RecursionScenario>();
}

TEST_F(ConcurrentCollectionsTest, ConcurrentHashMapViewsWriteThrough) {
	forBothReadModes<ViewsScenario>();
}

TEST_F(ConcurrentCollectionsTest, ConcurrentHashMapKeysUseJavaEqualsAcrossRelogin) {
	ConcurrentHashMap<Ref<Player>, int32_t> map;
	Ref<Player> before = Player::create(21);
	map.put(before, 1);
	Ref<Player> relogged = Player::create(21);
	EXPECT_EQ(map.get(relogged), 1);
	EXPECT_EQ(map.put(relogged, 2), 1);
	EXPECT_EQ(map.size(), 1);
	EXPECT_EQ(map.keySet().toVector().at(0), before);
	EXPECT_THROW(map.put(Ref<Player>(), 1), NullPointerException);

	ConcurrentKeySet<Ref<Player>> registered{AION_LOCK_CLASS(SiegeService::registered#stripe)};
	EXPECT_TRUE(registered.add(before));
	EXPECT_FALSE(registered.add(relogged));
	EXPECT_TRUE(registered.contains(relogged));
	EXPECT_EQ(registered.size(), 1);
	int32_t count = 0;
	for (Ptr<Player> player : registered)
		count += player->getObjectId() == 21 ? 1 : 0;
	EXPECT_EQ(count, 1);
	EXPECT_EQ(registered.snapshot().size(), 1u);
	EXPECT_TRUE(registered.addAll(std::vector<Ref<Player>>{Player::create(22)}));
	EXPECT_TRUE(registered.removeIf([](const Ptr<Player>& player) { return player->getObjectId() == 22; }));
	auto it = registered.iterator();
	(void)it.next();
	it.remove();
	EXPECT_TRUE(registered.isEmpty());
	EXPECT_FALSE(registered.remove(relogged));
}

TEST_F(ConcurrentCollectionsTest, ConcurrentHashMapBorrowsSurviveConcurrentRemoval) {
	int64_t before = liveObjects.load();
	ConcurrentHashMap<int32_t, Ref<Npc>> map;
	for (int32_t i = 0; i < 64; ++i)
		map.put(i, Npc::create(i));
	Ptr<Npc> borrowed = map.get(7);
	std::vector<Ptr<Npc>> iterated = map.values(); // snapshot of borrows
	std::thread remover([&map] {
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		map.clear();
		for (int32_t i = 0; i < 64; ++i)
			map.put(i, Npc::create(1000 + i)); // replacement values
	});
	remover.join();
	for (int i = 0; i < 4; ++i)
		Reclaimer::getInstance().reclaimNow(); // must not free what this task borrowed
	EXPECT_EQ(borrowed->getObjectId(), 7);
	for (Ptr<Npc> npc : iterated)
		EXPECT_LT(npc->getObjectId(), 64);
	EXPECT_EQ(map.get(7)->getObjectId(), 1007);
	map.clear();
	reclaimAll();
	EXPECT_EQ(liveObjects.load(), before);
}

TEST_F(ConcurrentCollectionsTest, ConcurrentHashMapReadersDoNotWaitForComputeCallbacks) {
	ConcurrentHashMap<int32_t, int32_t> map;
	map.put(1, 1);
	std::atomic<bool> inCallback{false};
	std::atomic<bool> release{false};
	std::thread computer([&] {
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		map.compute(1, [&](std::optional<int32_t> old) {
			inCallback = true;
			for (int i = 0; i < 5000 && !release; ++i) // bounded: the test never hangs
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			return std::optional<int32_t>(*old + 1);
		});
	});
	for (int i = 0; i < 5000 && !inCallback; ++i)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	ASSERT_TRUE(inCallback.load());
	auto start = std::chrono::steady_clock::now();
	EXPECT_EQ(map.get(1), 1); // lock-free: the old mapping, immediately
	EXPECT_EQ(map.values().toVector().size(), 1u);
	EXPECT_FALSE(map.stripeMonitor(1).tryLock());
	EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(1000));
	release = true;
	computer.join();
	EXPECT_EQ(map.get(1), 2);
}

TEST_F(ConcurrentCollectionsTest, CopyOnWriteArrayListFollowsJava) {
	CopyOnWriteArrayList<Ref<Player>> observers{AION_LOCK_CLASS(ObserveController::observers)};
	EXPECT_TRUE(observers.isEmpty());
	Ref<Player> a = Player::create(1);
	Ref<Player> b = Player::create(2);
	EXPECT_TRUE(observers.add(a));
	observers.add(0, b);
	EXPECT_FALSE(observers.addIfAbsent(Player::create(1)));
	EXPECT_EQ(observers.size(), 2);
	EXPECT_EQ(observers.get(0), b);
	EXPECT_THROW(observers.get(2), IndexOutOfBoundsException);
	EXPECT_THROW(observers.add(5, a), IndexOutOfBoundsException);
	EXPECT_TRUE(observers.contains(Player::create(2)));
	EXPECT_EQ(observers.indexOf(a), 1);

	int32_t visited = 0;
	for (Ptr<Player> observer : observers) { // in-place iteration over one immutable array
		observers.add(Player::create(100 + observer->getObjectId()));
		++visited;
	}
	EXPECT_EQ(visited, 2);
	EXPECT_EQ(observers.size(), 4);
	auto it = observers.iterator();
	(void)it.next();
	EXPECT_THROW(it.remove(), UnsupportedOperationException);

	EXPECT_EQ(observers.set(0, a), b);
	EXPECT_EQ(observers.removeAt(0), a);
	EXPECT_TRUE(observers.remove(Player::create(102)));
	EXPECT_TRUE(observers.removeIf([](const Ptr<Player>& player) { return player->getObjectId() == 101; }));
	EXPECT_FALSE(observers.removeIf([](const Ptr<Player>&) { return false; }));
	EXPECT_TRUE(observers.addAll(std::vector<Ref<Player>>{Player::create(7)}));
	int32_t sum = 0;
	observers.forEach([&sum](const Ptr<Player>& player) { sum += player->getObjectId(); });
	EXPECT_EQ(sum, 1 + 7);
	EXPECT_EQ(observers.snapshot().size(), 2u);
	observers.clear();
	EXPECT_TRUE(observers.isEmpty());
	EXPECT_TRUE(observers.begin() == std::default_sentinel);

	CopyOnWriteArraySet<int32_t> pendingAscension;
	EXPECT_TRUE(pendingAscension.add(1));
	EXPECT_FALSE(pendingAscension.add(1));
	EXPECT_TRUE(pendingAscension.contains(1));
	EXPECT_TRUE(pendingAscension.remove(1));
	EXPECT_TRUE(pendingAscension.isEmpty());
}

TEST_F(ConcurrentCollectionsTest, ConcurrentLinkedQueueAndDeque) {
	ConcurrentLinkedQueue<int32_t> enteringWorld{AION_LOCK_CLASS(PlayerEnterWorldService::enteringWorld)};
	EXPECT_FALSE(enteringWorld.poll().has_value());
	EXPECT_THROW(enteringWorld.element(), NoSuchElementException);
	for (int32_t id : {5, 6, 7})
		enteringWorld.add(id);
	EXPECT_TRUE(enteringWorld.contains(6));
	EXPECT_TRUE(enteringWorld.remove(6)); // interior removal
	EXPECT_EQ(enteringWorld.peek(), 5);
	EXPECT_EQ(enteringWorld.poll(), 5);
	EXPECT_EQ(enteringWorld.size(), 1);

	ConcurrentLinkedQueue<Ref<Npc>> traps;
	EXPECT_THROW(traps.offer(Ref<Npc>()), NullPointerException);
	Ref<Npc> trap = Npc::create(3);
	traps.offer(trap);
	EXPECT_EQ(traps.poll(), trap);

	ConcurrentLinkedDeque<int32_t> items;
	items.addLast(2);
	items.addFirst(1);
	EXPECT_EQ(items.peekLast(), 2);
	EXPECT_EQ(items.pollFirst(), 1);
	EXPECT_EQ(items.removeLast(), 2);
	EXPECT_THROW(items.getFirst(), NoSuchElementException);
}

namespace {

/** Value class whose Java equals runs a test hook (a value equals that modifies the map it is compared in). */
class HookedValue final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<HookedValue> create(int32_t id) { return makeRef<HookedValue>(id); }
	static inline std::function<void()> equalsHook;
	bool equals(const HookedValue& other) const {
		if (equalsHook)
			equalsHook();
		return id == other.id;
	}
	int32_t hashCode() const noexcept { return id; }

protected:
	explicit HookedValue(int32_t id) : id(id) { ++liveObjects; }
	~HookedValue() override { --liveObjects; }

private:
	const int32_t id;
};

} // namespace

// Review finding: replace(key, oldValue, newValue) called value equals outside the stripe's busy guard; an equals that removes the key made
// replace store into a stale link and retire the node a second time (double free). It must throw IllegalStateException and keep the map intact.
TEST_F(ConcurrentCollectionsTest, ConcurrentHashMapReplaceRejectsAValueEqualsThatModifiesTheStripe) {
	int64_t before = liveObjects.load();
	{
		ConcurrentHashMap<int32_t, Ref<HookedValue>> map;
		Ref<HookedValue> a = HookedValue::create(1);
		map.put(1, a);
		HookedValue::equalsHook = [&map] { (void)map.remove(1); };
		auto resetHook = finally([]() noexcept { HookedValue::equalsHook = nullptr; });
		Ref<HookedValue> equalToA = HookedValue::create(1); // not identical: Java equals runs
		EXPECT_THROW((void)map.replace(1, equalToA, HookedValue::create(2)), IllegalStateException);
		EXPECT_THROW((void)map.remove(1, equalToA), IllegalStateException);
		HookedValue::equalsHook = nullptr;
		EXPECT_EQ(map.size(), 1);
		EXPECT_EQ(map.get(1), a);
		EXPECT_TRUE(map.replace(1, a, HookedValue::create(3)));
		map.clear();
	}
	reclaimAll();
	EXPECT_EQ(liveObjects.load(), before) << "every node retired exactly once";
}

// Review finding: the in-place iterators of ConcurrentHashMap views and CopyOnWriteArrayList kept raw table/array pointers without a scope
// stamp, so a quiescentPoint() inside the loop (QuiescentScope loops: PeriodicSaveService, spawnAll) was an undetected use-after-free.
TEST_F(ConcurrentCollectionsTest, InPlaceIteratorsThrowAfterAQuiescentPointInCheckedBuilds) {
	if (!CHECKED)
		GTEST_SKIP() << "C1 is a checked-build check";
	ConcurrentHashMap<int32_t, Ref<Npc>> map;
	CopyOnWriteArrayList<Ref<Npc>> list;
	for (int32_t i = 0; i < 8; ++i) {
		map.put(i, Npc::create(i));
		list.add(Npc::create(100 + i));
	}
	QuiescentScope quiescent;
	{
		auto values = map.values();
		auto it = values.begin();
		ASSERT_FALSE(it == std::default_sentinel);
		EXPECT_NO_THROW(++it);
		quiescentPoint(); // the test scope is outermost (depth 1): effective
		EXPECT_THROW(++it, IllegalStateException) << "advancing reads nodes only the old publication protected";
		EXPECT_THROW((void)*it, IllegalStateException);
	}
	int32_t visited = 0;
	EXPECT_THROW(
		{
			for (auto [key, value] : map.entrySet()) {
				(void)key;
				(void)value;
				++visited;
				quiescentPoint();
			}
		},
		IllegalStateException);
	EXPECT_EQ(visited, 1);
	visited = 0;
	EXPECT_THROW(
		{
			for (Ptr<Npc> npc : list) {
				(void)npc;
				++visited;
				quiescentPoint();
			}
		},
		IllegalStateException);
	EXPECT_EQ(visited, 1);
	int32_t fresh = 0;
	for (Ptr<Npc> npc : list) // a new iterator in the new scope id is fine
		fresh += npc ? 1 : 0;
	EXPECT_EQ(fresh, 8);
	map.clear();
	list.clear();
}
