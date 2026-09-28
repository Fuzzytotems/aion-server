#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

/**
 * Java: com.aionemu.commons.utils.info.SystemInfo - information about the operating system, the program and its memory usage, logged at
 * server start and shown by the //sys admin command.
 * <p>
 * The JVM specific values are replaced by their native equivalents:
 * <table>
 * <tr><th>Java</th><th>C++</th></tr>
 * <tr><td>JVM: name, version and release date</td><td>C++: compiler, version, language standard and build type</td></tr>
 * <tr><td>JVM available CPUs</td><td>CPUs the process may run on (affinity)</td></tr>
 * <tr><td>Max. memory allowed (maximum heap size)</td><td>physical memory of the system</td></tr>
 * <tr><td>Allocated memory (heap size)</td><td>committed private memory of the process</td></tr>
 * <tr><td>Used memory (used heap)</td><td>private physical memory of the process (Windows: private working set, or the working set on
 * Windows versions before 10 21H1; Linux: resident set size)</td></tr>
 * </table>
 * Windows and Linux are supported; elsewhere unknown values are reported as such.
 *
 * @author lord_rex, Neon
 */
namespace aion::commons::utils::info::SystemInfo {

/**
 * @return lines like
 * <pre>
 * Max. memory allowed: 32,617 MiB
 * ├ Allocated memory:     143 MiB   (0 %)
 * └ Used memory:          120 MiB   (0 %)
 * </pre>
 */
std::vector<std::string> getMemoryInfo();

/**
 * @return lines like
 * <pre>
 * OS:  Windows 11 (amd64) version 10.0
 * C++: MSVC version 19.51.36231 (C++23, debug build)
 * Available CPUs:         16
 * </pre>
 */
std::vector<std::string> getSystemInfo();

/** Logs the system and memory information lines. */
void logAll();

/** @return the compiler name and version, e.g. "MSVC 19.51.36231" */
std::string getCompilerVersion();

/** @return the C++ standard the program was compiled for, e.g. "C++23" */
std::string getLanguageStandard();

/** @return the path of the running executable, empty if unknown */
std::filesystem::path getExecutablePath();

/** Java: ManagementFactory.getRuntimeMXBean().getStartTime() - the time the process was started */
std::chrono::system_clock::time_point getProcessStartTime();

namespace detail {

/** Java: new DecimalFormat("#,##0 'MiB'").format(value). Exposed for tests. */
std::string formatMiB(double mebibytes);

/** Java: new DecimalFormat("(0 %)").format(fraction). Exposed for tests. */
std::string formatPercent(double fraction);

/** The memory lines for the given values in MiB. Exposed for tests. */
std::vector<std::string> formatMemoryInfo(double max, double allocated, double used);

} // namespace detail

} // namespace aion::commons::utils::info::SystemInfo
