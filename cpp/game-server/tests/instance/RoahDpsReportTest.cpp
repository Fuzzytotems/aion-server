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

TEST_F(RoahDpsReportTest, DpsIsDamageOverTheSecondsRoundedHalfUp) {
	EXPECT_EQ(RoahDpsReport::dps(1'200'000, 300'000), 4000);
	EXPECT_EQ(RoahDpsReport::dps(1'000, 3'000), 333) << "333.33 rounds down";
	EXPECT_EQ(RoahDpsReport::dps(1'000, 1'500), 667) << "666.67 rounds up";
	EXPECT_EQ(RoahDpsReport::dps(5, 2'000), 3) << "2.5 rounds half up";
	EXPECT_EQ(RoahDpsReport::dps(128, 0), 128'000) << "a phase under 1 ms counts as 1 ms (float arithmetic, as Java's artifact DPS)";
	EXPECT_EQ(RoahDpsReport::dps(10, 1), 10'000);
	EXPECT_EQ(RoahDpsReport::seconds(312.44f), "312.4s");
	EXPECT_EQ(RoahDpsReport::seconds(0.05f), "0.1s");
}

TEST_F(RoahDpsReportTest, DamageCountsForTheArtifactPhaseUntilTheBossSpawnsThenForTheBossPhase) {
	report.runStarted();
	report.addDamage(1'000);
	report.addDamage(250);
	EXPECT_EQ(report.getArtifactPhaseDamage(), 1'250) << "the sum of the npcs' damage";
	EXPECT_EQ(report.getBossPhaseDamage(), 0);
	report.artifactDestroyed(10.0f, 100, 4000, false);
	report.addDamage(40); // a protector still alive when the stone died: before the boss spawns, still the stone's phase
	report.bossSpawned();
	report.addDamage(7);
	report.addDamage(3);
	EXPECT_EQ(report.getArtifactPhaseDamage(), 1'290);
	EXPECT_EQ(report.getBossPhaseDamage(), 10);
	EXPECT_EQ(report.getArtifactPhaseTotalDps(), 129) << "1290 over 10 s";
	EXPECT_EQ(report.getBossPhaseTotalDps(), 0) << "before the boss died";
}

TEST_F(RoahDpsReportTest, TheArtifactMessageTellsTimeDpsTotalDpsClassAverageAndPrebuff) {
	report.runStarted();
	report.addDamage(1'200'000); // the stone's own HP
	report.addDamage(81'500);    // the protectors
	report.artifactDestroyed(312.4f, 3841, 4000, false);
	EXPECT_EQ(report.getArtifactPhaseTotalDps(), 4102) << "1281500 / 312.4 = 4102.11";
	EXPECT_EQ(report.artifactMessage(), "Aether Stone destroyed in 312.4s: 3841 DPS on the stone, 4102 total DPS (class average 4000, prebuffed: no).");
	report.artifactDestroyed(250.0f, 4800, 5000, true);
	EXPECT_EQ(report.artifactMessage(), "Aether Stone destroyed in 250.0s: 4800 DPS on the stone, 5126 total DPS (class average 5000, prebuffed: yes).");
}

TEST_F(RoahDpsReportTest, ASuccessReportsBothPhasesWithTheirTotalsTheRunTotalAndTheRank) {
	report.runStarted();
	advance(312'400);
	report.addDamage(1'200'000);
	report.addDamage(81'500);
	report.artifactDestroyed(312.4f, 3841, 4000, false);
	advance(600); // the boss spawns a moment after the artifact's death
	report.bossSpawned();
	advance(45'200);
	report.addDamage(128'881); // the boss
	report.addDamage(9'119);   // an add the boss summoned
	report.bossKilled(128'881);
	EXPECT_TRUE(report.isBossKilled());
	EXPECT_FLOAT_EQ(report.getBossSeconds(), 45.2f);
	EXPECT_EQ(report.getBossDps(), 2851) << "128881 / 45.2 = 2851.35";
	EXPECT_EQ(report.getBossPhaseTotalDps(), 3053) << "138000 / 45.2 = 3053.10: the add counts";
	EXPECT_FLOAT_EQ(report.getTotalSeconds(), 358.2f);
	EXPECT_EQ(report.getRunTotalDps(), 3963) << "1419500 / 358.2 = 3962.87";
	advance(60'000);
	EXPECT_FLOAT_EQ(report.getTotalSeconds(), 358.2f) << "the run ended with the boss";
	EXPECT_EQ(report.successReport("Iron", "Bronze"),
		"[Eternal Challenge] Success | Aether Stone: 312.4s, 3841 DPS on the stone, 4102 total DPS (class average 4000) | Boss: 45.2s, 2851 DPS on "
		"the boss, 3053 total DPS | Total: 358.2s, 3963 total DPS | Rank: Iron -> Bronze");
}

TEST_F(RoahDpsReportTest, AFailureInTheBossPhaseReportsTheArtifactAndTheTimeSpentWithTheRunTotal) {
	report.runStarted();
	advance(250'000);
	report.addDamage(1'000'000);
	report.artifactDestroyed(250.0f, 4000, 4000, false);
	report.bossSpawned();
	advance(650'000);
	report.addDamage(350'000); // the boss still alive at the time limit
	EXPECT_FLOAT_EQ(report.getTotalSeconds(), 900.0f) << "while the run lasts: until now";
	report.runEnded();
	advance(5'000);
	report.runEnded();
	EXPECT_EQ(report.getRunTotalDps(), 1500) << "1350000 / 900";
	EXPECT_EQ(report.failureReport("time limit reached"), "[Eternal Challenge] Failed (time limit reached) | Aether Stone: 250.0s, 4000 DPS on the "
														   "stone, 4000 total DPS (class average 4000) | Time spent: 900.0s, 1500 total DPS")
		<< "the first runEnded freezes the time";
	EXPECT_FALSE(report.isBossKilled());
	EXPECT_EQ(report.getBossDps(), 0);
}

TEST_F(RoahDpsReportTest, AFailureInTheArtifactPhaseReportsTheTimeSpentWithTheRunTotal) {
	EXPECT_FALSE(report.isRunStarted());
	EXPECT_FLOAT_EQ(report.getTotalSeconds(), 0.0f);
	EXPECT_EQ(report.getRunTotalDps(), 0);
	report.runStarted();
	EXPECT_TRUE(report.isRunStarted());
	advance(83'270);
	report.addDamage(100'000); // the stone, still alive
	report.addDamage(66'540);  // protectors killed and alive
	report.runEnded();
	EXPECT_FALSE(report.isArtifactDestroyed());
	EXPECT_EQ(report.getArtifactPhaseTotalDps(), 0) << "the stone's phase did not finish";
	EXPECT_EQ(report.failureReport("you died"),
		"[Eternal Challenge] Failed (you died) | Aether Stone: not destroyed | Time spent: 83.3s, 2000 total DPS");
}

} // namespace
} // namespace aion::gameserver::custom::instance
