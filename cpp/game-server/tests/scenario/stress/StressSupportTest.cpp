// Unit tests of the parts of the stress harness that need neither a server nor a database: the option parsing that decides how long
// gs.scenario.m5a_stress runs and with how many clients, the names the clients use, and the two log-line classifiers the run's assertions are
// built on. They are ordinary unit tests (label "scenario", milliseconds); the run itself is gs.scenario.m5a_stress.

#include <gtest/gtest.h>

#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "stress/StressRun.h"

namespace aion::gameserver::scenario::stress {
namespace {

using Values = std::map<std::string, std::string, std::less<>>;

TEST(StressSupportTest, TheDefaultsAreSmallEnoughToRunByHand) {
	const StressOptions options = StressOptions::fromValues({});
	EXPECT_EQ(options.clients, 2);
	EXPECT_LE(options.duration.count(), 60) << "the default run must stay short: the registration asks for the real numbers";
	EXPECT_GT(options.roundsPerConnection, 0);
	EXPECT_GT(options.faultEveryNthRound, 0) << "the DAO fault injection is on by default, so a hand run exercises it too";
}

TEST(StressSupportTest, EveryNthRoundArmsTheInjectionSoADefaultRunReachesIt) {
	// the run asserts that the injection fired only when it had enough enter worlds to reach it, so the default must be small enough that the
	// 45 second default run (two rounds per connection) does reach it
	const StressOptions options = StressOptions::fromValues({});
	EXPECT_LE(options.faultEveryNthRound, options.roundsPerConnection);
}

TEST(StressSupportTest, TheRegistrationsNumbersAreRead) {
	const StressOptions options = StressOptions::fromValues(Values{{"AION_STRESS_CLIENTS", "20"}, {"AION_STRESS_MINUTES", "30"}});
	EXPECT_EQ(options.clients, 20);
	EXPECT_EQ(options.duration.count(), 30 * 60);
}

TEST(StressSupportTest, SecondsWinOverMinutes) {
	const StressOptions options = StressOptions::fromValues(Values{{"AION_STRESS_MINUTES", "30"}, {"AION_STRESS_SECONDS", "90"}});
	EXPECT_EQ(options.duration.count(), 90);
}

TEST(StressSupportTest, AnUnusableValueIsRefusedInsteadOfSilentlyRunningTheDefault) {
	// a typo in the registration must not turn a thirty minute nightly into a 45 second one that passes
	EXPECT_THROW(StressOptions::fromValues(Values{{"AION_STRESS_CLIENTS", "twenty"}}), std::runtime_error);
	EXPECT_THROW(StressOptions::fromValues(Values{{"AION_STRESS_MINUTES", "0"}}), std::runtime_error);
	EXPECT_THROW(StressOptions::fromValues(Values{{"AION_STRESS_CLIENTS", "-1"}}), std::runtime_error);
	// 0 is a legal value for the fault injection (it turns it off), so it is not refused
	EXPECT_EQ(StressOptions::fromValues(Values{{"AION_STRESS_FAULT_EVERY", "0"}}).faultEveryNthRound, 0);
}

TEST(StressSupportTest, EveryClientGetsItsOwnNameAndTheNamesAreLettersOnly) {
	std::set<std::string> names;
	for (int32_t i = 0; i < 20; i++) {
		const std::string name = StressRun::characterNameOf(i);
		EXPECT_TRUE(names.insert(name).second) << "two clients share the character name " << name;
		EXPECT_GE(name.size(), 2u);
		EXPECT_LE(name.size(), 16u) << name << " is longer than the character name limit";
		for (char c : name)
			EXPECT_TRUE((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
				<< name << " has a character NameConfig's [a-zA-Z]{2,16} pattern rejects";
	}
}

TEST(StressSupportTest, TheInjectedDaoErrorsAreTheFiveCatchBlocksOfOneLogout) {
	// the message texts of the DAO catch blocks the injection reaches, as their source writes them
	EXPECT_TRUE(isInjectedDaoError("2026-09-21 21:00:00 ERROR PlayerLifeStatsDAO - Could not update PlayerLifeStat data for player 42 from DB: "
	                               "AION_STRESS_FAULT_MARKER"));
	EXPECT_TRUE(isInjectedDaoError("2026-09-21 21:00:00 ERROR PlayerDAO - Error saving Player [id=42]"));
	EXPECT_TRUE(isInjectedDaoError("2026-09-21 21:00:00 ERROR PlayerDAO - Error storing old_level: 1 for player: 42"));
	EXPECT_TRUE(isInjectedDaoError("2026-09-21 21:00:00 ERROR DB - Failed to execute IU query UPDATE players set last_online = ? where id = ?"));
	EXPECT_TRUE(isInjectedDaoError("2026-09-21 21:00:00 ERROR DB - Failed to execute IU query UPDATE players SET online=? WHERE id=?"));
	// anything else is a failure of the run, including a DAO failure on a statement the injection does not poison
	EXPECT_FALSE(isInjectedDaoError("2026-09-21 21:00:00 ERROR DB - Failed to execute IU query UPDATE inventory SET item_count=? WHERE id=?"));
	EXPECT_FALSE(isInjectedDaoError("2026-09-21 21:00:00 ERROR World - Player [id=42] did not leave world cleanly"));
	EXPECT_FALSE(isInjectedDaoError("2026-09-21 21:00:00 ERROR ThreadPoolManager - Exception in a Runnable execution: boom"));
}

TEST(StressSupportTest, TheReusedIdPatternsAreTheMessagesThatExist) {
	const std::vector<std::string> patterns = reusedObjectIdWarnings();
	ASSERT_FALSE(patterns.empty());
	// each pattern is a substring of the message its source writes; a pattern that no longer matches would make the assertion unfalsifiable
	EXPECT_NE(std::string("Couldn't release ID 42 because it wasn't taken").find(patterns[0]), std::string::npos);
	EXPECT_NE(std::string("Attempt to remove Npc [id=42] from world but ID already belongs to Player [id=42]").find(patterns[1]),
		std::string::npos);
	EXPECT_NE(std::string("Duplicate object: Player [id=42], already present object: Npc [id=42]").find(patterns[2]), std::string::npos);
	EXPECT_NE(std::string("Player enterWorld fail: Duplicate character obj ID 42 found in world.").find(patterns[3]), std::string::npos);
}

} // namespace
} // namespace aion::gameserver::scenario::stress
