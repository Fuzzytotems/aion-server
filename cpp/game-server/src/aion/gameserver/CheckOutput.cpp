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

/**
 * runtime::liveInstancesOf's name rule (LiveInstanceCounters.h: equal to the counter's class name, or a trailing part of one after a "::")
 * without its `live != 0` filter, which summaryLiveCounts() must not have: a class at 0 live with a non-zero `created` is exactly the row that
 * turns a zeroLiveClasses() guard into an assertion. LiveInstanceCounters.cpp:118-132 is the source of truth for the rule;
 * CheckOutputTest.SummaryLiveCountsUseTheLiveInstancesOfNameRule pins the two against each other.
 */
bool matchesClassName(const std::string& className, const std::string& name) {
	return className == name || (className.size() > name.size() + 2 && className.ends_with(name) &&
									className.compare(className.size() - name.size() - 2, 2, "::") == 0);
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
		// model::stats::calc::functions::StatFunctionProxy is NOT here any more (M5b-2 part 3): its premise was that only a character's game stats
		// hold stat functions, and closing D7 ended that - every spawn casts its post-spawn skills (NpcSkillList.getPostSpawnSkills), and the
		// statup buffs of those npcs register their functions for 86,400,000 ms and legitimately survive the shutdown with their npcs (measured
		// 1,171 of 1,171 live in gs.smoke.startup, against 0 before). A character's own proxies go with its Player, which is still a row here;
		// the live-count table still prints the class for whoever reads a run.
		// what the M5a visibility work added: the restore task that pins a Player in Q3. world::knownlist::KnownObject is NOT here any more
		// (M5b-1): its premise was that only a logged-in character builds a known list, and registering the AI handlers ended that - a walking
		// npc builds one of the npcs around it (WalkManager::targetReached -> updateKnownlist) and those entries legitimately survive the
		// shutdown with their npcs (measured 15,304 of 15,814 live, against 0 in live_counts_baseline.txt). It is bounded, not leaked -
		// KnownList::update forgets what moves out of range - and the live-count table still prints it for whoever reads a run.
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
		// M5b-1 E-03. AttackUtil makes one AttackResult per hit; it travels in the result list of SM_ATTACK and in DelayedOnAttack, and nothing
		// keeps one once the hit was applied (measured 0 live of 183 created in the M5a gate run after A-06, 0 of 93 in the geo run). This is the
		// row that catches m5b-plan.md §8 risk 2, the DelayedOnAttack that pins both creatures. DropNpc is the guard of the header note: its only
		// constructor call site is behind registerDrop's partial, so created stays 0 until M5b-3.
		"controllers::attack::AttackResult",
		"model::gameobjects::DropNpc",
	};
	return *classes;
}

const std::vector<std::string>& CheckOutput::summaryLiveClasses() {
	// built once and never destroyed, like zeroLiveClasses()
	static const auto* classes = new std::vector<std::string>{
		// the combat classes of m5b-plan.md Q2. AggroInfo and KnownObject are bounded, not zero (see the header); AttackResult and DropNpc are
		// zeroLiveClasses() rows whose `created` half only this row can show.
		"controllers::attack::AggroInfo",
		"controllers::attack::AttackResult",
		"model::gameobjects::DropNpc",
		"world::knownlist::KnownObject",
		// the npc conservation of m5b-plan.md Q3: a killed npc respawns as a NEW Npc, so `live` must come back to the baseline while `created`
		// grows by the number of respawns
		"model::gameobjects::Npc",
	};
	return *classes;
}

std::vector<runtime::LiveCount> CheckOutput::summaryLiveCounts(const std::vector<runtime::LiveCount>& counts) {
	std::vector<runtime::LiveCount> result;
	result.reserve(summaryLiveClasses().size());
	for (const std::string& className : summaryLiveClasses()) {
		runtime::LiveCount row;
		row.className = className; // the requested name, so the row's key is stable whatever the counter's qualified spelling is
		for (const runtime::LiveCount& counter : counts) {
			if (!matchesClassName(counter.className, className))
				continue;
			row.live += counter.live;
			row.created += counter.created;
		}
		result.push_back(std::move(row));
	}
	return result;
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
	if (summary.started) {
		checked.liveLeaks = checkLiveCounts();
		checked.summaryCounts = summaryLiveCounts(runtime::liveCounts());
	}
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
	// m5b-client-session.md S-2. censusLeaks comes from runFinalCensus, which runs BEFORE the runtime shuts down; this is what the shutdown
	// itself left behind (RuntimeLifecycle::ShutdownReport::censusTracked, the number behind the "N objects removed from the world are still
	// alive" warning). Only the caller that performs the shutdown can measure it - LeakCensus::uninstall() empties the table before this file is
	// written - so a summary that says "unknown" means nobody passed it in, not that nothing survived.
	out << "censusTracked " << (summary.censusTracked ? std::to_string(*summary.censusTracked) : "unknown") << '\n';
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
	// M5b-1 E-03: the classes a gate reads as numbers (summaryLiveClasses()). A liveLeak row only exists for a class that is already failing, so
	// without these rows nothing in the summary can say that an AttackResult was ever created - and "0 live" of a class that was never created is
	// a guard, not an assertion (m5a-plan.md §10.2).
	for (const runtime::LiveCount& count : summary.summaryCounts)
		out << "liveCount " << count.className << ' ' << count.live << ' ' << count.created << '\n';
}

} // namespace aion::gameserver
