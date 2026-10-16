#include "ChildProcess.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "aion/commons/utils/StringUtils.h"

#include <Windows.h>

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::scenario {

namespace {

std::wstring widen(std::string_view utf8) {
	std::u16string utf16 = commons::utils::StringUtils::toUtf16(utf8);
	return std::wstring(utf16.begin(), utf16.end());
}

/**
 * A Windows handle owned by a scope. The child process constructor has several failure paths - an unwritable log file, an error file that
 * cannot be created, a create_directories that throws because a path component is a file, CreateProcess itself - and every one of them has to
 * close the handles opened before it. Doing that by hand is how the stdin NUL handle came to leak on the error-file path (stage 2 review).
 * CreateProcess inherits the handle VALUES into the child, so releasing ownership here after the call is right: the child has its own copies.
 */
class Handle {
public:
	Handle() noexcept = default;
	explicit Handle(HANDLE value) noexcept : handle(value) {}
	~Handle() {
		reset();
	}

	Handle(const Handle&) = delete;
	Handle& operator=(const Handle&) = delete;

	HANDLE get() const noexcept {
		return handle;
	}

	bool valid() const noexcept {
		return handle != nullptr && handle != INVALID_HANDLE_VALUE;
	}

	HANDLE release() noexcept {
		HANDLE value = handle;
		handle = nullptr;
		return value;
	}

	void reset(HANDLE value = nullptr) noexcept {
		if (valid())
			CloseHandle(handle);
		handle = value;
	}

private:
	HANDLE handle = nullptr;
};

/** One argument quoted like the MSVC runtime parses it (CommandLineToArgvW rules) */
std::wstring quote(const std::wstring& argument) {
	if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos)
		return argument;
	std::wstring quoted = L"\"";
	for (size_t i = 0;; i++) {
		size_t backslashes = 0;
		while (i < argument.size() && argument[i] == L'\\') {
			i++;
			backslashes++;
		}
		if (i == argument.size()) {
			quoted.append(backslashes * 2, L'\\');
			break;
		}
		if (argument[i] == L'"') {
			quoted.append(backslashes * 2 + 1, L'\\');
			quoted.push_back(L'"');
		} else {
			quoted.append(backslashes, L'\\');
			quoted.push_back(argument[i]);
		}
	}
	quoted.push_back(L'"');
	return quoted;
}

} // namespace

std::wstring ChildProcess::commandLine(const std::filesystem::path& executable, const std::vector<std::string>& arguments) {
	std::wstring line = quote(executable.wstring());
	for (const std::string& argument : arguments)
		line += L" " + quote(widen(argument));
	return line;
}

