#include "ChildProcess.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "aion/commons/utils/StringUtils.h"

#include <Windows.h>

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::chatserver::test {

namespace {

std::wstring widen(std::string_view utf8) {
	std::u16string utf16 = commons::utils::StringUtils::toUtf16(utf8);
	return std::wstring(utf16.begin(), utf16.end());
}

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

ChildProcess::ChildProcess(const std::filesystem::path& executable, const std::vector<std::string>& arguments,
	const std::filesystem::path& workingDirectory, const std::filesystem::path& logFileValue)
	: logFile(logFileValue) {
	std::filesystem::create_directories(logFile.parent_path());
	SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
	HANDLE log = CreateFileW(logFile.wstring().c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &inherit, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (log == INVALID_HANDLE_VALUE)
		throw std::runtime_error("cannot create " + logFile.string() + " (error " + std::to_string(GetLastError()) + ")");
	HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, nullptr);

	HANDLE jobObject = CreateJobObjectW(nullptr, nullptr);
	if (jobObject) {
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
		limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		if (!SetInformationJobObject(jobObject, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
			CloseHandle(jobObject);
			jobObject = nullptr;
		}
	}

	std::wstring line = quote(executable.wstring());
	for (const std::string& argument : arguments)
		line += L" " + quote(widen(argument));
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdInput = input;
	startup.hStdOutput = log;
	startup.hStdError = log;
	PROCESS_INFORMATION info{};
	std::wstring directory = workingDirectory.wstring();
	BOOL created = CreateProcessW(executable.wstring().c_str(), line.data(), nullptr, nullptr, TRUE, jobObject ? CREATE_SUSPENDED : 0, nullptr,
		directory.c_str(), &startup, &info);
	DWORD error = GetLastError();
	CloseHandle(log);
	if (input != INVALID_HANDLE_VALUE)
		CloseHandle(input);
	if (!created) {
		if (jobObject)
			CloseHandle(jobObject);
		throw std::runtime_error("cannot start " + executable.string() + " (error " + std::to_string(error) + ")");
	}
	if (jobObject) {
		if (!AssignProcessToJobObject(jobObject, info.hProcess)) {
			CloseHandle(jobObject);
			jobObject = nullptr;
		}
		ResumeThread(info.hThread);
	}
	CloseHandle(info.hThread);
	process = info.hProcess;
	job = jobObject;
}

ChildProcess::~ChildProcess() {
	if (process) {
		if (isRunning()) {
			TerminateProcess(static_cast<HANDLE>(process), 99);
			WaitForSingleObject(static_cast<HANDLE>(process), 10000);
		}
		CloseHandle(static_cast<HANDLE>(process));
	}
	if (job)
		CloseHandle(static_cast<HANDLE>(job));
}

std::optional<int32_t> ChildProcess::waitForExit(std::chrono::milliseconds timeout) {
	if (WaitForSingleObject(static_cast<HANDLE>(process), static_cast<DWORD>(timeout.count())) != WAIT_OBJECT_0)
		return std::nullopt;
	DWORD code = 0;
	GetExitCodeProcess(static_cast<HANDLE>(process), &code);
	return static_cast<int32_t>(code);
}

bool ChildProcess::isRunning() {
	return WaitForSingleObject(static_cast<HANDLE>(process), 0) == WAIT_TIMEOUT;
}

std::string ChildProcess::readLog() const {
	std::ifstream in(logFile, std::ios::binary);
	std::stringstream content;
	content << in.rdbuf();
	return content.str();
}

bool ChildProcess::waitForLog(std::string_view text, std::chrono::milliseconds timeout) {
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	for (;;) {
		bool running = isRunning();
		if (readLog().find(text) != std::string::npos)
			return true;
		if (!running || std::chrono::steady_clock::now() >= deadline)
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

} // namespace aion::chatserver::test
