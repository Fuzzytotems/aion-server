// Systematic interleaving tests of the collection shims (design §12.4 item 1): concurrent put/remove/compute/iterate against the Reclaimer,
// run under the PCT scheduler (AION_PCT_SCHEDULES overrides the schedule count; nightly runs use 100000) and ASan. Every body opens its own
// TaskScopes; readers dereference every borrowed element, so an early free is an ASan use-after-free.

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "CollectionsTestSupport.h"
#include "aion/gameserver/runtime/collections/Collections.h"
#include "aion/gameserver/runtime/lifetime/Pct.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::testcollections;

namespace {

class CollectionsPctTest : public CollectionsTest {
protected:
	void TearDown() override {
		CollectionsTest::TearDown();
		EXPECT_EQ(liveObjects.load(), liveAtStart);
	}
	void SetUp() override {
		CollectionsTest::SetUp();
		liveAtStart = liveObjects.load();
	}

	static pct::Options options() {
		pct::Options options;
		options.depth = 3;
		options.maxSteps = 4000;
		options.timeout = std::chrono::milliseconds(20000);
		return options;
	}

	static void expectPassed(const pct::ScheduleResult& result) {
		EXPECT_TRUE(result.completed) << "seed " << result.seed << ": " << result.failure;
		EXPECT_FALSE(result.deadlock) << "seed " << result.seed;
	}

	/** Drops this test's scope while the schedules run (bodies open their own), so the Reclaimer body is not pinned by the main thread. */
	pct::ScheduleResult explore(uint32_t defaultSchedules, uint64_t baseSeed, const pct::ScenarioFactory& factory,
		const std::function<void()>& check = {}) {
		scope.reset();
		pct::ScheduleResult result = pct::explore(pct::schedulesFromEnvironment(defaultSchedules), baseSeed, factory, check, options());
		Reclaimer::getInstance().drain();
		scope.emplace(AION_TASK_INFO(TaskKind::TEST));
		return result;
	}

	template <bool LockedReads>
	void putRemoveIterate(uint64_t baseSeed);

	int64_t liveAtStart = 0;
};

constexpr int32_t VALUE_FACTOR = 1000;

void reclaimerBody(const std::shared_ptr<std::atomic<int32_t>>& runningWriters) {
	for (int i = 0; i < 64 && runningWriters->load() > 0; ++i) {
		Reclaimer::getInstance().reclaimNow();
		pct::yieldPoint("test:reclaimer");
	}
}

/** Checks a borrowed Npc value against its key by dereferencing it (ASan catches a premature free). */
void checkValue(int32_t key, const Ptr<Npc>& npc) {
	if (!npc)
		throw IllegalStateException("null value in ConcurrentHashMap");
	if (npc->getObjectId() / VALUE_FACTOR != key)
		throw IllegalStateException("value of key " + std::to_string(key) + " has id " + std::to_string(npc->getObjectId()));
}

} // namespace

