#pragma once

// =====================================================================================================================================
// PROTOTYPE BENCHMARK CODE (design runtime-architecture.md §19 P4). Not part of the game server.
// =====================================================================================================================================
//
// Interface of the P4 movement / known-list microbenchmark scenario. The prototype model (ProtoModel.inl) is compiled twice, once with
// lock-free ConcurrentHashMap reads (namespace lockfree) and once with stripe-locked reads (namespace lockedreads, the AION_CHM_LOCKED_READS
// fallback of design §17), so both variants run the identical model code. A process runs exactly one scenario: ThreadPoolManager cannot be
// restarted after shutdown.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "p4/BenchSupport.h"

namespace aion::gameserver::bench {

/** Scenario parameters (defaults = design §19 P4). */
struct Options {
	/** Npc shells spawned at startup (movers included, flags excluded) */
	int32_t npcs = 80'000;
	/** walkers ticking in MoveTaskManager */
	int32_t movers = 2'000;
	/** Player shells; they activate map regions and receive broadcasts through the fake connections */
	int32_t players = 200;
	/** fake connections (byte-buffer sinks); players are attached round-robin */
	int32_t connections = 5;
	/** world map instances; 80k npcs on 16 maps of 2048 m is the density of one 8192 m map (1 npc / 840 m²) with realistic per-instance npc counts */
	int32_t maps = 16;
	int32_t mapSize = 2048;
	int32_t flagsPerMap = 2;
	int32_t playersPerCluster = 10;
	/** radius (m) around a cluster center in which movers spawn; players stay within 60 m */
	float moverSpawnRadius = 250.0f;
	float moverSpeed = 6.0f;
	float routeRadius = 40.0f;
	/** false (default): walker speed x0.7-1.3, route radius x0.5-1.5 and first route step are randomized, so arrivals spread over ticks.
	 *  true: identical walkers started in the same tick all arrive in the same tick (thundering herd of updateKnownlist every ~47 ticks) */
	bool synchronizedWalkers = false;

	int64_t periodMillis = 200;
	int32_t warmupTicks = 10;
	int32_t ticks = 300;
	/** ticks run with PCT counting hooks after the measured ticks (checked builds; excluded from latency statistics) */
	int32_t countTicks = 10;

	/** npc despawn + respawn (new object) per second; half of them hit movers */
	int32_t churnPerSecond = 100;
	/** players replaced by a new Player object per second (logout + enter world): keeps pair-add patterns 2 and 3 running */
	int32_t playerRelogsPerSecond = 2;
	/** one-time MOVE observers attached to random movers per second (iterator.remove path) */
	int32_t oneTimeObserversPerSecond = 50;
	/** fraction of movers created with a persistent MOVE observer (the rest take the isEmpty fast path) */
	double observerRatio = 0.1;
	/** fraction of players whose known list is updated per tick (players move every tick) */
	double playerUpdateFraction = 0.2;
	/** true: SM_MOVE every move step (conservative); false: Java NpcMoveController rule (mask or destination change) */
	bool broadcastEveryTick = true;

	/** gameserver.debug.serial_movement */
	bool serialMovement = false;
	/** AION_CHM_LOCKED_READS variant */
	bool lockedReads = false;
	/** WorldMapInstance.regions as an immutable flat array instead of the mechanical PartMap (fieldmap.toml override candidate) */
	bool flatRegions = false;
	/** Java instanceof checks through a virtual kind query instead of runtime::as<> (dynamic_cast), to measure the cast cost */
	bool fastInstanceof = false;
	/** stress knob: microseconds the despawning thread waits between setIsSpawned(false) and the region removal, widening the race window
	 *  that the addPair handshake closes (design §5.3); 0 = none */
	int32_t despawnWindowMicros = 0;
	uint64_t seed = 42;
	/** Reclaimer::Config::delayedFreeBytes in MB (checked builds, C3); -1 keeps the kernel default (64 MB) */
	int32_t delayedFreeMB = -1;
};

struct NamedValue {
	std::string name;
	std::string value;
};

struct Section {
	std::string title;
	std::vector<NamedValue> rows;
};

struct Results {
	bool completed = false;
	std::string failure;

	// pass criteria (design §19 P4): tick p99 < 50 ms in the checked build, no KnownList ghosts
	double tickP50 = 0;
	double tickP95 = 0;
	double tickP99 = 0;
	double tickMax = 0;
	double tickMean = 0;
	size_t ticksMeasured = 0;
	uint64_t ghosts = 0;
	uint64_t leakedShells = 0;
	size_t lockdepCycles = 0;

	/** everything else, printed in order */
	std::vector<Section> sections;
};

namespace lockfree {
Results runScenario(const Options& options);
}
namespace lockedreads {
Results runScenario(const Options& options);
}

} // namespace aion::gameserver::bench
