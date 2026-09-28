// ArrayList, LinkedList, ArrayDeque, PriorityQueue shims (design §3.3, RR-7, RR-8).

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "CollectionsTestSupport.h"
#include "aion/gameserver/runtime/collections/Collections.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::testcollections;

namespace {

std::vector<int32_t> contents(const ArrayList<int32_t>& list) {
	return list.snapshot();
}

class ArrayListTest : public CollectionsTest {};

} // namespace

TEST_F(ArrayListTest, BasicOperationsFollowJava) {
	ArrayList<int32_t> list{AION_LOCK_CLASS(ArrayListTest::list)};
	EXPECT_TRUE(list.isEmpty());
	list.add(1);
	list.add(3);
	list.add(1, 2);
	list.add(0, 0);
	EXPECT_EQ(contents(list), (std::vector<int32_t>{0, 1, 2, 3}));
	EXPECT_EQ(list.size(), 4);
	EXPECT_EQ(list.get(2), 2);
	EXPECT_EQ(list.set(2, 5), 2);
	EXPECT_EQ(list.removeAt(0), 0);
	list.add(1);
	EXPECT_EQ(contents(list), (std::vector<int32_t>{1, 5, 3, 1}));
	EXPECT_EQ(list.indexOf(1), 0);
	EXPECT_EQ(list.lastIndexOf(1), 3);
	EXPECT_EQ(list.indexOf(9), -1);
	EXPECT_TRUE(list.contains(5));
	EXPECT_TRUE(list.remove(1)); // remove(Object): the first equal element
	EXPECT_EQ(contents(list), (std::vector<int32_t>{5, 3, 1}));
	EXPECT_FALSE(list.remove(42));
	list.clear();
	EXPECT_TRUE(list.isEmpty());
	EXPECT_EQ(list.size(), 0);
}

TEST_F(ArrayListTest, IndexErrorsThrowIndexOutOfBounds) {
	ArrayList<int32_t> list{1, 2};
	EXPECT_THROW(list.get(2), IndexOutOfBoundsException);
	EXPECT_THROW(list.get(-1), IndexOutOfBoundsException);
	EXPECT_THROW(list.add(3, 0), IndexOutOfBoundsException);
	EXPECT_THROW(list.set(5, 0), IndexOutOfBoundsException);
	EXPECT_THROW(list.removeAt(2), IndexOutOfBoundsException);
	try {
		list.get(7);
	} catch (const IndexOutOfBoundsException& e) {
		EXPECT_STREQ(e.what(), "Index 7 out of bounds for length 2");
	}
	list.add(2, 3); // index == size is legal
	EXPECT_EQ(list.size(), 3);
}

TEST_F(ArrayListTest, JavaEqualsAcrossRelogin) {
	ArrayList<Ref<Player>> players{AION_LOCK_CLASS(DropNpc::allowedLooters)};
	Ref<Player> before = Player::create(7, "before");
	Ref<Player> other = Player::create(8);
	players.add(other);
	players.add(before);
	Ref<Player> relogged = Player::create(7, "relogged"); // new instance, same objectId
	EXPECT_TRUE(players.contains(relogged));
	EXPECT_EQ(players.indexOf(relogged), 1);
	EXPECT_TRUE(players.remove(relogged)); // removes the old instance, as Java equals does
	ASSERT_EQ(players.size(), 1);
	EXPECT_EQ(players.get(0), other);

	ArrayList<Ref<Npc>> npcs;
	Ref<Npc> npc = Npc::create(1);
	npcs.add(npc);
	EXPECT_FALSE(npcs.contains(Npc::create(1))); // no equals: identity
	EXPECT_TRUE(npcs.contains(npc));
}

