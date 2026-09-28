// Conformance scenarios for the six Java ConcurrentHashMap compute-callback sites of red team finding RR-1 (design §4.1, §19 P1), ported
// "as written" with synthetic classes: the callbacks take Monitors, nest map operations on the same and other maps, call a DAO inside a
// BlockingRegion and schedule/cancel tasks, from several threads at once.

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "CollectionsTestSupport.h"
#include "aion/gameserver/runtime/collections/Collections.h"
#include "aion/gameserver/runtime/sync/BlockingRegion.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::testcollections;

namespace {

class ComputeConformanceTest : public CollectionsTest {};

/** Runs `threads` bodies concurrently, each inside its own TaskScope, and joins them. */
void runConcurrently(int32_t threads, const std::function<void(int32_t)>& body) {
	std::vector<std::thread> workers;
	std::atomic<bool> go{false};
	for (int32_t t = 0; t < threads; ++t) {
		workers.emplace_back([&, t] {
			while (!go.load())
				std::this_thread::yield();
			TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
			body(t);
		});
	}
	go = true;
	for (std::thread& worker : workers)
		worker.join();
}

// ---- CreatureGameStats.java:68-100 ------------------------------------------------------------------------------------------------------

enum class StatEnum { MAXHP, SPEED, ATTACK, COUNT };

class StatFunction final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<StatFunction> create(StatEnum name, int32_t priority, int32_t owner) { return makeRef<StatFunction>(name, priority, owner); }
	StatEnum getName() const noexcept { return name; }
	int32_t getOwner() const noexcept { return owner; }
	int32_t getPriority() const noexcept { return priority; }
	int32_t compareTo(const StatFunction& other) const noexcept { return priority - other.priority; }

protected:
	StatFunction(StatEnum name, int32_t priority, int32_t owner) : name(name), priority(priority), owner(owner) {}
	~StatFunction() override = default;

private:
	const StatEnum name;
	const int32_t priority;
	const int32_t owner;
};

class CreatureGameStats {
public:
	using StatFunctions = RcArrayList<Ref<StatFunction>>;

	void addEffectOnly(const std::vector<Ref<StatFunction>>& functions) {
		for (const Ref<StatFunction>& functionToAdd : functions) {
			stats.compute(functionToAdd->getName(), [&](const StatEnum&, Ptr<StatFunctions> existing) -> Ref<StatFunctions> {
				Ref<StatFunctions> statFunctions(existing);
				if (!statFunctions) {
					statFunctions = StatFunctions::create(AION_LOCK_CLASS(CreatureGameStats::statFunctions));
					statFunctions->add(functionToAdd);
				} else {
					SYNCHRONIZED(*statFunctions) { // compute -> synchronized (statFunctions): a Monitor inside the stripe Monitor
						statFunctions->add(functionToAdd);
						statFunctions->sort(nullptr);
					}
				}
				return statFunctions;
			});
		}
	}

	bool endEffect(int32_t statOwner) {
		bool statsChanged = false;
		for (Ptr<StatFunctions> functions : stats.values()) {
			SYNCHRONIZED(*functions) {
				statsChanged |= functions->removeIf([statOwner](const Ptr<StatFunction>& function) { return function->getOwner() == statOwner; });
			}
		}
		return statsChanged;
	}

