// HashMap, LinkedHashMap, TreeMap, EnumMap, HashSet, LinkedHashSet, TreeSet shims (design §3.3, RR-1, RR-7, RR-8).

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "CollectionsTestSupport.h"
#include "aion/gameserver/runtime/collections/Collections.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::testcollections;

namespace {

class MapSetTest : public CollectionsTest {};

enum class StatEnum { MAXHP, SPEED, ATTACK };

template <class Map>
std::vector<typename Map::key_type> keysOf(const Map& map) {
	std::vector<typename Map::key_type> keys;
	for (const auto& key : map.keySet())
		keys.push_back(key);
	return keys;
}

} // namespace

TEST_F(MapSetTest, HashMapBasicsFollowJava) {
	HashMap<std::string, int32_t> map{AION_LOCK_CLASS(MapSetTest::map)};
	EXPECT_TRUE(map.isEmpty());
	EXPECT_FALSE(map.put("a", 1).has_value());
	EXPECT_EQ(map.put("a", 2), 2 - 1);
	EXPECT_EQ(map.get("a"), 2);
	EXPECT_FALSE(map.get("b").has_value());
	EXPECT_EQ(map.getOrDefault("b", 7), 7);
	EXPECT_EQ(map.putIfAbsent("a", 9), 2);
	EXPECT_FALSE(map.putIfAbsent("b", 3).has_value());
	EXPECT_TRUE(map.containsKey("b"));
	EXPECT_TRUE(map.containsValue(3));
	EXPECT_FALSE(map.containsValue(4));
	EXPECT_EQ(map.size(), 2);
	EXPECT_EQ(map.replace("b", 4), 3);
	EXPECT_FALSE(map.replace("c", 1).has_value());
	EXPECT_FALSE(map.replace("b", 3, 5));
	EXPECT_TRUE(map.replace("b", 4, 5));
	EXPECT_FALSE(map.remove("b", 4));
	EXPECT_TRUE(map.remove("b", 5));
	EXPECT_EQ(map.remove("a"), 2);
	EXPECT_FALSE(map.remove("a").has_value());
	EXPECT_TRUE(map.isEmpty());

	HashMap<int32_t, Ref<Npc>> npcs;
	EXPECT_FALSE(npcs.put(1, Ref<Npc>())); // Java HashMap allows null values
	EXPECT_TRUE(npcs.containsKey(1));
	EXPECT_FALSE(npcs.get(1));
	Ref<Npc> npc = Npc::create(1);
	EXPECT_FALSE(npcs.putIfAbsent(1, npc)); // a null mapping counts as absent
	EXPECT_EQ(npcs.get(1), npc);
	std::map<int32_t, Ref<Npc>> source{{2, Npc::create(2)}};
	npcs.putAll(source);
	HashMap<int32_t, Ref<Npc>> copy;
	copy.putAll(npcs);
	EXPECT_EQ(copy.size(), 2);
	copy.clear();
	EXPECT_TRUE(copy.isEmpty());
}

TEST_F(MapSetTest, KeysUseJavaEqualsAcrossRelogin) {
	HashMap<Ref<Player>, int32_t> rewards{AION_LOCK_CLASS(SiegeService::rewards)};
	Ref<Player> before = Player::create(11);
	rewards.put(before, 1);
	Ref<Player> relogged = Player::create(11);
	EXPECT_EQ(rewards.get(relogged), 1);
	EXPECT_EQ(rewards.put(relogged, 2), 1);
	std::vector<Ptr<Player>> keys = rewards.keySet();
	ASSERT_EQ(keys.size(), 1u);
	EXPECT_EQ(keys[0], before); // Java keeps the original key instance
	EXPECT_EQ(rewards.remove(relogged), 2);

	HashSet<Ref<Player>> looters;
	EXPECT_TRUE(looters.add(before));
	EXPECT_FALSE(looters.add(relogged));
	EXPECT_TRUE(looters.contains(relogged));
	EXPECT_TRUE(looters.remove(relogged));
	EXPECT_TRUE(looters.isEmpty());

	HashSet<Ref<Npc>> identity;
	EXPECT_TRUE(identity.add(Npc::create(1)));
	EXPECT_TRUE(identity.add(Npc::create(1)));
	EXPECT_EQ(identity.size(), 2);
}

TEST_F(MapSetTest, LinkedHashMapKeepsInsertionOrderThroughCompaction) {
	LinkedHashMap<int32_t, int32_t> map;
	for (int32_t i = 0; i < 100; ++i)
		map.put(i, i);
	map.put(5, 50); // re-put keeps the position
	for (int32_t i = 0; i < 100; i += 3)
		(void)map.remove(i); // triggers hole compaction
	map.put(0, 0);   // re-added keys go to the end
	std::vector<int32_t> keys = keysOf(map);
	std::vector<int32_t> expected;
	for (int32_t i = 0; i < 100; ++i)
		if (i % 3 != 0)
			expected.push_back(i);
	expected.push_back(0);
	EXPECT_EQ(keys, expected);
	EXPECT_EQ(map.get(5), 50);
	for (int32_t key : expected)
		EXPECT_TRUE(map.containsKey(key));
}

