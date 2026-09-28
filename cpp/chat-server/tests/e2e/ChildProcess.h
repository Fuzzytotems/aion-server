#pragma once

// A server executable started as a child process by the end-to-end test: arguments, working directory, stdout and stderr redirected into one
// log file. Windows only. The child lives in a job object with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE, so it dies with the test process even
// when no destructor runs (a CTest timeout); the destructor terminates a child that is still running. (Modelled on the game server's scenario
// harness, tests/scenario/ChildProcess.h, which the chat server tests must not depend on.)

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aion::chatserver::test {

class ChildProcess {
public:
	/** Starts the process. @throws std::runtime_error if it cannot be started */
	ChildProcess(const std::filesystem::path& executable, const std::vector<std::string>& arguments, const std::filesystem::path& workingDirectory,
		const std::filesystem::path& logFile);

	/** Terminates the process if it still runs */
	~ChildProcess();

	ChildProcess(const ChildProcess&) = delete;
	ChildProcess& operator=(const ChildProcess&) = delete;

	/** @return the exit code, std::nullopt if the process did not exit within the timeout */
	std::optional<int32_t> waitForExit(std::chrono::milliseconds timeout);

	bool isRunning();

	/** @return the current content of the log file */
	std::string readLog() const;

	/** Waits until the log contains the text, the process exited or the timeout passed. @return true if the text was found */
	bool waitForLog(std::string_view text, std::chrono::milliseconds timeout);

private:
	std::filesystem::path logFile;
	void* process = nullptr;
	void* job = nullptr;
};

} // namespace aion::chatserver::test