TEST_F(ArrayListTest, IteratorRemoveIsPositionalAndIdentityBased) {
	ArrayList<int32_t> list{1, 2, 1};
	auto it = list.iterator();
	EXPECT_EQ(it.next(), 1);
	EXPECT_EQ(it.next(), 2);
	EXPECT_EQ(it.next(), 1);
	it.remove(); // the LAST element, not the first equal one
	EXPECT_EQ(contents(list), (std::vector<int32_t>{1, 2}));
	EXPECT_FALSE(it.hasNext());
	EXPECT_THROW(it.next(), NoSuchElementException);
	EXPECT_THROW(it.remove(), IllegalStateException);

	Ref<Player> first = Player::create(5);
	Ref<Player> equalTwin = Player::create(5);
	ArrayList<Ref<Player>> players;
	players.add(first);
	players.add(equalTwin);
	auto playersIt = players.iterator();
	(void)playersIt.next();
	EXPECT_EQ(playersIt.next(), equalTwin);
	playersIt.remove(); // identity: the twin goes, the Java-equal first instance stays
	ASSERT_EQ(players.size(), 1);
	EXPECT_EQ(players.get(0), first);

	// removals through the iterator keep later positions right
	ArrayList<int32_t> numbers{4, 4, 4, 4};
	auto numbersIt = numbers.iterator();
	int32_t index = 0;
	while (numbersIt.hasNext()) {
		(void)numbersIt.next();
		if (index++ % 2 == 0)
			numbersIt.remove();
	}
	EXPECT_EQ(numbers.size(), 2);
}

TEST_F(ArrayListTest, IteratorRemoveOfConcurrentlyRemovedElementIsNoop) {
	Ref<Npc> a = Npc::create(1);
	Ref<Npc> b = Npc::create(2);
	ArrayList<Ref<Npc>> list;
	list.add(a);
	list.add(b);
	auto it = list.iterator();
	EXPECT_EQ(it.next(), a);
	EXPECT_TRUE(list.remove(a));
	it.remove(); // no-op
	ASSERT_EQ(list.size(), 1);
	EXPECT_EQ(list.get(0), b);
}

TEST_F(ArrayListTest, SnapshotIterationToleratesModification) {
	ArrayList<Ref<Npc>> list;
	for (int32_t i = 0; i < 3; ++i)
		list.add(Npc::create(i));
	int32_t visited = 0;
	for (Ptr<Npc> npc : list) {
		list.add(Npc::create(100 + npc->getObjectId())); // Java: ConcurrentModificationException; the shim iterates its snapshot
		list.removeAt(0);
		++visited;
	}
	EXPECT_EQ(visited, 3);
	EXPECT_EQ(list.size(), 3);
	EXPECT_EQ(list.get(0)->getObjectId(), 100);
}

TEST_F(ArrayListTest, RemoveIfEvaluatesInOrderAndSurvivesReentrantModification) {
	ArrayList<int32_t> list{1, 2, 3, 4, 5, 6};
	std::vector<int32_t> seen;
	EXPECT_TRUE(list.removeIf([&seen](int32_t value) {
		seen.push_back(value);
		return value % 2 == 0;
	}));
	EXPECT_EQ(seen, (std::vector<int32_t>{1, 2, 3, 4, 5, 6}));
	EXPECT_EQ(contents(list), (std::vector<int32_t>{1, 3, 5}));
	EXPECT_FALSE(list.removeIf([](int32_t) { return false; }));

	Ref<Npc> keep = Npc::create(1);
	Ref<Npc> drop = Npc::create(2);
	ArrayList<Ref<Npc>> npcs;
	npcs.add(keep);
	npcs.add(drop);
	Ref<Npc> added = Npc::create(3);
	EXPECT_TRUE(npcs.removeIf([&](const Ptr<Npc>& npc) {
		if (npc == keep)
			npcs.add(0, added); // reentrant modification from the predicate (same thread, reentrant Monitor)
		return npc == drop;
	}));
	ASSERT_EQ(npcs.size(), 2);
	EXPECT_EQ(npcs.get(0), added);
	EXPECT_EQ(npcs.get(1), keep);
}

