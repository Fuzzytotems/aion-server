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
#include <utility>
#include <vector>

#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/CheckOutput.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/animations/ObjectDeleteAnimation.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/items/detail/StaticDataLookups.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/LiveInstanceCounters.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
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
	EXPECT_NE(text.find("\ncensusLeaks 1\ncensusTracked unknown\nzombieCuts 2\nlockdepReports 3\nwatchdogDumps 4\nknownListNotifyFailures "),
		std::string::npos)
		<< text;
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

// m5b3-plan.md §16 (the stage-1 integration): a removed object whose references are still being dropped scan by scan when the census starts is
// a logout that is finishing, not a leak. gs.scenario.m5b wrote `Player <id> 3` twice in a row - the census hook logged 86, then 3, after its
// two scans - while the same shutdown left "0 objects still tracked" and 0 live Players: the KnownObjects and Effects of the logout that hold
// the Player are destroyed epoch by epoch, and two scans were not enough. Here five holders let go of the "Player" one per scan of this thread
// (the reclaimer's own thread, if a test started it, is ignored), so after runFinalCensus' two fixed scans the count is still 3 or 4 and
// falling; the census has to keep scanning until two checks in a row agree, and by then the count has reached 0 and the object is swept.
TEST(CheckOutputTest, FinalCensusWaitsForACountThatIsStillFalling) {
	LeakCensus& census = LeakCensus::getInstance();
	census.uninstall();
	LeakCensus::Config config;
	config.censusAfter = std::chrono::minutes(10);
	config.checkInterval = std::chrono::seconds(1);
	census.configure(config);
	census.install();

	runtime::Ref<CensusObject> player = CensusObject::create();
	census.onRemovedFromWorld(*player, "Player", 61);
	auto holders = std::make_shared<std::vector<runtime::Ref<CensusObject>>>();
	for (int i = 0; i < 5; i++)
		holders->push_back(player);
	player.reset();
	const std::thread::id scanner = std::this_thread::get_id();
	runtime::Reclaimer& reclaimer = runtime::Reclaimer::getInstance();
	const uint64_t hook = reclaimer.addPostScanHook("CheckOutputTest.fallingCount", [holders, scanner] {
		if (std::this_thread::get_id() == scanner && !holders->empty())
			holders->pop_back();
	});

	const std::filesystem::path dir = uniqueDirectory("falling");
	const std::vector<LeakCensus::LeakReport> leaks = CheckOutput::runFinalCensus(dir, [] { return false; }, std::chrono::seconds(10));
	reclaimer.removePostScanHook(hook);

	EXPECT_TRUE(holders->empty()) << "the census stopped scanning while the count was still falling (" << holders->size() << " holders left)";
	EXPECT_TRUE(leaks.empty()) << "a count that falls scan by scan is not a leak: " << readFile(dir / "census.txt");
	EXPECT_EQ(readFile(dir / "census.txt"), "# final census v1\n");

	holders->clear();
	reclaimer.drain();
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

// ---------------------------------------------------------------------------------------------------------------------------------------
// M5b-1 E-03 (m5b-plan.md §4, Q2/Q3) and m5b-client-session.md S-2.
// ---------------------------------------------------------------------------------------------------------------------------------------

/**
 * The four combat classes m5b-plan.md E-03 names, decided one by one rather than by analogy - which is what stage 1 had to do when it took
 * world::knownlist::KnownObject back out of zeroLiveClasses().
 * <p>
 * AttackResult: strict 0, and exercised. AttackUtil makes one per hit, it travels in the result list of SM_ATTACK and in DelayedOnAttack, and
 * nothing keeps one after the hit is applied - measured 0 live of 183 created in the green M5a gate run that followed A-06 (0 of 93 in the geo
 * run).
 * <p>
 * DropNpc: strict 0 while M5b-1's registerDrop was a whole-body AION_PARTIAL (a guard, created 0); NOT strict since M5b-3 (m5b3-plan.md D5,
 * G-06): DropRegistrationService holds a corpse's DropNpc until the corpse despawns, 300 s for an unlooted drop, so a stop within five minutes of
 * such a kill holds one in Java too. It is bounded by what the service holds (TheDropClassesAreBoundedByWhatTheDropServiceHolds below).
 * <p>
 * AggroInfo: NOT strict 0. The same M5a gate run reports 34 live of 36 created with censusLeaks 0, liveLeaks 0 and exitCode 0 (22 of 22 in the
 * geo run): an AggroList is an OwnedPart of a Creature, the shutdown does not despawn the world, and since A-06 the npcs fight each other, so an
 * npc still in a fight legitimately holds hate entries. Adding it would have turned that green run red.
 * <p>
 * DamageList: cannot be listed at all - a K5 confined value class (DamageList.h:18-22), not RefCounted, so no counter can ever see it and the row
 * would be dead forever.
 */
TEST(CheckOutputTest, TheCombatClassesOfTheM5bGateAreDecidedOneByOne) {
	const auto strict = [](std::string_view name) { return std::ranges::count(CheckOutput::zeroLiveClasses(), name) == 1; };
	const auto reported = [](std::string_view name) { return std::ranges::count(CheckOutput::summaryLiveClasses(), name) == 1; };

	EXPECT_TRUE(strict("controllers::attack::AttackResult")) << "0 live of 183 created in the M5a gate run: a strict zero that is exercised";
	EXPECT_FALSE(strict("model::gameobjects::DropNpc"))
		<< "a corpse with an unlooted drop keeps its DropNpc for 300 s: bounded by DropRegistrationService's map (m5b3-plan.md D5), not zero";
	EXPECT_FALSE(strict("controllers::attack::AggroInfo")) << "34 of 36 live in a GREEN gate run: bounded by the live creatures, not zero";
	EXPECT_FALSE(strict("controllers::attack::DamageList")) << "a value class no counter can see: the row would never fire";
	EXPECT_FALSE(reported("controllers::attack::DamageList")) << "and reporting 0 0 for it forever would only read as a pass";

	// what the gate reads as numbers instead
	EXPECT_TRUE(reported("controllers::attack::AggroInfo"));
	EXPECT_TRUE(reported("controllers::attack::AttackResult")) << "the created half is what makes the strict row an assertion";
	EXPECT_TRUE(reported("model::gameobjects::DropNpc")) << "the gates assert created = kills and live = dropNpcsHeld";
	EXPECT_TRUE(reported("model::drop::DropItem")) << "a drop of the static data (the custom drops of NpcDrop.dropCalculator)";
	EXPECT_TRUE(reported("RuntimeDropItem")) << "a drop made at run time (every global rule's and quest drop's new DropItem(new Drop(...)))";
	EXPECT_TRUE(reported("model::gameobjects::Npc")) << "Q3's npc conservation: live back to the baseline, created up by the respawns";
	EXPECT_TRUE(reported("world::knownlist::KnownObject")) << "the class stage 1 removed from the strict list is still bounded by the gate";

	// the decision, exercised: the numbers of the green run must produce no leak at all
	LogCapture capture("com.aionemu.gameserver.CheckOutput");
	const std::vector<LiveCount> gateRun{
		{"aion::gameserver::controllers::attack::AggroInfo", 34, 36},
		{"aion::gameserver::controllers::attack::AttackResult", 0, 183},
		{"aion::gameserver::world::knownlist::KnownObject", 15'238, 15'744},
		{"aion::gameserver::model::gameobjects::Npc", 82'126, 82'129},
	};
	EXPECT_TRUE(CheckOutput::checkLiveCounts(gateRun).empty())
		<< "the live counts of the green M5a gate run of 2026-09-22 must stay green: a strict AggroInfo row would fail it";

	// and a live AttackResult must still fail, or the strict row is decoration
	std::vector<LiveCount> leaked = gateRun;
	leaked[1].live = 1;
	const std::vector<LiveCount> leaks = CheckOutput::checkLiveCounts(leaked);
	ASSERT_EQ(leaks.size(), 1u);
	EXPECT_EQ(leaks[0].className, "aion::gameserver::controllers::attack::AttackResult");
}

/**
 * m5b3-plan.md D5 (G-06): DropNpc and the drop items are summary rows bounded by what DropRegistrationService holds, not zero rows. A corpse with
 * an unlooted drop keeps its DropNpc and its drop set in the service's two maps until it despawns - RespawnService.WITH_DROP_DECAY, 300 s - so a
 * server stopped within five minutes of such a kill holds them, in Java too. The dir form writes the bound (dropNpcsHeld, dropItemsHeld) and logs
 * an ERROR for a drop class with more live instances than the service holds, which is the leak (a DropNpc unregisterDrop missed, a drop item a
 * static kept). The drop item that every global rule makes is DropItem.cpp's anonymous-namespace RuntimeDropItem, whose counter the name rule
 * reaches only as "RuntimeDropItem": the dir-form half pins that against the real class.
 */
TEST(CheckOutputTest, TheDropClassesAreBoundedByWhatTheDropServiceHolds) {
	{
		// the numbers of a stop five seconds after a kill whose corpse still stands with ten entries (the M5b-3 gate's rate, §2.4)
		LogCapture capture("com.aionemu.gameserver.CheckOutput");
		const std::vector<LiveCount> standingCorpse{
			{"aion::gameserver::model::gameobjects::DropNpc", 1, 3},
			{"aion::gameserver::model::drop::`anonymous namespace'::RuntimeDropItem", 10, 30},
		};
		EXPECT_TRUE(CheckOutput::checkLiveCounts(standingCorpse).empty()) << "a standing corpse's drop is no zero-class leak";
		EXPECT_FALSE(capture.contains("error|")) << capture.str();
		const std::vector<LiveCount> rows = CheckOutput::summaryLiveCounts(standingCorpse);
		const auto row = [&rows](std::string_view name) {
			auto it = std::ranges::find(rows, name, &LiveCount::className);
			return it == rows.end() ? LiveCount{std::string(name), -1, 0} : *it;
		};
		EXPECT_EQ(row("model::gameobjects::DropNpc").live, 1);
		EXPECT_EQ(row("model::gameobjects::DropNpc").created, 3u);
		EXPECT_EQ(row("RuntimeDropItem").live, 10) << "MSVC's spelling of the anonymous namespace sits between the namespace and the class name";
		EXPECT_EQ(row("model::drop::DropItem").live, 0) << "the static-data drop item is a separate counter";
	}

	const std::filesystem::path dir = uniqueDirectory("drops");
	const auto summaryOf = [&dir](bool started) {
		CheckOutput::Summary summary;
		summary.started = started;
		CheckOutput::writeSummary(dir, summary);
		return readFile(dir / "m5a_summary.txt");
	};
	const auto rowOf = [](const std::string& text, std::string_view key) {
		const size_t at = text.find("\n" + std::string(key) + " ");
		return at == std::string::npos ? std::string("(no row)") : text.substr(at + 1, text.find('\n', at + 1) - at - 1);
	};
	{
		LogCapture capture("com.aionemu.gameserver.CheckOutput");
		const std::string notStarted = summaryOf(false);
		EXPECT_NE(notStarted.find("\ndropNpcsHeld unknown\ndropItemsHeld unknown\neffectsHeld "), std::string::npos)
			<< "a summary that was not `started` measured nothing, and the drop rows come before the G-07 rows: " << notStarted;
	}
	if (!runtime::LIVE_COUNTS_ENABLED) {
		std::filesystem::remove_all(dir);
		GTEST_SKIP() << "release build: makeRef and the Reclaimer count nothing (AION_CHECKED off)";
	}

	// Sparkie Carapace Fragment, item_templates.xml:874138 - the one candidate of JUNK_SPAKY_MATERIAL (m5b3-plan.md §2.4); DropItem's
	// constructor reads its option_slot_bonus through the ITEM_DATA lookup
	static const model::templates::item::ItemTemplate* const junk = [] {
		xml::LoadContext context;
		return xml::bindString<model::templates::item::ItemTemplate>(context,
			R"(<item_template id="182004793" name="Sparkie Carapace Fragment" level="5" cName="junk_spaky_05" mask="12414" max_stack_count="1000" quality="JUNK" price="300" desc="718718"/>)")
			.release();
	}();
	model::items::detail::StaticDataLookupsForTests lookups;
	lookups.itemTemplate = [](int32_t itemId) -> const model::templates::item::ItemTemplate* { return itemId == 182004793 ? junk : nullptr; };
	model::items::detail::setStaticDataLookupsForTests(lookups);

	services::drop::DropRegistrationService& service = services::drop::DropRegistrationService::getInstance();
	constexpr int32_t corpse = 0x7FFFFF01; // an object id no test of this binary registers
	const auto countsOf = [&](std::string_view key) {
		int64_t live = -1;
		uint64_t created = 0;
		const std::string line = rowOf(summaryOf(true), "liveCount " + std::string(key));
		std::istringstream in(line);
		std::string label, name;
		in >> label >> name >> live >> created;
		return std::pair<int64_t, uint64_t>{live, created};
	};
	// a custom drop's Drop lives in the static data (NpcDrop's DropGroup), DropItem::create(const Drop*) borrows it
	static const model::drop::Drop staticDrop(182004793, 1, 1, 100.0f);
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		runtime::Ref<model::gameobjects::DropNpc> dropNpc = model::gameobjects::DropNpc::create(corpse);
		runtime::Ref<runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>> dropItems =
			runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>::create();
		// initDropNpc's dropRegistrationMap.put and registerDrop's currentDropMap.put (DropRegistrationService.java:163, :82): the corpse's
		// DropNpc and its drop set with THREE entries - two run-time ones (a global rule's `new DropItem(new Drop(itemId, count, count, 100))`)
		// and a custom drop's (NpcDrop.dropCalculator: `new DropItem(drop)` over the static-data Drop) - so dropItemsHeld must count the
		// elements of the set, not the corpses (the M5b-3 gate's corpses hold up to ten each, §2.4)
		dropItems->add(model::drop::DropItem::create(model::drop::Drop(182004793, 1, 1, 100.0f)));
		dropItems->add(model::drop::DropItem::create(model::drop::Drop(182004793, 1, 1, 100.0f)));
		dropItems->add(model::drop::DropItem::create(&staticDrop));
		ASSERT_EQ(dropItems->size(), 3) << "three distinct DropItem objects: DropItem has identity equality, as in Java";
		service.getDropRegistrationMap().put(corpse, dropNpc);
		service.getCurrentDropMap().put(corpse, dropItems);
		{
			LogCapture capture("com.aionemu.gameserver.CheckOutput");
			const std::string held = summaryOf(true);
			EXPECT_EQ(rowOf(held, "dropNpcsHeld"), "dropNpcsHeld 1") << held;
			EXPECT_EQ(rowOf(held, "dropItemsHeld"), "dropItemsHeld 3") << "the elements of the one corpse's drop set: " << held;
			EXPECT_EQ(countsOf("model::gameobjects::DropNpc").first, 1) << rowOf(held, "liveCount model::gameobjects::DropNpc");
			EXPECT_EQ(countsOf("RuntimeDropItem").first, 2)
				<< "the real RuntimeDropItem must reach the RuntimeDropItem row: " << rowOf(held, "liveCount RuntimeDropItem");
			EXPECT_EQ(countsOf("model::drop::DropItem").first, 1)
				<< "the static-data drop item reaches its own row: " << rowOf(held, "liveCount model::drop::DropItem");
			// no line about a drop class at all: neither checkHeldDrops nor a zero row of checkLiveCounts may call the standing corpse a leak
			EXPECT_EQ(capture.str().find("Drop"), std::string::npos) << "what the service holds is no leak: " << capture.str();
		}

		// DropService.unregisterDrop (DropService.java:78-82: the corpse despawned) while something still keeps both objects: the leak the bound
		// exists for
		service.getCurrentDropMap().remove(corpse);
		service.getDropRegistrationMap().remove(corpse);
		{
			LogCapture capture("com.aionemu.gameserver.CheckOutput");
			const std::string leaked = summaryOf(true);
			EXPECT_EQ(rowOf(leaked, "dropNpcsHeld"), "dropNpcsHeld 0") << leaked;
			EXPECT_EQ(rowOf(leaked, "dropItemsHeld"), "dropItemsHeld 0") << leaked;
			EXPECT_TRUE(capture.contains("error|Live instance leak: 1 DropNpc instances are alive after the runtime shut down and "
										 "DropRegistrationService holds 0"))
				<< capture.str();
			EXPECT_TRUE(capture.contains("error|Live instance leak: 3 DropItem instances are alive after the runtime shut down and "
										 "DropRegistrationService holds 0"))
				<< "both drop item classes count against the bound: " << capture.str();
		}
	}
	runtime::Reclaimer::getInstance().drain();
	{
		LogCapture capture("com.aionemu.gameserver.CheckOutput");
		const std::string after = summaryOf(true);
		EXPECT_EQ(countsOf("model::gameobjects::DropNpc").first, 0) << "released and reclaimed: " << after;
		EXPECT_GE(countsOf("model::gameobjects::DropNpc").second, 1u) << "while `created` stays: " << after;
		EXPECT_EQ(countsOf("RuntimeDropItem").first, 0) << after;
		EXPECT_EQ(countsOf("model::drop::DropItem").first, 0) << after;
		EXPECT_EQ(capture.str().find("Drop"), std::string::npos) << capture.str();
	}
	model::items::detail::setStaticDataLookupsForTests({});
	std::filesystem::remove_all(dir);
}

