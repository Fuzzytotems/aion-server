// MinidumpWriter (watchdog minidumps, design §4.3, D5): snapshot dumps written by a helper process with a timeout, and in process on the
// writer thread. This test executable is its own helper: the modes below run at static initialization, before gtest's main.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "SyncTestSupport.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/sync/MinidumpWriter.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "aion/commons/utils/WindowsMacroGuard.h"
#endif

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::testsupport;
using namespace std::chrono_literals;

#if defined(_WIN32)

namespace {

constexpr std::string_view CHILD_ARGUMENT = "--minidump-test-child";
constexpr const wchar_t* HANG_VARIABLE = L"AION_MINIDUMP_TEST_HELPER_HANG";
constexpr const wchar_t* CRASH_VARIABLE = L"AION_MINIDUMP_TEST_HELPER_CRASH";
/** the exit code of a process killed by an unhandled C++ exception (it has the customer bit 0x20000000 set) */
constexpr UINT CPP_EXCEPTION_CODE = 0xE06D7363;

/**
 * Child modes of this executable: `--minidump-test-child <ms>` runs three sleeping threads for <ms> and exits 0 (a real process to dump);
 * `--write-minidump ...` is the helper (with AION_MINIDUMP_TEST_HELPER_HANG set in its environment it hangs instead, for the timeout test; with
 * AION_MINIDUMP_TEST_HELPER_CRASH it exits with the exception code of an unhandled C++ exception, like a crashed helper).
 */
int runChildModes() {
	if (__argc >= 3 && std::string_view(__argv[1]) == CHILD_ARGUMENT) {
		int millis = std::atoi(__argv[2]);
		std::vector<std::thread> threads;
		for (int i = 0; i < 3; ++i)
			threads.emplace_back([millis] { std::this_thread::sleep_for(std::chrono::milliseconds(millis)); });
		for (std::thread& thread : threads)
			thread.join();
		std::_Exit(0);
	}
	if (__argc >= 2 && std::string_view(__argv[1]) == MinidumpWriter::HELPER_ARGUMENT && GetEnvironmentVariableW(HANG_VARIABLE, nullptr, 0) != 0) {
		for (;;)
			Sleep(INFINITE);
	}
	if (__argc >= 2 && std::string_view(__argv[1]) == MinidumpWriter::HELPER_ARGUMENT && GetEnvironmentVariableW(CRASH_VARIABLE, nullptr, 0) != 0)
		ExitProcess(CPP_EXCEPTION_CODE);
	if (std::optional<int> exitCode = MinidumpWriter::runIfRequested(__argc, __argv))
		std::_Exit(*exitCode);
	return 0;
}

[[maybe_unused]] const int childModes = runChildModes();

std::filesystem::path uniqueDumpPath(std::string_view name) {
	return std::filesystem::temp_directory_path() /
		("aion-minidump-test-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(ThreadContext::current().threadId()) + "-" +
			std::string(name)) /
		"test.dmp";
}

/** A started child of this executable in CHILD mode. */
struct ChildProcess {
	PROCESS_INFORMATION info{};

	explicit ChildProcess(int millis) {
		std::wstring commandLine = L"\"" + MinidumpWriter::currentExecutable().wstring() + L"\" --minidump-test-child " + std::to_wstring(millis);
		STARTUPINFOW startup{};
		startup.cb = sizeof(startup);
		EXPECT_TRUE(CreateProcessW(MinidumpWriter::currentExecutable().c_str(), commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
			nullptr, nullptr, &startup, &info))
			<< GetLastError();
	}
	~ChildProcess() {
		if (info.hProcess != nullptr) {
			TerminateProcess(info.hProcess, 9);
			CloseHandle(info.hProcess);
			CloseHandle(info.hThread);
		}
	}
	ChildProcess(const ChildProcess&) = delete;
	ChildProcess& operator=(const ChildProcess&) = delete;
};

/** The minidump header and the number of threads in its ThreadListStream (-1 if the file is no minidump or has none). */
int64_t minidumpThreadCount(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	auto u32 = [&](size_t offset) -> uint32_t {
		if (offset + 4 > bytes.size())
			return 0;
		return static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset])) | static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset + 1])) << 8 |
			static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset + 2])) << 16 | static_cast<uint32_t>(static_cast<uint8_t>(bytes[offset + 3])) << 24;
	};
	if (bytes.size() < 32 || std::string_view(bytes.data(), 4) != "MDMP")
		return -1;
	uint32_t streams = u32(8);
	uint32_t directory = u32(12);
	for (uint32_t i = 0; i < streams; ++i) {
		size_t entry = directory + size_t{12} * i;
		if (u32(entry) == 3) // ThreadListStream
			return u32(u32(entry + 8));
	}
	return -1;
}

} // namespace

