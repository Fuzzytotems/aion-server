// Contract test of aion_gs_runtime_collections: instantiates the whole public API so that header changes that break the contract fail to
// compile. It includes the design's porting samples (runtime-architecture.md §14.2 b, d, g, h). Behaviour is tested by the other files of
// this directory.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/collections/Collections.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

using namespace aion::gameserver::runtime;

namespace {

class Npc final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Npc> create(int32_t objectId) { return makeRef<Npc>(objectId); }
	int32_t getObjectId() const noexcept { return objectId; }
	int32_t getNpcId() const noexcept { return 216530; }
	bool equals(const Npc& other) const noexcept { return objectId == other.objectId; }
	int32_t hashCode() const noexcept { return objectId; }

protected:
	explicit Npc(int32_t objectId) : objectId(objectId) {}
	~Npc() override = default;

private:
	const int32_t objectId;
};

enum class TaskId { DESPAWN, RESPAWN };
enum class StatEnum { MAXHP, SPEED };

/** Instantiates every member of the public API (not executed). */
[[maybe_unused]] void instantiateApi() {
	Ref<Npc> npc = Npc::create(1);
	Ptr<Npc> borrowed = npc;

	ArrayList<Ref<Npc>> traps{AION_LOCK_CLASS(LowerUdasTempleInstance::traps)};
	traps.add(npc);
	traps.add(0, borrowed);
	(void)traps.addAll(std::vector<Ptr<Npc>>{borrowed});
	Ptr<Npc> first = traps.get(0);
	(void)traps.set(0, npc);
	(void)traps.removeAt(0);
	(void)traps.remove(borrowed);
	(void)traps.contains(first);
	(void)traps.indexOf(first);
	(void)traps.lastIndexOf(first);
	(void)traps.size();
	(void)traps.isEmpty();
	(void)traps.removeIf([](const Ptr<Npc>& trap) { return trap->getNpcId() == 216531; });
	traps.sort([](const Ptr<Npc>& a, const Ptr<Npc>& b) { return a->getObjectId() - b->getObjectId(); });
	traps.replaceAll([](const Ptr<Npc>& trap) { return Ref<Npc>(trap); });
	traps.forEach([](const Ptr<Npc>&) {});
	for (Ptr<Npc> trap : traps)
		if (trap && trap->getNpcId() != 216531)
			(void)trap->getObjectId();
	auto it = traps.iterator();
	while (it.hasNext())
		if (it.next()->getObjectId() == 1)
			it.remove();
	std::vector<Ptr<Npc>> snapshot = traps.snapshot();
	SYNCHRONIZED(traps) {
		traps.clear();
	}

	ArrayList<int32_t> ids;
	ids.add(5);
	(void)ids.remove(5);
	(void)ids.removeAt(0);
	LinkedList<int32_t> linked;
	linked.addFirst(1);
	(void)linked.pollFirst();
	(void)linked.getLast();
	ArrayDeque<Ref<Npc>> deque;
	deque.push(npc);
	(void)deque.poll();
	PriorityQueue<int32_t> priorities([](const int32_t& a, const int32_t& b) { return a - b; });
	(void)priorities.offer(3);
	(void)priorities.peek();

	HashMap<int32_t, Ref<Npc>> byId;
	(void)byId.put(1, npc);
	Ptr<Npc> found = byId.get(1);
	(void)byId.getOrDefault(2, found);
	(void)byId.computeIfAbsent(2, [&](const int32_t&) { return npc; });
	(void)byId.compute(1, [](const int32_t&, Ptr<Npc> old) -> Ref<Npc> { return old; });
	(void)byId.merge(1, npc, [](Ptr<Npc> old, Ptr<Npc>) { return Ref<Npc>(old); });
	(void)byId.removeIf([](const int32_t& key, const Ptr<Npc>&) { return key > 5; });
	for (auto [key, value] : byId.entrySet())
		(void)value->getObjectId();
	for (Ptr<Npc> value : byId.values())
		(void)value;
	(void)byId.keySet().iterator();
	std::vector<Ptr<Npc>> values = byId.values();
	LinkedHashMap<std::string, int32_t> names;
	(void)names.put("a", 1);
	std::optional<int32_t> absent = names.get("b");
	TreeMap<int64_t, Ref<Npc>> equipment{AION_LOCK_CLASS(Equipment::equipment)};
	(void)equipment.firstEntry();
	(void)equipment.floorKey(5);
	(void)equipment.headMap(10);
	EnumMap<StatEnum, int32_t> stats;
	(void)stats.put(StatEnum::SPEED, 1);

	HashSet<Ref<Npc>> set;
	(void)set.add(npc);
	(void)set.contains(borrowed);
	LinkedHashSet<int32_t> ordered;
	(void)ordered.add(1);
	TreeSet<int32_t> sorted;
	(void)sorted.add(2);
	(void)sorted.first();
	(void)sorted.ceiling(1);

	// design §14.2g CreatureController.addTask / cancelTaskIfPresent / cancelAllTasks
	ConcurrentHashMap<int32_t, Ref<Npc>> tasks{AION_LOCK_CLASS(CreatureController::tasks#stripe)};
	(void)tasks.compute(static_cast<int32_t>(TaskId::DESPAWN), [&](Ptr<Npc> oldTask) -> Ref<Npc> {
		if (oldTask)
			(void)oldTask->getObjectId();
		return npc;
	});
	(void)tasks.remove(static_cast<int32_t>(TaskId::DESPAWN), borrowed);
	for (auto [id, task] : tasks.entrySet())
		if (task)
			(void)task->getObjectId();
	tasks.clear();
	// design §14.2h MoveTaskManager
	(void)tasks.putIfAbsent(npc->getObjectId(), npc);
	std::vector<Ptr<Npc>> creatures = tasks.values();
	(void)static_cast<bool>(tasks.remove(npc->getObjectId()));
	(void)tasks.computeIfPresent(1, [](const int32_t&, Ptr<Npc> value) { return Ref<Npc>(value); });
	(void)tasks.merge(1, npc, [](Ptr<Npc>, Ptr<Npc> given) { return Ref<Npc>(given); });
	(void)tasks.values().removeIf([](const Ptr<Npc>&) { return true; });
	(void)tasks.stripeMonitor(1);
	ConcurrentHashMap<int32_t, int32_t> counters;
	(void)counters.merge(1, 1, [](int32_t a, int32_t b) { return std::optional<int32_t>(a + b); });

	ConcurrentKeySet<int32_t> registeredObjects;
	(void)registeredObjects.add(1);
	(void)registeredObjects.contains(1);

	CopyOnWriteArrayList<Ref<Npc>> observers;
	(void)observers.add(npc);
	(void)observers.addIfAbsent(npc);
	for (Ptr<Npc> observer : observers)
		(void)observer;
	CopyOnWriteArraySet<int32_t> pendingAscension;
	(void)pendingAscension.add(1);

	ConcurrentLinkedQueue<Ref<Npc>> queue;
	(void)queue.offer(npc);
	(void)queue.poll();
	ConcurrentLinkedDeque<int32_t> itemsToBeDistributed;
	itemsToBeDistributed.addLast(1);
	(void)itemsToBeDistributed.pollFirst();

	Ref<RcArrayList<Ref<Npc>>> spawned = RcArrayList<Ref<Npc>>::create(AION_LOCK_CLASS(SummonerAI::spawnedNpc));
	spawned->add(npc);
	SYNCHRONIZED(*spawned) {
	}
	(void)snapshot;
	(void)values;
	(void)absent;
	(void)creatures;
}

} // namespace

TEST(CollectionsContractTest, JavaEqualityUsesEqualsWhenDeclared) {
	TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
	Ref<Npc> a = Npc::create(7);
	Ref<Npc> relogged = Npc::create(7);
	EXPECT_TRUE(JavaEquality<Ref<Npc>>::equals(Ptr<Npc>(a), Ptr<Npc>(relogged)));
	EXPECT_EQ(JavaEquality<Ref<Npc>>::hash(Ptr<Npc>(a)), JavaEquality<Ref<Npc>>::hash(Ptr<Npc>(relogged)));
	EXPECT_TRUE(JavaEquality<int32_t>::equals(3, 3));
	EXPECT_LT(JavaOrdering<int32_t>::compare(1, 2), 0);
}
