#include "aion/commons/utils/concurrent/RunnableStatsManager.h"

#include <algorithm>
#include <array>
#include <compare>
#include <fstream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <typeindex>
#include <unordered_map>
#include <variant>

#include <magic_enum/magic_enum.hpp>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/ClassName.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::utils::concurrent::RunnableStatsManager {

namespace {

const logging::Logger& log() {
	static const logging::Logger instance = logging::LoggerFactory::getLogger("com.aionemu.commons.utils.concurrent.RunnableStatsManager");
	return instance;
}

/** A consistent copy of a MethodStat's values */
struct MethodStatSnapshot {
	std::string className;
	std::string methodName;
	int64_t count = 0;
	int64_t total = 0;
	int64_t min = 0;
	int64_t max = 0;
};

class MethodStat {
public:
	MethodStat(std::string className, std::string methodName) : className(std::move(className)), methodName(std::move(methodName)) {}

	const std::string& getMethodName() const noexcept { return methodName; }

	void handleStats(int64_t runTime) {
		std::lock_guard lock(mutex);
		count++;
		total = static_cast<int64_t>(static_cast<uint64_t>(total) + static_cast<uint64_t>(runTime)); // wraps like Java instead of UB
		min = std::min(min, runTime);
		max = std::max(max, runTime);
	}

	std::optional<MethodStatSnapshot> snapshotIfExecuted() const {
		std::lock_guard lock(mutex);
		if (count <= 0)
			return std::nullopt;
		return MethodStatSnapshot{className, methodName, count, total, min, max};
	}

	void reset() {
		std::lock_guard lock(mutex);
		count = 0;
		total = 0;
		min = INT64_MAX;
		max = INT64_MIN;
	}

private:
	const std::string className;
	const std::string methodName;
	mutable std::mutex mutex;
	int64_t count = 0;
	int64_t total = 0;
	int64_t min = INT64_MAX;
	int64_t max = INT64_MIN;
};

class ClassStat {
public:
	explicit ClassStat(const std::type_info& type)
		: className(StringUtils::replace(getClassName(type), "aion::gameserver::", "")), runnableStat(className, "run()") {}

	MethodStat& getRunnableStat() noexcept { return runnableStat; }

	MethodStat& getMethodStat(std::string_view methodName) {
		if (methodName == "run()")
			return runnableStat;
		{
			std::shared_lock lock(mutex);
			if (MethodStat* stat = find(methodName))
				return *stat;
		}
		std::unique_lock lock(mutex);
		if (MethodStat* stat = find(methodName))
			return *stat;
		return *methodStats.emplace_back(std::make_unique<MethodStat>(className, std::string(methodName)));
	}

	void collectExecuted(std::vector<MethodStatSnapshot>& out) const {
		if (auto snapshot = runnableStat.snapshotIfExecuted())
			out.push_back(std::move(*snapshot));
		std::shared_lock lock(mutex);
		for (const auto& stat : methodStats) {
			if (auto snapshot = stat->snapshotIfExecuted())
				out.push_back(std::move(*snapshot));
		}
	}

	void reset() {
		runnableStat.reset();
		std::shared_lock lock(mutex);
		for (const auto& stat : methodStats)
			stat->reset();
	}

private:
	MethodStat* find(std::string_view methodName) const {
		for (const auto& stat : methodStats) {
			if (stat->getMethodName() == methodName)
				return stat.get();
		}
		return nullptr;
	}

