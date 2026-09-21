#include "aion/gameserver/CheckOutput.h"

#include <algorithm>
#include <fstream>
#include <thread>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/LiveInstanceCounters.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sync/LockOrderValidator.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver {

namespace {

const commons::logging::Logger& log() {
	static const auto* instance = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.CheckOutput"));
	return *instance;
}

std::ofstream openOutput(const std::filesystem::path& file) {
	std::filesystem::create_directories(file.parent_path());
	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	if (!out)
		throw commons::utils::IOException("Cannot write " + file.string());
	return out;
}

/** the file name of a source location without its directories */
std::string_view fileName(std::string_view path) {
	size_t slash = path.find_last_of("/\\");
	return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

} // namespace

void CheckOutput::writeUnportedTrace(const std::filesystem::path& dir) {
	std::ofstream out = openOutput(dir / "unported_trace.txt");
	runtime::writeUnportedTrace(out);
}

void CheckOutput::writePartialTrace(const std::filesystem::path& dir) {
	std::ofstream out = openOutput(dir / "partial_trace.txt");
	runtime::writePartialTrace(out);
}

void CheckOutput::writeLiveCounts(const std::filesystem::path& file) {
	std::ofstream out = openOutput(file);
	runtime::writeLiveCounts(out);
}

std::vector<runtime::LeakCensus::LeakReport> CheckOutput::runFinalCensus(const std::filesystem::path& dir, const std::function<bool()>& playersLeft,
	std::chrono::milliseconds logoutTimeout) {
	const auto deadline = std::chrono::steady_clock::now() + logoutTimeout;
	while (playersLeft() && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	drainPools(deadline);

	runtime::LeakCensus& census = runtime::LeakCensus::getInstance();
	runtime::LeakCensus::Config config = census.getConfig();
	config.censusAfter = std::chrono::milliseconds(0);
	config.checkInterval = std::chrono::milliseconds(0);
	config.zombieBreakerEnabled = false;
	census.configure(config);
	runtime::Reclaimer::getInstance().reclaimNow();
	runtime::Reclaimer::getInstance().reclaimNow();

	// An entry whose object has refcount 0 is NOT a leak: LeakCensus only ever reports an object that was still referenced when a census check
	// ran (`if (count == 0) continue; // waiting for reclamation`), so a 0 here means the last reference went away after that check and the
	// object is only waiting for the reclaimer to sweep it - which also removes its entry from the table. Under a loaded machine that window is
	// wide enough to put a Player into census.txt with refcount 0 while live_counts.txt already reports 0 live Players (measured in a full
	// `ctest -j 6`). Reclaim until no such entry is left, then report what is really still referenced.
	std::vector<runtime::LeakCensus::LeakReport> leaks;
	for (;;) {
		leaks = census.getLeaks();
		const bool pending = std::ranges::any_of(leaks, [](const runtime::LeakCensus::LeakReport& leak) { return leak.refCount == 0; });
		if (!pending || std::chrono::steady_clock::now() >= deadline)
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(25));
		runtime::Reclaimer::getInstance().reclaimNow();
	}
	std::erase_if(leaks, [](const runtime::LeakCensus::LeakReport& leak) { return leak.refCount == 0; });
	{
		std::ofstream out = openOutput(dir / "census.txt");
		writeCensus(out, leaks);
	}
	runBreakerPass();
	return leaks;
}

void CheckOutput::runBreakerPass() {
	runtime::LeakCensus& census = runtime::LeakCensus::getInstance();
	const runtime::LeakCensus::Config previous = census.getConfig();
	runtime::LeakCensus::Config config = previous;
	config.censusAfter = std::chrono::milliseconds(0);
	config.checkInterval = std::chrono::milliseconds(0);
	config.zombieBreakerEnabled = true;
	config.zombieBreakAfter = std::chrono::milliseconds(0);
	config.stalePinAfter = std::chrono::milliseconds(0);
	config.stalePinCheckInterval = std::chrono::milliseconds(0);
	census.configure(config); // resets the "next check" times, so the following scan runs both checks whatever the run did before
	runtime::Reclaimer::getInstance().reclaimNow();
	// the breaker bodies are posted to the instant pool: wait for them, so zombieCutCount() is final when this returns
	drainPools(std::chrono::steady_clock::now() + std::chrono::seconds(5));
	runtime::Reclaimer::getInstance().reclaimNow(); // frees what the cuts released
	// Restore the run's own thresholds. Without this the census stays armed at age 0 for the rest of the shutdown (RuntimeLifecycle::shutdown runs
	// after this step), so an object removed from the world during that shutdown could be cut and logged after main.cpp sampled zombieCutCount():
	// the summary and the log would then disagree. What this pass itself cut stays visible through zombieCuts, which §5.7 Q8 asserts is 0.
	census.configure(previous);
}

void CheckOutput::drainPools(std::chrono::steady_clock::time_point deadline) {
	// The plan's "drain the pools" step of F-07. There is no pool-wide quiesce call, so one barrier task per pool is queued behind everything
	// that was queued before it: when the barriers ran, no logout task of the network shutdown is still pending and pinning its Player. Periodic
	// tasks are untouched (RuntimeLifecycle::shutdown cancels them afterwards).
	utils::ThreadPoolManager& pool = utils::ThreadPoolManager::getInstance();
	std::vector<runtime::FutureRef> barriers;
	barriers.push_back(pool.submit([] {}));
	barriers.push_back(pool.submitLongRunning([] {}));
	for (const runtime::FutureRef& barrier : barriers) {
		if (!barrier) // the pools are already shut down: nothing is queued
			continue;
		while (!barrier->isDone() && std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void CheckOutput::writeCensus(std::ostream& out, const std::vector<runtime::LeakCensus::LeakReport>& leaks) {
	out << "# final census v1\n";
	for (const runtime::LeakCensus::LeakReport& leak : leaks) {
		out << leak.className << '\t' << leak.objectId << '\t' << leak.refCount << '\t';
		for (size_t i = 0; i < leak.pinningTasks.size(); i++) {
			const runtime::TaskInfo& task = leak.pinningTasks[i];
			if (i > 0)
				out << "; ";
			out << fileName(task.where.file_name()) << ':' << task.where.line() << ' ' << task.kind;
		}
		out << '\n';
	}
}

size_t CheckOutput::writeLockdepReports(const std::filesystem::path& dir) {
	std::vector<runtime::LockOrderValidator::Report> reports = runtime::LockOrderValidator::getInstance().getReports();
	std::ofstream out = openOutput(dir / "lockdep.txt");
	out << "# lockdep reports v1\n";
	for (const runtime::LockOrderValidator::Report& report : reports)
		out << report.text << "\noccurrences " << report.occurrences << '\n';
	return reports.size();
}

void CheckOutput::writeWatchdogDumps(const std::filesystem::path& dir, const std::vector<std::string>& dumps) {
	std::ofstream out = openOutput(dir / "watchdog.txt");
	out << "# watchdog dumps v1\n";
	for (const std::string& dump : dumps)
		out << dump << '\n';
}

const std::vector<std::string>& CheckOutput::zeroLiveClasses() {
	// built once and never destroyed: the check runs from the ShutdownHook thread, after the runtime shut down
	static const auto* classes = new std::vector<std::string>{
		// the character and what hangs off it (every one of these was created and reached 0 in the M5a gate runs of stage 2). Item is NOT here:
		// it is bounded by the surviving accounts instead (accountBoundedLiveClasses)
		"model::gameobjects::player::Player",
		"model::gameobjects::player::AbyssRank",
		"model::gameobjects::player::BlockList",
		"model::gameobjects::player::Cooldowns",
		"model::gameobjects::player::Macros",
		"model::gameobjects::player::PlayerSettings",
		"model::gameobjects::player::QuestStateList",
		"model::gameobjects::player::RecipeList",
		"model::skill::PlayerSkillList",
		"model::skill::PlayerSkillEntry",
		"model::stats::calc::functions::StatFunctionProxy",
		// what this wave added: the knownlist entries of the visibility work and the restore task that pins a Player in Q3
		"world::knownlist::KnownObject",
		"services::LifeStatsRestoreService::HpMpRestoreTask",
		// per-session helpers: the packet blobs an enter world builds and the pending login-server request of a login
		"network::aion::iteminfo::ItemInfoBlob",
		"network::aion::skillinfo::SkillEntryWriter",
		"network::loginserver::LoginServer::LoginRequest",
		"questEngine::model::QuestEnv",
		// creatures and tasks a character owns. None of them is created by the M5a scenario (no summon, pet, kisk or gathering on the scripted
		// path), so these rows are guards for the stress run, the real client and M5b rather than assertions the gate exercises today.
		"model::gameobjects::Summon",
		"model::gameobjects::Pet",
		"model::gameobjects::Kisk",
		"skillengine::task::AbstractInteractionTask",
		"skillengine::task::GatheringTask",
		"GatheringTask_ActionObserver",
		"controllers::observer::StanceObserver",
	};
	return *classes;
}

const std::vector<std::string>& CheckOutput::accountBoundedLiveClasses() {
	// see the header: the account warehouse of an Account that survives the shutdown keeps its Items, which is Java's own behaviour
	static const auto* classes = new std::vector<std::string>{
		"model::gameobjects::Item",
	};
	return *classes;
}

std::vector<runtime::LiveCount> CheckOutput::checkLiveCounts() {
	if constexpr (!runtime::LIVE_COUNTS_ENABLED)
		log().warn("Live instance leak check: nothing is counted in this build (AION_CHECKED is off), so the check reports nothing whatever the run "
				   "leaked; m5a_summary.txt says liveCountsEnabled false (m5a-plan.md §10.2)");
	return checkLiveCounts(runtime::liveCounts());
}

std::vector<runtime::LiveCount> CheckOutput::checkLiveCounts(const std::vector<runtime::LiveCount>& counts) {
	std::vector<std::string> checked = zeroLiveClasses();
	// The account-bounded classes are checked only while no Account survived: one that did keeps its account warehouse with its items, and how
	// many items that warehouse holds is not something this process can know while it writes its report (header, m5a-plan.md §10.1).
	const std::vector<runtime::LiveCount> accounts = runtime::liveInstancesOf(counts, {std::string("model::account::Account")});
	if (accounts.empty())
		checked.insert(checked.end(), accountBoundedLiveClasses().begin(), accountBoundedLiveClasses().end());
	else
		for (const runtime::LiveCount& unchecked : runtime::liveInstancesOf(counts, accountBoundedLiveClasses()))
			log().warn("Live instance leak check: {} of the {} {} instances are still alive, and an Account survived the shutdown (its connection "
					   "never reached LoginServer::onDisconnect), so its account warehouse may legitimately hold them: not checked here. The bound "
					   "is the scenario gate's, which knows the warehouses it filled (m5a-plan.md §10.1)",
				unchecked.live, unchecked.created, unchecked.className);

	std::vector<runtime::LiveCount> leaks = runtime::liveInstancesOf(counts, checked);
	for (const runtime::LiveCount& leak : leaks)
		log().error("Live instance leak: {} of the {} {} instances created since start are still alive after the runtime shut down (a class that "
					"belongs to a character must be at 0 once it logged out; m5a-plan.md D8)",
			leak.live, leak.created, leak.className);
	return leaks;
}

void CheckOutput::writeSummary(const std::filesystem::path& dir, const Summary& summary) {
	Summary checked = summary;
	if (summary.started)
		checked.liveLeaks = checkLiveCounts();
	std::ofstream out = openOutput(dir / "m5a_summary.txt");
	writeSummary(out, checked);
}

void CheckOutput::writeSummary(std::ostream& out, const Summary& summary) {
	out << "started " << (summary.started ? "true" : "false") << '\n';
	out << "exitCode " << summary.exitCode << '\n';
	out << "unportedHits " << runtime::unportedHitCount() << '\n';
	out << "partialHits " << runtime::partialHitCount() << '\n';
	out << "partialSites " << runtime::partialHits().size() << '\n';
	out << "censusLeaks " << summary.censusLeaks << '\n';
	out << "zombieCuts " << summary.zombieCuts << '\n';
	out << "lockdepReports " << summary.lockdepReports << '\n';
	out << "watchdogDumps " << summary.watchdogDumps << '\n';
	// m5a-plan.md W-07: every notifySee/notifyNotSee/notifyNotKnow catch counts as a failure. Java logs it as log.error("", ex), i.e. with an
	// empty message, so without this counter a controller that throws on every notification is only an unattributable ERROR line.
	out << "knownListNotifyFailures " << world::knownlist::KnownList::notifyFailureCount() << '\n';
	out << "atreianPassportDisabled " << (!summary.atreianPassportDisabled ? "unknown" : *summary.atreianPassportDisabled ? "true" : "false") << '\n';
	std::vector<std::string> packets = summary.notPortedClientPackets;
	std::ranges::sort(packets);
	for (const std::string& packet : packets)
		out << "notPortedClientPacket " << packet << '\n';
	// m5a-plan.md D8: the classes of zeroLiveClasses() that are still alive. Every one of them is also an ERROR line in the server log
	// (checkLiveCounts), so a gate that only reads the log still fails; these rows say which class and how many.
	// liveCountsEnabled says whether the check could see anything at all: the live-instance counters are compiled out in a release build
	// (runtime::LIVE_COUNTS_ENABLED, AION_CHECKED), where "liveLeaks 0" means "not measured", not "nothing leaked". A gate that relies on the
	// check must assert this row is true (m5a-plan.md §10.2).
	out << "liveCountsEnabled " << (runtime::LIVE_COUNTS_ENABLED ? "true" : "false") << '\n';
	out << "liveLeaks " << summary.liveLeaks.size() << '\n';
	for (const runtime::LiveCount& leak : summary.liveLeaks)
		out << "liveLeak " << leak.className << ' ' << leak.live << '\n';
}

} // namespace aion::gameserver
