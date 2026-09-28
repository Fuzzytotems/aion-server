#include "aion/commons/utils/info/SystemInfo.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <thread>

#include <fmt/format.h>

#ifdef _WIN32
#include <windows.h>
// windows.h must come first
#include <psapi.h>
#else
#include <sys/utsname.h>
#include <unistd.h>
#ifdef __linux__
#include <sched.h>
#endif
#endif

#include "aion/commons/logging/LoggerFactory.h"

namespace aion::commons::utils::info::SystemInfo {

namespace {

/** Fallback for getProcessStartTime: approximately the start time, captured during static initialization. */
const std::chrono::system_clock::time_point STATIC_INIT_TIME = std::chrono::system_clock::now();

constexpr double MIB = 1024.0 * 1024.0;

const logging::Logger& log() {
	static const logging::Logger instance = logging::LoggerFactory::getLogger("com.aionemu.commons.utils.info.SystemInfo");
	return instance;
}

/** Java: String.format("%" + width + "s", text) */
std::string leftPad(std::string_view text, size_t width) {
	return fmt::format("{:>{}}", text, width);
}

/** Java: System.getProperty("os.arch") - the architecture the program runs as */
std::string_view osArch() {
#if defined(_M_X64) || defined(__x86_64__)
	return "amd64";
#elif defined(_M_ARM64) || defined(__aarch64__)
	return "aarch64";
#elif defined(_M_IX86) || defined(__i386__)
	return "x86";
#else
	return "unknown";
#endif
}

struct OsNameAndVersion {
	std::string name;
	std::string version;
};

#ifdef _WIN32
/** Java: os.name and os.version as determined by the JDK (java_props_md.c) */
OsNameAndVersion osNameAndVersion() {
	OSVERSIONINFOEXW info{};
	info.dwOSVersionInfoSize = sizeof(info);
	// GetVersionEx reports Windows 8 to programs without a compatibility manifest, RtlGetVersion does not
	using RtlGetVersionFunction = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
	HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	auto rtlGetVersion = ntdll ? reinterpret_cast<RtlGetVersionFunction>(GetProcAddress(ntdll, "RtlGetVersion")) : nullptr;
	if (!rtlGetVersion || rtlGetVersion(reinterpret_cast<PRTL_OSVERSIONINFOW>(&info)) != 0)
		return {"Windows NT (unknown)", "unknown"};
	DWORD major = info.dwMajorVersion;
	DWORD minor = info.dwMinorVersion;
	DWORD build = info.dwBuildNumber;
	bool workstation = info.wProductType == VER_NT_WORKSTATION;
	std::string name = "Windows NT (unknown)";
	if (major == 10 && minor == 0) {
		if (workstation)
			name = build >= 22000 ? "Windows 11" : "Windows 10";
		else
			name = build > 26039 ? "Windows Server 2025" : build > 20347 ? "Windows Server 2022" : build > 17762 ? "Windows Server 2019" : "Windows Server 2016";
	} else if (major == 6) {
		switch (minor) {
			case 0:
				name = workstation ? "Windows Vista" : "Windows Server 2008";
				break;
			case 1:
				name = workstation ? "Windows 7" : "Windows Server 2008 R2";
				break;
			case 2:
				name = workstation ? "Windows 8" : "Windows Server 2012";
				break;
			case 3:
				name = workstation ? "Windows 8.1" : "Windows Server 2012 R2";
				break;
		}
	}
	return {name, fmt::format("{}.{}", major, minor)};
}
#else
OsNameAndVersion osNameAndVersion() {
	utsname names{};
	if (uname(&names) != 0)
		return {"unknown", "unknown"};
	return {names.sysname, names.release};
}
#endif

/** Java: Runtime.availableProcessors() */
uint32_t availableProcessors() {
#ifdef _WIN32
	DWORD_PTR processMask = 0;
	DWORD_PTR systemMask = 0;
	if (GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask) && processMask != 0)
		return static_cast<uint32_t>(std::popcount(static_cast<uint64_t>(processMask)));
	DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS); // processes spanning several processor groups
	if (count > 0)
		return count;
#elif defined(__linux__)
	cpu_set_t set;
	CPU_ZERO(&set);
	if (sched_getaffinity(0, sizeof(set), &set) == 0)
		return static_cast<uint32_t>(CPU_COUNT(&set));
#endif
	return std::max(1u, std::thread::hardware_concurrency());
}

