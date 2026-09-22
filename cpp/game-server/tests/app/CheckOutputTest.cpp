// CheckOutput (m5a-plan.md F-02, F-07, D8): the report formats of the check-output mode and the final census on demand: a removed object that
// is still referenced is reported with zero thresholds after two Reclaimer::reclaimNow() runs, an object at count 0 is not.
// Stage 3 (m5a-plan.md §10.1, §10.2): the zero-threshold breaker pass runFinalCensus ends with, and the live-count leak check - the classes it
// demands are 0, the account warehouse items it leaves to the connection bound, and the counters it reads.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/CheckOutput.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/model/animations/ObjectDeleteAnimation.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/LiveInstanceCounters.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver {
namespace {

using runtime::LeakCensus;
using runtime::LiveCount;

class CensusObject final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static runtime::Ref<CensusObject> create() { return runtime::makeRef<CensusObject>(); }

protected:
	CensusObject() = default;
	~CensusObject() override = default;
};

/** A removed world object with one retaining edge the zombie breaker may cut, i.e. the shape of the Player/Kisk cycle of cycles.toml. */
class CycleObject final : public runtime::RefCounted, public runtime::ZombieBreakable {
	AION_MAKE_REF_FRIEND

public:
	static runtime::Ref<CycleObject> create() { return runtime::makeRef<CycleObject>(); }
	static inline std::atomic<int32_t> live{0};
	runtime::Field<runtime::Ref<CycleObject>> peer;

	std::vector<const char*> breakKnownEdges() override {
		if (peer.exchange(nullptr))
			return {"peer"};
		return {};
	}

protected:
	CycleObject() { live.fetch_add(1); }
	~CycleObject() override { live.fetch_sub(1); }
};

/**
 * An object whose counted class name ends in "::GatheringTask_ActionObserver", one of the bare-name entries of
 * CheckOutput::zeroLiveClasses(). The real one lives in an anonymous namespace of GatheringTask.cpp; this one is a different type with the same
 * name, which is exactly what the check matches on (runtime::liveInstancesOf).
 */
class GatheringTask_ActionObserver final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static runtime::Ref<GatheringTask_ActionObserver> create() { return runtime::makeRef<GatheringTask_ActionObserver>(); }

protected:
	GatheringTask_ActionObserver() = default;
	~GatheringTask_ActionObserver() override = default;
};

/**
 * A controller that throws out of every known list notification: the W-07 case (m5a-plan.md), where Java swallows the exception and logs it with
 * an empty message and the C++ port counts it as well (KnownList.cpp:224-250).
 */
class ThrowingController final : public controllers::VisibleObjectController {
public:
	void see(model::gameobjects::VisibleObject&) override { throw runtime::IllegalStateException("notification failed"); }

	void notSee(model::gameobjects::VisibleObject&, model::animations::ObjectDeleteAnimation) override {
		throw runtime::IllegalStateException("notification failed");
	}

	void notKnow(model::gameobjects::VisibleObject&) override { throw runtime::IllegalStateException("notification failed"); }
};

/** KnownList with Java's add(VisibleObject) and del(VisibleObject, animation) exposed (both protected since S0b) */
class OpenKnownList final : public world::knownlist::KnownList {
public:
	explicit OpenKnownList(model::gameobjects::VisibleObject& owner) : KnownList(owner) {}

	bool addForTest(model::gameobjects::VisibleObject& object) { return add(object); }

	void delForTest(model::gameobjects::VisibleObject& object) { del(object, model::animations::ObjectDeleteAnimation::FADE_OUT); }
};

/**
 * A synthetic visible object with a plain KnownList, the shape of tests/world/WorldTestSupport.h's TestObject without the World: a known list
 * needs no map region, and VisibleObject::canSee says yes to any object, so add() reaches the see notification on its own.
 */
class NotifyingObject final : public model::gameobjects::VisibleObject {
	AION_MAKE_REF_FRIEND

public:
	static runtime::Ref<NotifyingObject> create(int32_t objectId) { return VisibleObject::create<NotifyingObject>(objectId); }

	NotifyingObject(CreateKey key, int32_t objectId)
		: VisibleObject(key, objectId, std::make_unique<ThrowingController>(), nullptr, nullptr, world::WorldPosition::create(210010000), false) {}

	std::string getName() override { return "NotifyingObject" + std::to_string(getObjectId()); }

	OpenKnownList& list() { return static_cast<OpenKnownList&>(getKnownList()); }

protected:
	void postConstruct() override {
		VisibleObject::postConstruct();
		getController().setOwner(*this);
		setKnownlist(std::make_unique<OpenKnownList>(*this));
	}

	~NotifyingObject() override = default;
};