/**
 * M5b-2 part 3 took model::stats::calc::functions::StatFunctionProxy out of zeroLiveClasses(), as M5b-1 did with KnownObject: closing D7 makes
 * every spawn cast its post-spawn skills (NpcSkillList.getPostSpawnSkills, SpawnEventHandler.java:20-22), and the statup buffs of those npcs
 * last 86,400,000 ms, so their stat functions survive the shutdown with their npcs. The numbers are gs.smoke.startup's of 2026-09-23, the first
 * run with the holdback removed; a strict row turned them into an ERROR line, which fails every gate's "no ERROR line" check.
 */
TEST(CheckOutputTest, TheStatFunctionsOfPostSpawnBuffsSurviveTheShutdownWithTheirNpcs) {
	LogCapture capture("com.aionemu.gameserver.CheckOutput");
	const std::vector<LiveCount> startup{
		{"aion::gameserver::model::stats::calc::functions::StatFunctionProxy", 1'171, 1'171},
		{"aion::gameserver::model::gameobjects::Npc", 82'127, 82'127},
	};
	EXPECT_TRUE(CheckOutput::checkLiveCounts(startup).empty()) << "the post-spawn buffs of a green startup are not a leak";
}

/**
 * m5b2-plan.md G-07, decided by measurement and not by the plan's §7 row (which asked for strict zeros): Effect, EffectReserved, Skill, the
 * StartMovingListener every Skill owns and the effect observers are counted summary rows, never zeroLiveClasses() rows. The numbers are the
 * live_counts.txt of the green gs.scenario.m5b run of 2026-09-24: the post-spawn statup buffs keep 309 Effects, their 309 Skills and those
 * Skills' 309 StartMovingListeners alive at the shutdown, and the 18 hide effects of the post-spawn hiders keep their Effect_ActionObservers.
 * A strict row would turn every green gate red with an ERROR line; what a gate asserts instead is the relation m5a_summary.txt writes beside
 * the rows (effectsHeld, skillsHeld, effectReservedCapacity).
 */
