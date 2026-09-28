#pragma once

// A child process of the M5a scenario harness (m5a-plan.md D4, F-04): a server executable started with arguments in a working directory, its
// stdout and stderr redirected into one log file that the harness reads while the process runs. Windows only.
//
// The login server is started in its own process group (CREATE_NEW_PROCESS_GROUP) and stopped with CTRL_BREAK_EVENT, which its console
// handler turns into an orderly shutdown (LoginServer.cpp); the harness then waits for the exit before it scans the log. The destructor
// terminates a process that is still running, so a failing test never leaves a server behind, and every child additionally lives in a job
// object with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE, so Windows reaps it even when no destructor runs (a CTest TIMEOUT or a crash of the test).

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
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
		/** if set, stderr goes here instead of into the log file (a tool whose stdout the harness parses, e.g. tools/oracle) */
		std::filesystem::path errorFile;
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

	/**
	 * The current content of the log file. A server that hits an unported body inside a spawn loop writes hundreds of megabytes of stack
	 * traces, so a caller that only needs the end or single lines uses readLogTail() or findLogLines() instead.
	 */
	std::string readLog() const;

	/** the last `maxBytes` bytes of the log, from the start of the first whole line in them */
	std::string readLogTail(size_t maxBytes) const;

	/** @return up to `maxMatches` log lines that contain `text`, read line by line (the whole log is never held in memory) */
	std::vector<std::string> findLogLines(std::string_view text, size_t maxMatches = 20) const;

	/**
	 * Waits until the log contains the text, the process exited or the timeout passed. Only the bytes that appeared since the last call are
	 * scanned, so waiting on a log that grows to hundreds of megabytes stays linear.
	 *
	 * @return true if the text was found
	 */
	bool waitForLog(std::string_view text, std::chrono::milliseconds timeout);

	const Options& options() const noexcept { return options_; }

	/** the command line CreateProcess gets (MSVC argument quoting), for logs and tests */
	static std::wstring commandLine(const std::filesystem::path& executable, const std::vector<std::string>& arguments);

private:
	/** @return true if the text is in the log, scanning only the bytes a previous call for the same text has not seen yet */
	bool scanLogFor(std::string_view text);

	Options options_;
	void* process = nullptr;
	/**
	 * A job object with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE holding the child (null if Windows refused to create one). Closing it kills the
	 * child, so a CTest TIMEOUT or a crash of the test process cannot orphan a server the way it could while the destructor was the only
	 * reaper.
	 */
	void* job = nullptr;
	uint32_t pid = 0;
	/** how many bytes of the log waitForLog already scanned, per searched text */
	std::map<std::string, uint64_t, std::less<>> scanned;
};

} // namespace aion::gameserver::scenario
