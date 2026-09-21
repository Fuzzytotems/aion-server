// CheckOutput (m5a-plan.md F-02, F-07, D8): the report formats of the check-output mode and the final census on demand: a removed object that
// is still referenced is reported with zero thresholds after two Reclaimer::reclaimNow() runs, an object at count 0 is not.

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "aion/gameserver/CheckOutput.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"

namespace aion::gameserver {
namespace {

using runtime::LeakCensus;

class CensusObject final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static runtime::Ref<CensusObject> create() { return runtime::makeRef<CensusObject>(); }

protected:
	CensusObject() = default;
	~CensusObject() override = default;
};

std::filesystem::path uniqueDirectory(std::string_view name) {
	auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	return std::filesystem::temp_directory_path() / ("aion_check_output_" + std::string(name) + "_" + std::to_string(stamp));
}

std::string readFile(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	std::stringstream content;
	content << in.rdbuf();
	return content.str();
}

TEST(CheckOutputTest, CensusLinesNameClassIdRefCountAndPinningSites) {
	LeakCensus::LeakReport player;
	player.className = "Player";
	player.objectId = 100;
	player.refCount = 2;
	player.pinningTasks.push_back(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::SCHEDULED});
	player.pinningTasks.push_back(runtime::TaskInfo{std::source_location::current(), runtime::TaskKind::INSTANT});
	LeakCensus::LeakReport item;
	item.className = "Item";
	item.objectId = 7;
	item.refCount = 1;
	std::ostringstream out;
	CheckOutput::writeCensus(out, {player, item});
	std::string text = out.str();
	EXPECT_TRUE(text.starts_with("# final census v1\nPlayer\t100\t2\tCheckOutputTest.cpp:")) << text;
	EXPECT_NE(text.find(" scheduled; CheckOutputTest.cpp:"), std::string::npos) << text;
	EXPECT_NE(text.find(" instant\nItem\t7\t1\t\n"), std::string::npos) << text;

	std::ostringstream empty;
	CheckOutput::writeCensus(empty, {});
	EXPECT_EQ(empty.str(), "# final census v1\n");
}

TEST(CheckOutputTest, SummaryListsTheCountsAndTheUnportedClientPackets) {
	CheckOutput::Summary summary;
	summary.exitCode = 0;
	summary.started = true;
	summary.atreianPassportDisabled = false;
	summary.censusLeaks = 1;
	summary.zombieCuts = 2;
	summary.lockdepReports = 3;
	summary.watchdogDumps = 4;
	summary.notPortedClientPackets = {"CM_SUBZONE_CHANGE", "CM_CHAT_AUTH"};
	std::ostringstream out;
	CheckOutput::writeSummary(out, summary);
	std::string text = out.str();
	EXPECT_TRUE(text.starts_with("started true\nexitCode 0\nunportedHits ")) << text;
	EXPECT_NE(text.find("\npartialHits "), std::string::npos) << text;
	EXPECT_NE(text.find("\npartialSites "), std::string::npos) << text;
	EXPECT_NE(text.find("\ncensusLeaks 1\nzombieCuts 2\nlockdepReports 3\nwatchdogDumps 4\natreianPassportDisabled false\n"), std::string::npos) << text;
	EXPECT_TRUE(text.ends_with("notPortedClientPacket CM_CHAT_AUTH\nnotPortedClientPacket CM_SUBZONE_CHANGE\n")) << text; // sorted

	CheckOutput::Summary unknown;
	std::ostringstream unknownOut;
	CheckOutput::writeSummary(unknownOut, unknown);
	EXPECT_NE(unknownOut.str().find("started false\n"), std::string::npos);
	EXPECT_NE(unknownOut.str().find("atreianPassportDisabled unknown\n"), std::string::npos);
}

TEST(CheckOutputTest, ReportFilesAreWrittenIntoTheOutputDirectory) {
	const std::filesystem::path dir = uniqueDirectory("files");
	CheckOutput::writeUnportedTrace(dir);
	CheckOutput::writePartialTrace(dir);
	CheckOutput::writeLiveCounts(dir / "live_counts.txt");
	EXPECT_EQ(CheckOutput::writeLockdepReports(dir), 0u);
	CheckOutput::writeWatchdogDumps(dir, {"STALL task stalled"});
	EXPECT_TRUE(readFile(dir / "unported_trace.txt").starts_with("# AION_UNPORTED trace v1"));
	EXPECT_TRUE(readFile(dir / "partial_trace.txt").starts_with("# AION_PARTIAL trace v1"));
	EXPECT_TRUE(readFile(dir / "live_counts.txt").starts_with("# live instance counts v1"));
	EXPECT_EQ(readFile(dir / "lockdep.txt"), "# lockdep reports v1\n");
	EXPECT_EQ(readFile(dir / "watchdog.txt"), "# watchdog dumps v1\nSTALL task stalled\n");
	std::filesystem::remove_all(dir);
}

TEST(CheckOutputTest, FinalCensusReportsRemovedObjectsThatAreStillReferenced) {
	LeakCensus& census = LeakCensus::getInstance();
	census.uninstall();
	LeakCensus::Config config;
	config.censusAfter = std::chrono::minutes(10);
	config.checkInterval = std::chrono::seconds(1);
	census.configure(config);
	census.install();

	runtime::Ref<CensusObject> kept = CensusObject::create();
	runtime::Ref<CensusObject> released = CensusObject::create();
	census.onRemovedFromWorld(*kept, "Player", 41);
	census.onRemovedFromWorld(*released, "Item", 42);
	released.reset();

	int playerChecks = 0;
	const std::filesystem::path dir = uniqueDirectory("census");
	std::vector<LeakCensus::LeakReport> leaks = CheckOutput::runFinalCensus(
		dir,
		[&playerChecks] { return ++playerChecks < 3; }, // the players leave after two checks
		std::chrono::seconds(10));
	EXPECT_EQ(playerChecks, 3);
	ASSERT_EQ(leaks.size(), 1u);
	EXPECT_EQ(leaks[0].className, "Player");
	EXPECT_EQ(leaks[0].objectId, 41);
	EXPECT_EQ(readFile(dir / "census.txt"), "# final census v1\nPlayer\t41\t1\t\n");

	kept.reset();
	runtime::Reclaimer::getInstance().drain();
	EXPECT_TRUE(census.getLeaks().empty());
	census.uninstall();
	census.configure(LeakCensus::Config{});
	std::filesystem::remove_all(dir);
}

} // namespace
} // namespace aion::gameserver