TEST_F(MapSetTest, TreeMapNavigation) {
	TreeMap<int64_t, std::string> map{AION_LOCK_CLASS(Equipment::equipment)};
	EXPECT_THROW(map.firstKey(), NoSuchElementException);
	EXPECT_FALSE(map.firstEntry().has_value());
	for (int64_t key : {40, 10, 30, 20})
		map.put(key, std::to_string(key));
	EXPECT_EQ(keysOf(map), (std::vector<int64_t>{10, 20, 30, 40}));
	EXPECT_EQ(map.firstKey(), 10);
	EXPECT_EQ(map.lastKey(), 40);
	EXPECT_EQ(map.floorKey(25), 20);
	EXPECT_EQ(map.floorKey(20), 20);
	EXPECT_FALSE(map.floorKey(5).has_value());
	EXPECT_EQ(map.ceilingKey(25), 30);
	EXPECT_FALSE(map.ceilingKey(41).has_value());
	EXPECT_EQ(map.lowerKey(20), 10);
	EXPECT_EQ(map.higherKey(20), 30);
	EXPECT_EQ(map.floorEntry(35)->value, "30");
	EXPECT_EQ(map.higherEntry(30)->key, 40);
	EXPECT_FALSE(map.lowerEntry(10).has_value());
	EXPECT_EQ(map.headMap(30).size(), 2u);
	EXPECT_EQ(map.headMap(30, true).size(), 3u);
	EXPECT_EQ(map.tailMap(30).size(), 2u);
	EXPECT_EQ(map.tailMap(30, false).size(), 1u);
	EXPECT_EQ(map.descendingMap().front().key, 40);
	EXPECT_EQ(map.pollFirstEntry()->key, 10);
	EXPECT_EQ(map.pollLastEntry()->value, "40");
	EXPECT_EQ(map.size(), 2);

	TreeMap<Ref<Player>, int32_t> byPlayer([](const Ptr<Player>& a, const Ptr<Player>& b) { return b->getObjectId() - a->getObjectId(); });
	byPlayer.put(Player::create(1), 1);
	byPlayer.put(Player::create(3), 3);
	byPlayer.put(Player::create(2), 2);
	EXPECT_EQ(byPlayer.firstKey()->getObjectId(), 3);
	EXPECT_EQ(byPlayer.get(Player::create(2)), 2); // comparator equality

	TreeMap<Ref<Npc>, int32_t> unordered; // no compareTo and no comparator: ClassCastException like Java
	EXPECT_THROW(unordered.put(Npc::create(1), 1), ClassCastException);
}

TEST_F(MapSetTest, EnumMapIteratesInOrdinalOrder) {
	EnumMap<StatEnum, int32_t> stats{AION_LOCK_CLASS(CreatureGameStats::stats)};
	stats.put(StatEnum::ATTACK, 3);
	stats.put(StatEnum::MAXHP, 1);
	stats.put(StatEnum::SPEED, 2);
	std::vector<int32_t> values = stats.values();
	EXPECT_EQ(values, (std::vector<int32_t>{1, 2, 3}));
}

