// =====================================================================================================================================
// PROTOTYPE BENCHMARK (design runtime-architecture.md §19 P4): movement / known-list microbenchmark on the runtime kernel.
// =====================================================================================================================================
//
// aion_gs_bench [options]
//   Runs one scenario (80k Npc shells, 2,000 walkers ticking every 200 ms on the ForkJoin pool, spawn/despawn churn, player and flag known
//   list updates, eager SM_MOVE broadcasts to fake connections, observer notifications) and prints tick latency, allocation and refcount
//   traffic, reclamation lag, bytes per Npc, contention and correctness checks (KnownList ghosts, leaks, lockdep).
//   Pass (design §19 P4): tick p99 < 50 ms in the checked build (RelWithDebInfo), no KnownList ghosts, no leaked shells.
//   Exit code: 0 pass, 1 criteria failed, 2 bad arguments or scenario error.
// Build it in RelWithDebInfo (checked) and Release (unchecked); not registered with ctest. Run one scenario per process.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <format>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/base/Checked.h"
#include "p4/Scenario.h"

namespace {

using aion::gameserver::bench::Options;
using aion::gameserver::bench::Results;

constexpr double P99_LIMIT_MS = 50.0;

const char* buildName() {
#if AION_CHECKED && defined(NDEBUG)
	return "RelWithDebInfo (checked, optimized)";
#elif AION_CHECKED
	return "Debug (checked)";
#else
	return "Release (unchecked)";
#endif
}

void printUsage() {
	std::puts(
		"usage: aion_gs_bench [options]\n"
		"  --npcs N                 Npc shells (default 80000)\n"
		"  --movers N               walkers in MoveTaskManager (default 2000)\n"
		"  --players N              player shells (default 200)\n"
		"  --connections N          fake connections (default 5)\n"
		"  --maps N / --map-size M  world map instances and their size in meters (default 16 / 2048)\n"
		"  --flags-per-map N        flag npcs per map (default 2)\n"
		"  --period MS              tick period (default 200)\n"
		"  --warmup N / --ticks N   warmup and measured ticks (default 10 / 300)\n"
		"  --count-ticks N          ticks with kernel counting hooks, checked builds (default 10)\n"
		"  --churn N                npc respawns per second (default 100)\n"
		"  --relogs N               player relogs (new Player object) per second (default 2)\n"
		"  --one-time-observers N   one-time MOVE observers attached per second (default 50)\n"
		"  --observer-ratio X       movers with a persistent MOVE observer (default 0.1)\n"
		"  --player-updates X       fraction of players updating their known list per tick (default 0.2)\n"
		"  --speed M_PER_S          walker speed (default 6)\n"
		"  --java-mask-rule         SM_MOVE only on mask/destination change (default: every move step)\n"
		"  --synchronized-walkers   identical walkers started together (all arrive in the same tick: worst case)\n"
		"  --serial                 serial movement (gameserver.debug.serial_movement)\n"
		"  --locked-reads           stripe-locked ConcurrentHashMap reads (AION_CHM_LOCKED_READS variant)\n"
		"  --flat-regions           immutable flat region index instead of the PartMap lookup\n"
		"  --fast-instanceof        virtual kind query instead of runtime::as<> (dynamic_cast) for Java instanceof\n"
		"  --seed N                 layout and churn seed (default 42)\n"
		"  --delayed-free-mb N      checked-build delayed-free FIFO size in MB (default: kernel default, 64)\n"
		"  --profile                inclusive phase timers (adds timer overhead; combine with --serial)\n"
		"  --despawn-window-us N    stress: wait N us between setIsSpawned(false) and region removal (exercises the addPair handshake)\n"
		"  --quick                  short run: 5 warmup, 50 measured, 5 counted ticks");
}

bool parseArguments(int argc, char** argv, Options& options) {
	for (int i = 1; i < argc; ++i) {
		std::string_view arg = argv[i];
		auto next = [&](const char* name) -> const char* {
			if (i + 1 >= argc) {
				std::fprintf(stderr, "missing value for %s\n", name);
				return nullptr;
			}
			return argv[++i];
		};
		auto intValue = [&](const char* name, int32_t& target) {
			const char* value = next(name);
			if (value == nullptr)
				return false;
			target = static_cast<int32_t>(std::strtol(value, nullptr, 10));
			return true;
		};
		auto doubleValue = [&](const char* name, double& target) {
			const char* value = next(name);
			if (value == nullptr)
				return false;
			target = std::strtod(value, nullptr);
			return true;
		};
		bool ok = true;
		if (arg == "--help" || arg == "-h") {
			printUsage();
			std::exit(0);
		} else if (arg == "--npcs") {
			ok = intValue("--npcs", options.npcs);
		} else if (arg == "--movers") {
			ok = intValue("--movers", options.movers);
		} else if (arg == "--players") {
			ok = intValue("--players", options.players);
		} else if (arg == "--connections") {
			ok = intValue("--connections", options.connections);
		} else if (arg == "--maps") {
			ok = intValue("--maps", options.maps);
		} else if (arg == "--map-size") {
			ok = intValue("--map-size", options.mapSize);
		} else if (arg == "--flags-per-map") {
			ok = intValue("--flags-per-map", options.flagsPerMap);
		} else if (arg == "--period") {
			int32_t period = 0;
			ok = intValue("--period", period);
			options.periodMillis = period;
		} else if (arg == "--warmup") {
			ok = intValue("--warmup", options.warmupTicks);
		} else if (arg == "--ticks") {
			ok = intValue("--ticks", options.ticks);
		} else if (arg == "--count-ticks") {
			ok = intValue("--count-ticks", options.countTicks);
		} else if (arg == "--churn") {
			ok = intValue("--churn", options.churnPerSecond);
		} else if (arg == "--relogs") {
			ok = intValue("--relogs", options.playerRelogsPerSecond);
		} else if (arg == "--one-time-observers") {
			ok = intValue("--one-time-observers", options.oneTimeObserversPerSecond);
		} else if (arg == "--observer-ratio") {
			ok = doubleValue("--observer-ratio", options.observerRatio);
		} else if (arg == "--player-updates") {
			ok = doubleValue("--player-updates", options.playerUpdateFraction);
		} else if (arg == "--speed") {
			double speed = 0;
			ok = doubleValue("--speed", speed);
			options.moverSpeed = static_cast<float>(speed);
		} else if (arg == "--java-mask-rule") {
			options.broadcastEveryTick = false;
		} else if (arg == "--synchronized-walkers") {
			options.synchronizedWalkers = true;
		} else if (arg == "--serial") {
			options.serialMovement = true;
		} else if (arg == "--locked-reads") {
			options.lockedReads = true;
		} else if (arg == "--despawn-window-us") {
			ok = intValue("--despawn-window-us", options.despawnWindowMicros);
		} else if (arg == "--fast-instanceof") {
			options.fastInstanceof = true;
		} else if (arg == "--flat-regions") {
			options.flatRegions = true;
		} else if (arg == "--delayed-free-mb") {
			ok = intValue("--delayed-free-mb", options.delayedFreeMB);
		} else if (arg == "--seed") {
			const char* value = next("--seed");
			ok = value != nullptr;
			if (ok)
				options.seed = std::strtoull(value, nullptr, 10);
		} else if (arg == "--profile") {
			aion::gameserver::bench::phaseProfiling = true;
		} else if (arg == "--quick") {
			options.warmupTicks = 5;
			options.ticks = 50;
			options.countTicks = 5;
		} else {
			std::fprintf(stderr, "unknown option: %s\n", argv[i]);
			ok = false;
		}
		if (!ok)
			return false;
	}
	if (options.npcs < options.movers || options.movers < 0 || options.maps <= 0 || options.mapSize < 256 || options.periodMillis <= 0 ||
		options.ticks <= 0 || options.warmupTicks < 1 || options.players < 0 || options.connections < 1) {
		std::fprintf(stderr, "invalid option values (need npcs >= movers >= 0, maps > 0, map-size >= 256, period > 0, ticks > 0, warmup >= 1, "
												 "connections >= 1)\n");
		return false;
	}
	return true;
}

} // namespace