TEST(CheckOutputTest, TheSkillClassesAreCountedAgainstARelationNotAZero) {
	const auto strict = [](std::string_view name) { return std::ranges::count(CheckOutput::zeroLiveClasses(), name) == 1; };
	const auto reported = [](std::string_view name) { return std::ranges::count(CheckOutput::summaryLiveClasses(), name) == 1; };
	for (const char* name : {"skillengine::model::Effect", "skillengine::model::EffectReserved", "skillengine::model::Skill",
			 "controllers::observer::StartMovingListener", "skillengine::model::Effect_ActionObserver", "skillengine::model::Effect_ActionObserver_2",
			 "skillengine::effect::RootEffect_ActionObserver", "skillengine::effect::AlwaysDodgeEffect_AttackStatusObserver",
			 "skillengine::effect::AlwaysResistEffect_AttackStatusObserver"}) {
		EXPECT_FALSE(strict(name)) << name << ": the post-spawn buffs keep it alive at every shutdown, so a strict zero fails a green run";
		EXPECT_TRUE(reported(name)) << name << ": the gate reads its live and created numbers from m5a_summary.txt";
	}

	LogCapture capture("com.aionemu.gameserver.CheckOutput");
	const std::vector<LiveCount> gateRun{
		{"aion::gameserver::skillengine::model::Effect", 309, 325},
		{"aion::gameserver::skillengine::model::EffectReserved", 0, 332},
		{"aion::gameserver::skillengine::model::Skill", 309, 309},
		{"aion::gameserver::controllers::observer::StartMovingListener", 309, 309},
		{"aion::gameserver::skillengine::model::Effect_ActionObserver", 18, 18},
		{"aion::gameserver::skillengine::model::Effect_ActionObserver_2", 18, 18},
	};
	EXPECT_TRUE(CheckOutput::checkLiveCounts(gateRun).empty()) << "the skill classes of a green gate run are not a leak by themselves";
	const std::vector<LiveCount> rows = CheckOutput::summaryLiveCounts(gateRun);
	const auto row = [&rows](std::string_view name) {
		auto it = std::ranges::find(rows, name, &LiveCount::className);
		return it == rows.end() ? LiveCount{std::string(name), -1, 0} : *it;
	};
	EXPECT_EQ(row("skillengine::model::Effect").live, 309);
	EXPECT_EQ(row("skillengine::model::Effect").created, 325u);
	EXPECT_EQ(row("skillengine::model::EffectReserved").created, 332u) << "0 live with 332 created: the created half makes the bound mean something";
	EXPECT_EQ(row("skillengine::model::Effect_ActionObserver").live, 18) << "the _2 counter must not be summed into its prefix";
	EXPECT_EQ(row("skillengine::effect::RootEffect_ActionObserver").live, 0) << "no counter: 0 0";
}

