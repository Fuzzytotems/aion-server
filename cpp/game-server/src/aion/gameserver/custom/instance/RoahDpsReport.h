#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::custom::instance {

/**
 * The DPS report of one Eternal Challenge run (RoahCustomInstanceHandler): the Aether Stone phase as the handler measures it, the boss phase
 * from the boss's spawn to its death, and the whole run from the fly ring. It builds the texts the handler sends to the player's chat.
 * <p>
 * The boss fight is timed from the boss's engagement (bossEngaged: the first time the boss hated the player, by sight or by a hit) to its
 * death; the time from the spawn to the engagement is the preparation (resting, rebuffing, walking up), reported apart.
 * <p>
 * Besides the DPS on the stone and on the boss (their max HP over the phase's time), it sums the damage the player (with his summons and
 * servants) dealt to every npc of the instance per phase (addDamage, from the npcs' aggro lists): the total DPS of a phase and of the run.
 * <p>
 * C++-dev only, not in Java (docs/deviations/P5-13.md, "C++-dev only: the Eternal Challenge DPS report"). The clock is a parameter so the
 * tests control it; the handler uses the wall clock (Java's System.currentTimeMillis, as for its own start time).
 * <p>
 * Thread-safety: every member is a Field, as the handler's own phase fields (Java's plain fields): a phase step and a concurrent read see
 * whole values; as in the handler, two phase steps at the same moment (the time limit and the boss's death) may both land. The damage sums
 * are AtomicLongs (two npcs may die at once). The handler owns it as a part (const unique_ptr).
 */
class RoahDpsReport {
public:
	/** milliseconds since the epoch */
	using Clock = std::function<int64_t()>;

	explicit RoahDpsReport(Clock clock);
	RoahDpsReport(const RoahDpsReport&) = delete;
	RoahDpsReport& operator=(const RoahDpsReport&) = delete;

	/** the player passed the entry ring: the run and the Aether Stone phase start */
	void runStarted();
	/** the Aether Stone died: the handler's measured time (seconds) and DPS, the class average it compares with, the prebuff flag */
	void artifactDestroyed(float seconds, int32_t dps, int32_t classAverageDps, bool wasPrebuffed);
	/** the boss spawned: the boss phase (its damage) and the preparation start */
	void bossSpawned();
	/** the boss hated the player (sight or hit): the first call after the spawn starts the fight's clock; later calls change nothing */
	void bossEngaged();
	/** the boss died: its max HP over the fight's time (from bossEngaged, or the spawn if it never engaged) is the boss DPS */
	void bossKilled(int64_t bossMaxHp);
	/** the run ended without the boss's death (time limit, death, leaving): freezes the total time, once */
	void runEnded();
	/** damage the player dealt to one npc: it counts for the boss phase once the boss spawned, before that for the Aether Stone phase */
	void addDamage(int64_t damage);

	bool isRunStarted() const { return runStart.get().has_value(); }
	bool isArtifactDestroyed() const { return artifactSeconds.get().has_value(); }
	bool isBossEngaged() const { return bossStart.get().has_value(); }
	bool isBossKilled() const { return bossDps.get().has_value(); }
	/** the boss fight's time in seconds, from the engagement (0 before the boss died) */
	float getBossSeconds() const { return bossSeconds.get().value_or(0.0f); }
	/** the preparation's time in seconds, from the boss's spawn to its engagement (0 before the boss died) */
	float getPrepSeconds() const { return prepSeconds.get().value_or(0.0f); }
	/** the boss DPS (0 before the boss died) */
	int32_t getBossDps() const { return bossDps.get().value_or(0); }
	/** the run's time in seconds until it ended (the boss's death or runEnded), or until now while it lasts (0 before the run started) */
	float getTotalSeconds() const;
	/** the damage summed for the Aether Stone phase / the boss phase */
	int64_t getArtifactPhaseDamage() const { return artifactPhaseDamage.get(); }
	int64_t getBossPhaseDamage() const { return bossPhaseDamage.get(); }
	/** the Aether Stone phase's damage over its time (0 before the stone died) */
	int32_t getArtifactPhaseTotalDps() const;
	/** the boss phase's damage over the fight's time (0 before the boss died) */
	int32_t getBossPhaseTotalDps() const;
	/** both phases' damage over the run's time (getTotalSeconds) */
	int32_t getRunTotalDps() const;

	/** "Aether Stone destroyed in 312.4s: 3841 DPS on the stone, 4102 total DPS (class average 4000, prebuffed: no)." */
	std::string artifactMessage() const;
	/**
	 * "[Eternal Challenge] Success | Aether Stone: 312.4s, 3841 DPS on the stone, 4102 total DPS (class average 4000) | Boss: 45.2s (prep 60.3s),
	 * 2851 DPS on the boss, 2851 total DPS | Total: 418.5s, 3937 total DPS | Rank: Iron -> Bronze"
	 */
	std::string successReport(std::string_view oldRank, std::string_view newRank) const;
	/**
	 * "[Eternal Challenge] Failed (reason) | Aether Stone: 312.4s, 3841 DPS on the stone, 4102 total DPS (class average 4000) | Time spent:
	 * 900.0s, 1500 total DPS", or "Aether Stone: not destroyed" when the run ended in the first phase
	 */
	std::string failureReport(std::string_view reason) const;

	/** damage (or max HP) / seconds rounded half up (Java's Math.round(float), as the handler's artifact DPS); a phase under 1 ms is 1 ms */
	static int32_t dps(int64_t damage, int64_t millis);
	/** seconds with one decimal, e.g. "312.4s" */
	static std::string seconds(float value);

private:
	std::string artifactPart() const;

	const Clock clock;
	runtime::Field<std::optional<int64_t>> runStart{};
	runtime::Field<std::optional<float>> artifactSeconds{};
	runtime::Field<int32_t> artifactDps{};
	runtime::Field<int32_t> avgClassDps{};
	runtime::Field<bool> prebuffed{};
	runtime::Field<std::optional<int64_t>> bossSpawn{};
	runtime::Field<std::optional<int64_t>> bossStart{};
	runtime::Field<std::optional<int64_t>> runEnd{};
	runtime::Field<std::optional<float>> bossSeconds{};
	runtime::Field<std::optional<float>> prepSeconds{};
	runtime::Field<std::optional<int32_t>> bossDps{};
	runtime::AtomicLong artifactPhaseDamage{AION_LOCK_CLASS(RoahDpsReport::artifactPhaseDamage)};
	runtime::AtomicLong bossPhaseDamage{AION_LOCK_CLASS(RoahDpsReport::bossPhaseDamage)};
};

} // namespace aion::gameserver::custom::instance