struct MemoryBytes {
	double total = 0;
	double allocated = 0;
	double used = 0;
};

MemoryBytes memoryBytes() {
	MemoryBytes bytes;
#ifdef _WIN32
	MEMORYSTATUSEX status{};
	status.dwLength = sizeof(status);
	if (GlobalMemoryStatusEx(&status))
		bytes.total = static_cast<double>(status.ullTotalPhys);
	// PROCESS_MEMORY_COUNTERS_EX2 (Windows 10 21H1+, not declared by older SDKs): adds the private working set, which, unlike the working set,
	// does not count shared pages (DLLs) and therefore fits "used" as a part of the committed private memory
	struct ProcessMemoryCountersEx2 {
		PROCESS_MEMORY_COUNTERS_EX base;
		SIZE_T PrivateWorkingSetSize;
		ULONG64 SharedCommitUsage;
	} counters{};
	counters.base.cb = sizeof(counters);
	if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
		bytes.allocated = static_cast<double>(counters.base.PrivateUsage);
		bytes.used = static_cast<double>(counters.PrivateWorkingSetSize);
	} else if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters.base), sizeof(counters.base))) {
		bytes.allocated = static_cast<double>(counters.base.PrivateUsage);
		bytes.used = static_cast<double>(counters.base.WorkingSetSize);
	}
#else
	long pageSize = sysconf(_SC_PAGESIZE);
	long pages = sysconf(_SC_PHYS_PAGES);
	if (pageSize > 0 && pages > 0)
		bytes.total = static_cast<double>(pageSize) * static_cast<double>(pages);
#ifdef __linux__
	std::ifstream statm("/proc/self/statm"); // size resident shared text lib data dt, in pages
	double size, resident, shared, text, lib, data;
	if (pageSize > 0 && statm >> size >> resident >> shared >> text >> lib >> data) {
		bytes.allocated = data * static_cast<double>(pageSize);
		bytes.used = resident * static_cast<double>(pageSize);
	}
#endif
#endif
	return bytes;
}

std::string_view compilerName() {
#if defined(__clang__)
	return "Clang";
#elif defined(_MSC_VER)
	return "MSVC";
#elif defined(__GNUC__)
	return "GCC";
#else
	return "unknown compiler";
#endif
}