/**
 * The relation side of G-07: the directory form writes effectsHeld, skillsHeld and effectReservedCapacity after the pure summary, walked over
 * the world's creatures. A process that never created a world - this test binary, unless a test loaded the world maps - writes "unknown"
 * rather than a 0 nobody measured, exactly as censusTracked does; a summary that was not `started` never walks.
 */
TEST(CheckOutputTest, TheDirectoryFormWritesTheHeldEffectRowsOrUnknown) {
	const std::filesystem::path dir = uniqueDirectory("held");
	LogCapture capture("com.aionemu.gameserver.CheckOutput");
	const auto rowsOf = [&dir](bool started) {
		CheckOutput::Summary summary;
		summary.started = started;
		CheckOutput::writeSummary(dir, summary);
		return readFile(dir / "m5a_summary.txt");
	};
	const std::string notStarted = rowsOf(false);
	EXPECT_NE(notStarted.find("\neffectsHeld unknown\nskillsHeld unknown\neffectReservedCapacity unknown\n"), std::string::npos) << notStarted;
	EXPECT_TRUE(notStarted.ends_with("\neffectReservedCapacity unknown\n")) << "the three rows close the file: " << notStarted;

	const std::string started = rowsOf(true);
	if (!dataholders::DataManager::WORLD_MAPS_DATA)
		EXPECT_NE(started.find("\neffectsHeld unknown\nskillsHeld unknown\neffectReservedCapacity unknown\n"), std::string::npos)
			<< "no world was created in this process, so nothing was walked: " << started;
	else
		EXPECT_EQ(started.find("Held unknown"), std::string::npos) << "a world exists, so the rows are numbers: " << started;
	std::filesystem::remove_all(dir);
}

