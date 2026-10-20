#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::custom::instance {

/**
 * The DPS report of one Eternal Challenge run (RoahCustomInstanceHandler): the Aether Stone phase as the handler measures it, the boss phase
 * from the boss's spawn to its death, and the whole run from the fly ring. It builds the texts the handler sends to the player's chat.
 * <p>
 * C++-dev only, not in Java (docs/deviations/P5-13.md, "C++-dev only: the Eternal Challenge DPS report"). The clock is a parameter so the
 * tests control it; the handler uses the wall clock (Java's System.currentTimeMillis, as for its own start time).
 * <p>
 * Thread-safety: every member is a Field, as the handler's own phase fields (Java's plain fields): a phase step and a concurrent read see
 * whole values; as in the handler, two phase steps at the same moment (the time limit and the boss's death) may both land. The handler owns
 * it as a part (const unique_ptr).
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
	/** the boss spawned: the boss phase starts */
	void bossSpawned();
	/** the boss died: its max HP over the boss phase's time is the boss DPS */
	void bossKilled(int64_t bossMaxHp);
	/** the run ended without the boss's death (time limit, death, leaving): freezes the total time, once */
	void runEnded();

	bool isRunStarted() const { return runStart.get().has_value(); }
	bool isArtifactDestroyed() const { return artifactSeconds.get().has_value(); }
	bool isBossKilled() const { return bossDps.get().has_value(); }
	/** the boss phase's time in seconds (0 before the boss died) */
	float getBossSeconds() const { return bossSeconds.get().value_or(0.0f); }
	/** the boss DPS (0 before the boss died) */
	int32_t getBossDps() const { return bossDps.get().value_or(0); }
	/** the run's time in seconds until it ended (the boss's death or runEnded), or until now while it lasts (0 before the run started) */
	float getTotalSeconds() const;

	/** "Aether Stone destroyed in 312.4s: 9615 DPS (class average 4000, prebuffed: no)." */
	std::string artifactMessage() const;
	/**
	 * "[Eternal Challenge] Success | Aether Stone: 312.4s, 9615 DPS (class average 4000) | Boss: 45.2s, 2851 DPS | Total: 357.6s | Rank: Iron
	 * -> Bronze"
	 */
	std::string successReport(std::string_view oldRank, std::string_view newRank) const;
	/**
	 * "[Eternal Challenge] Failed (reason) | Aether Stone: 312.4s, 9615 DPS (class average 4000) | Time spent: 900.0s", or "Aether Stone: not
	 * destroyed" when the run ended in the first phase
	 */
	std::string failureReport(std::string_view reason) const;

	/** maxHp / seconds rounded half up (Java's Math.round(float), as the handler's artifact DPS); a phase under 1 ms counts as 1 ms */
	static int32_t dps(int64_t maxHp, int64_t millis);
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
	runtime::Field<std::optional<int64_t>> bossStart{};
	runtime::Field<std::optional<int64_t>> runEnd{};
	runtime::Field<std::optional<float>> bossSeconds{};
	runtime::Field<std::optional<int32_t>> bossDps{};
};

} // namespace aion::gameserver::custom::instance
