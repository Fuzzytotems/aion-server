// C++-dev only (owner, 2026-10-19): the Eternal Challenge DPS report (custom/instance/RoahDpsReport), with a clock the test sets. The texts are
// the formats of docs/deviations/P5-13.md, "C++-dev only: the Eternal Challenge DPS report".

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "aion/gameserver/custom/instance/RoahDpsReport.h"

namespace aion::gameserver::custom::instance {
namespace {

class RoahDpsReportTest : public ::testing::Test {
protected:
	int64_t now = 1'700'000'000'000LL;
	RoahDpsReport report{[this] { return now; }};

	void advance(int64_t millis) { now += millis; }
};

TEST_F(RoahDpsReportTest, DpsIsMaxHpOverTheSecondsRoundedHalfUp) {
	EXPECT_EQ(RoahDpsReport::dps(1'200'000, 300'000), 4000);
	EXPECT_EQ(RoahDpsReport::dps(1'000, 3'000), 333) << "333.33 rounds down";
	EXPECT_EQ(RoahDpsReport::dps(1'000, 1'500), 667) << "666.67 rounds up";
	EXPECT_EQ(RoahDpsReport::dps(5, 2'000), 3) << "2.5 rounds half up";
	EXPECT_EQ(RoahDpsReport::dps(128, 0), 128'000) << "a phase under 1 ms counts as 1 ms (float arithmetic, as Java's artifact DPS)";
	EXPECT_EQ(RoahDpsReport::dps(10, 1), 10'000);
	EXPECT_EQ(RoahDpsReport::seconds(312.44f), "312.4s");
	EXPECT_EQ(RoahDpsReport::seconds(0.05f), "0.1s");
}

TEST_F(RoahDpsReportTest, TheArtifactMessageTellsTimeDpsClassAverageAndPrebuff) {
	report.runStarted();
	report.artifactDestroyed(312.4f, 3841, 4000, false);
	EXPECT_EQ(report.artifactMessage(), "Aether Stone destroyed in 312.4s: 3841 DPS (class average 4000, prebuffed: no).");
	report.artifactDestroyed(250.0f, 4800, 5000, true);
	EXPECT_EQ(report.artifactMessage(), "Aether Stone destroyed in 250.0s: 4800 DPS (class average 5000, prebuffed: yes).");
}

TEST_F(RoahDpsReportTest, ASuccessReportsBothPhasesTheTotalAndTheRank) {
	report.runStarted();
	advance(312'400);
	report.artifactDestroyed(312.4f, 3841, 4000, false);
	advance(600); // the boss spawns a moment after the artifact's death
	report.bossSpawned();
	advance(45'200);
	report.bossKilled(128'881);
	EXPECT_TRUE(report.isBossKilled());
	EXPECT_FLOAT_EQ(report.getBossSeconds(), 45.2f);
	EXPECT_EQ(report.getBossDps(), 2851) << "128881 / 45.2 = 2851.35";
	EXPECT_FLOAT_EQ(report.getTotalSeconds(), 358.2f);
	advance(60'000);
	EXPECT_FLOAT_EQ(report.getTotalSeconds(), 358.2f) << "the run ended with the boss";
	EXPECT_EQ(report.successReport("Iron", "Bronze"), "[Eternal Challenge] Success | Aether Stone: 312.4s, 3841 DPS (class average 4000) | Boss: "
													  "45.2s, 2851 DPS | Total: 358.2s | Rank: Iron -> Bronze");
}

TEST_F(RoahDpsReportTest, AFailureInTheBossPhaseReportsTheArtifactAndTheTimeSpent) {
	report.runStarted();
	advance(250'000);
	report.artifactDestroyed(250.0f, 4800, 4000, false);
	report.bossSpawned();
	advance(650'000);
	EXPECT_FLOAT_EQ(report.getTotalSeconds(), 900.0f) << "while the run lasts: until now";
	report.runEnded();
	advance(5'000);
	report.runEnded();
	EXPECT_EQ(report.failureReport("time limit reached"),
		"[Eternal Challenge] Failed (time limit reached) | Aether Stone: 250.0s, 4800 DPS (class average 4000) | Time spent: 900.0s")
		<< "the first runEnded freezes the time";
	EXPECT_FALSE(report.isBossKilled());
	EXPECT_EQ(report.getBossDps(), 0);
}

TEST_F(RoahDpsReportTest, AFailureInTheArtifactPhaseReportsTheTimeSpentOnly) {
	EXPECT_FALSE(report.isRunStarted());
	EXPECT_FLOAT_EQ(report.getTotalSeconds(), 0.0f);
	report.runStarted();
	EXPECT_TRUE(report.isRunStarted());
	advance(83'270);
	report.runEnded();
	EXPECT_FALSE(report.isArtifactDestroyed());
	EXPECT_EQ(report.failureReport("you died"), "[Eternal Challenge] Failed (you died) | Aether Stone: not destroyed | Time spent: 83.3s");
}

} // namespace
} // namespace aion::gameserver::custom::instance