template <bool LockedReads>
void CollectionsPctTest::putRemoveIterate(uint64_t baseSeed) {
	using Map = ConcurrentHashMap<int32_t, Ref<Npc>, LockedReads>;
	std::shared_ptr<Map> lastMap;
	pct::ScheduleResult result = explore(
		12, baseSeed,
		[&lastMap](uint64_t) {
			auto map = std::make_shared<Map>(AION_LOCK_CLASS(CollectionsPctTest::map#stripe));
			lastMap = map;
			auto writers = std::make_shared<std::atomic<int32_t>>(2);
			std::vector<std::function<void()>> bodies;
			bodies.push_back([map, writers] {
				TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
				for (int32_t i = 0; i < 40; ++i) {
					int32_t key = i % 8;
					map->put(key, Npc::create(key * VALUE_FACTOR + i));
					(void)map->remove((i * 3) % 8);
					pct::yieldPoint("test:writerA");
				}
				--*writers;
			});
			bodies.push_back([map, writers] {
				TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
				for (int32_t round = 0; round < 3; ++round) {
					for (int32_t key = 8; key < 40; ++key) // grows the tables: resizes race with readers
						map->computeIfAbsent(key, [key, round](const int32_t&) { return Npc::create(key * VALUE_FACTOR + round); });
					for (int32_t key = 8; key < 40; key += 2)
						(void)map->remove(key);
					map->removeIf([](const int32_t& key, const Ptr<Npc>&) { return key >= 8 && key % 3 == 0; });
				}
				--*writers;
			});
			bodies.push_back([map, writers] {
				for (int32_t round = 0; round < 6 && writers->load() > 0; ++round) {
					TaskScope scope(AION_TASK_INFO(TaskKind::TEST)); // a new scope per round lets the Reclaimer free older nodes
					for (auto [key, value] : map->entrySet())
						checkValue(key, value);
					for (int32_t key = 0; key < 40; ++key) {
						if (Ptr<Npc> value = map->get(key))
							checkValue(key, value);
					}
				}
			});
			bodies.push_back([writers] { reclaimerBody(writers); });
			return bodies;
		},
		[&lastMap] {
			TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
			int32_t iterated = 0;
			for (auto [key, value] : lastMap->entrySet()) {
				checkValue(key, value);
				++iterated;
			}
			if (iterated != lastMap->size())
				throw IllegalStateException("size " + std::to_string(lastMap->size()) + " but iterated " + std::to_string(iterated));
			for (int32_t key = 8; key < 40; key += 2)
				if (lastMap->containsKey(key))
					throw IllegalStateException("removed key present: " + std::to_string(key));
		});
	lastMap.reset();
	expectPassed(result);
}

TEST_F(CollectionsPctTest, ConcurrentHashMapPutRemoveIterateVersusReclaimer) {
	putRemoveIterate<false>(1000);
}

TEST_F(CollectionsPctTest, ConcurrentHashMapLockedReadsPutRemoveIterateVersusReclaimer) {
	putRemoveIterate<true>(5000);
}

TEST_F(CollectionsPctTest, ConcurrentHashMapNestedComputeVersusReadsAndResize) {
	using Map = ConcurrentHashMap<int32_t, Ref<Npc>>;
	struct State {
		Map map{AION_LOCK_CLASS(CollectionsPctTest::nested#stripe)};
		Monitor other{AION_LOCK_CLASS(CollectionsPctTest::other)};
		std::atomic<int32_t> recursiveThrows{0};
		std::atomic<int32_t> computedAbsent{0};
		std::atomic<int32_t> writers{2};
		int32_t sameStripeKey = 0;
	};
	std::shared_ptr<State> last;
	pct::ScheduleResult result = explore(
		12, 2000,
		[&last](uint64_t) {
			auto state = std::make_shared<State>();
			last = state;
			{
				TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
				Monitor& stripe = state->map.stripeMonitor(1);
				state->sameStripeKey = 2;
				while (&state->map.stripeMonitor(state->sameStripeKey) != &stripe)
					++state->sameStripeKey;
			}
			auto shared = std::shared_ptr<std::atomic<int32_t>>(state, &state->writers);
			std::vector<std::function<void()>> bodies;
			bodies.push_back([state] {
				for (int32_t round = 0; round < 4; ++round) {
					TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
					state->map.compute(1, [&](const int32_t&, Ptr<Npc> old) {
						state->map.put(state->sameStripeKey, Npc::create(state->sameStripeKey * VALUE_FACTOR)); // reentrant, same stripe
						for (int32_t key = 100; key < 116; ++key)
							state->map.put(key, Npc::create(key * VALUE_FACTOR + round)); // resize while the callback holds the stripe
						if (!old)
							++state->computedAbsent; // computeIfAbsent of the computed, still absent key is a recursive update
						SYNCHRONIZED(state->other) { // a Monitor inside the stripe Monitor
							try {
								state->map.computeIfAbsent(1, [] { return Npc::create(1 * VALUE_FACTOR); });
							} catch (const IllegalStateException&) {
								++state->recursiveThrows;
							}
						}
						return Npc::create(1 * VALUE_FACTOR + (old ? old->getObjectId() % VALUE_FACTOR + 1 : 0));
					});
				}
				--state->writers;
			});
			bodies.push_back([state] {
				TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
				for (int32_t round = 0; round < 4; ++round) {
					Ptr<Npc> value = state->map.computeIfAbsent(1, [] { return Npc::create(1 * VALUE_FACTOR + 50); });
					checkValue(1, value);
					(void)state->map.merge(200 + round, Npc::create((200 + round) * VALUE_FACTOR), [](Ptr<Npc>, Ptr<Npc> given) { return given; });
					for (int32_t key = 100; key < 116; ++key)
						(void)state->map.remove(key);
				}
				--state->writers;
			});
			bodies.push_back([state] {
				for (int32_t round = 0; round < 8 && state->writers.load() > 0; ++round) {
					TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
					if (Ptr<Npc> value = state->map.get(1))
						checkValue(1, value);
					for (Ptr<Npc> value : state->map.values())
						(void)value->getObjectId();
					for (auto [key, value] : state->map.entrySet())
						checkValue(key, value);
				}
			});
			bodies.push_back([shared] { reclaimerBody(shared); });
			return bodies;
		},
		[&last] {
			if (last->recursiveThrows.load() != last->computedAbsent.load())
				throw IllegalStateException("recursive computeIfAbsent threw " + std::to_string(last->recursiveThrows.load()) + " times for " +
					std::to_string(last->computedAbsent.load()) + " absent computes");
			TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
			Ptr<Npc> one = last->map.get(1);
			checkValue(1, one);
			for (int32_t round = 0; round < 4; ++round)
				checkValue(200 + round, last->map.get(200 + round));
		});
	last.reset();
	expectPassed(result);
}

TEST_F(CollectionsPctTest, CopyOnWriteArrayListWritersVersusInPlaceIteration) {
	struct State {
		CopyOnWriteArrayList<Ref<Npc>> list{AION_LOCK_CLASS(CollectionsPctTest::observers)};
		std::atomic<int32_t> writers{2};
	};
	std::shared_ptr<State> last;
	pct::ScheduleResult result = explore(
		12, 3000,
		[&last](uint64_t) {
			auto state = std::make_shared<State>();
			last = state;
			auto writers = std::shared_ptr<std::atomic<int32_t>>(state, &state->writers);
			std::vector<std::function<void()>> bodies;
			for (int32_t writer = 0; writer < 2; ++writer) {
				bodies.push_back([state, writer] {
					TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
					for (int32_t i = 0; i < 20; ++i) {
						Ref<Npc> npc = Npc::create(writer * VALUE_FACTOR + i);
						state->list.add(npc);
						if (i % 3 == 0)
							(void)state->list.remove(npc);
						if (i % 5 == 0)
							(void)state->list.removeIf([writer](const Ptr<Npc>& other) { return other->getObjectId() / VALUE_FACTOR == writer && other->getObjectId() % 2 == 0; });
					}
					--state->writers;
				});
			}
			bodies.push_back([state] {
				for (int32_t round = 0; round < 8 && state->writers.load() > 0; ++round) {
					TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
					for (Ptr<Npc> npc : state->list)
						(void)npc->getObjectId();
					if (!state->list.isEmpty()) {
						try {
							(void)state->list.get(0)->getObjectId();
						} catch (const IndexOutOfBoundsException&) { // emptied concurrently
						}
					}
				}
			});
			bodies.push_back([writers] { reclaimerBody(writers); });
			return bodies;
		},
		[&last] {
			TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
			for (Ptr<Npc> npc : last->list) {
				int32_t local = npc->getObjectId() % VALUE_FACTOR;
				if (local % 3 == 0)
					throw IllegalStateException("removed element still present: " + std::to_string(npc->getObjectId()));
			}
		});
	last.reset();
	expectPassed(result);
}

TEST_F(CollectionsPctTest, SynchronizedShimsStayConsistentUnderContention) {
	struct State {
		HashMap<int32_t, int32_t> counters{AION_LOCK_CLASS(CollectionsPctTest::counters)};
		ArrayList<Ref<Npc>> list{AION_LOCK_CLASS(CollectionsPctTest::list)};
		ConcurrentHashMap<int32_t, int32_t> concurrentCounters{AION_LOCK_CLASS(CollectionsPctTest::concurrentCounters#stripe)};
	};
	constexpr int32_t THREADS = 3;
	constexpr int32_t INCREMENTS = 30;
	std::shared_ptr<State> last;
	pct::ScheduleResult result = explore(
		12, 4000,
		[&last](uint64_t) {
			auto state = std::make_shared<State>();
			last = state;
			std::vector<std::function<void()>> bodies;
			for (int32_t thread = 0; thread < THREADS; ++thread) {
				bodies.push_back([state, thread] {
					TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
					for (int32_t i = 0; i < INCREMENTS; ++i) {
						state->counters.merge(i % 4, 1, [](int32_t a, int32_t b) { return a + b; });
						state->concurrentCounters.compute(i % 4, [](std::optional<int32_t> old) { return std::optional<int32_t>(old.value_or(0) + 1); });
						Ref<Npc> npc = Npc::create(thread * VALUE_FACTOR + i);
						state->list.add(npc);
						if (i % 2 == 0) {
							auto it = state->list.iterator();
							while (it.hasNext())
								if (it.next() == npc)
									it.remove();
						}
						for (Ptr<Npc> element : state->list)
							(void)element->getObjectId();
					}
				});
			}
			return bodies;
		},
		[&last] {
			TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
			int32_t total = 0;
			int32_t concurrentTotal = 0;
			for (int32_t key = 0; key < 4; ++key) {
				total += last->counters.get(key).value_or(0);
				concurrentTotal += last->concurrentCounters.get(key).value_or(0);
			}
			if (total != THREADS * INCREMENTS || concurrentTotal != THREADS * INCREMENTS)
				throw IllegalStateException("lost updates: " + std::to_string(total) + " / " + std::to_string(concurrentTotal));
			if (last->list.size() != THREADS * INCREMENTS / 2)
				throw IllegalStateException("list size " + std::to_string(last->list.size()));
		});
	last.reset();
	expectPassed(result);
}
