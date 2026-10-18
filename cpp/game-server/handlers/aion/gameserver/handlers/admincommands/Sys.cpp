#include "aion/gameserver/handlers/admincommands/Sys.h"

#include <chrono>
#include <format>
#include <optional>
#include <string>
#include <vector>

#include "aion/commons/utils/ExitCode.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/info/SystemInfo.h"
#include "aion/commons/utils/info/VersionInfo.h"
#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/ShutdownConfig.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Sys);

// Java Sys.java:22-29
Sys::Sys()
	: AdminCommand("sys", "Shows and controls the system environment.",
		  "<info> - Shows general system information.\n"
		  "<memory> [gc] - Shows memory usage statistics and optionally runs the garbage collector.\n"
		  "<threadpool> - Shows thread pool manager info.\n"
		  "<restart|shutdown> [delay] - Restarts or shuts down the server after the specified delay in seconds (default: uses delay from config).\n") {
}

// Java Sys.java:31-62
void Sys::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty()) { // parity= if (params == null || params.length < 1) {
		sendInfo(admin);
		return;
	}

	if (commons::utils::StringUtils::equalsIgnoreCase("info", params[0])) { // parity= if ("info".equalsIgnoreCase(params[0])) {
		const auto local = utils::time::ServerTime::now().get_local_time(); // parity: C++ the "H:mm:ss" pattern, below
		const std::chrono::hh_mm_ss<std::chrono::milliseconds> time(local - std::chrono::floor<std::chrono::days>(local)); // parity: (the same)
		sendInfo(admin, "System information at: " + std::format("{}:{:02}:{:02}", time.hours().count(), time.minutes().count(), time.seconds().count())); // parity= sendInfo(player, "System information at: " + ServerTime.now().format(DateTimeFormatter.ofPattern("H:mm:ss")));
		sendInfo(admin, commons::utils::info::VersionInfo::commons().toString(configs::main::GSConfig::TIME_ZONE_ID.load())); // parity= sendInfo(player, VersionInfo.commons.toString(GSConfig.TIME_ZONE_ID));
		sendInfo(admin, GameServer::versionInfo().toString(configs::main::GSConfig::TIME_ZONE_ID.load())); // parity= sendInfo(player, GameServer.versionInfo.toString(GSConfig.TIME_ZONE_ID));
		sendInfo(admin, std::span<const std::string>(commons::utils::info::SystemInfo::getSystemInfo())); // parity= sendInfo(player, SystemInfo.getSystemInfo());
	} else if (commons::utils::StringUtils::equalsIgnoreCase("memory", params[0])) { // parity= } else if ("memory".equalsIgnoreCase(params[0])) {
		if (params.size() > 1 && commons::utils::StringUtils::equalsIgnoreCase("gc", params[1])) { // parity= if (params.length > 1 && "gc".equalsIgnoreCase(params[1])) {
			auto time = std::chrono::steady_clock::now(); // parity= long time = System.currentTimeMillis();
			// the port has no garbage collector (deviation): the timing around nothing stays
			// parity= System.gc();
			const int64_t took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - time).count(); // parity: (below)
			sendInfo(admin, "Garbage collection took " + std::to_string(took) + " ms"); // parity= sendInfo(player, "Garbage collection took " + (System.currentTimeMillis() - time) + " ms");
		}
		sendInfo(admin, std::span<const std::string>(commons::utils::info::SystemInfo::getMemoryInfo())); // parity= sendInfo(player, SystemInfo.getMemoryInfo());
	} else if (commons::utils::StringUtils::equalsIgnoreCase("threadpool", params[0])) { // parity= } else if ("threadpool".equalsIgnoreCase(params[0])) {
		std::vector<std::string> stats = ThreadPoolManager::getInstance().getStats();
		for (std::string stat : stats) {
			std::erase(stat, '\t'); // parity= sendInfo(player, stat.replaceAll("\t", ""));
			sendInfo(admin, stat); // parity: (the same)
		}
	} else if (commons::utils::StringUtils::equalsIgnoreCase("shutdown", params[0])) { // parity= } else if ("shutdown".equalsIgnoreCase(params[0])) {
		initShutdown(commons::utils::ExitCode::NORMAL, params.size() == 1 ? std::nullopt : std::optional<std::string>(params[1])); // parity= initShutdown(ExitCode.NORMAL, params.length == 1 ? null : params[1]);
	} else if (commons::utils::StringUtils::equalsIgnoreCase("restart", params[0])) { // parity= } else if ("restart".equalsIgnoreCase(params[0])) {
		initShutdown(commons::utils::ExitCode::RESTART, params.size() == 1 ? std::nullopt : std::optional<std::string>(params[1])); // parity= initShutdown(ExitCode.RESTART, params.length == 1 ? null : params[1]);
	} else {
		sendInfo(admin);
	}
}

// Java Sys.java:64-67
void Sys::initShutdown(int32_t exitCode, std::optional<std::string> delay) {
	int32_t delaySeconds = !delay.has_value() ? configs::main::ShutdownConfig::DELAY.load() : commons::utils::parseInt(*delay); // parity= int delaySeconds = delay == null ? ShutdownConfig.DELAY : Integer.parseInt(delay);
	GameServer::initShutdown(exitCode, delaySeconds);
}

} // namespace aion::gameserver::handlers::admincommands