	const std::string className;
	MethodStat runnableStat;
	mutable std::shared_mutex mutex;
	std::vector<std::unique_ptr<MethodStat>> methodStats; // guarded by mutex, elements have stable addresses
};

struct Registry {
	std::shared_mutex mutex;
	std::unordered_map<std::type_index, std::unique_ptr<ClassStat>> classStats;
};

Registry& registry() {
	static Registry instance;
	return instance;
}

ClassStat& getClassStat(const std::type_info& type) {
	Registry& r = registry();
	std::type_index key(type);
	{
		std::shared_lock lock(r.mutex);
		if (auto it = r.classStats.find(key); it != r.classStats.end())
			return *it->second;
	}
	std::unique_lock lock(r.mutex);
	auto [it, inserted] = r.classStats.try_emplace(key);
	if (inserted)
		it->second = std::make_unique<ClassStat>(type);
	return *it->second;
}

constexpr auto SORT_BY_VALUES = magic_enum::enum_values<SortBy>();

/** Java: SortBy.xmlAttributeName */
std::string_view xmlAttributeName(SortBy sortBy) noexcept {
	switch (sortBy) {
		case SortBy::AVG:
			return "average";
		case SortBy::COUNT:
			return "count";
		case SortBy::TOTAL:
			return "total";
		case SortBy::NAME:
			return "class";
		case SortBy::METHOD:
			return "method";
		case SortBy::MIN:
			return "min";
		case SortBy::MAX:
			return "max";
	}
	return {};
}

using ComparableValue = std::variant<int64_t, std::string_view>;

/** Java: SortBy.getComparableValueOf */
ComparableValue getComparableValueOf(SortBy sortBy, const MethodStatSnapshot& stat) {
	switch (sortBy) {
		case SortBy::AVG:
			return stat.total / stat.count; // count is positive for all dumped stats
		case SortBy::COUNT:
			return stat.count;
		case SortBy::TOTAL:
			return stat.total;
		case SortBy::NAME:
			return std::string_view(stat.className);
		case SortBy::METHOD:
			return std::string_view(stat.methodName);
		case SortBy::MIN:
			return stat.min;
		case SortBy::MAX:
			return stat.max;
	}
	throw IllegalStateException("Unknown SortBy value");
}

/**
 * Java: the string part of SortBy.comparator. Compares by the first differing char, then by length (shorter first).
 * <p>
 * Deviation: for differing chars where exactly one is upper case, Java puts the char with the larger code first, otherwise the smaller one.
 * That is not transitive ('0' &lt; '[' &lt; 'Z' &lt; '0'), but std::stable_sort requires a strict weak ordering. Here all other chars sort
 * before upper case chars and chars of the same group by code. This matches Java whenever letters, '_' or other chars above 'Z' are compared.
 */
int compareNames(std::string_view s1, std::string_view s2) noexcept {
	auto key = [](char c) {
		bool upperCase = c >= 'A' && c <= 'Z';
		return std::pair(upperCase, static_cast<unsigned char>(c));
	};
	size_t n = std::min(s1.size(), s2.size());
	for (size_t k = 0; k < n; k++) {
		if (s1[k] != s2[k])
			return key(s1[k]) < key(s2[k]) ? -1 : 1;
	}
	return s1.size() < s2.size() ? -1 : s1.size() > s2.size() ? 1 : 0;
}

/** Java: SortBy.comparator */
int compare(SortBy sortBy, const MethodStatSnapshot& o1, const MethodStatSnapshot& o2) {
	ComparableValue c1 = getComparableValueOf(sortBy, o1);
	ComparableValue c2 = getComparableValueOf(sortBy, o2);
	if (std::holds_alternative<int64_t>(c1)) { // numbers are sorted descending
		auto order = std::get<int64_t>(c2) <=> std::get<int64_t>(c1);
		return order < 0 ? -1 : order > 0 ? 1 : 0;
	}
	int result = compareNames(std::get<std::string_view>(c1), std::get<std::string_view>(c2));
	if (result != 0)
		return result;
	return sortBy == SortBy::METHOD ? compare(SortBy::NAME, o1, o2) : 0;
}

/** Java: NumberFormat.getInstance(Locale.ENGLISH).format(long) */
std::string formatNumber(int64_t value) {
	uint64_t magnitude = value < 0 ? uint64_t{0} - static_cast<uint64_t>(value) : static_cast<uint64_t>(value);
	std::string digits = std::to_string(magnitude);
	std::string result = value < 0 ? "-" : "";
	for (size_t i = 0; i < digits.size(); i++) {
		if (i > 0 && (digits.size() - i) % 3 == 0)
			result += ',';
		result += digits[i];
	}
	return result;
}

std::string toString(const ComparableValue& value) {
	if (auto* number = std::get_if<int64_t>(&value))
		return formatNumber(*number);
	return std::string(std::get<std::string_view>(value));
}

void appendAttribute(std::string& sb, SortBy sortBy, const std::string& value, int32_t fillTo) {
	sb += xmlAttributeName(sortBy);
	sb += '=';
	bool isName = sortBy == SortBy::NAME || sortBy == SortBy::METHOD;
	int32_t padding = std::max(0, fillTo - StringUtils::utf16Length(value));
	if (!isName)
		sb.append(static_cast<size_t>(padding), ' ');
	sb += '"';
	sb += value;
	sb += "\" ";
	if (isName)
		sb.append(static_cast<size_t>(padding), ' ');
}

} // namespace

