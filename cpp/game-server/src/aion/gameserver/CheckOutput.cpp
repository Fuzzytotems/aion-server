#include "aion/gameserver/CheckOutput.h"

#include <algorithm>
#include <fstream>
#include <thread>

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
	std::ofstream out = openOutput(dir / "census.txt");
	writeCensus(out, leaks);
	return leaks;
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

void CheckOutput::writeSummary(const std::filesystem::path& dir, const Summary& summary) {
	std::ofstream out = openOutput(dir / "m5a_summary.txt");
	writeSummary(out, summary);
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
}

} // namespace aion::gameserver