ChildProcess::ChildProcess(Options options) : options_(std::move(options)) {
	// every handle below belongs to a Handle guard, so each of the throwing paths closes what was opened before it (see Handle)
	std::filesystem::create_directories(options_.logFile.parent_path());
	SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
	Handle log(CreateFileW(options_.logFile.wstring().c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &inherit,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
	if (!log.valid()) {
		DWORD errorCode = GetLastError();
		throw std::runtime_error("cannot create " + options_.logFile.string() + " (error " + std::to_string(errorCode) + ")");
	}
	Handle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, nullptr));
	Handle errorFile;
	if (!options_.errorFile.empty()) {
		std::filesystem::create_directories(options_.errorFile.parent_path()); // throws if a path component is a file: the guards above close
		errorFile.reset(CreateFileW(options_.errorFile.wstring().c_str(), FILE_APPEND_DATA,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &inherit, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
		if (!errorFile.valid()) {
			DWORD errorCode = GetLastError();
			throw std::runtime_error("cannot create " + options_.errorFile.string() + " (error " + std::to_string(errorCode) + ")");
		}
	}
	HANDLE errors = errorFile.valid() ? errorFile.get() : log.get();

	// A job object with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE, so Windows reaps the child whenever THIS process goes away - not only through the
	// destructor. Without it, a CTest TIMEOUT (900 s, while ScenarioServers alone budgets a 10 min startup and Oracle::run waits up to 20 min)
	// or a crash of the gate leaves aion_game_server and aion_login_server running, holding their test schemas and the log directory. Measured:
	// killing only the test process left both servers alive. The process is created suspended and resumed after the assignment so it cannot
	// spawn anything outside the job.
	Handle jobObject(CreateJobObjectW(nullptr, nullptr));
	if (jobObject.valid()) {
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
		limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		if (!SetInformationJobObject(jobObject.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
			jobObject.reset();
	}

	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdInput = input.get();
	startup.hStdOutput = log.get();
	startup.hStdError = errors;
	PROCESS_INFORMATION info{};
	std::wstring line = commandLine(options_.executable, options_.arguments);
	DWORD flags = options_.newProcessGroup ? CREATE_NEW_PROCESS_GROUP : 0;
	if (jobObject.valid())
		flags |= CREATE_SUSPENDED;
	std::wstring directory = options_.workingDirectory.wstring();
	BOOL created = CreateProcessW(options_.executable.wstring().c_str(), line.data(), nullptr, nullptr, TRUE, flags, nullptr,
		directory.empty() ? nullptr : directory.c_str(), &startup, &info);
	DWORD error = GetLastError();
	if (!created)
		throw std::runtime_error("cannot start " + options_.executable.string() + " (error " + std::to_string(error) + ")");
	Handle processHandle(info.hProcess);
	Handle threadHandle(info.hThread);
	if (jobObject.valid()) {
		if (!AssignProcessToJobObject(jobObject.get(), processHandle.get()))
			jobObject.reset(); // no job: the destructor is the only reaper again, as before
		ResumeThread(threadHandle.get()); // always resumed, assigned or not, or the child would never run
	}
	job = jobObject.release();
	process = processHandle.release();
	pid = info.dwProcessId;
}

ChildProcess::~ChildProcess() {
	if (process != nullptr) {
		if (isRunning()) {
			TerminateProcess(static_cast<HANDLE>(process), 99);
			WaitForSingleObject(static_cast<HANDLE>(process), 10000);
		}
		CloseHandle(static_cast<HANDLE>(process));
	}
	if (job != nullptr)
		CloseHandle(static_cast<HANDLE>(job)); // KILL_ON_JOB_CLOSE: anything the child left behind dies with this handle
}

std::optional<int32_t> ChildProcess::waitForExit(std::chrono::milliseconds timeout) {
	DWORD result = WaitForSingleObject(static_cast<HANDLE>(process), static_cast<DWORD>(timeout.count()));
	if (result != WAIT_OBJECT_0)
		return std::nullopt;
	DWORD code = 0;
	GetExitCodeProcess(static_cast<HANDLE>(process), &code);
	return static_cast<int32_t>(code);
}

bool ChildProcess::isRunning() {
	return WaitForSingleObject(static_cast<HANDLE>(process), 0) == WAIT_TIMEOUT;
}

bool ChildProcess::sendCtrlBreak() {
	return GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid) != 0;
}

void ChildProcess::terminate(uint32_t exitCode) {
	if (isRunning()) {
		TerminateProcess(static_cast<HANDLE>(process), exitCode);
		WaitForSingleObject(static_cast<HANDLE>(process), 10000);
	}
}

std::string ChildProcess::readLog() const {
	std::ifstream in(options_.logFile, std::ios::binary);
	std::stringstream content;
	content << in.rdbuf();
	return content.str();
}

std::string ChildProcess::readLogTail(size_t maxBytes) const {
	std::ifstream in(options_.logFile, std::ios::binary);
	if (!in)
		return {};
	in.seekg(0, std::ios::end);
	uint64_t size = static_cast<uint64_t>(in.tellg());
	uint64_t from = size > maxBytes ? size - maxBytes : 0;
	in.seekg(static_cast<std::streamoff>(from), std::ios::beg);
	std::stringstream content;
	content << in.rdbuf();
	std::string tail = content.str();
	if (from > 0) {
		size_t newline = tail.find('\n');
		if (newline != std::string::npos)
			tail.erase(0, newline + 1);
	}
	return tail;
}

std::vector<std::string> ChildProcess::findLogLines(std::string_view text, size_t maxMatches) const {
	std::vector<std::string> matches;
	std::ifstream in(options_.logFile, std::ios::binary);
	std::string line;
	while (matches.size() < maxMatches && std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (line.find(text) != std::string::npos)
			matches.push_back(line);
	}
	return matches;
}

bool ChildProcess::scanLogFor(std::string_view text) {
	// only the bytes that appeared since the last call are read: a game server that hits an unported body inside spawnAll writes a stack trace
	// per object and grows its log to hundreds of megabytes, and re-reading it on every 100 ms poll takes longer than the startup itself
	uint64_t& offset = scanned[std::string(text)];
	std::ifstream in(options_.logFile, std::ios::binary);
	if (!in)
		return false;
	// the text may straddle the end of what the last call read, so the last text.size() - 1 bytes are read again
	uint64_t overlap = text.empty() ? 0 : text.size() - 1;
	uint64_t from = offset > overlap ? offset - overlap : 0;
	in.seekg(static_cast<std::streamoff>(from), std::ios::beg);
	if (!in)
		return false;
	std::stringstream content;
	content << in.rdbuf();
	std::string chunk = content.str();
	size_t found = chunk.find(text);
	if (found != std::string::npos) {
		offset = from + found; // stays at the match, so asking for the same text again finds it again
		return true;
	}
	offset = from + chunk.size();
	return false;
}

bool ChildProcess::waitForLog(std::string_view text, std::chrono::milliseconds timeout) {
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	for (;;) {
		bool running = isRunning();
		if (scanLogFor(text))
			return true;
		if (!running || std::chrono::steady_clock::now() >= deadline)
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

} // namespace aion::gameserver::scenario
