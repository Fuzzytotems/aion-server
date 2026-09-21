#pragma once

// A child process of the M5a scenario harness (m5a-plan.md D4, F-04): a server executable started with arguments in a working directory, its
// stdout and stderr redirected into one log file that the harness reads while the process runs. Windows only.
//
// The login server is started in its own process group (CREATE_NEW_PROCESS_GROUP) and stopped with CTRL_BREAK_EVENT, which its console
// handler turns into an orderly shutdown (LoginServer.cpp); the harness then waits for the exit before it scans the log. The destructor
// terminates a process that is still running, so a failing test never leaves a server behind.

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aion::gameserver::scenario {

class ChildProcess {
public:
	struct Options {
		std::filesystem::path executable;
		std::vector<std::string> arguments;
		std::filesystem::path workingDirectory;
		/** stdout and stderr go to this file (truncated) */
		std::filesystem::path logFile;
		/** CREATE_NEW_PROCESS_GROUP, required for sendCtrlBreak() */
		bool newProcessGroup = false;
	};

	/** Starts the process. @throws std::runtime_error if it cannot be started */
	explicit ChildProcess(Options options);

	/** Terminates the process if it still runs */
	~ChildProcess();

	ChildProcess(const ChildProcess&) = delete;
	ChildProcess& operator=(const ChildProcess&) = delete;

	/** @return the exit code, std::nullopt if the process did not exit within the timeout */
	std::optional<int32_t> waitForExit(std::chrono::milliseconds timeout);

	bool isRunning();

	/** GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT) to the process group. @return false if Windows refused (no shared console) */
	bool sendCtrlBreak();

	/** TerminateProcess with the given exit code (does nothing if it already exited) */
	void terminate(uint32_t exitCode = 99);

	uint32_t processId() const noexcept { return pid; }

	/** the current content of the log file */
	std::string readLog() const;

	/** Waits until the log contains the text, the process exited or the timeout passed. @return true if the text was found */
	bool waitForLog(std::string_view text, std::chrono::milliseconds timeout);

	const Options& options() const noexcept { return options_; }

	/** the command line CreateProcess gets (MSVC argument quoting), for logs and tests */
	static std::wstring commandLine(const std::filesystem::path& executable, const std::vector<std::string>& arguments);

private:
	Options options_;
	void* process = nullptr;
	uint32_t pid = 0;
};

} // namespace aion::gameserver::scenario