TEST_F(ArrayListTest, SortUsesComparatorsUnderTheMonitor) {
	Monitor statsLock{AION_LOCK_CLASS(ArrayListTest::statsLock)};
	ArrayList<Ref<Player>> players{AION_LOCK_CLASS(ArrayListTest::players)};
	for (int32_t id : {5, 1, 4, 2, 3})
		players.add(Player::create(id));
	players.sort([&](const Ptr<Player>& a, const Ptr<Player>& b) {
		EXPECT_TRUE(players.monitor().isHeldByCurrentThread());
		SYNCHRONIZED(statsLock) { // comparators may take other Monitors
			return b->getObjectId() - a->getObjectId();
		}
	});
	std::vector<int32_t> ids;
	for (Ptr<Player> player : players)
		ids.push_back(player->getObjectId());
	EXPECT_EQ(ids, (std::vector<int32_t>{5, 4, 3, 2, 1}));

	players.sort(nullptr); // Java sort(null): natural ordering via compareTo
	EXPECT_EQ(players.get(0)->getObjectId(), 1);
	EXPECT_EQ(players.get(4)->getObjectId(), 5);

	ArrayList<std::string> names{"b", "c", "a"};
	names.sort([](const std::string& a, const std::string& b) { return a < b; }); // C++ "less" predicates work too
	EXPECT_EQ(names.snapshot(), (std::vector<std::string>{"a", "b", "c"}));

	ArrayList<int32_t> list{3, 1, 2};
	EXPECT_THROW(list.sort([&list](int32_t a, int32_t b) {
		list.add(9); // structural modification from the comparator
		return a - b;
	}),
		IllegalStateException);
	EXPECT_EQ(list.size(), 3);
	list.sort([&list](int32_t a, int32_t b) { return a - b + (list.contains(a) ? 0 : 1); }); // reads are allowed
	EXPECT_EQ(contents(list), (std::vector<int32_t>{1, 2, 3}));
}

TEST_F(ArrayListTest, BulkOperationsUseJavaEquals) {
	ArrayList<int32_t> list;
	EXPECT_TRUE(list.addAll(std::vector<int32_t>{1, 2, 3, 4}));
	EXPECT_FALSE(list.addAll(std::vector<int32_t>{}));
	EXPECT_TRUE(list.removeAll(std::vector<int32_t>{2, 4, 8}));
	EXPECT_EQ(contents(list), (std::vector<int32_t>{1, 3}));
	EXPECT_TRUE(list.retainAll(std::vector<int32_t>{3}));
	EXPECT_EQ(contents(list), (std::vector<int32_t>{3}));
	list.replaceAll([](int32_t value) { return value * 10; });
	EXPECT_EQ(list.get(0), 30);

	ArrayList<Ref<Player>> players;
	Ref<Player> player = Player::create(1);
	EXPECT_TRUE(players.addAll(std::vector<Ptr<Player>>{player, Player::create(2)}));
	EXPECT_TRUE(players.removeAll(std::vector<Ref<Player>>{Player::create(2)})); // Java-equal instance
	ASSERT_EQ(players.size(), 1);
	players.replaceAll([](const Ptr<Player>& p) { return Player::create(p->getObjectId() + 1); });
	EXPECT_EQ(players.get(0)->getObjectId(), 2);

	ArrayList<int32_t> copy;
	EXPECT_TRUE(copy.addAll(list)); // another shim is a range (snapshot iteration)
	std::vector<int32_t> visited;
	copy.forEach([&visited](int32_t value) { visited.push_back(value); });
	EXPECT_EQ(visited, (std::vector<int32_t>{30}));

	ArrayList<int32_t> shuffled{1, 2, 3, 4, 5, 6, 7, 8};
	shuffled.shuffle();
	std::vector<int32_t> sorted = shuffled.snapshot();
	std::ranges::sort(sorted);
	EXPECT_EQ(sorted, (std::vector<int32_t>{1, 2, 3, 4, 5, 6, 7, 8}));
}

TEST_F(ArrayListTest, BorrowedElementSurvivesRemovalUntilTheTaskEnds) {
	ArrayList<Ref<Npc>> list;
	int64_t before = liveObjects.load();
	list.add(Npc::create(42));
	Ptr<Npc> removed = list.removeAt(0); // the list held the only Ref
	Reclaimer::getInstance().reclaimNow();
	Reclaimer::getInstance().reclaimNow();
	EXPECT_EQ(removed->getObjectId(), 42); // the published epoch of this task keeps it alive (ASan would report a use-after-free)
	EXPECT_EQ(liveObjects.load(), before + 1);
	reclaimAll();
	EXPECT_EQ(liveObjects.load(), before);
}