	int32_t count(StatEnum stat) const {
		Ptr<StatFunctions> functions = stats.get(stat);
		return functions ? functions->size() : 0;
	}
	bool sorted(StatEnum stat) const {
		Ptr<StatFunctions> functions = stats.get(stat);
		if (!functions)
			return true;
		std::vector<Ptr<StatFunction>> items = functions->snapshot();
		return std::ranges::is_sorted(items, [](const Ptr<StatFunction>& a, const Ptr<StatFunction>& b) { return a->getPriority() < b->getPriority(); });
	}

private:
	ConcurrentHashMap<StatEnum, Ref<StatFunctions>> stats{AION_LOCK_CLASS(CreatureGameStats::stats#stripe)};
};

// ---- PlayerContainer.java:51-54 ---------------------------------------------------------------------------------------------------------

class NamedPlayer final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<NamedPlayer> create(int32_t objectId, std::string name) { return makeRef<NamedPlayer>(objectId, std::move(name)); }
	int32_t getObjectId() const noexcept { return objectId; }
	std::string getName() const {
		SYNCHRONIZED(*this) {
			return name;
		}
	}
	void setName(std::string newName) {
		SYNCHRONIZED(*this) {
			name = std::move(newName);
		}
	}
	bool equals(const NamedPlayer& other) const noexcept { return objectId == other.objectId; }
	int32_t hashCode() const noexcept { return objectId; }

protected:
	NamedPlayer(int32_t objectId, std::string name) : objectId(objectId), name(std::move(name)) {}
	~NamedPlayer() override = default;

private:
	const int32_t objectId;
	std::string name; // guarded by the object monitor (test stand-in for Field<std::string>)
};

class PlayerContainer {
public:
	void add(NamedPlayer& player) {
		playersById.put(player.getObjectId(), Ref<NamedPlayer>(player));
		playersByName.put(player.getName(), Ref<NamedPlayer>(player));
	}
	Ptr<NamedPlayer> get(const std::string& name) const { return playersByName.get(name); }
	int32_t nameCount() const { return playersByName.size(); }

	void updateCachedPlayerName(const std::string& oldName, NamedPlayer& player) {
		playersByName.compute(oldName, [&](const std::string&, Ptr<NamedPlayer> p) -> Ptr<NamedPlayer> {
			playersByName.put(player.getName(), Ref<NamedPlayer>(player)); // put on another key inside compute (same stripe: reentrant)
			return p && player.equals(*p) ? Ptr<NamedPlayer>() : p;
		});
	}