TEST(MinidumpWriterTest, HelperProcessDumpsARealChildProcessThatKeepsRunning) {
	HangGuard guard(120s);
	ASSERT_TRUE(MinidumpWriter::isSelfHelperRegistered()) << "the static initializer registered this executable as a helper";
	ChildProcess child(3000);
	ASSERT_NE(child.info.hProcess, nullptr);
	std::this_thread::sleep_for(500ms); // let the child start its threads
	std::filesystem::path file = uniqueDumpPath("child");
	MinidumpWriter::Result result = MinidumpWriter::writeWithHelper(MinidumpWriter::currentExecutable(), child.info.dwProcessId, file, 60s);
	ASSERT_TRUE(result.written()) << MinidumpWriter::statusName(result.status) << ": " << result.message;
	EXPECT_NE(result.message.find("helper process"), std::string::npos) << result.message;
	EXPECT_GE(minidumpThreadCount(file), 4) << "main thread and the three sleeping threads of the child";

	// the snapshot left no thread of the child suspended: it finishes its sleeps and exits normally
	ASSERT_EQ(WaitForSingleObject(child.info.hProcess, 30000), WAIT_OBJECT_0);
	DWORD exitCode = 99;
	GetExitCodeProcess(child.info.hProcess, &exitCode);
	EXPECT_EQ(exitCode, 0u);
	std::error_code error;
	std::filesystem::remove_all(file.parent_path(), error);
}

TEST(MinidumpWriterTest, HangingHelperIsTerminatedAfterTheTimeoutAndTheTargetKeepsRunning) {
	HangGuard guard(120s);
	ChildProcess child(2000);
	ASSERT_NE(child.info.hProcess, nullptr);
	std::filesystem::path file = uniqueDumpPath("hang");
	SetEnvironmentVariableW(HANG_VARIABLE, L"1"); // inherited by the helper only
	auto started = std::chrono::steady_clock::now();
	MinidumpWriter::Result result = MinidumpWriter::writeWithHelper(MinidumpWriter::currentExecutable(), child.info.dwProcessId, file, 1500ms);
	auto waited = std::chrono::steady_clock::now() - started;
	SetEnvironmentVariableW(HANG_VARIABLE, nullptr);
	EXPECT_EQ(result.status, MinidumpWriter::Status::TIMED_OUT) << result.message;
	EXPECT_NE(result.message.find("terminated"), std::string::npos) << result.message;
	EXPECT_GE(waited, 1500ms);
	EXPECT_LT(waited, 30s);
	EXPECT_FALSE(std::filesystem::exists(file));
	ASSERT_EQ(WaitForSingleObject(child.info.hProcess, 30000), WAIT_OBJECT_0) << "the dump target keeps running";
	std::error_code error;
	std::filesystem::remove_all(file.parent_path(), error);
}