void handleStats(const std::type_info& type, int64_t runTime) {
	getClassStat(type).getRunnableStat().handleStats(runTime);
}

void handleStats(const std::type_info& type, std::string_view methodName, int64_t runTime) {
	getClassStat(type).getMethodStat(methodName).handleStats(runTime);
}

std::vector<std::string> getClassStatsLines(std::optional<SortBy> sortBy) {
	std::vector<MethodStatSnapshot> methodStats;
	{
		Registry& r = registry();
		std::shared_lock lock(r.mutex);
		for (const auto& [type, classStat] : r.classStats)
			classStat->collectExecuted(methodStats);
	}

	if (sortBy)
		std::ranges::stable_sort(methodStats, [&](const auto& a, const auto& b) { return compare(*sortBy, a, b) < 0; });

	std::vector<std::string> lines;
	lines.emplace_back(R"(<?xml version="1.0" encoding="UTF-8" standalone="no"?>)");
	lines.emplace_back("<entries>");
	lines.emplace_back("\t<!-- This XML contains statistics about execution times. -->");
	lines.emplace_back("\t<!-- Submitted results will help the developers to optimize the server. -->");

	std::array<std::vector<std::string>, SORT_BY_VALUES.size()> values;
	std::array<int32_t, SORT_BY_VALUES.size()> maxLength{};
	for (SortBy sort : SORT_BY_VALUES) {
		size_t i = magic_enum::enum_integer(sort);
		for (const MethodStatSnapshot& stat : methodStats) {
			std::string value = toString(getComparableValueOf(sort, stat));
			maxLength[i] = std::max(maxLength[i], StringUtils::utf16Length(value));
			values[i].push_back(std::move(value));
		}
	}

	auto ordinal = [](SortBy sort) { return static_cast<size_t>(magic_enum::enum_integer(sort)); };
	for (size_t k = 0; k < methodStats.size(); k++) {
		std::string sb = "\t<entry ";
		std::array<bool, SORT_BY_VALUES.size()> set;
		set.fill(true);
		if (sortBy) {
			switch (*sortBy) {
				case SortBy::NAME:
				case SortBy::METHOD:
					appendAttribute(sb, SortBy::NAME, values[ordinal(SortBy::NAME)][k], maxLength[ordinal(SortBy::NAME)]);
					set[ordinal(SortBy::NAME)] = false;
					appendAttribute(sb, SortBy::METHOD, values[ordinal(SortBy::METHOD)][k], maxLength[ordinal(SortBy::METHOD)]);
					set[ordinal(SortBy::METHOD)] = false;
					break;
				default:
					appendAttribute(sb, *sortBy, values[ordinal(*sortBy)][k], maxLength[ordinal(*sortBy)]);
					set[ordinal(*sortBy)] = false;
					break;
			}
		}
		for (SortBy sort : SORT_BY_VALUES) {
			if (set[ordinal(sort)])
				appendAttribute(sb, sort, values[ordinal(sort)][k], maxLength[ordinal(sort)]);
		}
		sb += "/>";
		lines.push_back(std::move(sb));
	}

	lines.emplace_back("</entries>");
	return lines;
}

void clear() {
	Registry& r = registry();
	std::shared_lock lock(r.mutex);
	// reset in place: concurrent handleStats calls may hold references to the stat objects
	for (const auto& [type, classStat] : r.classStats)
		classStat->reset();
}

void dumpClassStats() {
	dumpClassStats(std::nullopt);
}

void dumpClassStats(std::optional<SortBy> sortBy) {
	dumpClassStats(sortBy, std::filesystem::path("./log/stats") / "MethodStats.log");
}

void dumpClassStats(std::optional<SortBy> sortBy, const std::filesystem::path& file) {
	try {
		std::vector<std::string> lines = getClassStatsLines(sortBy);
		if (file.has_parent_path())
			std::filesystem::create_directories(file.parent_path());
		std::ofstream out(file); // text mode: system line separators, like Java's PrintStream.println
		if (!out)
			throw IOException("Could not open " + file.string() + " for writing");
		for (const std::string& line : lines)
			out << line << '\n';
		out.close();
		if (!out)
			throw IOException("Could not write " + file.string());
	} catch (const std::exception& e) {
		log().warn("", e);
	}
}

} // namespace aion::commons::utils::concurrent::RunnableStatsManager
