#include "aion/gameserver/runtime/sync/MinidumpWriter.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <condition_variable>
#include <cstdio>
#include <format>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// dbghelp.h and processsnapshot.h must follow windows.h
#include <dbghelp.h>
#include <processsnapshot.h>
#include <shellapi.h>
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "Shell32.lib")

#include "aion/commons/utils/WindowsMacroGuard.h"
#endif

// Implementation notes
// - Snapshot flags: a virtual address clone with threads and their contexts is all a MiniDumpWithThreadInfo dump reads (no handle capture, so
//   no handle tracing is needed). PSS_CREATE_USE_VM_ALLOCATIONS keeps the snapshot facility off the target's heap, and
//   PSS_CREATE_BREAKAWAY with PSS_CREATE_BREAKAWAY_OPTIONAL (which is invalid alone) lets the clone leave a job where the job allows it without
//   failing where it does not.
// - The helper's exit code carries the Win32 error: 0x20000000 (the customer bit) | error, so it never collides with 0 (written) or 1 (usage).
//   Exit codes from 0xC0000000 are NTSTATUS-style exception codes of a crashed helper (0xC0000005, 0xE06D7363 for an unhandled C++ exception)
//   and are tested first: several of them have the customer bit set as well.
// - Paths in messages are UTF-8 (path::string() throws for characters outside the ANSI code page).
// - In-process writer: one detached thread created on first use, waiting on a leaked state (usable during static destruction). A job that does
//   not finish in time leaves `abandoned` set; later calls return FAILED immediately instead of queueing behind a hung DbgHelp call.