TEST_F(MapSetTest, ComputeFamilyShapesAndNullResults) {
	HashMap<int32_t, Ref<Npc>> map;
	Ref<Npc> npc = Npc::create(1);
	EXPECT_EQ(map.computeIfAbsent(1, [&](const int32_t& key) {
		EXPECT_EQ(key, 1);
		return npc;
	}),
		npc);
	EXPECT_EQ(map.computeIfAbsent(1, []() -> Ref<Npc> {
		ADD_FAILURE() << "present: no call";
		return nullptr;
	}),
		npc);
	EXPECT_FALSE(map.computeIfAbsent(2, [] { return Ref<Npc>(); })); // null result: no mapping
	EXPECT_FALSE(map.containsKey(2));
	EXPECT_EQ(map.compute(1, [](Ptr<Npc> old) { return old; }), npc); // single-argument form, Ptr result
	EXPECT_FALSE(map.compute(1, [](const int32_t&, Ptr<Npc>) -> Ref<Npc> { return nullptr; }));
	EXPECT_FALSE(map.containsKey(1));
	EXPECT_FALSE(map.computeIfPresent(1, [](Ptr<Npc> value) { return value; }));
	map.put(1, npc);
	Ref<Npc> replacement = Npc::create(2);
	EXPECT_EQ(map.computeIfPresent(1, [&](const int32_t&, const Ptr<Npc>&) { return replacement; }), replacement);
	EXPECT_EQ(map.merge(1, npc, [](Ptr<Npc> old, Ptr<Npc> given) { return old->getObjectId() > given->getObjectId() ? old : given; }), replacement);
	EXPECT_EQ(map.merge(3, npc, [](Ptr<Npc>, Ptr<Npc>) -> Ref<Npc> { return nullptr; }), npc); // absent: stores the value
	EXPECT_FALSE(map.merge(3, npc, [](Ptr<Npc>, Ptr<Npc>) -> Ref<Npc> { return nullptr; }));  // null result removes
	EXPECT_THROW(map.merge(4, Ref<Npc>(), [](Ptr<Npc>, Ptr<Npc> given) { return given; }), NullPointerException);

	HashMap<std::string, int32_t> counters;
	for (int i = 0; i < 3; ++i)
		counters.merge("kills", 1, [](int32_t a, int32_t b) { return a + b; }); // plain value result
	EXPECT_EQ(counters.get("kills"), 3);
	EXPECT_EQ(counters.compute("kills", [](const std::string&, std::optional<int32_t> old) { return old ? std::optional<int32_t>(*old * 2) : std::nullopt; }),
		6);
	EXPECT_FALSE(counters.compute("kills", [](std::optional<int32_t>) { return std::optional<int32_t>(); }).has_value());
	EXPECT_TRUE(counters.isEmpty());
}

TEST_F(MapSetTest, RecursiveUpdateOfTheSameKeyThrowsOtherKeysWork) {
	Monitor other{AION_LOCK_CLASS(MapSetTest::other)};
	HashMap<int32_t, int32_t> map{AION_LOCK_CLASS(MapSetTest::recursive)};
	map.put(1, 10);
	EXPECT_THROW(map.compute(1, [&map](std::optional<int32_t> old) {
		map.put(1, 99);
		return old;
	}),
		IllegalStateException);
	EXPECT_EQ(map.get(1), 10);
	EXPECT_FALSE(map.monitor().isHeldByCurrentThread()); // released by the exception

	try {
		map.computeIfAbsent(2, [&map]() {
			map.computeIfAbsent(2, [] { return 1; });
			return 2;
		});
		FAIL() << "expected IllegalStateException";
	} catch (const IllegalStateException& e) {
		EXPECT_STREQ(e.what(), "Recursive update");
	}

	EXPECT_EQ(map.compute(1, [&](const int32_t& key, std::optional<int32_t> old) {
		EXPECT_EQ(map.get(1), 10); // reads of the key see the old mapping
		map.put(2, 20);            // other keys: allowed (reentrant Monitor)
		map.compute(3, [&](std::optional<int32_t>) {
			SYNCHRONIZED(other) { // callbacks may take other Monitors
				SYNCHRONIZED(map) { // and the map's own Monitor (reentrant)
					return std::optional<int32_t>(30);
				}
			}
		});
		map.remove(4);
		return std::optional<int32_t>(*old + key);
	}),
		11);
	EXPECT_EQ(map.get(2), 20);
	EXPECT_EQ(map.get(3), 30);

	TreeMap<int32_t, int32_t> sorted;
	sorted.put(5, 5);
	EXPECT_THROW(sorted.computeIfPresent(5, [&sorted](int32_t value) {
		(void)sorted.remove(5);
		return value;
	}),
		IllegalStateException);
	EXPECT_EQ(sorted.get(5), 5);
}

TEST_F(MapSetTest, ComparatorsAndKeyEqualsMustNotModifyTheMap) {
	TreeMap<int32_t, int32_t>* self = nullptr;
	TreeMap<int32_t, int32_t> map([&self](const int32_t& a, const int32_t& b) {
		if (a == 99 || b == 99)
			self->put(100, 100);
		return a - b;
	});
	self = &map;
	map.put(1, 1);
	EXPECT_THROW(map.put(99, 99), IllegalStateException);
	EXPECT_EQ(map.size(), 1);
}