int main(int argc, char** argv) {
	Options options;
	if (!parseArguments(argc, argv, options)) {
		printUsage();
		return 2;
	}

	std::printf("=== P4 movement / known-list microbenchmark (PROTOTYPE shells, design runtime-architecture.md §19 P4) ===\n");
	std::printf("build: %s\n", buildName());
	std::printf("scenario: %d npcs, %d movers, %d players, %d connections, %d maps x %d m, %lld ms period, %d warmup + %d measured ticks, "
							"churn %d/s, SM_MOVE %s, walkers %s, movement %s, CHM reads %s, regions %s\n\n",
		options.npcs, options.movers, options.players, options.connections, options.maps, options.mapSize,
		static_cast<long long>(options.periodMillis), options.warmupTicks, options.ticks, options.churnPerSecond,
		options.broadcastEveryTick ? "every step" : "Java rule", options.synchronizedWalkers ? "synchronized" : "randomized",
		options.serialMovement ? "serial" : "parallel",
		options.lockedReads ? "stripe-locked" : "lock-free", options.flatRegions ? "flat" : "PartMap");
	std::fflush(stdout);

	Results results;
	try {
		results = options.lockedReads ? aion::gameserver::bench::lockedreads::runScenario(options)
																	: aion::gameserver::bench::lockfree::runScenario(options);
	} catch (const std::exception& e) {
		std::fprintf(stderr, "scenario failed: %s\n", e.what());
		return 2;
	}

	for (const aion::gameserver::bench::Section& section : results.sections) {
		std::printf("--- %s\n", section.title.c_str());
		for (const aion::gameserver::bench::NamedValue& row : section.rows)
			std::printf("  %-78s %s\n", row.name.c_str(), row.value.c_str());
		std::printf("\n");
	}

	bool checked = aion::gameserver::runtime::CHECKED;
	bool latencyOk = results.tickP99 < P99_LIMIT_MS;
	bool ghostsOk = results.ghosts == 0 && results.leakedShells == 0;
	bool pass = results.completed && ghostsOk && (latencyOk || !checked);
	std::printf("=== RESULT: %s\n", pass ? "PASS" : "FAIL");
	std::printf("  tick p99 %.3f ms (limit %.0f ms, %s)%s\n", results.tickP99, P99_LIMIT_MS, latencyOk ? "ok" : "exceeded",
		checked ? "" : " - the latency criterion applies to the checked build");
	std::printf("  known-list ghosts %llu, leaked shells %llu, lockdep cycles %zu%s%s\n", static_cast<unsigned long long>(results.ghosts),
		static_cast<unsigned long long>(results.leakedShells), results.lockdepCycles, results.completed ? "" : ", scenario incomplete: ",
		results.failure.c_str());
	return pass ? 0 : 1;
}