	ConcurrentHashMap<std::string, Ref<NamedPlayer>> playersByName{AION_LOCK_CLASS(PlayerContainer::playersByName#stripe)};

private:
	ConcurrentHashMap<int32_t, Ref<NamedPlayer>> playersById{AION_LOCK_CLASS(PlayerContainer::playersById#stripe)};
};

// ---- GaleCycloneAI.java:31-37 -----------------------------------------------------------------------------------------------------------

class GaleCycloneObserver final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<GaleCycloneObserver> create(Player& player, Npc& owner) { return makeRef<GaleCycloneObserver>(player, owner); }
	const Ref<Player> player;
	const Ref<Npc> owner;

protected:
	GaleCycloneObserver(Player& player, Npc& owner) : player(player), owner(owner) {}
	~GaleCycloneObserver() override = default;
};

class ObserveController {
public:
	void addObserver(GaleCycloneObserver& observer) {
		SYNCHRONIZED(observers) { // ObserveController.addObserver: synchronized (observers)
			observers.add(Ref<GaleCycloneObserver>(observer));
		}
	}
	int32_t observerCount() const { return observers.size(); }
	void clear() { observers.clear(); }

private:
	CopyOnWriteArrayList<Ref<GaleCycloneObserver>> observers{AION_LOCK_CLASS(ObserveController::observers)};
};

class GaleCycloneAI {
public:
	explicit GaleCycloneAI(Npc& owner) : owner(owner) {}

	void handleCreatureSee(Player& player, ObserveController& playerObserveController) {
		observed.computeIfAbsent(player.getObjectId(), [&](const int32_t&) {
			Ref<GaleCycloneObserver> galeCycloneObserver = GaleCycloneObserver::create(player, *owner);
			playerObserveController.addObserver(*galeCycloneObserver); // Monitor of another object inside the stripe Monitor
			return galeCycloneObserver;
		});
	}
	int32_t observedCount() const { return observed.size(); }
	void clear() { observed.clear(); }

private:
	const Ref<Npc> owner;
	ConcurrentHashMap<int32_t, Ref<GaleCycloneObserver>> observed{AION_LOCK_CLASS(GaleCycloneAI::observed#stripe)};
};

// ---- LegionService.java:154-161 ---------------------------------------------------------------------------------------------------------

class PlayerCommonData final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<PlayerCommonData> create(int32_t id) { return makeRef<PlayerCommonData>(id); }
	const int32_t playerObjId;

protected:
	explicit PlayerCommonData(int32_t id) : playerObjId(id) {}
	~PlayerCommonData() override = default;
};

class LegionMember final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<LegionMember> create(int32_t id) { return makeRef<LegionMember>(id); }
	void setPlayerData(Ptr<PlayerCommonData> data) {
		SYNCHRONIZED(*this) {
			playerData = Ref<PlayerCommonData>(data);
		}
	}
	bool hasPlayerData() const {
		SYNCHRONIZED(*this) {
			return static_cast<bool>(playerData);
		}
	}
	const int32_t objectId;

protected:
	explicit LegionMember(int32_t id) : objectId(id) {}
	~LegionMember() override = default;

private:
	Ref<PlayerCommonData> playerData; // guarded by the object monitor
};

/** DAO stand-in: a connection-pool Monitor and a blocking "query" inside a BlockingRegion. */
struct LegionMemberDAO {
	static inline std::atomic<int32_t> loads{0};
	static inline Monitor connectionPool{AION_LOCK_CLASS(DatabaseFactory::pool)};

	static Ref<LegionMember> loadLegionMember(int32_t playerObjectId) {
		++loads;
		SYNCHRONIZED(connectionPool) {
			BlockingRegion query("LegionMemberDAO.loadLegionMember");
			std::this_thread::sleep_for(std::chrono::microseconds(200));
		}
		return playerObjectId % 10 == 0 ? Ref<LegionMember>() : LegionMember::create(playerObjectId); // not a legion member: null
	}
};

class PlayerService {
public:
	Ptr<PlayerCommonData> getOrLoadPlayerCommonData(int32_t id) {
		return commonData.computeIfAbsent(id, [](const int32_t& key) { return PlayerCommonData::create(key); }); // nested computeIfAbsent, other map
	}

private:
	ConcurrentHashMap<int32_t, Ref<PlayerCommonData>> commonData{AION_LOCK_CLASS(PlayerService::commonData#stripe)};
};

class LegionService {
public:
	explicit LegionService(PlayerService& playerService) : playerService(playerService) {}

	Ptr<LegionMember> getLegionMember(int32_t playerObjectId) {
		return legionMemberById.computeIfAbsent(playerObjectId, [&](const int32_t&) {
			Ref<LegionMember> lm = LegionMemberDAO::loadLegionMember(playerObjectId); // DAO call inside the stripe Monitor
			if (lm)
				lm->setPlayerData(playerService.getOrLoadPlayerCommonData(playerObjectId));
			return lm;
		});
	}
	int32_t cached() const { return legionMemberById.size(); }

private:
	PlayerService& playerService;
	ConcurrentHashMap<int32_t, Ref<LegionMember>> legionMemberById{AION_LOCK_CLASS(LegionService::legionMemberById#stripe)};
};

// ---- SpawnsData.java:83-101 -------------------------------------------------------------------------------------------------------------

class SpawnGroup final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<SpawnGroup> create(int32_t mapId, int32_t npcId) { return makeRef<SpawnGroup>(mapId, npcId); }
	const int32_t mapId;
	const int32_t npcId;

protected:
	SpawnGroup(int32_t mapId, int32_t npcId) : mapId(mapId), npcId(npcId) {}
	~SpawnGroup() override = default;
};

struct Spawn {
	int32_t npcId;
	bool custom;
};

class SpawnsData {
public:
	using SpawnGroups = RcArrayList<Ref<SpawnGroup>>;
	using MapSpawns = RcHashMap<int32_t, Ref<SpawnGroups>>;

	void addRegularSpawns(int32_t mapId, const std::vector<Spawn>& spawns) {
		allSpawnMaps.compute(mapId, [&](const int32_t&, Ptr<MapSpawns> existing) -> Ref<MapSpawns> {
			Ref<MapSpawns> mapSpawns(existing);
			if (!mapSpawns)
				mapSpawns = MapSpawns::create(AION_LOCK_CLASS(SpawnsData::mapSpawns));
			std::vector<int32_t> customs;
			for (const Spawn& spawn : spawns) {
				if (std::ranges::find(customs, spawn.npcId) != customs.end())
					continue;
				if (spawn.custom) {
					(void)mapSpawns->remove(spawn.npcId);
					customs.push_back(spawn.npcId);
				}
				Ptr<SpawnGroups> spawnGroups = mapSpawns->computeIfAbsent(spawn.npcId, [] { return SpawnGroups::create(AION_LOCK_CLASS(SpawnsData::groups)); });
				spawnGroups->add(SpawnGroup::create(mapId, spawn.npcId));
			}
			return mapSpawns;
		});
	}

	int32_t groupCount(int32_t mapId, int32_t npcId) const {
		Ptr<MapSpawns> mapSpawns = allSpawnMaps.get(mapId);
		if (!mapSpawns)
			return 0;
		Ptr<SpawnGroups> groups = mapSpawns->get(npcId);
		return groups ? groups->size() : 0;
	}

private:
	ConcurrentHashMap<int32_t, Ref<MapSpawns>> allSpawnMaps{AION_LOCK_CLASS(SpawnsData::allSpawnMaps#stripe)};
};

// ---- Preview.java:211-227 ---------------------------------------------------------------------------------------------------------------

/** Minimal Future stand-in: cancel is a CAS, the body captures Refs only. */
class FakeFuture final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<FakeFuture> create(std::function<void()> body) { return makeRef<FakeFuture>(std::move(body)); }
	bool cancel() noexcept {
		bool expected = false;
		return cancelled.compare_exchange_strong(expected, true);
	}
	bool isCancelled() const noexcept { return cancelled.load(); }
	void run() {
		if (!isCancelled())
			body();
	}

protected:
	explicit FakeFuture(std::function<void()> body) : body(std::move(body)) {}
	~FakeFuture() override = default;

private:
	std::atomic<bool> cancelled{false};
	std::function<void()> body;
};

/** Scheduler stand-in: tasks are queued under a plain mutex (never held while a task runs) and run by a worker thread. */
class FakeScheduler {
public:
	Ref<FakeFuture> schedule(std::function<void()> body) {
		Ref<FakeFuture> future = FakeFuture::create(std::move(body));
		std::scoped_lock lock(mutex);
		queue.push_back(future);
		return future;
	}
	bool runOne() {
		Ref<FakeFuture> next;
		{
			std::scoped_lock lock(mutex);
			if (queue.empty())
				return false;
			next = std::move(queue.front());
			queue.pop_front();
		}
		next->run();
		return true;
	}

private:
	std::mutex mutex;
	std::deque<Ref<FakeFuture>> queue;
};

class Preview {
public:
	explicit Preview(FakeScheduler& scheduler) : scheduler(scheduler) {}

	void schedulePreviewReset(Player& player) {
		PREVIEW_RESETS.compute(player.getObjectId(), [&](const int32_t&, Ptr<FakeFuture> resetTask) -> Ref<FakeFuture> {
			if (resetTask)
				resetTask->cancel(); // cancel inside the stripe Monitor
			Ref<Player> pinned(player);
			return scheduler.schedule([this, pinned] { // schedule inside the stripe Monitor; the task later removes the key
				++resets;
				(void)PREVIEW_RESETS.remove(pinned->getObjectId());
			});
		});
	}

	ConcurrentHashMap<int32_t, Ref<FakeFuture>> PREVIEW_RESETS{AION_LOCK_CLASS(Preview::PREVIEW_RESETS#stripe)};
	std::atomic<int32_t> resets{0};

private:
	FakeScheduler& scheduler;
};

} // namespace

TEST_F(ComputeConformanceTest, CreatureGameStatsComputeTakesTheListMonitor) {
	CreatureGameStats stats;
	constexpr int32_t THREADS = 4;
	constexpr int32_t EFFECTS = 50;
	runConcurrently(THREADS, [&stats](int32_t thread) {
		for (int32_t effect = 0; effect < EFFECTS; ++effect) {
			int32_t owner = thread * 1000 + effect;
			stats.addEffectOnly({StatFunction::create(StatEnum::MAXHP, (effect * 7) % 13, owner), StatFunction::create(StatEnum::SPEED, effect % 5, owner)});
			if (effect % 2 == 1)
				EXPECT_TRUE(stats.endEffect(owner));
		}
	});
	EXPECT_EQ(stats.count(StatEnum::MAXHP), THREADS * EFFECTS / 2);
	EXPECT_EQ(stats.count(StatEnum::SPEED), THREADS * EFFECTS / 2);
	EXPECT_TRUE(stats.sorted(StatEnum::MAXHP));
	EXPECT_TRUE(stats.sorted(StatEnum::SPEED));
	EXPECT_EQ(stats.count(StatEnum::ATTACK), 0);
}

TEST_F(ComputeConformanceTest, PlayerContainerRenamePutsInsideCompute) {
	PlayerContainer container;
	std::vector<Ref<NamedPlayer>> players;
	constexpr int32_t PLAYERS = 64;
	for (int32_t i = 0; i < PLAYERS; ++i) {
		players.push_back(NamedPlayer::create(i, "old" + std::to_string(i)));
		container.add(*players.back());
	}
	// at least one rename whose old and new names share a stripe, and one where they do not
	bool sameStripe = false;
	bool otherStripe = false;
	for (int32_t i = 0; i < PLAYERS; ++i) {
		bool same = &container.playersByName.stripeMonitor("old" + std::to_string(i)) == &container.playersByName.stripeMonitor("new" + std::to_string(i));
		(same ? sameStripe : otherStripe) = true;
	}
	EXPECT_TRUE(sameStripe);
	EXPECT_TRUE(otherStripe);

	// One renaming thread (two concurrent renames whose names cross two stripes in opposite directions would deadlock, as they can with Java
	// bins; see the collections stage report) plus threads reading and writing unrelated keys of the same map.
	std::atomic<bool> renamed{false};
	runConcurrently(4, [&](int32_t thread) {
		if (thread == 0) {
			for (int32_t i = 0; i < PLAYERS; ++i) {
				std::string oldName = players[static_cast<size_t>(i)]->getName();
				players[static_cast<size_t>(i)]->setName("new" + std::to_string(i));
				container.updateCachedPlayerName(oldName, *players[static_cast<size_t>(i)]);
				EXPECT_EQ(container.get("new" + std::to_string(i)), players[static_cast<size_t>(i)]);
			}
			renamed = true;
			return;
		}
		Ref<NamedPlayer> visitor = NamedPlayer::create(1000 + thread, "visitor" + std::to_string(thread));
		while (!renamed) {
			for (int32_t i = 0; i < PLAYERS; ++i)
				(void)container.get("old" + std::to_string(i)); // lock-free reads racing with the compute callbacks
			container.playersByName.put(visitor->getName(), visitor);
			(void)container.playersByName.remove(visitor->getName());
		}
	});
	for (int32_t i = 0; i < PLAYERS; ++i) {
		EXPECT_FALSE(container.get("old" + std::to_string(i))) << i;
		EXPECT_EQ(container.get("new" + std::to_string(i)), players[i]) << i;
	}
	EXPECT_EQ(container.nameCount(), PLAYERS);

	// renaming to the SAME name puts the computed key: Recursive update (Java only avoids it because names always differ)
	std::string name = players[0]->getName();
	EXPECT_THROW(container.updateCachedPlayerName(name, *players[0]), IllegalStateException);
	EXPECT_EQ(container.get(name), players[0]);
}

TEST_F(ComputeConformanceTest, GaleCycloneObserverAddedOncePerPlayer) {
	Ref<Npc> cyclone = Npc::create(900);
	GaleCycloneAI ai(*cyclone);
	std::vector<Ref<Player>> players;
	std::vector<std::unique_ptr<ObserveController>> controllers;
	for (int32_t i = 0; i < 8; ++i) {
		players.push_back(Player::create(i));
		controllers.push_back(std::make_unique<ObserveController>());
	}
	runConcurrently(4, [&](int32_t) {
		for (int32_t round = 0; round < 20; ++round)
			for (size_t i = 0; i < players.size(); ++i)
				ai.handleCreatureSee(*players[i], *controllers[i]);
	});
	EXPECT_EQ(ai.observedCount(), 8);
	for (const auto& controller : controllers)
		EXPECT_EQ(controller->observerCount(), 1);
	ai.clear();
	for (const auto& controller : controllers)
		controller->clear(); // breaks the observer -> player/npc edges
}

TEST_F(ComputeConformanceTest, LegionServiceCallsDaoAndNestedComputeInsideComputeIfAbsent) {
	LegionMemberDAO::loads = 0;
	PlayerService playerService;
	LegionService legionService(playerService);
	constexpr int32_t IDS = 40;
	runConcurrently(4, [&](int32_t) {
		for (int32_t id = 1; id <= IDS; ++id) {
			Ptr<LegionMember> member = legionService.getLegionMember(id);
			if (id % 10 == 0) {
				EXPECT_FALSE(member);
			} else {
				ASSERT_TRUE(member);
				EXPECT_EQ(member->objectId, id);
				EXPECT_TRUE(member->hasPlayerData());
			}
		}
	});
	EXPECT_EQ(legionService.cached(), IDS - IDS / 10);
	// members are loaded exactly once per id (the callback runs under the stripe Monitor); null results are not cached and reload each time
	EXPECT_EQ(LegionMemberDAO::loads.load(), (IDS - IDS / 10) + 4 * (IDS / 10));
}

TEST_F(ComputeConformanceTest, SpawnsDataNestedComputeOnInnerMaps) {
	SpawnsData data;
	runConcurrently(4, [&data](int32_t thread) {
		for (int32_t mapId = 0; mapId < 5; ++mapId) {
			std::vector<Spawn> spawns;
			for (int32_t npc = 0; npc < 10; ++npc)
				spawns.push_back({1000 + npc, false});
			data.addRegularSpawns(mapId, spawns);
			if (thread == 0 && mapId == 4)
				data.addRegularSpawns(mapId, {{5000, true}, {5000, true}, {5001, false}}); // custom: remove + computeIfAbsent, duplicate skipped
		}
	});
	for (int32_t mapId = 0; mapId < 5; ++mapId)
		for (int32_t npc = 0; npc < 10; ++npc)
			EXPECT_EQ(data.groupCount(mapId, 1000 + npc), 4) << mapId << " " << npc;
	EXPECT_EQ(data.groupCount(4, 5000), 1);
	EXPECT_EQ(data.groupCount(4, 5001), 1);
	EXPECT_EQ(data.groupCount(3, 5000), 0);
}

TEST_F(ComputeConformanceTest, PreviewCancelsAndSchedulesInsideCompute) {
	FakeScheduler scheduler;
	Preview preview(scheduler);
	std::vector<Ref<Player>> players;
	for (int32_t i = 0; i < 6; ++i)
		players.push_back(Player::create(i));
	std::atomic<bool> producersDone{false};
	std::thread executor([&] {
		for (;;) {
			TaskScope scope(AION_TASK_INFO(TaskKind::SCHEDULED));
			if (!scheduler.runOne()) {
				if (producersDone)
					break;
				std::this_thread::yield();
			}
		}
		TaskScope scope(AION_TASK_INFO(TaskKind::SCHEDULED));
		while (scheduler.runOne()) {
		}
	});
	runConcurrently(3, [&](int32_t thread) {
		for (int32_t round = 0; round < 100; ++round)
			preview.schedulePreviewReset(*players[static_cast<size_t>((round + thread) % 6)]);
	});
	producersDone = true;
	executor.join();
	while (scheduler.runOne()) { // the executor drained the queue before it stopped; nothing is left
		ADD_FAILURE() << "task left in the queue";
	}
	EXPECT_GE(preview.resets.load(), 6);
	EXPECT_LE(preview.resets.load(), 300);
	EXPECT_TRUE(preview.PREVIEW_RESETS.isEmpty()); // every uncancelled reset removed its key
}