TEST_F(ArrayListTest, SynchronizedLocksTheListMonitorAndIsEmptyStaysLockFree) {
	ArrayList<int32_t> list{AION_LOCK_CLASS(SummonerAI::spawnedNpc)};
	std::atomic<bool> isEmptyReturned{false};
	std::atomic<bool> addFinished{false};
	std::thread other;
	SYNCHRONIZED(list) {
		EXPECT_TRUE(list.monitor().isHeldByCurrentThread());
		list.add(1); // reentrant
		other = std::thread([&] {
			TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
			(void)list.isEmpty();
			isEmptyReturned = true;
			list.add(2); // blocks until the synchronized block ends
			addFinished = true;
		});
		for (int i = 0; i < 2000 && !isEmptyReturned; ++i)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		EXPECT_TRUE(isEmptyReturned.load());
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		EXPECT_FALSE(addFinished.load());
	}
	other.join();
	EXPECT_TRUE(addFinished.load());
	EXPECT_EQ(list.size(), 2);

	Ref<RcArrayList<Ref<Npc>>> spawned = RcArrayList<Ref<Npc>>::create(AION_LOCK_CLASS(SummonerAI::spawnedNpcRc));
	spawned->add(Npc::create(1));
	SYNCHRONIZED(*spawned) {
		EXPECT_TRUE(static_cast<const ArrayList<Ref<Npc>>&>(*spawned).monitor().isHeldByCurrentThread());
		EXPECT_EQ(spawned->size(), 1);
	}
}

TEST_F(ArrayListTest, LinkedListDequeOperations) {
	LinkedList<int32_t> list;
	EXPECT_FALSE(list.poll().has_value());
	EXPECT_THROW(list.getFirst(), NoSuchElementException);
	EXPECT_THROW(list.removeLast(), NoSuchElementException);
	list.addLast(2);
	list.addFirst(1);
	list.push(0);
	EXPECT_TRUE(list.offer(3));
	EXPECT_EQ(list.getFirst(), 0);
	EXPECT_EQ(list.getLast(), 3);
	EXPECT_EQ(list.peekLast(), 3);
	EXPECT_EQ(list.pop(), 0);
	EXPECT_EQ(list.pollLast(), 3);
	EXPECT_EQ(list.removeFirst(), 1);
	EXPECT_EQ(list.element(), 2);
	EXPECT_EQ(list.size(), 1);
}

TEST_F(ArrayListTest, ArrayDequeRejectsNullAndFollowsJava) {
	ArrayDeque<Ref<Npc>> deque{AION_LOCK_CLASS(ArrayListTest::deque)};
	EXPECT_THROW(deque.add(Ref<Npc>()), NullPointerException);
	EXPECT_THROW(deque.pop(), NoSuchElementException);
	EXPECT_FALSE(deque.poll());
	Ref<Npc> a = Npc::create(1);
	Ref<Npc> b = Npc::create(2);
	deque.push(a);     // head
	deque.offer(b);    // tail
	EXPECT_EQ(deque.peekFirst(), a);
	EXPECT_EQ(deque.peekLast(), b);
	std::vector<Ptr<Npc>> order = deque.snapshot();
	ASSERT_EQ(order.size(), 2u);
	EXPECT_EQ(order[0], a);
	EXPECT_TRUE(deque.contains(b));
	EXPECT_TRUE(deque.removeIf([&b](const Ptr<Npc>& npc) { return npc == b; }));
	auto it = deque.iterator();
	EXPECT_EQ(it.next(), a);
	it.remove();
	EXPECT_TRUE(deque.isEmpty());
}

TEST_F(ArrayListTest, PriorityQueueOrdersByComparatorOrNaturalOrder) {
	PriorityQueue<int32_t> natural;
	for (int32_t value : {5, 1, 4, 2, 3})
		natural.offer(value);
	std::vector<int32_t> polled;
	while (auto value = natural.poll())
		polled.push_back(*value);
	EXPECT_EQ(polled, (std::vector<int32_t>{1, 2, 3, 4, 5}));

	PriorityQueue<Ref<Player>> reversed([](const Ptr<Player>& a, const Ptr<Player>& b) { return b->getObjectId() - a->getObjectId(); });
	for (int32_t id : {2, 9, 4})
		reversed.add(Player::create(id));
	EXPECT_EQ(reversed.peek()->getObjectId(), 9);
	EXPECT_TRUE(reversed.remove(Player::create(9))); // Java equals
	EXPECT_EQ(reversed.element()->getObjectId(), 4);
	EXPECT_TRUE(reversed.contains(Player::create(2)));
	EXPECT_EQ(reversed.size(), 2);
	reversed.clear();
	EXPECT_THROW(reversed.element(), NoSuchElementException);
}