/**
 * summaryLiveCounts() keeps `created` for a class at 0 live, which is the whole point of the row: liveLeak lines only exist for a class that is
 * already failing, and runtime::liveInstancesOf drops every counter at 0 live (LiveInstanceCounters.cpp:121-122), so neither can tell the gate
 * that an AttackResult was ever created. A class no counter matches reports 0 0.
 */
TEST(CheckOutputTest, SummaryLiveCountsReportCreatedEvenWhenNothingIsLive) {
	const std::vector<LiveCount> counts{
		{"aion::gameserver::controllers::attack::AggroInfo", 34, 36},
		{"aion::gameserver::controllers::attack::AttackResult", 0, 183},
		{"aion::gameserver::model::gameobjects::Npc", 82'126, 82'129},
		{"aion::gameserver::model::gameobjects::SummonedHouseNpc", 2'060, 2'060},
	};
	const std::vector<LiveCount> rows = CheckOutput::summaryLiveCounts(counts);
	ASSERT_EQ(rows.size(), CheckOutput::summaryLiveClasses().size());
	for (size_t i = 0; i < rows.size(); i++)
		EXPECT_EQ(rows[i].className, CheckOutput::summaryLiveClasses()[i]) << "the requested name, in list order, so the gate's key is stable";

	const auto row = [&rows](std::string_view name) {
		auto it = std::ranges::find(rows, name, &LiveCount::className);
		return it == rows.end() ? LiveCount{std::string(name), -1, 0} : *it;
	};
	EXPECT_EQ(row("controllers::attack::AggroInfo").live, 34);
	EXPECT_EQ(row("controllers::attack::AggroInfo").created, 36u);
	EXPECT_EQ(row("controllers::attack::AttackResult").live, 0);
	EXPECT_EQ(row("controllers::attack::AttackResult").created, 183u) << "0 live with 183 created is an assertion; 0 live with 0 created is not";
	EXPECT_EQ(row("model::gameobjects::DropNpc").live, 0);
	EXPECT_EQ(row("model::gameobjects::DropNpc").created, 0u) << "no counter at all in these counts: 0 0, the honest answer for a class never created";
	EXPECT_EQ(row("model::gameobjects::Npc").live, 82'126) << "SummonedHouseNpc must not be summed into Npc: the rule matches on \"::\" + name";
	EXPECT_EQ(row("model::gameobjects::Npc").created, 82'129u);
}

/** The name rule of summaryLiveCounts() is runtime::liveInstancesOf's, minus its `live != 0` filter (CheckOutput.cpp's matchesClassName). */
TEST(CheckOutputTest, SummaryLiveCountsUseTheLiveInstancesOfNameRule) {
	for (const std::string& className : CheckOutput::summaryLiveClasses()) {
		const std::vector<LiveCount> counts{{"aion::gameserver::" + className, 7, 9}, {"aion::gameserver::Other" + className, 5, 5},
			{"aion::gameserver::model::gameobjects::Unrelated", 3, 3}};
		const std::vector<LiveCount> viaRuntime = runtime::liveInstancesOf(counts, {className});
		ASSERT_EQ(viaRuntime.size(), 1u) << className << ": a trailing part after \"::\" matches, a longer word ending in it does not";

		const std::vector<LiveCount> rows = CheckOutput::summaryLiveCounts(counts);
		const auto it = std::ranges::find(rows, className, &LiveCount::className);
		ASSERT_NE(it, rows.end()) << className;
		EXPECT_EQ(it->live, viaRuntime[0].live) << className << ": the two matchers must agree wherever liveInstancesOf can answer";
		EXPECT_EQ(it->created, viaRuntime[0].created) << className;
	}
}

/** The liveCount rows of m5a_summary.txt, written after the liveLeak rows so a clean summary still ends with the live-count block. */
TEST(CheckOutputTest, SummaryWritesOneLiveCountRowPerReportedClass) {
	CheckOutput::Summary summary;
	summary.started = true;
	summary.summaryCounts = {{"controllers::attack::AggroInfo", 34, 36}, {"controllers::attack::AttackResult", 0, 183},
		{"model::gameobjects::DropNpc", 0, 0}};
	std::ostringstream out;
	CheckOutput::writeSummary(out, summary);
	const std::string text = out.str();
	EXPECT_NE(text.find("\nliveLeaks 0\nliveCount controllers::attack::AggroInfo 34 36\n"), std::string::npos) << text;
	EXPECT_NE(text.find("\nliveCount controllers::attack::AttackResult 0 183\n"), std::string::npos) << text;
	EXPECT_TRUE(text.ends_with("\nliveCount model::gameobjects::DropNpc 0 0\n")) << text;

	std::ostringstream none;
	CheckOutput::writeSummary(none, CheckOutput::Summary{});
	EXPECT_EQ(none.str().find("\nliveCount "), std::string::npos) << "the pure form writes only what it was given";
}

/**
 * m5b-client-session.md S-2. The five-kill session ended with "1 objects removed from the world are still alive" and no report named the object.
 * censusLeaks cannot carry that number: runFinalCensus runs BEFORE RuntimeLifecycle::shutdown, so an object that leaves the world during the
 * shutdown itself is in neither census.txt nor liveLeaks. Only the caller that performs the shutdown can measure it
 * (RuntimeLifecycle::ShutdownReport::censusTracked), and LeakCensus::uninstall() has emptied the table by the time this file is written - so an
 * unset value must say "unknown" and not a 0 nobody measured, or a gate asserting the row would pass vacuously.
 */
TEST(CheckOutputTest, SummaryCarriesTheShutdownCensusTrackedCountOrSaysUnknown) {
	const auto write = [](const CheckOutput::Summary& summary) {
		std::ostringstream out;
		CheckOutput::writeSummary(out, summary);
		return out.str();
	};
	CheckOutput::Summary summary;
	summary.censusLeaks = 0;
	EXPECT_NE(write(summary).find("\ncensusLeaks 0\ncensusTracked unknown\n"), std::string::npos) << write(summary);

	summary.censusTracked = 0;
	EXPECT_NE(write(summary).find("\ncensusTracked 0\n"), std::string::npos) << write(summary);

	summary.censusTracked = 1; // the client session's number: 0 census leaks and 1 object still tracked at the end of the shutdown
	EXPECT_NE(write(summary).find("\ncensusLeaks 0\ncensusTracked 1\n"), std::string::npos)
		<< "the row must print the value it was given, not censusLeaks and not a constant:\n"
		<< write(summary);
}

/**
 * The directory form is what a real run writes, and it is the form that fills the rows itself (`started` set means the run reached the final
 * census). The `created` half must survive the object: a class at 0 live with a non-zero `created` is what m5b-plan.md Q2 asserts, and a summary
 * that read `created` from something transient would report `0 0` for a perfectly exercised class.
 */
TEST(CheckOutputTest, TheDirectoryFormFillsTheLiveCountRowsFromTheProcessCounters) {
	if (!runtime::LIVE_COUNTS_ENABLED)
		GTEST_SKIP() << "release build: makeRef and the Reclaimer count nothing (AION_CHECKED off)";
	const std::filesystem::path dir = uniqueDirectory("summary");
	LogCapture capture("com.aionemu.gameserver.CheckOutput"); // the deliberate live AttackResult below is an ERROR line by design
	const auto row = [&dir] {
		CheckOutput::Summary summary;
		summary.started = true;
		CheckOutput::writeSummary(dir, summary);
		const std::string text = readFile(dir / "m5a_summary.txt");
		const size_t at = text.find("\nliveCount controllers::attack::AttackResult ");
		return at == std::string::npos ? std::string("(no row)") : text.substr(at + 1, text.find('\n', at + 1) - at - 1);
	};
	const std::string before = row();
	ASSERT_TRUE(before.starts_with("liveCount controllers::attack::AttackResult ")) << before;

	int64_t live = 0;
	uint64_t created = 0;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		runtime::Ref<controllers::attack::AttackResult> hit =
			controllers::attack::AttackResult::create(12.0f, controllers::attack::AttackStatus::NORMALHIT);
		const std::string withOne = row();
		ASSERT_EQ(sscanf_s(withOne.c_str(), "liveCount controllers::attack::AttackResult %lld %llu", &live, &created), 2) << withOne;
		EXPECT_GE(live, 1) << "the row must read the process counters: " << withOne;
		EXPECT_GE(created, 1u) << withOne;
	}
	runtime::Reclaimer::getInstance().drain();

	int64_t liveAfter = 1;
	uint64_t createdAfter = 0;
	const std::string after = row();
	ASSERT_EQ(sscanf_s(after.c_str(), "liveCount controllers::attack::AttackResult %lld %llu", &liveAfter, &createdAfter), 2) << after;
	EXPECT_EQ(liveAfter, 0) << "and follow it back to 0 once the Reclaimer freed it: " << after;
	EXPECT_EQ(createdAfter, created) << "while `created` stays - that is the half that makes the strict zero an assertion: " << after;
	std::filesystem::remove_all(dir);
}

} // namespace
} // namespace aion::gameserver
