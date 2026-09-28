#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <typeinfo>
#include <vector>

/**
 * Java: com.aionemu.commons.utils.concurrent.RunnableStatsManager - collects execution time statistics (count, total, min, max) per class and
 * method, when commons.runnablestats.enable is set (see CommonsConfig::RUNNABLESTATS_ENABLE; the callers check the flag), and dumps them as
 * XML to log/stats/MethodStats.log.
 * <p>
 * Classes are identified by their std::type_info (Java: Class), usually the dynamic type of the executed object:
 * <pre>
 * RunnableStatsManager::handleStats(typeid(packet), "runImpl()", durationNanos);
 * </pre>
 * All functions are thread safe.
 *
 * @author NB4L1
 */
namespace aion::commons::utils::concurrent::RunnableStatsManager {

/** Java: SortBy - the order of the dumped entries. Numbers are sorted descending, names ascending. */
enum class SortBy { AVG, COUNT, TOTAL, NAME, METHOD, MIN, MAX };

/** Adds one execution of the run() method of the given runnable class. */
void handleStats(const std::type_info& type, int64_t runTime);

/** Adds one execution of the named method of the given class (e.g. "runImpl()"). */
void handleStats(const std::type_info& type, std::string_view methodName, int64_t runTime);

/**
 * C++ addition: adds one execution of the named method of a class given by name instead of a type, for work whose C++ type says nothing
 * (type-erased tasks, lambdas), e.g. <tt>handleStats("ai/instance/darkPoeta/CalindiFlamelordAI.cpp:88", "run()", nanos)</tt>. The key is
 * shown verbatim as the class name. Statistics are kept per displayed class name, so a key equal to the displayed name of a std::type_info
 * (the C++ name without the "aion::gameserver::" prefix) shares that type's entries.
 */
void handleStats(std::string_view key, std::string_view methodName, int64_t runTime);

/** Writes the statistics of all methods executed at least once to ./log/stats/MethodStats.log, in unspecified order. */
void dumpClassStats();

/** Writes the statistics of all methods executed at least once to ./log/stats/MethodStats.log, sorted as specified (or unsorted if empty). */
void dumpClassStats(std::optional<SortBy> sortBy);

/** Like dumpClassStats(sortBy), but writes to the given file. Errors are logged as warnings. */
void dumpClassStats(std::optional<SortBy> sortBy, const std::filesystem::path& file);

/**
 * Resets all statistics, as if nothing had been executed yet. Not in Java; used to isolate tests and to restart measurements.
 * Executions recorded concurrently with clear() may be kept or discarded.
 */
void clear();

/** @return the lines that dumpClassStats writes (an XML document) */
std::vector<std::string> getClassStatsLines(std::optional<SortBy> sortBy);

} // namespace aion::commons::utils::concurrent::RunnableStatsManager