namespace {

struct ComparatorFault : std::runtime_error {
	ComparatorFault() : std::runtime_error("comparator fault (expected by the test)") {}
};

std::vector<Player*> identities(const std::vector<Ptr<Player>>& elements) {
	std::vector<Player*> raw;
	for (const Ptr<Player>& element : elements)
		raw.push_back(element.rawPointer());
	std::sort(raw.begin(), raw.end());
	return raw;
}

std::vector<Player*> identities(const std::vector<Ref<Player>>& elements) {
	std::vector<Player*> raw;
	for (const Ref<Player>& element : elements)
		raw.push_back(element.get());
	std::sort(raw.begin(), raw.end());
	return raw;
}

} // namespace

// Review / stress finding: a throwing comparator must never lose elements or leave null Refs (Java: the order may be partially changed).
TEST_F(ArrayListTest, ThrowingComparatorsNeverLoseElements) {
	std::vector<Ref<Player>> originals;
	ArrayList<Ref<Player>> list;
	for (int32_t i = 0; i < 200; ++i) {
		originals.push_back(Player::create((i * 7919) % 200));
		list.add(originals.back());
	}
	int32_t compares = 0;
	EXPECT_THROW(list.sort([&](const Ptr<Player>& a, const Ptr<Player>& b) {
		if (++compares == 300)
			throw ComparatorFault();
		return a->getObjectId() - b->getObjectId();
	}),
		ComparatorFault);
	ASSERT_EQ(list.size(), 200);
	EXPECT_EQ(identities(list.snapshot()), identities(originals)) << "ArrayList::sort lost, duplicated or nulled elements";
	list.sort([](const Ptr<Player>& a, const Ptr<Player>& b) { return a->getObjectId() - b->getObjectId(); });
	EXPECT_EQ(list.get(0)->getObjectId(), 0);
	EXPECT_EQ(list.get(199)->getObjectId(), 199);

	bool armed = false;
	compares = 0;
	PriorityQueue<Ref<Player>> queue(JavaComparator<Ref<Player>>([&](const Ptr<Player>& a, const Ptr<Player>& b) -> int32_t {
		if (armed && ++compares == 3)
			throw ComparatorFault();
		return a->getObjectId() - b->getObjectId();
	}));
	std::vector<Ref<Player>> offered;
	for (int32_t i = 0; i < 64; ++i) {
		offered.push_back(Player::create((i * 37) % 64));
		queue.offer(offered.back());
	}
	offered.push_back(Player::create(-1)); // sifts towards the root: several comparisons
	armed = true;
	EXPECT_THROW(queue.offer(offered.back()), ComparatorFault);
	EXPECT_EQ(queue.size(), 65);
	EXPECT_EQ(identities(queue.snapshot()), identities(offered)) << "PriorityQueue::offer lost or nulled elements";
	compares = 0;
	Ptr<Player> head = queue.peek();
	EXPECT_THROW((void)queue.poll(), ComparatorFault);
	EXPECT_EQ(queue.size(), 64) << "a throwing poll removed exactly the head";
	std::erase_if(offered, [&](const Ref<Player>& player) { return player.get() == head.rawPointer(); });
	EXPECT_EQ(identities(queue.snapshot()), identities(offered)) << "PriorityQueue::poll lost or nulled other elements";
	compares = 0;
	try {
		EXPECT_TRUE(queue.remove(offered[10]));
	} catch (const ComparatorFault&) {
		// thrown while sifting the element that filled the slot: the removal itself happened
	}
	armed = false;
	std::vector<int32_t> ids;
	while (Nullable<Ref<Player>> polled = queue.poll())
		ids.push_back(polled->getObjectId());
	EXPECT_EQ(ids.size(), 63u) << "remove took out its element; every other element is still there (heap order may be broken, Java too)";
}
