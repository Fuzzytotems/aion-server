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
	std::filesystem::create_directories(options_.logFile.parent_path());
	SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
	HANDLE log = CreateFileW(options_.logFile.wstring().c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &inherit,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (log == INVALID_HANDLE_VALUE)
		throw std::runtime_error("cannot create " + options_.logFile.string() + " (error " + std::to_string(GetLastError()) + ")");
	HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, nullptr);

	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdInput = input;
	startup.hStdOutput = log;
	startup.hStdError = log;
	PROCESS_INFORMATION info{};
	std::wstring line = commandLine(options_.executable, options_.arguments);
	DWORD flags = options_.newProcessGroup ? CREATE_NEW_PROCESS_GROUP : 0;
	std::wstring directory = options_.workingDirectory.wstring();
	BOOL created = CreateProcessW(options_.executable.wstring().c_str(), line.data(), nullptr, nullptr, TRUE, flags, nullptr,
		directory.empty() ? nullptr : directory.c_str(), &startup, &info);
	DWORD error = GetLastError();
	CloseHandle(log);
	if (input != INVALID_HANDLE_VALUE)
		CloseHandle(input);
	if (!created)
		throw std::runtime_error("cannot start " + options_.executable.string() + " (error " + std::to_string(error) + ")");
	CloseHandle(info.hThread);
	process = info.hProcess;
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

} // namespace aion::gameserver::scenario