TEST_F(MapSetTest, ViewsWriteThrough) {
	HashMap<int32_t, std::string> map;
	for (int32_t i = 0; i < 6; ++i)
		map.put(i, i % 2 == 0 ? "even" : "odd");

	auto keysIt = map.keySet().iterator();
	while (keysIt.hasNext())
		if (keysIt.next() == 0)
			keysIt.remove();
	EXPECT_FALSE(map.containsKey(0));

	auto valuesIt = map.values().iterator(); // duplicate values: removal is by the iterated entry's key
	int32_t index = 0;
	while (valuesIt.hasNext()) {
		(void)valuesIt.next();
		if (index++ == 1)
			valuesIt.remove();
	}
	EXPECT_FALSE(map.containsKey(2));
	EXPECT_EQ(map.size(), 4);

	EXPECT_TRUE(map.values().removeIf([](const std::string& value) { return value == "odd"; }));
	EXPECT_EQ(keysOf(map), (std::vector<int32_t>{4}));

	map.put(7, "x");
	for (auto [key, value] : map.entrySet())
		EXPECT_EQ(map.get(key), value);
	auto entryIt = map.entrySet().iterator();
	while (entryIt.hasNext())
		if (entryIt.next().getKey() == 7)
			entryIt.remove();
	EXPECT_FALSE(map.containsKey(7));
	EXPECT_TRUE(map.values().remove("even"));
	EXPECT_TRUE(map.isEmpty());

	// an entry changed after the snapshot is not removed by its stale iterator
	map.put(1, "a");
	auto staleIt = map.entrySet().iterator();
	(void)staleIt.next();
	map.put(1, "b");
	staleIt.remove();
	EXPECT_EQ(map.get(1), "b");
}

TEST_F(MapSetTest, RemoveIfReplaceAllAndForEach) {
	HashMap<int32_t, int32_t> map;
	for (int32_t i = 0; i < 10; ++i)
		map.put(i, i * i);
	EXPECT_TRUE(map.removeIf([&map](const int32_t& key, const int32_t&) {
		if (key == 1)
			map.put(100, 1); // reentrant insertion from the predicate
		return key % 2 == 1;
	}));
	EXPECT_EQ(map.size(), 6);
	map.replaceAll([](const int32_t& key, const int32_t& value) { return key == 100 ? value : -value; });
	int64_t sum = 0;
	map.forEach([&sum](const int32_t&, const int32_t& value) { sum += value; });
	EXPECT_EQ(sum, 1 - (0 + 4 + 16 + 36 + 64));
}

TEST_F(MapSetTest, SetsFollowJava) {
	LinkedHashSet<int32_t> ordered;
	for (int32_t value : {3, 1, 2, 1})
		ordered.add(value);
	EXPECT_EQ(ordered.snapshot(), (std::vector<int32_t>{3, 1, 2}));
	EXPECT_TRUE(ordered.containsAll(std::vector<int32_t>{1, 2}));
	EXPECT_FALSE(ordered.containsAll(std::vector<int32_t>{1, 4}));
	EXPECT_TRUE(ordered.addAll(std::vector<int32_t>{4, 3}));
	EXPECT_TRUE(ordered.removeAll(std::vector<int32_t>{3, 9}));
	EXPECT_TRUE(ordered.retainAll(std::vector<int32_t>{1, 4}));
	EXPECT_EQ(ordered.snapshot(), (std::vector<int32_t>{1, 4}));
	EXPECT_TRUE(ordered.removeIf([](int32_t value) { return value == 4; }));
	std::vector<int32_t> visited;
	ordered.forEach([&visited](int32_t value) { visited.push_back(value); });
	EXPECT_EQ(visited, (std::vector<int32_t>{1}));

	Ref<Player> player = Player::create(3);
	HashSet<Ref<Player>> players;
	players.add(player);
	players.add(Player::create(4));
	auto it = players.iterator();
	while (it.hasNext()) {
		Ptr<Player> next = it.next();
		if (next->getObjectId() == 3) {
			players.remove(next);
			players.add(Player::create(3)); // Java-equal replacement added meanwhile
			it.remove();                    // identity: the replacement stays
		}
	}
	EXPECT_TRUE(players.contains(player));
	EXPECT_EQ(players.size(), 2);

	TreeSet<int32_t> sorted;
	EXPECT_THROW(sorted.first(), NoSuchElementException);
	for (int32_t value : {50, 10, 30})
		sorted.add(value);
	EXPECT_EQ(sorted.first(), 10);
	EXPECT_EQ(sorted.last(), 50);
	EXPECT_EQ(sorted.floor(35), 30);
	EXPECT_EQ(sorted.lower(30), 10);
	EXPECT_EQ(sorted.ceiling(30), 30);
	EXPECT_EQ(sorted.higher(30), 50);
	EXPECT_FALSE(sorted.higher(50).has_value());
	EXPECT_EQ(sorted.descendingSet(), (std::vector<int32_t>{50, 30, 10}));
	EXPECT_EQ(sorted.pollFirst(), 10);
	EXPECT_EQ(sorted.pollLast(), 50);
	EXPECT_EQ(sorted.size(), 1);

	TreeSet<Ref<Player>> byId; // natural ordering via compareTo
	byId.add(Player::create(2));
	byId.add(Player::create(1));
	EXPECT_FALSE(byId.add(Player::create(2)));
	EXPECT_EQ(byId.first()->getObjectId(), 1);
}