std::string compilerVersionNumber() {
#if defined(__clang__)
	return fmt::format("{}.{}.{}", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(_MSC_VER)
	return fmt::format("{}.{}.{}", _MSC_VER / 100, _MSC_VER % 100, _MSC_FULL_VER % 100000);
#elif defined(__GNUC__)
	return fmt::format("{}.{}.{}", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
	return "unknown";
#endif
}

} // namespace

namespace detail {

std::string formatMiB(double mebibytes) {
	if (std::isnan(mebibytes))
		return "NaN";
	int64_t value = static_cast<int64_t>(std::nearbyint(mebibytes)); // round half even, like DecimalFormat
	std::string digits = std::to_string(value < 0 ? -value : value);
	std::string result = value < 0 ? "-" : "";
	for (size_t i = 0; i < digits.size(); i++) {
		if (i > 0 && (digits.size() - i) % 3 == 0)
			result += ',';
		result += digits[i];
	}
	return result + " MiB";
}

std::string formatPercent(double fraction) {
	if (std::isnan(fraction))
		return "NaN";
	if (std::isinf(fraction))
		return fraction > 0 ? "(∞ %)" : "(-∞ %)";
	return fmt::format("({} %)", static_cast<int64_t>(std::nearbyint(fraction * 100)));
}

std::vector<std::string> formatMemoryInfo(double max, double allocated, double used) {
	return {
		"Max. memory allowed: " + leftPad(formatMiB(max), 9),
		"├ Allocated memory:  " + leftPad(formatMiB(allocated), 9) + leftPad(formatPercent(allocated / max), 8),
		"└ Used memory:       " + leftPad(formatMiB(used), 9) + leftPad(formatPercent(used / max), 8),
	};
}

} // namespace detail

std::vector<std::string> getMemoryInfo() {
	MemoryBytes bytes = memoryBytes();
	// Java divides the byte counts with integer division
	return detail::formatMemoryInfo(std::floor(bytes.total / MIB), std::floor(bytes.allocated / MIB), std::floor(bytes.used / MIB));
}

std::vector<std::string> getSystemInfo() {
	uint32_t availableCPUs = availableProcessors();
	std::string totalCPUs;
	if (const char* numberOfProcessors = std::getenv("NUMBER_OF_PROCESSORS"))
		totalCPUs = numberOfProcessors;
	else
		totalCPUs = std::to_string(std::thread::hardware_concurrency());
	totalCPUs = totalCPUs.empty() || totalCPUs == std::to_string(availableCPUs) ? "" : leftPad(" (of " + totalCPUs + ')', 12);

	OsNameAndVersion os = osNameAndVersion();
#ifdef NDEBUG
	constexpr std::string_view buildType = "release";
#else
	constexpr std::string_view buildType = "debug";
#endif
	return {
		fmt::format("OS:  {} ({}) version {}", os.name, osArch(), os.version),
		fmt::format("C++: {} version {} ({}, {} build)", compilerName(), compilerVersionNumber(), getLanguageStandard(), buildType),
		"Available CPUs:     " + leftPad(std::to_string(availableCPUs), 6) + totalCPUs,
	};
}

void logAll() {
	for (const auto& info : {getSystemInfo(), getMemoryInfo()}) {
		for (const std::string& line : info)
			log().info(std::string_view(line));
	}
}

std::string getCompilerVersion() {
	return std::string(compilerName()) + " " + compilerVersionNumber();
}

std::string getLanguageStandard() {
#ifdef _MSVC_LANG
	long standard = _MSVC_LANG;
#else
	long standard = __cplusplus;
#endif
	// intermediate values denote a draft of the next standard, e.g. MSVC's /std:c++latest (used for C++23) reports 202400
	if (standard >= 202600L)
		return "C++26";
	if (standard > 202002L)
		return "C++23";
	if (standard > 201703L)
		return "C++20";
	return "C++17";
}

std::filesystem::path getExecutablePath() {
#ifdef _WIN32
	std::wstring buffer(MAX_PATH, L'\0');
	while (true) {
		DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (length == 0)
			return {};
		if (length < buffer.size()) {
			buffer.resize(length);
			return std::filesystem::path(buffer);
		}
		buffer.resize(buffer.size() * 2);
	}
#elif defined(__linux__)
	std::error_code error;
	std::filesystem::path path = std::filesystem::read_symlink("/proc/self/exe", error);
	return error ? std::filesystem::path() : path;
#else
	return {};
#endif
}

std::chrono::system_clock::time_point getProcessStartTime() {
#ifdef _WIN32
	FILETIME creation{};
	FILETIME exit{};
	FILETIME kernel{};
	FILETIME user{};
	if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
		constexpr int64_t FILETIME_UNIX_EPOCH = 116444736000000000LL; // 100 ns intervals from 1601-01-01 to 1970-01-01
		int64_t intervals = (static_cast<int64_t>(creation.dwHighDateTime) << 32) | creation.dwLowDateTime;
		auto sinceEpoch = std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>(intervals - FILETIME_UNIX_EPOCH);
		return std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(sinceEpoch));
	}
#elif defined(__linux__)
	try {
		std::ifstream statFile("/proc/self/stat");
		std::string stat((std::istreambuf_iterator<char>(statFile)), std::istreambuf_iterator<char>());
		std::ifstream systemStat("/proc/stat");
		long clockTicks = sysconf(_SC_CLK_TCK);
		size_t commandEnd = stat.rfind(')'); // the command name may contain spaces
		if (commandEnd != std::string::npos && clockTicks > 0) {
			std::istringstream fields(stat.substr(commandEnd + 2)); // starts with field 3
			std::string field;
			for (int i = 3; i <= 22 && fields >> field; i++) {
				if (i != 22) // starttime, in clock ticks after boot
					continue;
				long double startTicks = std::stold(field);
				for (std::string line; std::getline(systemStat, line);) {
					if (line.starts_with("btime ")) {
						auto bootSeconds = std::stoll(line.substr(6));
						auto sinceEpoch = std::chrono::duration<long double>(bootSeconds + startTicks / clockTicks);
						return std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(sinceEpoch));
					}
				}
			}
		}
	} catch (const std::exception&) {
		// fall back to the static initialization time
	}
#endif
	return STATIC_INIT_TIME;
}

} // namespace aion::commons::utils::info::SystemInfo