namespace aion::gameserver::runtime {

namespace {

constexpr int EXIT_WRITTEN = 0;
constexpr int EXIT_USAGE = 1;
constexpr int EXIT_ERROR_BIT = 0x20000000;
/** the lowest NTSTATUS error / exception code (severity bits 11) */
constexpr unsigned long EXIT_CRASH_MIN = 0xC0000000ul;
constexpr std::string_view USAGE = "usage: <executable> --write-minidump <pid> <file>";

std::atomic<bool>& selfHelperFlag() noexcept {
	static std::atomic<bool> flag{false}; // lint: L5 process-wide registration flag, written once by main before other threads exist
	return flag;
}

#if defined(_WIN32)
/** a path as UTF-8 text */
std::string utf8Text(const std::filesystem::path& path) {
	std::u8string text = path.u8string();
	return std::string(text.begin(), text.end());
}

std::string win32Error(std::string_view what, DWORD error) {
	return std::format("{} failed: error {}", what, error);
}

BOOL CALLBACK snapshotCallback(PVOID, PMINIDUMP_CALLBACK_INPUT input, PMINIDUMP_CALLBACK_OUTPUT output) {
	if (input != nullptr && output != nullptr && input->CallbackType == IsProcessSnapshotCallback)
		output->Status = S_FALSE; // the process handle is a snapshot handle (HPSS)
	return TRUE;
}

/** The dump itself; returns 0 or a Win32 error, with a message. */
std::pair<DWORD, std::string> writeSnapshot(uint32_t pid, const std::filesystem::path& file) {
	DWORD access = PROCESS_CREATE_PROCESS | PROCESS_VM_READ | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE;
	HANDLE process = pid == GetCurrentProcessId() ? GetCurrentProcess() : OpenProcess(access, FALSE, pid);
	if (process == nullptr) {
		DWORD error = GetLastError();
		return {error, win32Error(std::format("OpenProcess({})", pid), error)};
	}
	struct ProcessCloser {
		HANDLE handle;
		~ProcessCloser() {
			if (handle != GetCurrentProcess())
				CloseHandle(handle);
		}
	} closeProcess{process};

	auto flags = static_cast<PSS_CAPTURE_FLAGS>(PSS_CAPTURE_VA_CLONE | PSS_CAPTURE_THREADS | PSS_CAPTURE_THREAD_CONTEXT |
		PSS_CREATE_BREAKAWAY | PSS_CREATE_BREAKAWAY_OPTIONAL | PSS_CREATE_USE_VM_ALLOCATIONS | PSS_CREATE_RELEASE_SECTION);
	HPSS snapshot = nullptr;
	if (DWORD error = PssCaptureSnapshot(process, flags, CONTEXT_ALL, &snapshot); error != ERROR_SUCCESS)
		return {error, win32Error("PssCaptureSnapshot", error)};
	struct SnapshotCloser {
		HPSS handle;
		~SnapshotCloser() { PssFreeSnapshot(GetCurrentProcess(), handle); }
	} closeSnapshot{snapshot};

	std::error_code ignored;
	if (file.has_parent_path())
		std::filesystem::create_directories(file.parent_path(), ignored);
	HANDLE out = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (out == INVALID_HANDLE_VALUE) {
		DWORD error = GetLastError();
		return {error, win32Error("CreateFile " + utf8Text(file), error)};
	}
	MINIDUMP_CALLBACK_INFORMATION callback{&snapshotCallback, nullptr};
	auto type = static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
	BOOL written = MiniDumpWriteDump(static_cast<HANDLE>(snapshot), pid, out, type, nullptr, nullptr, &callback);
	DWORD error = written ? ERROR_SUCCESS : GetLastError();
	CloseHandle(out);
	if (!written) {
		std::filesystem::remove(file, ignored);
		// MiniDumpWriteDump reports HRESULTs through GetLastError; keep a non-zero code
		return {error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error, win32Error("MiniDumpWriteDump", error)};
	}
	return {ERROR_SUCCESS, {}};
}

std::wstring quoteArgument(const std::wstring& argument) {
	// CommandLineToArgvW rules: backslashes are literal unless they precede a quote
	std::wstring quoted = L"\"";
	size_t backslashes = 0;
	for (wchar_t c : argument) {
		if (c == L'\\') {
			++backslashes;
			continue;
		}
		quoted.append(c == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
		quoted.push_back(c);
		backslashes = 0;
	}
	quoted.append(backslashes * 2, L'\\'); // before the closing quote
	quoted.push_back(L'"');
	return quoted;
}
#endif

/** The in-process writer thread's state (leaked). */
struct InProcessWriter {
	std::mutex mutex; // confined: runtime-internal, never held while other locks are taken
	std::condition_variable condition;
	bool started = false;
	bool abandoned = false;
	bool pending = false;
	bool done = false;
	std::filesystem::path file;
	MinidumpWriter::Result result;
	std::mutex callerMutex; // serializes callers
};

InProcessWriter& inProcessWriter() {
	static auto* writer = new InProcessWriter(); // lint: L5 leaked writer state, usable by a watchdog dump during static destruction
	return *writer;
}

} // namespace

const char* MinidumpWriter::statusName(Status status) noexcept {
	switch (status) {
		case Status::WRITTEN:
			return "WRITTEN";
		case Status::FAILED:
			return "FAILED";
		case Status::TIMED_OUT:
			return "TIMED_OUT";
		case Status::UNSUPPORTED:
			return "UNSUPPORTED";
	}
	return "?";
}

void MinidumpWriter::registerSelfHelper() noexcept {
	selfHelperFlag().store(true, std::memory_order_release);
}

bool MinidumpWriter::isSelfHelperRegistered() noexcept {
	return selfHelperFlag().load(std::memory_order_acquire);
}

std::optional<int> MinidumpWriter::runIfRequested(int argc, char* argv[]) {
	registerSelfHelper();
	if (argc < 2 || argv == nullptr || argv[1] == nullptr || std::string_view(argv[1]) != HELPER_ARGUMENT)
		return std::nullopt;
	if (argc != 4) {
		std::fprintf(stderr, "%s (%d arguments given)\n", USAGE.data(), argc - 1);
		return EXIT_USAGE;
	}
	std::string_view pidText = argv[2];
	uint32_t pid = 0;
	auto [end, error] = std::from_chars(pidText.data(), pidText.data() + pidText.size(), pid);
	if (error != std::errc() || end != pidText.data() + pidText.size() || pid == 0) {
		std::fprintf(stderr, "%s (invalid pid '%s')\n", USAGE.data(), argv[2]);
		return EXIT_USAGE;
	}
#if defined(_WIN32)
	// the path arrives in the ANSI code page of CreateProcessW's command line conversion; use the wide command line instead
	int wideArgc = 0;
	LPWSTR* wideArgv = CommandLineToArgvW(GetCommandLineW(), &wideArgc);
	bool wide = wideArgv != nullptr && wideArgc == 4 && std::wstring_view(wideArgv[1]) == L"--write-minidump";
	std::filesystem::path file = wide ? std::filesystem::path(wideArgv[3]) : std::filesystem::path(argv[3]);
	if (wideArgv != nullptr)
		LocalFree(wideArgv);
	auto [code, message] = writeSnapshot(pid, file);
	if (code == ERROR_SUCCESS)
		return EXIT_WRITTEN;
	std::fprintf(stderr, "%s\n", message.c_str());
	return EXIT_ERROR_BIT | static_cast<int>(code & 0x0FFFFFFF);
#else
	return EXIT_USAGE;
#endif
}

std::filesystem::path MinidumpWriter::currentExecutable() {
#if defined(_WIN32)
	std::wstring buffer(MAX_PATH, L'\0');
	for (;;) {
		DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (length == 0)
			return {};
		if (length < buffer.size()) {
			buffer.resize(length);
			return std::filesystem::path(buffer);
		}
		buffer.resize(buffer.size() * 2);
	}
#else
	return {};
#endif
}

MinidumpWriter::Result MinidumpWriter::writeSnapshotDump(uint32_t pid, const std::filesystem::path& file) {
#if defined(_WIN32)
	auto [code, message] = writeSnapshot(pid, file);
	if (code == ERROR_SUCCESS)
		return {Status::WRITTEN, "written from a process snapshot"};
	return {Status::FAILED, message};
#else
	static_cast<void>(pid);
	static_cast<void>(file);
	return {Status::UNSUPPORTED, "minidumps are only written on Windows"};
#endif
}

MinidumpWriter::Result MinidumpWriter::writeWithHelper(const std::filesystem::path& helper, uint32_t pid, const std::filesystem::path& file,
	std::chrono::milliseconds timeout) {
#if defined(_WIN32)
	std::error_code ignored;
	if (file.has_parent_path())
		std::filesystem::create_directories(file.parent_path(), ignored);
	std::filesystem::path absoluteFile = std::filesystem::absolute(file, ignored);
	std::wstring commandLine = quoteArgument(helper.wstring()) + L" " + std::wstring(HELPER_ARGUMENT.begin(), HELPER_ARGUMENT.end()) + L" " +
		std::to_wstring(pid) + L" " + quoteArgument(absoluteFile.wstring());
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION child{};
	if (!CreateProcessW(helper.c_str(), commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child))
		return {Status::FAILED, win32Error("CreateProcess " + utf8Text(helper), GetLastError())};
	CloseHandle(child.hThread);
	DWORD waited = WaitForSingleObject(child.hProcess, static_cast<DWORD>(std::min<int64_t>(timeout.count(), INFINITE - 1)));
	Result result;
	if (waited != WAIT_OBJECT_0) {
		TerminateProcess(child.hProcess, 1);
		WaitForSingleObject(child.hProcess, 5000);
		std::filesystem::remove(absoluteFile, ignored);
		result = {Status::TIMED_OUT, std::format("helper process {} (pid {}) did not finish within {} ms and was terminated", utf8Text(helper),
										 child.dwProcessId, timeout.count())};
	} else {
		DWORD exitCode = 1;
		GetExitCodeProcess(child.hProcess, &exitCode);
		if (exitCode == EXIT_WRITTEN)
			result = {Status::WRITTEN, std::format("written by helper process {} (pid {})", utf8Text(helper), child.dwProcessId)};
		else if (exitCode >= EXIT_CRASH_MIN)
			result = {Status::FAILED, std::format("helper process {} crashed with exception code 0x{:08X}", utf8Text(helper), exitCode)};
		else if ((exitCode & EXIT_ERROR_BIT) != 0)
			result = {Status::FAILED, std::format("helper process {} failed: error {}", utf8Text(helper), exitCode & ~static_cast<DWORD>(EXIT_ERROR_BIT))};
		else
			result = {Status::FAILED, std::format("helper process {} exited with code {} (no minidump helper mode?)", utf8Text(helper), exitCode)};
	}
	CloseHandle(child.hProcess);
	return result;
#else
	static_cast<void>(helper);
	static_cast<void>(pid);
	static_cast<void>(file);
	static_cast<void>(timeout);
	return {Status::UNSUPPORTED, "minidumps are only written on Windows"};
#endif
}

MinidumpWriter::Result MinidumpWriter::writeInProcess(const std::filesystem::path& file, std::chrono::milliseconds timeout) {
#if defined(_WIN32)
	InProcessWriter& w = inProcessWriter();
	std::scoped_lock callerLock(w.callerMutex);
	std::unique_lock lock(w.mutex);
	if (w.abandoned)
		return {Status::FAILED, "the in-process minidump writer did not finish an earlier dump and is disabled"};
	if (!w.started) {
		w.started = true;
		std::thread([&w] {
			std::unique_lock threadLock(w.mutex);
			for (;;) {
				w.condition.wait(threadLock, [&w] { return w.pending; });
				w.pending = false;
				std::filesystem::path target = w.file;
				threadLock.unlock();
				Result result = writeSnapshotDump(GetCurrentProcessId(), target);
				if (result.written())
					result.message = "written in process from a process snapshot";
				threadLock.lock();
				w.result = std::move(result);
				w.done = true;
				w.condition.notify_all();
			}
		}).detach();
	}
	w.file = file;
	w.done = false;
	w.pending = true;
	w.condition.notify_all();
	if (!w.condition.wait_for(lock, timeout, [&w] { return w.done; })) {
		w.abandoned = true;
		return {Status::TIMED_OUT, std::format("the in-process minidump writer did not finish within {} ms (abandoned)", timeout.count())};
	}
	return w.result;
#else
	static_cast<void>(file);
	static_cast<void>(timeout);
	return {Status::UNSUPPORTED, "minidumps are only written on Windows"};
#endif
}

} // namespace aion::gameserver::runtime
