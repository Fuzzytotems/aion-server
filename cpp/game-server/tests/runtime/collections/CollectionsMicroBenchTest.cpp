// Opt-in micro benchmark of the collection shims (design §2.5 cost table, P4 input). Runs only with AION_COLLECTIONS_BENCH=1; meaningful in
// Release builds. Prints nanoseconds per operation.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "CollectionsTestSupport.h"
#include "aion/gameserver/runtime/collections/Collections.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::testcollections;

namespace {

bool benchEnabled() {
	const char* value = std::getenv("AION_COLLECTIONS_BENCH"); // test-only switch
	return value != nullptr && *value == '1';
}

template <class F>
void measure(const char* name, int64_t operations, F&& body) {
	auto start = std::chrono::steady_clock::now();
	body();
	auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
	std::printf("[bench] %-52s %8.1f ns/op\n", name, static_cast<double>(nanos) / static_cast<double>(operations));
}

class CollectionsMicroBenchTest : public CollectionsTest {};

int64_t sink = 0; // printed at the end: keeps the measured loops observable

} // namespace

TEST_F(CollectionsMicroBenchTest, CostsPerOperation) {
	if (!benchEnabled())
		GTEST_SKIP() << "set AION_COLLECTIONS_BENCH=1 to run";
	constexpr int32_t KEYS = 10000;
	constexpr int64_t N = 2'000'000;

	ConcurrentHashMap<int32_t, Ref<Npc>, false> lockFree;
	ConcurrentHashMap<int32_t, Ref<Npc>, true> locked;
	HashMap<int32_t, Ref<Npc>> hashMap;
	std::vector<Ref<Npc>> npcs;
	for (int32_t i = 0; i < KEYS; ++i) {
		npcs.push_back(Npc::create(i));
		lockFree.put(i, npcs.back());
		locked.put(i, npcs.back());
		hashMap.put(i, npcs.back());
	}
	measure("ConcurrentHashMap::get (lock-free)", N, [&] {
		for (int64_t i = 0; i < N; ++i)
			sink += lockFree.get(static_cast<int32_t>(i % KEYS)).rawPointer() != nullptr;
	});
	measure("ConcurrentHashMap::get (AION_CHM_LOCKED_READS)", N, [&] {
		for (int64_t i = 0; i < N; ++i)
			sink += locked.get(static_cast<int32_t>(i % KEYS)).rawPointer() != nullptr;
	});
	measure("HashMap::get (collection Monitor)", N, [&] {
		for (int64_t i = 0; i < N; ++i)
			sink += hashMap.get(static_cast<int32_t>(i % KEYS)).rawPointer() != nullptr;
	});
	measure("ConcurrentHashMap::put (replace existing key)", N / 4, [&] {
		for (int64_t i = 0; i < N / 4; ++i)
			lockFree.put(static_cast<int32_t>(i % KEYS), npcs[static_cast<size_t>(i % KEYS)]);
	});
	reclaimAll();
	measure("ConcurrentHashMap::compute (replace existing key)", N / 4, [&] {
		for (int64_t i = 0; i < N / 4; ++i)
			lockFree.compute(static_cast<int32_t>(i % KEYS), [](Ptr<Npc> old) { return old; });
	});
	reclaimAll();
	measure("ConcurrentHashMap iteration per element (10k map)", 100 * KEYS, [&] {
		for (int round = 0; round < 100; ++round)
			for (Ptr<Npc> npc : lockFree.values())
				sink += npc.rawPointer() != nullptr;
	});

	ArrayList<Ref<Npc>> list;
	CopyOnWriteArrayList<Ref<Npc>> observers;
	for (int32_t i = 0; i < 300; ++i) {
		list.add(npcs[static_cast<size_t>(i)]);
		observers.add(npcs[static_cast<size_t>(i)]);
	}
	measure("ArrayList::get", N, [&] {
		for (int64_t i = 0; i < N; ++i)
			sink += list.get(static_cast<int32_t>(i % 300)).rawPointer() != nullptr;
	});
	measure("ArrayList snapshot iteration of 300 (per loop)", 20000, [&] {
		for (int round = 0; round < 20000; ++round)
			for (Ptr<Npc> npc : list)
				sink += npc.rawPointer() != nullptr;
	});
	measure("CopyOnWriteArrayList in-place iteration of 300 (per loop)", 20000, [&] {
		for (int round = 0; round < 20000; ++round)
			for (Ptr<Npc> npc : observers)
				sink += npc.rawPointer() != nullptr;
	});
	measure("ArrayList::add + removeAt(last)", N / 4, [&] {
		for (int64_t i = 0; i < N / 4; ++i) {
			list.add(npcs[0]);
			(void)list.removeAt(300);
		}
	});

	// 4 threads: 3 readers, 1 writer on the lock-free map
	std::atomic<bool> stop{false};
	std::atomic<int64_t> reads{0};
	std::vector<std::thread> readers;
	for (int t = 0; t < 3; ++t) {
		readers.emplace_back([&] {
			int64_t local = 0;
			while (!stop) {
				TaskScope readerScope(AION_TASK_INFO(TaskKind::TEST));
				for (int32_t i = 0; i < 1000; ++i)
					local += lockFree.get(i % KEYS).rawPointer() != nullptr;
			}
			reads += local;
		});
	}
	auto start = std::chrono::steady_clock::now();
	int64_t writes = 0;
	while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500)) {
		for (int32_t i = 0; i < 1000; ++i, ++writes)
			lockFree.put(i % KEYS, npcs[static_cast<size_t>(i % KEYS)]);
		reclaimAll();
	}
	stop = true;
	for (std::thread& reader : readers)
		reader.join();
	std::printf("[bench] contended: %lld reads/s per reader thread, %lld puts/s (1 writer)\n", static_cast<long long>(reads.load() * 2 / 3),
		static_cast<long long>(writes * 2));
	std::printf("[bench] checksum %lld\n", static_cast<long long>(sink));
	lockFree.clear();
	locked.clear();
	hashMap.clear();
}