TEST(MinidumpWriterTest, FailuresAreReportedWithoutADump) {
	std::filesystem::path file = uniqueDumpPath("failures");
	// a helper without the helper mode: cmd.exe exits with its own code
	wchar_t system[MAX_PATH];
	ASSERT_GT(GetSystemDirectoryW(system, MAX_PATH), 0u);
	MinidumpWriter::Result notAHelper =
		MinidumpWriter::writeWithHelper(std::filesystem::path(system) / L"whoami.exe", GetCurrentProcessId(), file, 30s);
	EXPECT_EQ(notAHelper.status, MinidumpWriter::Status::FAILED) << notAHelper.message;

	MinidumpWriter::Result missing = MinidumpWriter::writeWithHelper(file.parent_path() / "missing-helper.exe", GetCurrentProcessId(), file, 5s);
	EXPECT_EQ(missing.status, MinidumpWriter::Status::FAILED);
	EXPECT_NE(missing.message.find("CreateProcess"), std::string::npos) << missing.message;

	// review finding (wave 3b-2): a crashed helper's exception code has the customer bit set and was reported as "failed: error 0xC06D7363"
	SetEnvironmentVariableW(CRASH_VARIABLE, L"1"); // inherited by the helper only
	MinidumpWriter::Result crashed = MinidumpWriter::writeWithHelper(MinidumpWriter::currentExecutable(), GetCurrentProcessId(), file, 30s);
	SetEnvironmentVariableW(CRASH_VARIABLE, nullptr);
	EXPECT_EQ(crashed.status, MinidumpWriter::Status::FAILED) << crashed.message;
	EXPECT_NE(crashed.message.find("crashed with exception code 0xE06D7363"), std::string::npos) << crashed.message;
	EXPECT_EQ(crashed.message.find("failed: error"), std::string::npos) << crashed.message;

	MinidumpWriter::Result noProcess = MinidumpWriter::writeSnapshotDump(0xFFFFFFF0u, file);
	EXPECT_EQ(noProcess.status, MinidumpWriter::Status::FAILED);
	EXPECT_FALSE(std::filesystem::exists(file));
	std::error_code error;
	std::filesystem::remove_all(file.parent_path(), error);
}

TEST(MinidumpWriterTest, InProcessWriterDumpsTheCurrentProcessFromASnapshot) {
	HangGuard guard(120s);
	std::filesystem::path file = uniqueDumpPath("in-process");
	MinidumpWriter::Result result = MinidumpWriter::writeInProcess(file, 60s);
	ASSERT_TRUE(result.written()) << result.message;
	EXPECT_GE(minidumpThreadCount(file), 2) << "this thread and the writer thread";
	MinidumpWriter::Result again = MinidumpWriter::writeInProcess(file, 60s);
	EXPECT_TRUE(again.written()) << again.message;
	std::error_code error;
	std::filesystem::remove_all(file.parent_path(), error);
}

TEST(MinidumpWriterTest, HelperModeArguments) {
	char program[] = "server.exe";
	char helper[] = "--write-minidump";
	char notANumber[] = "12x";
	char path[] = "out.dmp";
	char other[] = "--check-static-data";
	char* none[] = {program, nullptr};
	char* unrelated[] = {program, other, nullptr};
	char* badPid[] = {program, helper, notANumber, path, nullptr};
	char* missingPath[] = {program, helper, notANumber, nullptr};
	EXPECT_EQ(MinidumpWriter::runIfRequested(1, none), std::nullopt);
	EXPECT_EQ(MinidumpWriter::runIfRequested(2, unrelated), std::nullopt);
	// usage errors print a usage line (review finding, wave 3b-2: a misconfigured helper only showed "exited with code 1")
	testing::internal::CaptureStderr();
	EXPECT_EQ(MinidumpWriter::runIfRequested(4, badPid), std::optional<int>(1));
	std::fflush(stderr);
	std::string badPidOutput = testing::internal::GetCapturedStderr();
	EXPECT_NE(badPidOutput.find("usage: <executable> --write-minidump <pid> <file> (invalid pid '12x')"), std::string::npos) << badPidOutput;
	testing::internal::CaptureStderr();
	EXPECT_EQ(MinidumpWriter::runIfRequested(3, missingPath), std::optional<int>(1));
	std::fflush(stderr);
	std::string missingPathOutput = testing::internal::GetCapturedStderr();
	EXPECT_NE(missingPathOutput.find("usage: <executable> --write-minidump <pid> <file> (2 arguments given)"), std::string::npos) << missingPathOutput;
	EXPECT_TRUE(MinidumpWriter::isSelfHelperRegistered());
	EXPECT_STREQ(MinidumpWriter::statusName(MinidumpWriter::Status::TIMED_OUT), "TIMED_OUT");
}

#endif