/** Captures the messages of one logger subtree ("level|message" lines) while it exists. */
class LogCapture {
public:
	explicit LogCapture(std::string loggerName) : name(std::move(loggerName)) {
		auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
		sink->set_pattern("%l|%v");
		commons::logging::LoggerFactory::configure(name, {.sinks = {sink}, .additive = false});
	}
	~LogCapture() { commons::logging::LoggerFactory::removeConfig(name); }
	LogCapture(const LogCapture&) = delete;
	LogCapture& operator=(const LogCapture&) = delete;

	std::string str() const {
		std::string text = stream.str();
		std::erase(text, '\r');
		return text;
	}
	bool contains(std::string_view text) const { return str().find(text) != std::string::npos; }

private:
	std::string name;
	std::ostringstream stream;
};

/** Polls `predicate` in real time for at most `limit` (the zombie breakers run on the instant pool). */
bool waitFor(const std::function<bool()>& predicate, std::chrono::milliseconds limit = std::chrono::seconds(10)) {
	const auto deadline = std::chrono::steady_clock::now() + limit;
	while (!predicate()) {
		if (std::chrono::steady_clock::now() >= deadline)
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return true;
}

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

/** the counter rows of `counts` whose class name ends with `name` */
std::vector<LiveCount> named(const std::vector<LiveCount>& counts, std::string_view name) {
	std::vector<LiveCount> found;
	for (const LiveCount& count : counts)
		if (count.className.ends_with(name))
			found.push_back(count);
	return found;
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
	// knownListNotifyFailures is the W-07 counter: every KnownList notifySee/notifyNotSee/notifyNotKnow catch counts as a failure, and §5.7 Q8
	// asserts it is 0. Java logs those with an empty message (`log.error("", ex)`), so without this key the gate has no attributable signal.
	EXPECT_NE(text.find("\ncensusLeaks 1\nzombieCuts 2\nlockdepReports 3\nwatchdogDumps 4\nknownListNotifyFailures "), std::string::npos) << text;
	EXPECT_NE(text.find("\natreianPassportDisabled false\n"), std::string::npos) << text;
	// sorted, and followed by the live-count rows stage 3 appended: liveLeaks is the last line of a clean summary. liveCountsEnabled says
	// whether the counters this check reads exist in this build at all (§10.2): "liveLeaks 0" of a release build means "not measured".
	EXPECT_NE(text.find("\nnotPortedClientPacket CM_CHAT_AUTH\nnotPortedClientPacket CM_SUBZONE_CHANGE\nliveCountsEnabled "), std::string::npos)
		<< text;
	EXPECT_NE(text.find(std::string("\nliveCountsEnabled ") + (runtime::LIVE_COUNTS_ENABLED ? "true" : "false") + "\nliveLeaks "), std::string::npos)
		<< text;
	EXPECT_TRUE(text.ends_with("liveLeaks 0\n")) << text;

	CheckOutput::Summary unknown;
	std::ostringstream unknownOut;
	CheckOutput::writeSummary(unknownOut, unknown);
	EXPECT_NE(unknownOut.str().find("started false\n"), std::string::npos);
	EXPECT_NE(unknownOut.str().find("atreianPassportDisabled unknown\n"), std::string::npos);
}

/**
 * The value of the knownListNotifyFailures row (m5a-plan.md W-07, §5.7 Q8). The test above asserts the key and stops before the number, and the
 * summary has no Summary field to carry it: the row reads the process counter KnownList::notifyFailureCount() directly (CheckOutput.cpp:257), so
 * only a real counted failure can tell a writeSummary that prints it from one that prints a literal 0 (docs/deviations/P4-10.md, the change
 * request to this chunk). §5.7 Q8 and both startup smoke tests are green either way.
 * <p>
 * The three failures below are counted where KnownList swallows them (notifySee, notifyNotSee, notifyNotKnow, KnownList.cpp:224-250), by the same
 * throwing controller tests/world/KnownListTest.cpp drives through World.spawn - here through add() and del() of the known list itself, because
 * tests/app has no world.
 */
TEST(CheckOutputTest, TheSummaryRowCarriesTheKnownListNotifyFailureCount) {
	const auto summary = [] {
		std::ostringstream out;
		CheckOutput::writeSummary(out, CheckOutput::Summary{});
		return out.str();
	};
	world::knownlist::KnownList::resetNotifyFailureCountForTests();
	EXPECT_NE(summary().find("\nknownListNotifyFailures 0\n"), std::string::npos) << summary();

	{
		// Java logs every swallowed notification with an empty message (log.error("", ex)); the test does not need them on the console
		LogCapture capture("com.aionemu.gameserver.world.knownlist.KnownList");
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		runtime::Ref<NotifyingObject> watcher = NotifyingObject::create(990001);
		runtime::Ref<NotifyingObject> seen = NotifyingObject::create(990002);

		ASSERT_TRUE(watcher->list().addForTest(*seen)) << "the entry is added, then the see notification throws";
		watcher->list().delForTest(*seen); // the entry was visible, so notSee and notKnow both run and both throw

		EXPECT_EQ(world::knownlist::KnownList::notifyFailureCount(), 3u) << "one per swallowed see / notSee / notKnow";
		EXPECT_TRUE(capture.contains("notification failed")) << capture.str();
	}
	runtime::Reclaimer::getInstance().drain();

	EXPECT_NE(summary().find("\nknownListNotifyFailures 3\n"), std::string::npos)
		<< "the row must print KnownList::notifyFailureCount(), not a constant:\n"
		<< summary();

	world::knownlist::KnownList::resetNotifyFailureCountForTests();
	EXPECT_NE(summary().find("\nknownListNotifyFailures 0\n"), std::string::npos) << "and follow it back to 0 for the next reader";
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

// m5a-plan.md §10.3: runFinalCensus must end with runBreakerPass(), the one place that ever arms the zombie breaker and the stale-pin check in a
// gate run - the configured thresholds are 30 and 10 minutes against a run of one to three minutes. Without the call the gate's "zombieCuts 0"
// and "no stale pin" rows only assert absence, so nothing else in the tree notices that the pass is gone.
TEST(CheckOutputTest, FinalCensusEndsWithTheZeroThresholdBreakerPass) {
	LeakCensus& census = LeakCensus::getInstance();
	census.uninstall();
	LeakCensus::Config config; // the run's own thresholds: census 10 min, zombie breaker 30 min - neither can fire by age here
	census.configure(config);
	census.install();

	const uint64_t cutsBefore = census.zombieCutCount();
	const int32_t liveBefore = CycleObject::live.load();
	LogCapture capture("com.aionemu.gameserver.runtime.LeakCensus");
	runtime::Ref<CycleObject> player = CycleObject::create();
	runtime::Ref<CycleObject> kisk = CycleObject::create();
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		player->peer = kisk;
		kisk->peer = player;
	}
	census.onRemovedFromWorld(*player, "Player", 51);
	census.onRemovedFromWorld(*kisk, "Kisk", 52);
	player.reset();
	kisk.reset();
	EXPECT_EQ(CycleObject::live.load(), liveBefore + 2) << "the cycle keeps both objects alive";

	const std::filesystem::path dir = uniqueDirectory("breaker");
	const std::vector<LeakCensus::LeakReport> leaks = CheckOutput::runFinalCensus(
		dir, [] { return false; }, std::chrono::seconds(10));

	// the pass runs after census.txt was written, so the census still reports what it then cuts
	EXPECT_EQ(leaks.size(), 2u) << "both halves of the cycle are still referenced when the census is written";
	EXPECT_NE(readFile(dir / "census.txt").find("Player\t51\t1\t"), std::string::npos) << readFile(dir / "census.txt");
	EXPECT_TRUE(waitFor([&] { return census.zombieCutCount() >= cutsBefore + 2; }))
		<< "runFinalCensus must end with runBreakerPass(): with the breaker off or at its 30 minute threshold the cycle is never cut "
		<< "(cuts " << census.zombieCutCount() << ", expected " << cutsBefore + 2 << ")";
	EXPECT_EQ(census.zombieCutCount(), cutsBefore + 2) << "one cut per edge of the two-object cycle";
	EXPECT_TRUE(capture.contains("Zombie breaker: cut peer of Player (object id 51)")) << capture.str();
	EXPECT_TRUE(capture.contains("Zombie breaker: cut peer of Kisk (object id 52)")) << capture.str();

	// the breaker bodies run on the instant pool and their pin is released when their task is, so the reclamation is polled
	EXPECT_TRUE(waitFor([&] {
		runtime::Reclaimer::getInstance().drain();
		return CycleObject::live.load() == liveBefore;
	})) << "the cut cycle is reclaimed";
	EXPECT_TRUE(census.getLeaks().empty());
	census.uninstall();
	census.configure(LeakCensus::Config{});
	std::filesystem::remove_all(dir);
}

// m5a-plan.md §10.1: Item is not a strict zero. A login loads the ACCOUNT warehouse (AccountService.cpp:98-104) and the logout only detaches its
// owner (PlayerLeaveWorldService.cpp:170, Java PlayerLeaveWorldService.java:146), so an Account whose connection never reached
// LoginServer::onDisconnect survives the shutdown with its warehouse and its items. It passes today only because the scenario's warehouses are
// empty (0 78 Item in every gate report). Everything that belongs to a character stays at the strict 0.
TEST(CheckOutputTest, TheLiveCountCheckBoundsAccountWarehouseItemsByTheSurvivingAccounts) {
	const LiveCount account{"aion::gameserver::model::account::Account", 1, 3};
	const LiveCount items{"aion::gameserver::model::gameobjects::Item", 4, 78};
	const LiveCount player{"aion::gameserver::model::gameobjects::player::Player", 1, 6};
	const LiveCount npcs{"aion::gameserver::model::gameobjects::Npc", 82'127, 82'131};

	{
		LogCapture capture("com.aionemu.gameserver.CheckOutput");
		EXPECT_TRUE(CheckOutput::checkLiveCounts({account, items, npcs}).empty())
			<< "an Account that survived the shutdown may hold its account warehouse items; the world is never checked";
		EXPECT_TRUE(capture.contains("warning|Live instance leak check: 4 of the 78 aion::gameserver::model::gameobjects::Item")) << capture.str();
	}
	{
		// no Account survived: no account warehouse survived either, so every Item of the run must be gone
		LogCapture capture("com.aionemu.gameserver.CheckOutput");
		const std::vector<LiveCount> leaks = CheckOutput::checkLiveCounts({items, npcs});
		ASSERT_EQ(leaks.size(), 1u) << "with no Account alive the items are checked";
		EXPECT_EQ(leaks[0].className, items.className);
		EXPECT_EQ(leaks[0].live, 4);
		EXPECT_TRUE(capture.contains("error|Live instance leak: 4 of the 78 aion::gameserver::model::gameobjects::Item")) << capture.str();
	}
	{
		// the character's own classes are strict whatever the accounts do
		LogCapture capture("com.aionemu.gameserver.CheckOutput");
		const std::vector<LiveCount> leaks = CheckOutput::checkLiveCounts({account, items, player, npcs});
		ASSERT_EQ(leaks.size(), 1u) << "only the Player";
		EXPECT_EQ(leaks[0].className, player.className);
		EXPECT_TRUE(capture.contains("error|Live instance leak: 1 of the 6 aion::gameserver::model::gameobjects::player::Player")) << capture.str();
	}
	EXPECT_EQ(std::ranges::count(CheckOutput::zeroLiveClasses(), "model::gameobjects::Item"), 0)
		<< "the strict list must not demand 0 Item (m5a-plan.md §10.1)";
	ASSERT_EQ(CheckOutput::accountBoundedLiveClasses().size(), 1u);
	EXPECT_EQ(CheckOutput::accountBoundedLiveClasses()[0], "model::gameobjects::Item");
}

// The no-argument form reads the process-wide counters of a checked build (D8, I-03). The counters are compiled out in a release build, where
// the check reports nothing whatever the run leaked - which is what the liveCountsEnabled row of m5a_summary.txt says (§10.2).
TEST(CheckOutputTest, TheLiveCountCheckReadsTheProcessCountersOfAZeroClass) {
	if (!runtime::LIVE_COUNTS_ENABLED) {
		LogCapture capture("com.aionemu.gameserver.CheckOutput");
		EXPECT_TRUE(CheckOutput::checkLiveCounts().empty());
		EXPECT_TRUE(capture.contains("nothing is counted in this build")) << "a release build must say that the check measured nothing";
		GTEST_SKIP() << "release build: makeRef and the Reclaimer count nothing (AION_CHECKED off)";
	}
	LogCapture capture("com.aionemu.gameserver.CheckOutput"); // also keeps this test's deliberate leak out of the test log
	const auto observers = [] { return named(CheckOutput::checkLiveCounts(), "::GatheringTask_ActionObserver"); };
	EXPECT_TRUE(observers().empty()) << "nothing of that class is alive before the test creates one";

	runtime::Ref<GatheringTask_ActionObserver> observer = GatheringTask_ActionObserver::create();
	const std::vector<LiveCount> leaks = observers();
	ASSERT_EQ(leaks.size(), 1u) << "checkLiveCounts() must report a live class of zeroLiveClasses()";
	EXPECT_EQ(leaks[0].live, 1);
	// the ERROR line is what fails the gate's "no ERROR line" assertion even where nothing reads the counts (m5a-plan.md D8)
	EXPECT_TRUE(capture.contains("error|Live instance leak: 1 of the 1 ") && capture.contains("GatheringTask_ActionObserver instances"))
		<< capture.str();

	observer.reset();
	runtime::Reclaimer::getInstance().drain();
	EXPECT_TRUE(observers().empty()) << "and stop reporting it once the Reclaimer freed it";
}

} // namespace
} // namespace aion::gameserver
